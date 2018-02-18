/*
 * Tegra Hypervisor manager
 *
 * Instantiates virtualization-related resources.
 *
 * Copyright (C) 2014-2016, NVIDIA CORPORATION. All rights reserved.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2.  This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/module.h>

#include <linux/tegra-soc.h>
#include <linux/tegra-ivc.h>

#include "syscalls.h"
#include "tegra_hv.h"
#include <linux/tegra-ivc-instance.h>

#define ERR(...) pr_err("tegra_hv: " __VA_ARGS__)
#define INFO(...) pr_info("tegra_hv: " __VA_ARGS__)

struct tegra_hv_data;

struct hv_ivc {
	struct tegra_hv_data	*hvd;

	/*
	 * ivc_devs are stored in an id-indexed array; this field indicates
	 * a valid array entry.
	 */
	int			valid;

	/* channel configuration */
	struct ivc		ivc;
	const struct tegra_hv_queue_data *qd;
	const struct guest_ivc_info *givci;
	int			other_guestid;

	const struct tegra_hv_ivc_ops *cookie_ops;
	struct tegra_hv_ivc_cookie cookie;

	/* This lock synchronizes the reserved flag. */
	struct mutex		lock;
	int			reserved;

	char			name[16];
};

#define cookie_to_ivc_dev(_cookie) \
	container_of(_cookie, struct hv_ivc, cookie)

/* Describe all info needed to do IVC to one particular guest */
struct guest_ivc_info {
	uintptr_t shmem;	/* IO remapped shmem */
	size_t length;		/* length of shmem */
};

struct hv_mempool {
	struct tegra_hv_ivm_cookie ivmk;
	const struct ivc_mempool *mpd;
	struct mutex lock;
	int reserved;
};

struct tegra_hv_data {
	const struct ivc_info_page *info;
	int guestid;

	struct guest_ivc_info *guest_ivc_info;

	/* ivc_devs is indexed by queue id */
	struct hv_ivc *ivc_devs;
	uint32_t max_qid;

	/* array with length info->nr_mempools */
	struct hv_mempool *mempools;

	struct class *hv_class;
};

/*
 * Global HV state for read-only access by tegra_hv_... APIs
 *
 * This should be accessed only through get_hvd().
 */
static const struct tegra_hv_data *tegra_hv_data;

static void ivc_raise_irq(struct ivc *ivc_channel)
{
	struct hv_ivc *ivc = container_of(ivc_channel, struct hv_ivc, ivc);
	hyp_raise_irq(ivc->qd->raise_irq, ivc->other_guestid);
}

static const struct tegra_hv_data *get_hvd(void)
{
	if (!tegra_hv_data) {
		INFO("%s: not initialized yet\n", __func__);
		return ERR_PTR(-EPROBE_DEFER);
	} else
		return tegra_hv_data;
}

const struct ivc_info_page *tegra_hv_get_ivc_info(void)
{
	const struct tegra_hv_data *hvd = get_hvd();

	if (IS_ERR(hvd))
		return (void *)hvd;
	else
		return tegra_hv_data->info;
}
EXPORT_SYMBOL(tegra_hv_get_ivc_info);

int tegra_hv_get_vmid(void)
{
	const struct tegra_hv_data *hvd = get_hvd();

	if (IS_ERR(hvd))
		return -1;
	else
		return hvd->guestid;
}
EXPORT_SYMBOL(tegra_hv_get_vmid);

static void ivc_handle_notification(struct hv_ivc *ivc)
{
	struct tegra_hv_ivc_cookie *ivck = &ivc->cookie;

	/* This function should only be used when callbacks are specified. */
	BUG_ON(!ivc->cookie_ops);

	/* there are data in the queue, callback */
	if (ivc->cookie_ops->rx_rdy && tegra_ivc_can_read(&ivc->ivc))
		ivc->cookie_ops->rx_rdy(ivck);

	/* there is space in the queue to write, callback */
	if (ivc->cookie_ops->tx_rdy && tegra_ivc_can_write(&ivc->ivc))
		ivc->cookie_ops->tx_rdy(ivck);
}

