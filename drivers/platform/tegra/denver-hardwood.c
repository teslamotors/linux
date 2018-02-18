/*
 *  arch/arm64/mach-tegra/denver-hardwood.c
 *
 * Copyright (c) 2014-2015, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/dma-mapping.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/cpu.h>

#include <asm/cacheflush.h>
#include <asm/cpu.h>
#include <asm/cputype.h>
#include <asm/smp_plat.h>

#include "denver-knobs.h" /* backdoor detection */

#include "denver-hardwood.h"

#define TRACER_OSDUMP_BUFFER_CMD		100
#define TRACER_OSDUMP_BUFFER_CMD_DATA	101
#define HW_CMD(c, b, m) ((c) | ((b) << 8) | ((m) << 16))

#define PHYS_NS_BIT (1ULL << 40)

struct hardwood_buf {
	unsigned long va;
	dma_addr_t pa;
};

struct hardwood_device {
	/* device info */
	int cpu;
	int logical_cpu;
	char name[32];
	struct miscdevice dev;
	struct irqaction irq;

	/* sleeping support */
	int signaled;
	wait_queue_head_t wait_q;

	/* bitmask for buffer availability */
	ulong buf_status;
	spinlock_t buf_status_lock;

	/* bitmask for buffer occupation by userspace */
	ulong buf_occupied;

	/* Per CPU buffers */
	struct hardwood_buf bufs[N_BUFFER];
};

static struct hardwood_device hardwood_devs[N_CPU];

static int minor_map[N_CPU] = { -1 };

static int TRACER_IRQS[] = { 48, 54 };

/* Single IRQ for all CPUs for V102 onward */
static int osdump_irq = 54;

static u64 osdump_version;
static bool one_irq_per_cpu;

static char *tracer_names;
static u64 tracer_names_size;

static u32 buffer_size = BUFFER_SIZE;
static u32 buffer_order = BUFFER_ORDER;
static u32 num_buffers = N_BUFFER;

static bool hardwood_supported;
static bool hardwood_init_done;
static DEFINE_MUTEX(hardwood_init_lock);

/* Thread for probing buffers when some CPUs are hot-unplugged */
static struct task_struct *agent_thread;
static bool agent_stopped;
static bool agent_signaled;
static spinlock_t agent_lock;
static spinlock_t hw_lock;

static void hardwood_init_agent(void);

static int hardwood_irq_setup(void);

static int hardwood_late_init(void);

static bool check_buffers(struct hardwood_device *dev, bool lock);

core_param(osdump_irq, osdump_irq, int, 0644);

static irqreturn_t hardwood_handler(int irq, void *dev_id)
{
	struct hardwood_device *dev = (struct hardwood_device *) dev_id;

	pr_debug("IRQ%d received\n", irq);
	if (one_irq_per_cpu && num_online_cpus() == N_CPU) {
		/* All CPUs online and IRQ is per-cpu; wake target CPU */
		dev->signaled = 1;
		wake_up_interruptible(&dev->wait_q);
		pr_debug("CPU%d is interrupted\n", dev->cpu);
	} else {
		bool already_running;
		spin_lock(&agent_lock);
		agent_signaled = true;
		already_running = agent_thread->state == TASK_RUNNING;
		spin_unlock(&agent_lock);
		if (!already_running) {
			/* Some CPUs offline or IRQ shared; use agent */
			wake_up_process(agent_thread);
			pr_debug("Agent wake by CPU%d interrupt\n", dev->cpu);
		}
	}

	return IRQ_HANDLED;
}

static int hardwood_open(struct inode *inode, struct file *file)
{
	int cpu;
	int found = 0;
	int minor = iminor(inode);

	if (!hardwood_supported) {
		pr_warn("hardwood is not available.\n");
		return -ENOENT;
	}

	for (cpu = 0; cpu < N_CPU; ++cpu)
		if (minor_map[cpu] == minor) {
			found = 1;
			break;
		}
	if (!found)
		return -ENOENT;

	/* Lazy init */
	if (hardwood_late_init())
		return -ENOMEM;

	file->private_data = &hardwood_devs[cpu];
	return nonseekable_open(inode, file);
}

