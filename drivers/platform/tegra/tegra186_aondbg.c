/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION. All rights reserved.
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
 * You should have eeceived a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/tegra-aon.h>
#include <linux/mailbox_client.h>
#include <linux/uaccess.h>
#include <linux/completion.h>
#include <linux/jiffies.h>

#include <asm/cacheflush.h>

#include "aon-ivc-dbg-messages.h"

#define AON_REQUEST_MASK	0xF
#define AON_REQUESTS_TOTAL	(AON_REQUEST_TYPE_MAX + 1)
#define AON_PSTATES_TOTAL	7
#define AON_PSTATE_MASK		(0x7F << AON_REQUESTS_TOTAL)
#define AON_STATE(req)		((req & AON_PSTATE_MASK) >>  AON_REQUESTS_TOTAL)

#define TX_BLOCK_PERIOD 20

#define AON_ROOT	0
#define AON_PM		1
#define AON_MODS	2
#define AON_STATS	3

#define IVC_DBG_CH_FRAME_SIZE 128
#define MODS_DEFAULT_VAL 0xFFFF
#define PSTATE_NAME_MAX_LEN 15

static struct device *aondev;

struct tegra_aondbg {
	struct device *dev;
	struct mbox_client cl;
	struct mbox_chan *mbox;
	struct dentry *aon_root;
};

struct aon_dbgfs_node {
	char *name;
	u32 id;
	u8 pdr_id;
	mode_t mode;
	struct completion *wait_on;
	const struct file_operations *fops;
	char data[IVC_DBG_CH_FRAME_SIZE];
};

struct dbgfs_dir {
	const char *name;
	struct dentry *dir;
	struct dbgfs_dir *parent;
};

static struct dbgfs_dir aon_dbgfs_dirs[] = {
	{.name = "aon", .parent = NULL},
	{.name = "aon_pm", .parent = &aon_dbgfs_dirs[AON_ROOT]},
	{.name = "aon_mods", .parent = &aon_dbgfs_dirs[AON_ROOT]},
	{.name = "stats", .parent = &aon_dbgfs_dirs[AON_PM]}
};

static const char *const pstates[] = {"idle", "standby", "dormant", "deepidle",
				"deepstandby", "deepdormant", "active"};

static u32 thresholds[AON_PSTATES_TOTAL];

static u32 mods_result = MODS_DEFAULT_VAL;

static unsigned int completion_timeout = 50;

static DEFINE_MUTEX(aon_mutex);
static DEFINE_SPINLOCK(mods);
static DEFINE_SPINLOCK(completion);

static struct aon_dbgfs_node aon_nodes[];

static void set_mods_result(u32 result)
{
	spin_lock(&mods);
	mods_result = result;
	spin_unlock(&mods);
}

static unsigned int get_mods_result(void)
{
	u32 val;

	spin_lock(&mods);
	val = mods_result;
	spin_unlock(&mods);

	return val;
}

static void set_completion_timeout(unsigned int timeout)
{
	spin_lock(&completion);
	completion_timeout = timeout;
	spin_unlock(&completion);
}

static unsigned int get_completion_timeout(void)
{
	unsigned int val;

	spin_lock(&completion);
	val = completion_timeout;
	spin_unlock(&completion);

	return val;
}