static irqreturn_t ivc_dev_cookie_irq_handler(int irq, void *data)
{
	struct hv_ivc *ivcd = data;
	ivc_handle_notification(ivcd);
	return IRQ_HANDLED;
}

static void ivc_release_irq(struct hv_ivc *ivc)
{
	BUG_ON(!ivc);

	free_irq(ivc->qd->irq, ivc);
}

static int ivc_request_cookie_irq(struct hv_ivc *ivcd)
{
	return request_irq(ivcd->qd->irq, ivc_dev_cookie_irq_handler, 0,
			ivcd->name, ivcd);
}

static int tegra_hv_add_ivc(struct tegra_hv_data *hvd,
		const struct tegra_hv_queue_data *qd)
{
	struct hv_ivc *ivc;
	int ret;
	int rx_first;
	uintptr_t rx_base, tx_base;
	uint32_t i;

	ivc = &hvd->ivc_devs[qd->id];
	BUG_ON(ivc->valid);
	ivc->valid = 1;
	ivc->hvd = hvd;
	ivc->qd = qd;

	if (qd->peers[0] == hvd->guestid)
		ivc->other_guestid = qd->peers[1];
	else if (qd->peers[1] == hvd->guestid)
		ivc->other_guestid = qd->peers[0];
	else
		BUG();


	/*
	 * Locate the guest_ivc_info representing the remote guest accessed
	 * through this channel.
	 */
	for (i = 0; i < hvd->info->nr_areas; i++) {
		if (hvd->info->areas[i].guest == ivc->other_guestid) {
			ivc->givci = &hvd->guest_ivc_info[i];
			break;
		}
	}
	BUG_ON(i == hvd->info->nr_areas);

	BUG_ON(ivc->givci->shmem == 0);

	mutex_init(&ivc->lock);

	if (qd->peers[0] == qd->peers[1]) {
		/*
		 * The queue ids of loopback queues are always consecutive, so
		 * the even-numbered one receives in the first area.
		 */
		rx_first = (qd->id & 1) == 0;
	} else {
		rx_first = hvd->guestid == qd->peers[0];
	}

	BUG_ON(qd->offset >= ivc->givci->length);
	BUG_ON(qd->offset + qd->size * 2 > ivc->givci->length);

	if (rx_first) {
		rx_base = ivc->givci->shmem + qd->offset;
		tx_base = ivc->givci->shmem + qd->offset + qd->size;
	} else {
		tx_base = ivc->givci->shmem + qd->offset;
		rx_base = ivc->givci->shmem + qd->offset + qd->size;
	}

	snprintf(ivc->name, sizeof(ivc->name), "ivc%u", qd->id);

	INFO("adding ivc%u: rx_base=%lx tx_base = %lx size=%x\n",
			qd->id, rx_base, tx_base, qd->size);
	tegra_ivc_init(&ivc->ivc, rx_base, tx_base, qd->nframes, qd->frame_size,
			NULL, ivc_raise_irq);

	/* We may have rebooted, so the channel could be active. */
	ret = tegra_ivc_channel_sync(&ivc->ivc);
	if (ret != 0)
		return  ret;

	INFO("added %s\n", ivc->name);

	return 0;
}

struct hv_ivc *ivc_device_by_id(const struct tegra_hv_data *hvd, uint32_t id)
{
	if (id > hvd->max_qid)
		return NULL;
	else {
		struct hv_ivc *ivc = &hvd->ivc_devs[id];
		if (ivc->valid)
			return ivc;
		else
			return NULL;
	}
}

static void tegra_hv_ivc_cleanup(struct tegra_hv_data *hvd)
{
	if (!hvd->ivc_devs)
		return;

	kfree(hvd->ivc_devs);
	hvd->ivc_devs = NULL;
}