static void _hw_run_cmd(u64 data)
{
	asm volatile (
	"	sys 0, c11, c0, 1, %0\n"
	"	sys 0, c11, c0, 0, %1\n"
	:
	: "r"(data), "r" (TRACER_OSDUMP_BUFFER_CMD)
	);
}

static void _hw_set_data(u64 data)
{
	asm volatile (
	"	sys 0, c11, c0, 1, %0\n"
	"	sys 0, c11, c0, 0, %1\n"
	:
	: "r" (data), "r" (TRACER_OSDUMP_BUFFER_CMD_DATA)
	);
}

static void _hw_get_data(u64 *data)
{
	asm volatile ("sysl %0, 0, c11, c0, 0" : "=r" (*data));
}

static int hw_run_cmd(u64 data)
{
	return smp_call_function_denver((smp_call_func_t) _hw_run_cmd,
					(void *)data, 1);
}

static int hw_set_data(u64 data)
{
	return smp_call_function_denver((smp_call_func_t) _hw_set_data,
					(void *)data, 1);
}

static int hw_get_data(u64 *data)
{
	return smp_call_function_denver((smp_call_func_t)_hw_get_data,
					(void *)data, 1);
}

static inline int bad_buf_size(u32 size)
{
	return size < BUFFER_SIZE ||
		size > MAX_BUFFER_SIZE ||
		size & ~PAGE_MASK;
}

static long hardwood_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	u32 trace_cmd;
	struct hardwood_cmd op;
	struct hardwood_device *dev;

	if (copy_from_user(&op, (void __user *)arg, sizeof(op)))
		return -EFAULT;

	if (op.buffer_id >= num_buffers)
		return -EINVAL;

	trace_cmd = HW_CMD(op.core_id, op.buffer_id, _IOC_NR(cmd));

	switch (cmd) {
	/* CMD with input argument */
	case HARDWOOD_IOCTL_SET_PHYS_ADDR:
	case HARDWOOD_IOCTL_SET_CLIENT_VER:
		spin_lock(&hw_lock);
		hw_set_data(op.data);
		hw_run_cmd(trace_cmd);
		spin_unlock(&hw_lock);
		break;

	/* CMD with output argument */
	case HARDWOOD_IOCTL_GET_IP_ADDRESS:
	case HARDWOOD_IOCTL_GET_STATUS:
	case HARDWOOD_IOCTL_GET_BYTES_USED:
	case HARDWOOD_IOCTL_RELEASE_BUFFER:
	case HARDWOOD_IOCTL_OVERFLOW_COUNT:
	case HARDWOOD_IOCTL_GET_OSDUMP_VER:
		spin_lock(&hw_lock);
		hw_run_cmd(trace_cmd);
		hw_get_data(&op.data);
		spin_unlock(&hw_lock);
		if (copy_to_user((void __user *)arg, &op, sizeof(op)))
			return -EFAULT;
		break;

	/* CMD without input/output argument */
	case HARDWOOD_IOCTL_ENABLE_TRACE_DUMP:
	case HARDWOOD_IOCTL_DISABLE_TRACE_DUMP:
	case HARDWOOD_IOCTL_MARK_EMPTY:
		spin_lock(&hw_lock);
		hw_run_cmd(trace_cmd);
		spin_unlock(&hw_lock);
		dev = &hardwood_devs[op.core_id];
		if (cmd == HARDWOOD_IOCTL_MARK_EMPTY)
			/* Removing its occupying bit */
			clear_bit(op.buffer_id, &dev->buf_occupied);
		break;

	case HARDWOOD_IOCTL_SET_TRACER_MASK:
	case HARDWOOD_IOCTL_CLR_TRACER_MASK:
		spin_lock(&hw_lock);
		hw_set_data(op.data);
		hw_run_cmd(HW_CMD(0, op.buffer_id, cmd));
		spin_unlock(&hw_lock);
		break;

	/* SW-only CMD */
	case HARDWOOD_IOCTL_GET_PHYS_ADDR:
		op.data = hardwood_devs[op.core_id].bufs[op.buffer_id].pa;
		op.data &= ~PHYS_NS_BIT;
		if (copy_to_user((void __user *)arg, &op, sizeof(op)))
			return -EFAULT;
		break;

	case HARDWOOD_IOCTL_WAKEUP_READERS:
		pr_debug("core%d is FORCED to wake up\n", op.core_id);
		dev = &hardwood_devs[op.core_id];
		check_buffers(dev, true);
		dev->signaled = 1;
		wake_up_interruptible(&dev->wait_q);
		break;

	case HARDWOOD_IOCTL_GET_TR_NAMES_SZ:
		op.data = tracer_names_size;
		if (copy_to_user((void __user *)arg, &op, sizeof(op)))
			return -EFAULT;
		break;

	case HARDWOOD_IOCTL_GET_TR_NAMES:
		if (copy_to_user((void __user *)op.data, tracer_names,
			tracer_names_size))
			return -EFAULT;
		break;

	default:
		return -ENOTTY;
	}
	return 0;
}

