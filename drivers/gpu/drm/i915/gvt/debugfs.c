#include <linux/module.h>
#include <linux/pci.h>
#include <linux/debugfs.h>

#include "i915_drv.h"
#include "debugfs.h"
#include "gvt.h"

#define DIR_NAME_LENGTH 13
static struct dentry *d_gvt_debug;
static struct dentry *d_per_vgpu[GVT_MAX_VGPU];
static char vgpu_dir_name[GVT_MAX_VGPU][DIR_NAME_LENGTH];

void intel_vgpu_cleanup_debug_statistics(struct intel_vgpu *vgpu)
{
	struct intel_engine_cs *engine;
	struct intel_vgpu_workload *pos;
	unsigned int i;

	for_each_engine(engine, vgpu->gvt->dev_priv, i) {
		/* free the vgpu statistics dispatched to host i915 */
		list_for_each_entry(pos,
			&vgpu->workload_q_head[engine->id], list) {
			if (pos->req && pos->req->perf.vgpu)
				pos->req->perf.vgpu = NULL;
		}

	}
}

static int per_engine_show(struct seq_file *m, void *data)
{
	u64 *value = (u64 *) m->private;
	int i;

	for (i = 0; i < I915_NUM_ENGINES; i++)
		seq_printf(m, "%lld\n", value[i]);

	return 0;
}

static int per_engine_open(struct inode *inode, struct file *file)
{
	return single_open(file, per_engine_show, inode->i_private);
}

static const struct file_operations per_engine_fops = {
	.open = per_engine_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


struct dentry *intel_gvt_init_debugfs(struct intel_gvt *gvt)
{
	if (!d_gvt_debug) {
		d_gvt_debug = debugfs_create_dir("gvt", NULL);

		if (!d_gvt_debug) {
			pr_warn("Could not create 'gvt' debugfs directory\n");
			return NULL;
		}
	}

	debugfs_create_file("wait_workload_cycles", 0444, d_gvt_debug,
			gvt->stat.wait_workload_cycles, &per_engine_fops);
	debugfs_create_file("pick_hit_cnt", 0444, d_gvt_debug,
			gvt->stat.pick_hit_cnt, &per_engine_fops);
	debugfs_create_file("pick_miss_cnt", 0444, d_gvt_debug,
			gvt->stat.pick_miss_cnt, &per_engine_fops);
	return d_gvt_debug;
}

void intel_vgpu_destroy_debugfs(struct intel_vgpu *vgpu)
{
	int vgpu_id = vgpu->id;

	debugfs_remove_recursive(d_per_vgpu[vgpu_id]);
}

static int mmio_accounting_show(struct seq_file *m, void *data)
{
	struct intel_vgpu *vgpu = (struct intel_vgpu *)m->private;
	struct vgpu_mmio_accounting_reg_stat *stat;
	unsigned long count;

	mutex_lock(&vgpu->stat.mmio_accounting_lock);

	if (!vgpu->stat.mmio_accounting_reg_stats)
		goto out;

	seq_puts(m, "* MMIO read statistics *\n");
	seq_puts(m, "------------------------\n");

	for (count = 0; count < (2 * 1024 * 1024 / 4); count++) {
		stat = &vgpu->stat.mmio_accounting_reg_stats[count];
		if (!stat->r_count)
			continue;

		seq_printf(m, "[ 0x%lx ]\t[ read ] count [ %llu ]\tcycles [ %llu ]\n",
				count * 4, stat->r_count, stat->r_cycles);
	}

	seq_puts(m, "\n");

	seq_puts(m, "* MMIO write statistics *\n");
	seq_puts(m, "-------------------------\n");

	for (count = 0; count < (2 * 1024 * 1024 / 4); count++) {
		stat = &vgpu->stat.mmio_accounting_reg_stats[count];
		if (!stat->w_count)
			continue;

		seq_printf(m, "[ 0x%lx ]\t[ write ] count [ %llu ]\tcycles [ %llu ]\n",
				count * 4, stat->w_count, stat->w_cycles);
	}
out:
	mutex_unlock(&vgpu->stat.mmio_accounting_lock);

	return 0;
}


static int mmio_accounting_open(struct inode *inode, struct file *file)
{
	return single_open(file, mmio_accounting_show, inode->i_private);
}

static ssize_t mmio_accounting_write(struct file *file,
		const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct intel_vgpu *vgpu = (struct intel_vgpu *)s->private;
	struct intel_gvt *gvt = vgpu->gvt;
	char buf[32];

	if (*ppos && count > sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, count))
		return -EFAULT;

	mutex_lock(&vgpu->stat.mmio_accounting_lock);

	if (!strncmp(buf, "start", 5)) {
		if (vgpu->stat.mmio_accounting) {
			pr_err("mmio accounting has already started.\n");
			goto out;
		}

		if (!vgpu->stat.mmio_accounting_reg_stats) {
			vgpu->stat.mmio_accounting_reg_stats = vzalloc(
				sizeof(struct vgpu_mmio_accounting_reg_stat)
				* (2 * 1024 * 1024 / 4));
			if (!vgpu->stat.mmio_accounting_reg_stats) {
				pr_err("fail to allocate memory for mmio accounting.\n");
				goto out;
			}
		}

		mutex_lock(&gvt->lock);
		vgpu->stat.mmio_accounting = true;
		mutex_unlock(&gvt->lock);

		pr_info("VM %d start mmio accounting.\n", vgpu->id);
	} else if (!strncmp(buf, "stop", 4)) {
		mutex_lock(&gvt->lock);
		vgpu->stat.mmio_accounting = false;
		mutex_unlock(&gvt->lock);

		pr_info("VM %d stop mmio accounting.\n", vgpu->id);
	} else if (!strncmp(buf, "clean", 5)) {
		mutex_lock(&gvt->lock);
		vgpu->stat.mmio_accounting = false;
		mutex_unlock(&gvt->lock);

		if (vgpu->stat.mmio_accounting_reg_stats) {
			vfree(vgpu->stat.mmio_accounting_reg_stats);
			vgpu->stat.mmio_accounting_reg_stats = NULL;
		}

		pr_info("VM %d stop and clean mmio accounting statistics.\n",
				vgpu->id);
	}
out:
	mutex_unlock(&vgpu->stat.mmio_accounting_lock);
	return count;
}