static struct aon_dbg_response *aon_create_ivc_dbg_req(u32 request,
					u32 flag, u32 data)
{
	struct tegra_aondbg *aondbg;
	struct aon_dbg_request req;
	struct aon_dbg_response *resp;
	struct tegra_aon_mbox_msg msg;
	int ret = 0;
	uint16_t pstate = 0;
	unsigned int timeout = 0;

	aondbg = dev_get_drvdata(aondev);
	pstate = (request > AON_REQUEST_TYPE_MAX) ?
			__builtin_ctz(AON_STATE(request)) : 0;
	req.req_type = request & AON_REQUEST_MASK;
	switch (req.req_type) {
	case AON_PM_TARGET_POWER_STATE:
		req.data.pm_xfer.type.pstate.flag = flag;
		req.data.pm_xfer.type.pstate.state = (flag) ? data : 0;
		break;
	case AON_PM_THRESHOLD:
		req.data.pm_xfer.type.threshold.flag = flag;
		req.data.pm_xfer.type.threshold.state = pstate;
		req.data.pm_xfer.type.threshold.val = data;
		break;
	case AON_PM_WAKE_TIMEOUT:
		req.data.pm_xfer.type.wake_tout.flag = flag;
		req.data.pm_xfer.type.wake_tout.timeout = (flag) ? data : 0;
		break;
	case AON_PM_FORCE_SLEEP:
		req.data.pm_xfer.type.force_sleep.force_entry = data;
		break;
	case AON_MODS_LOOPS_TEST:
		req.data.mods_xfer.loops = data;
		break;
	case AON_PM_VDD_RTC_RETENTION:
		req.data.pm_xfer.type.retention.flag = flag;
		req.data.pm_xfer.type.retention.enable = (flag) ? data : 0;
		break;
	case AON_PM_SC8_COUNT:
	case AON_MODS_CRC:
	case AON_PM_STATE_TIME:
	case AON_PM_STATE_COUNT:
	case AON_PING_TEST:
		break;
	default:
		dev_err(aondev, "Invalid aon dbg request\n");
		return ERR_PTR(-EINVAL);
	}

	msg.length = sizeof(struct aon_dbg_request);
	msg.data = (void *)&req;
	ret = mbox_send_message(aondbg->mbox, (void *)&msg);
	if (ret < 0) {
		dev_err(aondev, "mbox_send_message failed\n");
		return ERR_PTR(ret);
	}
	timeout = get_completion_timeout();
	ret = wait_for_completion_timeout(aon_nodes[req.req_type].wait_on,
				msecs_to_jiffies(timeout));
	if (!ret) {
		dev_err(aondev, "No response\n");
		return ERR_PTR(-ETIMEDOUT);
	}
	resp = (void *)aon_nodes[req.req_type].data;
	if (resp->resp_type > AON_REQUEST_TYPE_MAX) {
		dev_err(aondev, "Invalid aon dbg response\n");
		return ERR_PTR(-EIO);
	}
	if (resp->status != AON_DBG_STATUS_OK) {
		dev_err(aondev, "Request failed\n");
		return ERR_PTR(-EIO);
	}

	return resp;
}


static ssize_t aon_pm_target_power_state_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[PSTATE_NAME_MAX_LEN];
	char *name;
	struct aon_dbg_response *resp;
	int i;

	if (count >= sizeof(buf))
		return -EINVAL;

	if (strncpy_from_user(buf, user_buf, count) <= 0)
		return -EFAULT;

	buf[count] = '\0';
	name = strim(buf);

	for (i = 0; i < ARRAY_SIZE(pstates); i++) {
		if (!strcmp(name, pstates[i]))
			break;
	}

	if (i >= ARRAY_SIZE(pstates))
		return -EINVAL;

	mutex_lock(&aon_mutex);
	resp = aon_create_ivc_dbg_req(AON_PM_TARGET_POWER_STATE,
					WRITE, i);
	mutex_unlock(&aon_mutex);
	if (IS_ERR(resp))
		return PTR_ERR(resp);

	return count;
}

static int aon_pm_target_power_state_show(struct seq_file *file, void *data)
{
	struct aon_dbg_response *resp;
	int pstate_index;

	mutex_lock(&aon_mutex);
	resp = aon_create_ivc_dbg_req(*(u32 *)file->private, READ, 0);
	if (IS_ERR(resp)) {
		mutex_unlock(&aon_mutex);
		return PTR_ERR(resp);
	}
	pstate_index = resp->data.pm_xfer.type.pstate.state;
	if (pstate_index < AON_PSTATES_TOTAL) {
		seq_printf(file, "%s\n",
			pstates[pstate_index]);
	} else {
		dev_err(aondev, "invalid pstate in response\n");
	}
	mutex_unlock(&aon_mutex);

	return 0;
}