static bool is_buffer_ready(int cpu, int buf)
{
	u64 status;
	u64 bytes_used;

	pr_debug("Probing buffer %d:%d\n", cpu, buf);

	spin_lock(&hw_lock);
	hw_run_cmd(HW_CMD(cpu, buf, HARDWOOD_GET_BYTES_USED));
	hw_get_data(&bytes_used);
	bytes_used &= 0xffffffff;

	hw_run_cmd(HW_CMD(cpu, buf, HARDWOOD_GET_STATUS));
	hw_get_data(&status);
	status &= 0xff;
	spin_unlock(&hw_lock);

	if ((bytes_used > 0) && (status == 1))
		pr_debug("buffer %d:%d is READY (used=%llu, status=%llu).\n",
			cpu, buf, bytes_used, status);
	else
		pr_debug("buffer %d:%d is BUSY (used=%llu, status=%llu).\n",
			cpu, buf, bytes_used, status);

	return (bytes_used > 0) && (status == 1);
}

static bool check_buffers(struct hardwood_device *dev, bool lock)
{
	int i;

	if (lock)
		spin_lock(&dev->buf_status_lock);

	pr_debug("++ buf_status = %lx, occupied = %lx\n",
		dev->buf_status, dev->buf_occupied);

	/* Poll each buffer */
	for (i = 0; i < num_buffers; ++i)
		if (is_buffer_ready(dev->cpu, i)) {
			/* Present buffer only if not occupied */
			if (!test_bit(i, &dev->buf_occupied))
				set_bit(i, &dev->buf_status);
		}

	if (lock)
		spin_unlock(&dev->buf_status_lock);

	pr_debug("-- buf_status = %lx, occupied = %lx\n",
		dev->buf_status, dev->buf_occupied);

	return dev->buf_status != 0;
}

static int agent_thread_fn(void *data)
{
	int cpu;
	struct hardwood_device *dev;
	bool already_signaled;
	while (!agent_stopped) {
		/* spinlock needed because IRQ happening after if() and before
		 * set_current_state() could cause us to miss the IRQ */
		spin_lock_irq(&agent_lock);
		already_signaled = agent_signaled;
		if (!already_signaled)
			set_current_state(TASK_INTERRUPTIBLE);
		spin_unlock_irq(&agent_lock);

		if (already_signaled) {
			/* signaled during last pass, so do not sleep */
			pr_debug("agent: still signaled; continuing\n");
		} else {
			schedule();
			/* woken up by interrupt */
			pr_debug("agent: woken up by IRQ\n");
		}
		set_current_state(TASK_RUNNING);
		agent_signaled = false;

		for (cpu = 0; cpu < N_CPU; ++cpu) {
			pr_debug("agent: checking CPU %d\n", cpu);
			dev = &hardwood_devs[cpu];

			if (check_buffers(dev, true)) {
				pr_debug("Waking up reader %d\n", cpu);
				dev->signaled = 1;
				wake_up_interruptible(&dev->wait_q);
			}
		}

		pr_debug("agent: done one pass\n");
	}
	return 0;
}