static void __init tegra_hv_cleanup(struct tegra_hv_data *hvd)
{
	/*
	 * Destroying IVC channels in use is not supported. Once it's possible
	 * for IVC channels to be reserved, we no longer clean up.
	 */
	BUG_ON(tegra_hv_data != NULL);

	kfree(hvd->mempools);
	hvd->mempools = NULL;

	tegra_hv_ivc_cleanup(hvd);

	if (hvd->guest_ivc_info) {
		uint32_t i;
		BUG_ON(!hvd->info);
		for (i = 0; i < hvd->info->nr_areas; i++) {
			if (hvd->guest_ivc_info[i].shmem) {
				iounmap((void *)hvd->guest_ivc_info[i].shmem);
				hvd->guest_ivc_info[i].shmem = 0;
			}
		}

		kfree(hvd->guest_ivc_info);
		hvd->guest_ivc_info = NULL;

		iounmap((void *)hvd->info);
		hvd->info = NULL;
	}

	if (hvd->hv_class) {
		class_destroy(hvd->hv_class);
		hvd->hv_class = NULL;
	}
}

static ssize_t vmid_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	const struct tegra_hv_data *hvd = get_hvd();

	BUG_ON(!hvd);
	return snprintf(buf, PAGE_SIZE, "%d\n", hvd->guestid);
}
static CLASS_ATTR_RO(vmid);

static int __init tegra_hv_setup(struct tegra_hv_data *hvd)
{
	uint64_t info_page;
	uint32_t i;
	int ret;

	ret = hyp_read_gid(&hvd->guestid);
	if (ret != 0) {
		ERR("Failed to read guest id\n");
		return -ENODEV;
	}

	hvd->hv_class = class_create(THIS_MODULE, "tegra_hv");
	if (IS_ERR(hvd->hv_class)) {
		ERR("class_create() failed\n");
		return PTR_ERR(hvd->hv_class);
	}

	ret = class_create_file(hvd->hv_class, &class_attr_vmid);
	if (ret != 0) {
		ERR("failed to create vmid file: %d\n", ret);
		return ret;
	}

	ret = hyp_read_ivc_info(&info_page);
	if (ret != 0) {
		ERR("failed to obtain IVC info page: %d\n", ret);
		return ret;
	}

	hvd->info = (struct ivc_info_page *)ioremap_cache(info_page,
			PAGE_SIZE);
	if (hvd->info == NULL) {
		ERR("failed to map IVC info page (%llx)\n", info_page);
		return -ENOMEM;
	}

	hvd->guest_ivc_info = kzalloc(hvd->info->nr_areas *
			sizeof(*hvd->guest_ivc_info), GFP_KERNEL);
	if (hvd->guest_ivc_info == NULL) {
		ERR("failed to allocate %u-entry givci\n",
				hvd->info->nr_areas);
		return -ENOMEM;
	}

	for (i = 0; i < hvd->info->nr_areas; i++) {
		hvd->guest_ivc_info[i].shmem = (uintptr_t)ioremap_cache(
				hvd->info->areas[i].pa,
				hvd->info->areas[i].size);
		if (hvd->guest_ivc_info[i].shmem == 0) {
			ERR("can't map area for guest %u (%llx)\n",
					hvd->info->areas[i].guest,
					hvd->info->areas[i].pa);
			return -ENOMEM;
		}
		hvd->guest_ivc_info[i].length = hvd->info->areas[i].size;
	}

	/*
	 * Determine the largest queue id in order to allocate a queue id-
	 * indexed array and device nodes.
	 */
	hvd->max_qid = 0;
	for (i = 0; i < hvd->info->nr_queues; i++) {
		const struct tegra_hv_queue_data *qd =
				&ivc_info_queue_array(hvd->info)[i];
		if (qd->id > hvd->max_qid)
			hvd->max_qid = qd->id;
	}

	hvd->ivc_devs = kzalloc((hvd->max_qid + 1) * sizeof(*hvd->ivc_devs),
			GFP_KERNEL);
	if (hvd->ivc_devs == NULL) {
		ERR("failed to allocate %u-entry ivc_devs array\n",
				hvd->info->nr_queues);
		return -ENOMEM;
	}

	/* instantiate the IVC */
	for (i = 0; i < hvd->info->nr_queues; i++) {
		const struct tegra_hv_queue_data *qd =
				&ivc_info_queue_array(hvd->info)[i];
		ret = tegra_hv_add_ivc(hvd, qd);
		if (ret != 0) {
			ERR("failed to add queue #%u\n", qd->id);
			return ret;
		}
	}

	hvd->mempools =
		kzalloc(hvd->info->nr_mempools * sizeof(*hvd->mempools),
								GFP_KERNEL);
	if (hvd->mempools == NULL) {
		ERR("failed to allocate %u-entry mempools array\n",
				hvd->info->nr_mempools);
		return -ENOMEM;
	}

	/* Initialize mempools. */
	for (i = 0; i < hvd->info->nr_mempools; i++) {
		const struct ivc_mempool *mpd =
				&ivc_info_mempool_array(hvd->info)[i];
		struct tegra_hv_ivm_cookie *ivmk = &hvd->mempools[i].ivmk;

		hvd->mempools[i].mpd = mpd;
		mutex_init(&hvd->mempools[i].lock);

		ivmk->ipa = mpd->pa;
		ivmk->size = mpd->size;
		ivmk->peer_vmid = mpd->peer_vmid;

		INFO("added mempool %u: ipa=%llx size=%llx peer=%u\n",
				mpd->id, mpd->pa, mpd->size, mpd->peer_vmid);
	}

	return 0;
}