static int aon_pm_target_power_state_open(struct inode *inode,
					struct file *file)
{
	return single_open(file, aon_pm_target_power_state_show,
			inode->i_private);
}

static const struct file_operations aon_pm_target_power_state_fops = {
	.open = aon_pm_target_power_state_open,
	.llseek = seq_lseek,
	.read = seq_read,
	.write = aon_pm_target_power_state_write,
	.release = single_release
};

static int aon_pm_show(void *data, u64 *val)
{
	struct aon_dbg_response *resp;

	mutex_lock(&aon_mutex);
	resp = aon_create_ivc_dbg_req(*(u32 *)data, READ, 0);
	if (IS_ERR(resp)) {
		mutex_unlock(&aon_mutex);
		return PTR_ERR(resp);
	}
	switch (resp->resp_type) {
	case AON_PM_WAKE_TIMEOUT:
		*val = resp->data.pm_xfer.type.wake_tout.timeout;
		break;
	case AON_PM_THRESHOLD:
		*val = resp->data.pm_xfer.type.threshold.val;
		break;
	case AON_PM_VDD_RTC_RETENTION:
		*val = resp->data.pm_xfer.type.retention.enable;
		break;
	case AON_PM_SC8_COUNT:
		*val = resp->data.pm_xfer.type.sc8_count.count;
		break;
	default:
		dev_err(aondev, "Invalid pm response\n");
		break;
	}
	mutex_unlock(&aon_mutex);

	return 0;
}