static const struct file_operations mmio_accounting_fops = {
	.open = mmio_accounting_open,
	.write = mmio_accounting_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int intel_vgpu_create_debugfs(struct intel_vgpu *vgpu)
{
	int retval;
	int vgpu_id;
	struct dentry *perf_dir_entry;

	vgpu_id = vgpu->id;
	retval = sprintf(vgpu_dir_name[vgpu_id], "vgpu%d", vgpu_id);
	if (retval <= 0) {
		pr_err("GVT: failed to generating dirname:  vpu-%d\n", vgpu_id);
		return -EINVAL;
	}

	/* create vgpu directory */
	d_per_vgpu[vgpu_id] = debugfs_create_dir(vgpu_dir_name[vgpu_id],
			d_gvt_debug);
	if (d_per_vgpu[vgpu_id] == NULL) {
		pr_err("GVT: creation faiure for debugfs directory: vpu-%d\n",
				vgpu_id);
		return -EINVAL;
	}
	perf_dir_entry = d_per_vgpu[vgpu_id];

	mutex_init(&vgpu->stat.mmio_accounting_lock);
	debugfs_create_file("mmio_accounting", 0444, perf_dir_entry,
			vgpu, &mmio_accounting_fops);
#define CR_NODE(a, b) \
	debugfs_create_u64 (#a, 0444, perf_dir_entry, &(b))
#define CR_ENODE(a, b) \
	debugfs_create_file(#a, 0444, perf_dir_entry, b, &per_engine_fops)

	CR_NODE(gtt_mmio_rcnt, vgpu->stat.gtt_mmio_rcnt);
	CR_NODE(gtt_mmio_wcnt, vgpu->stat.gtt_mmio_wcnt);
	CR_NODE(gtt_mmio_wcycles, vgpu->stat.gtt_mmio_wcycles);
	CR_NODE(gtt_mmio_rcycles, vgpu->stat.gtt_mmio_rcycles);
	CR_NODE(mmio_rcnt, vgpu->stat.mmio_rcnt);
	CR_NODE(mmio_wcnt, vgpu->stat.mmio_wcnt);
	CR_NODE(mmio_wcycles, vgpu->stat.mmio_wcycles);
	CR_NODE(mmio_rcycles, vgpu->stat.mmio_rcycles);
	CR_NODE(mmio_wl_cycles, vgpu->stat.mmio_wl_cycles);
	CR_NODE(mmio_rl_cycles, vgpu->stat.mmio_rl_cycles);
	CR_NODE(mmio_wl_cnt, vgpu->stat.mmio_wl_cnt);
	CR_NODE(mmio_rl_cnt, vgpu->stat.mmio_rl_cnt);
	CR_NODE(wp_cnt, vgpu->stat.wp_cnt);
	CR_NODE(wp_cycles, vgpu->stat.wp_cycles);
	CR_NODE(ppgtt_wp_cnt, vgpu->stat.ppgtt_wp_cnt);
	CR_NODE(ppgtt_wp_cycles, vgpu->stat.ppgtt_wp_cycles);
	CR_NODE(spt_find_hit_cnt, vgpu->stat.spt_find_hit_cnt);
	CR_NODE(spt_find_hit_cycles, vgpu->stat.spt_find_hit_cycles);
	CR_NODE(spt_find_miss_cnt, vgpu->stat.spt_find_miss_cnt);
	CR_NODE(spt_find_miss_cycles, vgpu->stat.spt_find_miss_cycles);
	CR_NODE(gpt_find_hit_cnt, vgpu->stat.gpt_find_hit_cnt);
	CR_NODE(gpt_find_hit_cycles, vgpu->stat.gpt_find_hit_cycles);
	CR_NODE(gpt_find_miss_cnt, vgpu->stat.gpt_find_miss_cnt);
	CR_NODE(gpt_find_miss_cycles, vgpu->stat.gpt_find_miss_cycles);
	CR_NODE(shadow_last_level_page_cnt,
			vgpu->stat.shadow_last_level_page_cnt);
	CR_NODE(shadow_last_level_page_cycles,
			vgpu->stat.shadow_last_level_page_cycles);
	CR_NODE(oos_page_cnt, vgpu->stat.oos_page_cnt);
	CR_NODE(oos_page_cycles, vgpu->stat.oos_page_cycles);
	CR_NODE(oos_pte_cnt, vgpu->stat.oos_pte_cnt);
	CR_NODE(oos_pte_cycles, vgpu->stat.oos_pte_cycles);
	CR_ENODE(gpu_cycles, vgpu->stat.gpu_cycles);
	CR_ENODE(requests_completed_cnt, vgpu->stat.requests_completed_cnt);
	CR_ENODE(requests_submitted_cnt, vgpu->stat.requests_submitted_cnt);
	CR_ENODE(elsp_cnt, vgpu->stat.elsp_cnt);
	CR_ENODE(workload_submit_cycles, vgpu->stat.workload_submit_cycles);
	CR_ENODE(workload_queue_in_out_cycles,
			vgpu->stat.workload_queue_in_out_cycles);
	CR_ENODE(pick_workload_cycles, vgpu->stat.pick_workload_cycles);
	CR_ENODE(schedule_misc_cycles, vgpu->stat.schedule_misc_cycles);
	CR_ENODE(dispatch_lock_cycles, vgpu->stat.dispatch_lock_cycles);
	CR_ENODE(dispatch_cycles, vgpu->stat.dispatch_cycles);
	CR_ENODE(wait_complete_cycles, vgpu->stat.wait_complete_cycles);
	CR_ENODE(after_complete_cycles, vgpu->stat.after_complete_cycles);
	CR_ENODE(scan_shadow_wl_cycles, vgpu->stat.scan_shadow_wl_cycles);

	return 0;
}