static int __init tegra_hv_init(void)
{
	struct tegra_hv_data *hvd;
	int ret;

	if (!is_tegra_hypervisor_mode())
		return -ENODEV;

	hvd = kzalloc(sizeof(*hvd), GFP_KERNEL);
	if (!hvd) {
		ERR("failed to allocate hvd\n");
		return -ENOMEM;
	}

	ret = tegra_hv_setup(hvd);
	if (ret != 0) {
		tegra_hv_cleanup(hvd);
		kfree(hvd);
		return ret;
	}

	/*
	 * Ensure that all contents of hvd are visible before they are visible
	 * to other threads.
	 */
	smp_wmb();

	BUG_ON(tegra_hv_data);
	tegra_hv_data = hvd;
	INFO("initialized\n");

	return 0;
}

static int ivc_dump(struct hv_ivc *ivc)
{
	INFO("IVC#%d: IRQ=%d nframes=%d frame_size=%d offset=%d\n",
			ivc->qd->id, ivc->qd->irq,
			ivc->qd->nframes, ivc->qd->frame_size, ivc->qd->offset);

	return 0;
}

struct tegra_hv_ivc_cookie *tegra_hv_ivc_reserve(struct device_node *dn,
		int id, const struct tegra_hv_ivc_ops *ops)
{
	const struct tegra_hv_data *hvd = get_hvd();
	struct hv_ivc *ivc;
	struct tegra_hv_ivc_cookie *ivck;
	int ret;

	if (IS_ERR(hvd))
		return (void *)hvd;

	ivc = ivc_device_by_id(hvd, id);
	if (ivc == NULL)
		return ERR_PTR(-ENODEV);

	mutex_lock(&ivc->lock);
	if (ivc->reserved) {
		ret = -EBUSY;
	} else {
		ivc->reserved = 1;
		ret = 0;
	}
	mutex_unlock(&ivc->lock);

	if (ret != 0)
		return ERR_PTR(ret);

	ivc->cookie_ops = ops;

	ivck = &ivc->cookie;
	ivck->irq = ivc->qd->irq;
	ivck->peer_vmid = ivc->other_guestid;
	ivck->nframes = ivc->qd->nframes;
	ivck->frame_size = ivc->qd->frame_size;

	if (ivc->cookie_ops) {
		ivc_handle_notification(ivc);
		/* request our irq */
		ret = ivc_request_cookie_irq(ivc);
		if (ret) {
			mutex_lock(&ivc->lock);
			BUG_ON(!ivc->reserved);
			ivc->reserved = 0;
			mutex_unlock(&ivc->lock);
			return ERR_PTR(ret);
		}
	}

	/* return pointer to the cookie */
	return ivck;
}
EXPORT_SYMBOL(tegra_hv_ivc_reserve);