static int aon_pm_store(void *data, u64 val)
{
	struct aon_dbg_response *resp;

	mutex_lock(&aon_mutex);
	resp = aon_create_ivc_dbg_req(*(u32 *)data, WRITE, val);
	mutex_unlock(&aon_mutex);
	if (IS_ERR(resp))
		return PTR_ERR(resp);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(aon_pm_threshold_fops, aon_pm_show,
			aon_pm_store, "%lld\n");
DEFINE_SIMPLE_ATTRIBUTE(aon_pm_wake_timeout_fops, aon_pm_show,
			aon_pm_store, "%lld\n");
DEFINE_SIMPLE_ATTRIBUTE(aon_pm_force_sleep_fops, NULL,
			aon_pm_store, "%lld\n");
DEFINE_SIMPLE_ATTRIBUTE(aon_pm_retention_fops, aon_pm_show,
			aon_pm_store, "%lld\n");
DEFINE_SIMPLE_ATTRIBUTE(aon_pm_sc8_count_fops, aon_pm_show,
			NULL, "%lld\n");

static int aon_pm_state_times_show(struct seq_file *file, void *data)
{
	struct aon_dbg_response *resp;
	int i, len;
	u64 *arr;

	mutex_lock(&aon_mutex);
	resp = aon_create_ivc_dbg_req(*(u32 *)file->private, READ, 0);
	if (IS_ERR(resp)) {
		mutex_unlock(&aon_mutex);
		return PTR_ERR(resp);
	}
	len = ARRAY_SIZE(resp->data.pm_xfer.type.state_times.state_durations);
	arr = resp->data.pm_xfer.type.state_times.state_durations;
	for (i = 0; i < len; i++)
		seq_printf(file, "%s: %llu\n", pstates[i], arr[i]);
	mutex_unlock(&aon_mutex);

	return 0;
}

static int aon_pm_state_times_open(struct inode *inode, struct file *file)
{
	return single_open(file, aon_pm_state_times_show, inode->i_private);
}

static const struct file_operations aon_pm_state_times_fops = {
	.open = aon_pm_state_times_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

static int aon_pm_state_counts_show(struct seq_file *file, void *data)
{
	struct aon_dbg_response *resp;
	int i, len;
	u32 *arr;

	mutex_lock(&aon_mutex);
	resp = aon_create_ivc_dbg_req(*(u32 *)file->private, READ, 0);
	if (IS_ERR(resp)) {
		mutex_unlock(&aon_mutex);
		return PTR_ERR(resp);
	}
	len = ARRAY_SIZE(resp->data.pm_xfer.type.state_counts.state_entry_counts);
	arr = resp->data.pm_xfer.type.state_counts.state_entry_counts;
	for (i = 0; i < len; i++)
		seq_printf(file, "%s: %u\n", pstates[i], arr[i]);
	mutex_unlock(&aon_mutex);

	return 0;
}

static int aon_pm_state_counts_open(struct inode *inode, struct file *file)
{
	return single_open(file, aon_pm_state_counts_show, inode->i_private);
}

static const struct file_operations aon_pm_state_counts_fops = {
	.open = aon_pm_state_counts_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

static int aon_ping_show(struct seq_file *file, void *data)
{
	struct aon_dbg_response *resp;
	int ret = 0;

	mutex_lock(&aon_mutex);
	resp = aon_create_ivc_dbg_req(*(u32 *)file->private, READ, 0);
	if (IS_ERR(resp))
		ret = PTR_ERR(resp);
	else
		seq_printf(file, "%s\n", resp->data.ping_xfer.data);
	mutex_unlock(&aon_mutex);

	return ret;
}

static int aon_ping_open(struct inode *inode, struct file *file)
{
	return single_open(file, aon_ping_show, inode->i_private);
}

static const struct file_operations aon_ping_fops = {
	.open = aon_ping_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

static int aon_mods_loops_store(void *data, u64 val)
{
	struct aon_dbg_response *resp;
	int ret = 0;

	mutex_lock(&aon_mutex);
	set_mods_result(MODS_DEFAULT_VAL);
	resp = aon_create_ivc_dbg_req(*(u32 *)data, WRITE, val);
	if (IS_ERR(resp))
		ret = PTR_ERR(resp);
	else
		set_mods_result(resp->status);
	mutex_unlock(&aon_mutex);

	return ret;
}

DEFINE_SIMPLE_ATTRIBUTE(aon_mods_loops_fops, NULL,
			aon_mods_loops_store, "%lld\n");

static int aon_mods_result_show(void *data, u64 *val)
{
	*val = get_mods_result();
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(aon_mods_result_fops, aon_mods_result_show,
			NULL, "%lld\n");

static int aon_mods_crc_show(void *data, u64 *val)
{
	struct aon_dbg_response *resp;
	int ret = 0;

	mutex_lock(&aon_mutex);
	resp = aon_create_ivc_dbg_req(*(u32 *)data, READ, 0);
	if (IS_ERR(resp))
		ret = PTR_ERR(resp);
	else
		*val = resp->data.crc_xfer.crc;
	mutex_unlock(&aon_mutex);

	return ret;
}

DEFINE_SIMPLE_ATTRIBUTE(aon_mods_crc_fops, aon_mods_crc_show,
			NULL, "%llx\n");

static int aon_timeout_show(void *data, u64 *val)
{
	*val = get_completion_timeout();
	return 0;
}

static int aon_timeout_store(void *data, u64 val)
{
	set_completion_timeout(val);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(aon_timeout_fops, aon_timeout_show,
			aon_timeout_store, "%lld\n");

static struct aon_dbgfs_node aon_nodes[] = {
	{.name = "target_power_state", .id = AON_PM_TARGET_POWER_STATE,
			.pdr_id = AON_PM, .mode = S_IRUGO | S_IWUSR,
			.fops = &aon_pm_target_power_state_fops,},
	{.name = "threshold", .id = AON_PM_THRESHOLD, .pdr_id = AON_PM,
			.mode = S_IRUGO | S_IWUSR,
			.fops = &aon_pm_threshold_fops,},
	{.name = "wake_timeout", .id = AON_PM_WAKE_TIMEOUT, .pdr_id = AON_PM,
			.mode = S_IRUGO | S_IWUSR,
			.fops = &aon_pm_wake_timeout_fops,},
	{.name = "force_sleep", .id = AON_PM_FORCE_SLEEP, .pdr_id = AON_PM,
			.mode = S_IRUGO | S_IWUSR,
			.fops = &aon_pm_force_sleep_fops,},
	{.name = "state_times", .id = AON_PM_STATE_TIME,
			.pdr_id = AON_STATS, .mode = S_IRUGO,
			.fops = &aon_pm_state_times_fops,},
	{.name = "state_counts", .id = AON_PM_STATE_COUNT,
			.pdr_id = AON_STATS, .mode = S_IRUGO,
			.fops = &aon_pm_state_counts_fops,},
	{.name = "loops", .id = AON_MODS_LOOPS_TEST, .pdr_id = AON_MODS,
			.mode = S_IWUSR, .fops = &aon_mods_loops_fops, },
	{.name = "result", .id = AON_MODS_RESULT, .pdr_id = AON_MODS,
			.mode = S_IRUGO , .fops = &aon_mods_result_fops,},
	{.name = "crc", .id = AON_MODS_CRC, .pdr_id = AON_MODS,
			.mode = S_IRUGO , .fops = &aon_mods_crc_fops,},
	{.name = "ping", .id = AON_PING_TEST, .pdr_id = AON_ROOT,
			.mode = S_IRUGO | S_IWUSR, .fops = &aon_ping_fops,},
	{.name = "enable_retention", .id = AON_PM_VDD_RTC_RETENTION,
			.pdr_id = AON_PM, .mode = S_IRUGO | S_IWUSR,
			.fops = &aon_pm_retention_fops,},
	{.name = "sc8_count", .id = AON_PM_SC8_COUNT,
			.pdr_id = AON_STATS, .mode = S_IRUGO,
			.fops = &aon_pm_sc8_count_fops,},
	{.name = "completion_timeout", .pdr_id = AON_ROOT,
			.mode = S_IRUGO | S_IWUSR, .fops = &aon_timeout_fops,},
};

static void tegra_aondbg_recv_msg(struct mbox_client *cl, void *rx_msg)
{
	struct tegra_aon_mbox_msg *msg;
	struct aon_dbg_response *resp;

	msg = (struct tegra_aon_mbox_msg *)rx_msg;
	resp = (void *)msg->data;
	if (resp->resp_type > AON_REQUEST_TYPE_MAX) {
		dev_err(aondev, "Multiple request types in 1 response\n");
		return;
	}
	memcpy(aon_nodes[resp->resp_type].data, msg->data,
					IVC_DBG_CH_FRAME_SIZE);
	complete(aon_nodes[resp->resp_type].wait_on);
}

static int aon_pm_dbg_init(struct tegra_aondbg *aon)
{
	struct dentry *d;
	struct dentry *td;
	int i, j;

	d = debugfs_create_dir(aon_dbgfs_dirs[0].name, NULL);
	if (IS_ERR_OR_NULL(d))
		goto clean;
	aon_dbgfs_dirs[0].dir = d;
	aon->aon_root = d;

	for (i = 1; i < ARRAY_SIZE(aon_dbgfs_dirs); i++) {
		d = debugfs_create_dir(aon_dbgfs_dirs[i].name,
					aon_dbgfs_dirs[i].parent->dir);
		if (IS_ERR_OR_NULL(d))
			goto clean;
		aon_dbgfs_dirs[i].dir = d;
	}

	for (i = 0; i < ARRAY_SIZE(aon_nodes); i++) {
		if (strcmp(aon_nodes[i].name, "threshold")) {
			d = debugfs_create_file(aon_nodes[i].name,
				aon_nodes[i].mode,
				aon_dbgfs_dirs[aon_nodes[i].pdr_id].dir,
				&aon_nodes[i].id, aon_nodes[i].fops);
			if (IS_ERR_OR_NULL(d))
				goto clean;
			continue;
		}
		d = debugfs_create_dir(aon_nodes[i].name,
				aon_dbgfs_dirs[aon_nodes[i].pdr_id].dir);
		if (IS_ERR_OR_NULL(d))
			goto clean;
		/*
		 * We only need to loop till off by one as we don't need
		 * active state as a separate node in threshold directory.
		 */
		for (j = 0; j < ARRAY_SIZE(pstates) - 1; j++) {
			thresholds[j] = aon_nodes[i].id |
					BIT(j + AON_REQUESTS_TOTAL);
			td = debugfs_create_file(pstates[j], aon_nodes[i].mode,
					d, &thresholds[j], aon_nodes[i].fops);
			if (IS_ERR_OR_NULL(d)) {
				d = td;
				goto clean;
			}
		}
	}

	return 0;

clean:
	debugfs_remove_recursive(aon->aon_root);
	return PTR_ERR(d);
}

#define NV(p) "nvidia," p

static int tegra_aondbg_probe(struct platform_device *pdev)
{
	struct tegra_aondbg *aondbg;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	int i;
	int ret;

	aondev = &pdev->dev;
	dev_dbg(dev, "aondbg driver probe()\n");

	if (!np) {
		dev_err(dev, "tegra-aondbg: DT data required.\n");
		return -EINVAL;
	}

	aondbg = devm_kzalloc(&pdev->dev, sizeof(*aondbg), GFP_KERNEL);
	if (!aondbg)
		return -ENOMEM;
	dev_set_drvdata(&pdev->dev, aondbg);
	aondbg->dev = &pdev->dev;
	aondbg->cl.dev = &pdev->dev;
	aondbg->cl.tx_block = true;
	aondbg->cl.tx_tout = TX_BLOCK_PERIOD;
	aondbg->cl.knows_txdone = false;
	aondbg->cl.rx_callback = tegra_aondbg_recv_msg;
	aondbg->mbox = mbox_request_channel(&aondbg->cl, 0);
	if (IS_ERR(aondbg->mbox)) {
		dev_warn(&pdev->dev,
			 "can't get mailbox channel (%d)\n",
			 (int)PTR_ERR(aondbg->mbox));
		return PTR_ERR(aondbg->mbox);
	}
	dev_dbg(dev, "aondbg->mbox = %p\n", aondbg->mbox);

	for (i = 0; i < ARRAY_SIZE(aon_nodes); i++) {
		aon_nodes[i].wait_on = devm_kzalloc(&pdev->dev,
					sizeof(struct completion), GFP_KERNEL);
		if (!aon_nodes[i].wait_on) {
			dev_err(dev, "out of memory.\n");
			return -ENOMEM;
		}
		init_completion(aon_nodes[i].wait_on);
	}

	ret = aon_pm_dbg_init(aondbg);
	if (ret) {
		dev_err(dev, "failed to create debugfs nodes.\n");
		return ret;
	}
	dev_info(dev, "aondbg driver probe() OK\n");

	return 0;
}

static int tegra_aondbg_remove(struct platform_device *pdev)
{
	struct tegra_aondbg *aondbg;

	aondbg = dev_get_drvdata(&pdev->dev);
	mbox_free_channel(aondbg->mbox);
	debugfs_remove_recursive(aondbg->aon_root);

	return 0;
}

static const struct of_device_id tegra_aondbg_of_match[] = {
	{
		.compatible = "nvidia,tegra186-aondbg",
	},
	{},
};
MODULE_DEVICE_TABLE(of, tegra_aondbg_of_match);

static struct platform_driver tegra_aondbg_driver = {
	.driver = {
		.name   = "tegra186-aondbg",
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(tegra_aondbg_of_match),
	},
	.probe = tegra_aondbg_probe,
	.remove = tegra_aondbg_remove,
};
module_platform_driver(tegra_aondbg_driver);

MODULE_DESCRIPTION("AON DBG driver");
MODULE_AUTHOR("NVIDIA");
MODULE_LICENSE("GPL v2");