static ssize_t hardwood_read(struct file *file, char __user *p, size_t s,
	loff_t *r)
{
	int buf_id = -EAGAIN;
	struct hardwood_device *dev;
	bool found;

	dev = (struct hardwood_device *)file->private_data;

	if (s != sizeof(u32)) {
		pr_debug("must use u32 to read.\n");
		return 0;
	}

	pr_debug("core = %d, buf_status = %lx, occupied = %lx\n",
		 dev == &hardwood_devs[0] ? 0 : 1, dev->buf_status,
		 dev->buf_occupied);

	/* Check if any buffers is available NOW */
	spin_lock(&dev->buf_status_lock);
	if (dev->buf_status) {
		buf_id = find_first_bit(&dev->buf_status, num_buffers);
		clear_bit(buf_id, &dev->buf_status);
		set_bit(buf_id, &dev->buf_occupied);
	}
	spin_unlock(&dev->buf_status_lock);

	if (buf_id < 0) {
		/* No buffer available for readout */
		pr_debug("[HW] CPU%d is waiting on %p\n",
			dev == &hardwood_devs[0] ? 0 : 1, &dev->wait_q);

		/* Sleep until signaled by IRQ handler */
		if (wait_event_interruptible(dev->wait_q, dev->signaled))
			return -ERESTARTSYS;

		/* Restore signal */
		dev->signaled = 0;

		pr_debug("CPU%d is woken up\n", dev->cpu);

		spin_lock(&dev->buf_status_lock);

		/* Re-probe */
		found = true;
		if (!dev->buf_status)
			found = check_buffers(dev, false);

		/* Some buffers are available NOW, we grab one */
		if (found) {
			buf_id = find_first_bit(&dev->buf_status, num_buffers);
			clear_bit(buf_id, &dev->buf_status);
			set_bit(buf_id, &dev->buf_occupied);
		}

		spin_unlock(&dev->buf_status_lock);
	}

	if (buf_id >= 0) {
		pr_debug("core = %d, buf_status = %lx, buf_id = %d\n",
			dev == &hardwood_devs[0] ? 0 : 1, dev->buf_status,
			buf_id);
		if (!is_buffer_ready(dev->cpu, buf_id))
			return -EIO;

		/* Invalidate the cache */
		FLUSH_DCACHE_AREA((void *)dev->bufs[buf_id].va, buffer_size);

		if (copy_to_user((void __user *)p, &buf_id, sizeof(u32)))
			return -ENOMEM;

		return s;
	}

	return -EBUSY;
}

static const struct file_operations fops = {
	.owner		= THIS_MODULE,
	.open		= hardwood_open,
	.read		= hardwood_read,
	.unlocked_ioctl	= hardwood_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= hardwood_ioctl,
#endif
};

static int init_one_buffer(int cpu, int buf_id)
{
	int i;
	u64 trace_cmd;
	struct hardwood_buf *buf;
	u32 *ptr;

	buf = &hardwood_devs[cpu].bufs[buf_id];
	buf->va = 0;
	buf->pa = 0;

	buf->va = __get_free_pages(GFP_KERNEL, buffer_order);
	buf->pa = virt_to_phys((void *)buf->va);
	if (!buf->va || !buf->pa)
		return -ENOMEM;

	pr_debug("buf[%d:%d] allocated phys=%p, size=%d, order=%d\n",
		cpu, buf_id, (void *)buf->pa, buffer_size, buffer_order);

	/* Set NS bit because the kernel is always non-secure */
	buf->pa |= PHYS_NS_BIT;

	spin_lock(&hw_lock);
	/* Set buffer physical address */
	trace_cmd = HW_CMD(cpu, buf_id, HARDWOOD_SET_PHYS_ADDR);
	hw_set_data(buf->pa);
	hw_run_cmd(trace_cmd);

	/* Set buffer physical size */
	trace_cmd = HW_CMD(cpu, buf_id, HARDWOOD_SET_BUFFER_SIZE);
	hw_set_data(buffer_size);
	hw_run_cmd(trace_cmd);
	spin_unlock(&hw_lock);

	ptr = (u32 *)buf->va;
	for (i = 0; i < buffer_size; i += sizeof(u32))
		/* Fill in some poison data */
		*ptr++ = 0xdeadbeef;

	return 0;
}

static void query_tracer_names(void)
{
	u32 trace_cmd;
	u64 ret;

	spin_lock(&hw_lock);
	trace_cmd = HW_CMD(0, 0, HARDWOOD_GET_TR_NAMES_SZ);
	hw_run_cmd(trace_cmd);
	hw_get_data(&tracer_names_size);

	tracer_names = kmalloc(tracer_names_size, GFP_KERNEL);
	if (!tracer_names) {
		pr_debug("failed to allocate %lld bytes for tracer names\n",
			tracer_names_size);
		spin_unlock(&hw_lock);
		return;
	}
	BUG_ON(((u64)tracer_names) & 0x7); /* Must be 8-bytes aligned */

	trace_cmd = HW_CMD(0, 0, HARDWOOD_GET_TR_NAMES);
	hw_set_data((u64)tracer_names);
	hw_run_cmd(trace_cmd);
	hw_get_data(&ret);
	spin_unlock(&hw_lock);

	if (!ret) {
		pr_debug("failed to query tracer names\n");
		tracer_names = NULL;
		return;
	}
}