int tegra_hv_ivc_unreserve(struct tegra_hv_ivc_cookie *ivck)
{
	struct hv_ivc *ivc;
	int ret;

	if (ivck == NULL)
		return -EINVAL;

	ivc = cookie_to_ivc_dev(ivck);

	mutex_lock(&ivc->lock);
	if (ivc->reserved) {
		if (ivc->cookie_ops)
			ivc_release_irq(ivc);
		ivc->cookie_ops = NULL;
		ivc->reserved = 0;
		ret = 0;
	} else {
		ret = -EINVAL;
	}
	mutex_unlock(&ivc->lock);

	return ret;
}
EXPORT_SYMBOL(tegra_hv_ivc_unreserve);

int tegra_hv_ivc_write(struct tegra_hv_ivc_cookie *ivck, const void *buf,
		int size)
{
	struct ivc *ivc = &cookie_to_ivc_dev(ivck)->ivc;

	return tegra_ivc_write(ivc, buf, size);
}
EXPORT_SYMBOL(tegra_hv_ivc_write);

int tegra_hv_ivc_read(struct tegra_hv_ivc_cookie *ivck, void *buf, int size)
{
	struct ivc *ivc = &cookie_to_ivc_dev(ivck)->ivc;

	return tegra_ivc_read(ivc, buf, size);
}
EXPORT_SYMBOL(tegra_hv_ivc_read);

int tegra_hv_ivc_read_peek(struct tegra_hv_ivc_cookie *ivck, void *buf,
			   int off, int count)
{
	struct ivc *ivc = &cookie_to_ivc_dev(ivck)->ivc;

	return tegra_ivc_read_peek(ivc, buf, off, count);
}
EXPORT_SYMBOL(tegra_hv_ivc_read_peek);

int tegra_hv_ivc_can_read(struct tegra_hv_ivc_cookie *ivck)
{
	struct ivc *ivc = &cookie_to_ivc_dev(ivck)->ivc;

	return tegra_ivc_can_read(ivc);
}
EXPORT_SYMBOL(tegra_hv_ivc_can_read);

int tegra_hv_ivc_can_write(struct tegra_hv_ivc_cookie *ivck)
{
	struct ivc *ivc = &cookie_to_ivc_dev(ivck)->ivc;

	return tegra_ivc_can_write(ivc);
}
EXPORT_SYMBOL(tegra_hv_ivc_can_write);

int tegra_hv_ivc_tx_empty(struct tegra_hv_ivc_cookie *ivck)
{
	struct ivc *ivc = &cookie_to_ivc_dev(ivck)->ivc;

	return tegra_ivc_tx_empty(ivc);
}
EXPORT_SYMBOL(tegra_hv_ivc_tx_empty);

uint32_t tegra_hv_ivc_tx_frames_available(struct tegra_hv_ivc_cookie *ivck)
{
	struct ivc *ivc = &cookie_to_ivc_dev(ivck)->ivc;

	return tegra_ivc_tx_frames_available(ivc);
}
EXPORT_SYMBOL(tegra_hv_ivc_tx_frames_available);

int tegra_hv_ivc_dump(struct tegra_hv_ivc_cookie *ivck)
{
	struct hv_ivc *ivc = cookie_to_ivc_dev(ivck);
	return ivc_dump(ivc);
}
EXPORT_SYMBOL(tegra_hv_ivc_dump);