static void free_buffers(void)
{
	int i, j;
	struct hardwood_buf *buf;

	for (i = 0; i < N_CPU; i++)
		for (j = 0; j < num_buffers; j++) {
			buf = &hardwood_devs[i].bufs[j];
			if (buf->va)
				free_pages(buf->va, buffer_order);
		}
}

static void config_num_buffers(void)
{
	int cpu;
	u32 cmd = HARDWOOD_SET_NUM_BUFFERS;

	if (osdump_version < OSDUMP_VER_NUM_BUFFERS) {
		num_buffers = N_BUFFER_LEGACY;
		return;
	}

	spin_lock(&hw_lock);
	for (cpu = 0; cpu < N_CPU; cpu++)
		hw_run_cmd(HW_CMD(cpu, num_buffers, cmd));
	spin_unlock(&hw_lock);
}

static int hardwood_irq_setup(void)
{
	struct irqaction *irq;
	struct hardwood_device *hdev;
	int cpu;
	u32 cmd;

	for (cpu = 0; cpu < N_CPU; ++cpu) {
		hdev = &hardwood_devs[cpu];

		if (osdump_version >= OSDUMP_VER_OSDUMP_IRQS) {
			BUG_ON(osdump_irq < 32 || osdump_irq > 1024);

			/* Use the most reliable IRQ# */
			TRACER_IRQS[0] = osdump_irq;

			spin_lock(&hw_lock);
			if (cpu == 0) {
				/* Inform MTS the IRQ to use for CPU0 */
				cmd = HARDWOOD_SET_OSDUMP_IRQS;
				hw_run_cmd(HW_CMD(cpu, TRACER_IRQS[cpu], cmd));
			} else {
				/* Let CPU0 dispatch for other CPUs */
				hw_run_cmd(HW_CMD(cpu, 0, HARDWOOD_SET_IRQ_TARGET));
			}
			spin_unlock(&hw_lock);
		}

		if (cpu == 0 || osdump_version < OSDUMP_VER_OSDUMP_IRQS) {
			irq = &hdev->irq;
			irq->name = hdev->name;
			irq->flags = 0;
			irq->handler = hardwood_handler;
			irq->dev_id = (void *)hdev;
			irq->irq = TRACER_IRQS[cpu];
			BUG_ON(setup_irq(irq->irq, irq));
			WARN_ON(irq_force_affinity(irq->irq,
				cpumask_of(hdev->logical_cpu)));
		}
	}

	return 0;
}

static inline int hardwood_late_init(void)
{
	int i, j;
	int ret = 0;

	if (hardwood_init_done)
		return 0;

	/* use mutex b/c below code might sleep */
	mutex_lock(&hardwood_init_lock);
	if (!hardwood_init_done) {
		/* Have the 2 CPUs share an IRQ if supported */
		one_irq_per_cpu = osdump_version < OSDUMP_VER_OSDUMP_IRQS;

		hardwood_irq_setup();

		hardwood_init_agent();

		config_num_buffers();

		for (i = 0; i < N_CPU; i++)
			for (j = 0; j < num_buffers; j++) {
				ret = init_one_buffer(i, j);
				if (ret) {
					pr_err("hardwood: buf alloc failed\n");
					free_buffers();
					goto exit;
				}
			}

		hardwood_init_done = 1;
	}

	if (osdump_version >= OSDUMP_VER_TRACER_NAMES)
		query_tracer_names();

exit:
	mutex_unlock(&hardwood_init_lock);
	return ret;
}