void *tegra_hv_ivc_read_get_next_frame(struct tegra_hv_ivc_cookie *ivck)
{
	struct ivc *ivc = &cookie_to_ivc_dev(ivck)->ivc;

	return tegra_ivc_read_get_next_frame(ivc);
}
EXPORT_SYMBOL(tegra_hv_ivc_read_get_next_frame);

void *tegra_hv_ivc_write_get_next_frame(struct tegra_hv_ivc_cookie *ivck)
{
	struct ivc *ivc = &cookie_to_ivc_dev(ivck)->ivc;

	return tegra_ivc_write_get_next_frame(ivc);
}
EXPORT_SYMBOL(tegra_hv_ivc_write_get_next_frame);

int tegra_hv_ivc_write_advance(struct tegra_hv_ivc_cookie *ivck)
{
	struct ivc *ivc = &cookie_to_ivc_dev(ivck)->ivc;

	return tegra_ivc_write_advance(ivc);
}
EXPORT_SYMBOL(tegra_hv_ivc_write_advance);

int tegra_hv_ivc_read_advance(struct tegra_hv_ivc_cookie *ivck)
{
	struct ivc *ivc = &cookie_to_ivc_dev(ivck)->ivc;

	return tegra_ivc_read_advance(ivc);
}
EXPORT_SYMBOL(tegra_hv_ivc_read_advance);

struct ivc *tegra_hv_ivc_convert_cookie(struct tegra_hv_ivc_cookie *ivck)
{
	return &cookie_to_ivc_dev(ivck)->ivc;
}
EXPORT_SYMBOL(tegra_hv_ivc_convert_cookie);

struct tegra_hv_ivm_cookie *tegra_hv_mempool_reserve(struct device_node *dn,
		unsigned id)
{
	uint32_t i;
	struct hv_mempool *mempool;
	int reserved;

	if (!tegra_hv_data)
		return ERR_PTR(-EPROBE_DEFER);

	/* Locate a mempool with matching id. */
	for (i = 0; i < tegra_hv_data->info->nr_mempools; i++) {
		mempool = &tegra_hv_data->mempools[i];
		if (mempool->mpd->id == id)
			break;
	}

	if (i == tegra_hv_data->info->nr_mempools)
		return ERR_PTR(-ENODEV);

	mutex_lock(&mempool->lock);
	reserved = mempool->reserved;
	mempool->reserved = 1;
	mutex_unlock(&mempool->lock);

	return reserved ? ERR_PTR(-EBUSY) : &mempool->ivmk;
}
EXPORT_SYMBOL(tegra_hv_mempool_reserve);

int tegra_hv_mempool_unreserve(struct tegra_hv_ivm_cookie *ivmk)
{
	int reserved;
	struct hv_mempool *mempool = container_of(ivmk, struct hv_mempool,
			ivmk);

	mutex_lock(&mempool->lock);
	reserved = mempool->reserved;
	mempool->reserved = 0;
	mutex_unlock(&mempool->lock);

	return reserved ? 0 : -EINVAL;
}
EXPORT_SYMBOL(tegra_hv_mempool_unreserve);

int tegra_hv_ivc_channel_notified(struct tegra_hv_ivc_cookie *ivck)
{
	struct ivc *ivc = &cookie_to_ivc_dev(ivck)->ivc;

	return tegra_ivc_channel_notified(ivc);
}
EXPORT_SYMBOL(tegra_hv_ivc_channel_notified);

void tegra_hv_ivc_channel_reset(struct tegra_hv_ivc_cookie *ivck)
{
	struct hv_ivc *ivc = cookie_to_ivc_dev(ivck);

	if (ivc->cookie_ops) {
		ERR("reset unsupported with callbacks");
		BUG();
	}

	tegra_ivc_channel_reset(&ivc->ivc);
}
EXPORT_SYMBOL(tegra_hv_ivc_channel_reset);

core_initcall(tegra_hv_init);

MODULE_LICENSE("GPL");