static int hardwood_cpu_notify(struct notifier_block *self,
					 unsigned long action, void *hcpu)
{
	long cpu = (long) hcpu;
	long new_cpu;

	switch (action) {
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		new_cpu = cpu;
		break;

	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		new_cpu = cpu == 0 ? 1 : 0;
		break;

	default:
		return NOTIFY_OK;
	}

	spin_lock(&hw_lock);
	hw_run_cmd(HW_CMD(cpu, new_cpu, HARDWOOD_SET_IRQ_TARGET));
	spin_unlock(&hw_lock);

	return NOTIFY_OK;
}

static struct notifier_block hardwood_cpu_notifier = {
	.notifier_call = hardwood_cpu_notify,
};

static void hardwood_init_agent(void)
{
	spin_lock_init(&agent_lock);
	if (osdump_version < OSDUMP_VER_OSDUMP_IRQS)
		register_hotcpu_notifier(&hardwood_cpu_notifier);
	agent_thread = kthread_create(agent_thread_fn, NULL, "hardwood-agent");
}

static ssize_t bufsize_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", buffer_size);
}

static ssize_t bufsize_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int base;
	unsigned long size;

	if (hardwood_init_done) {
		dev_err(dev, "buffer size can only be set once\n");
		return -EINVAL;
	}

	base = (buf[0] == '0' && buf[1] == 'x') ? 16 : 10;
	if (kstrtoul(buf, base, &size)) {
		dev_err(dev, "invalid buffer size string: %s\n", buf);
		return -EINVAL;
	}

	if (bad_buf_size(size)) {
		dev_err(dev, "invalid buffer size: %lu\n", size);
		return -EINVAL;
	}

	buffer_size = size;
	buffer_order = get_order(size);

	return count;
}

static ssize_t version_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", (unsigned int)osdump_version);
}

static DEVICE_ATTR(version, S_IRUGO, version_show, NULL);
static DEVICE_ATTR(bufsize, S_IRUGO | S_IWUSR, bufsize_show, bufsize_store);

static __init void init_one_cpu(int logical_cpu)
{
	struct hardwood_device *hdev;
	int cpu;

	cpu = MPIDR_AFFINITY_LEVEL(cpu_logical_map(logical_cpu), 0);

	hdev = &hardwood_devs[cpu];
	memset(hdev, 0, sizeof(*hdev));

	sprintf(hdev->name, "hardwood-%d", cpu);
	hdev->cpu = cpu;
	hdev->logical_cpu = logical_cpu;

	init_waitqueue_head(&hdev->wait_q);

	hdev->dev.minor = MISC_DYNAMIC_MINOR;
	hdev->dev.name = hdev->name;
	hdev->dev.fops = &fops;
	misc_register(&hdev->dev);
	minor_map[cpu] = hdev->dev.minor;
	pr_debug("minor for cpu[%d] is %d\n", cpu, minor_map[cpu]);

	hdev->buf_status = 0;
	hdev->buf_occupied = 0;
	hdev->signaled = 0;
	spin_lock_init(&hdev->buf_status_lock);
}

static __init void init_osdump_version(void)
{
	spin_lock(&hw_lock);
	hw_run_cmd(HW_CMD(0, 0, HARDWOOD_GET_OSDUMP_VER));
	hw_get_data(&osdump_version);
	spin_unlock(&hw_lock);
}

static __init int hardwood_init(void)
{
	int cpu;
	struct device *dev;
	struct cpuinfo_arm64 *cpuinfo;

	hardwood_supported = denver_backdoor_enabled();

	spin_lock_init(&hw_lock);

	if (hardwood_supported) {
		init_osdump_version();
		pr_info("Denver: hardwood version %lld.\n", osdump_version);

		for_each_online_cpu(cpu) {
			/* Only init on Denver CPUs */
			cpuinfo = &per_cpu(cpu_data, cpu);
			if (MIDR_IMPLEMENTOR(cpuinfo->reg_midr) ==
			    ARM_CPU_IMP_NVIDIA)
				init_one_cpu(cpu);
		}

		/* Create buffer size attribute at hardwood-0 */
		dev = hardwood_devs[0].dev.this_device;
		if (sysfs_create_file(&dev->kobj, &dev_attr_bufsize.attr))
			dev_err(dev, "failed to create sysfs: buf_size.\n");
		if (sysfs_create_file(&dev->kobj, &dev_attr_version.attr))
			dev_err(dev, "failed to create sysfs: version.\n");
	}

	return 0;
}

late_initcall(hardwood_init);
