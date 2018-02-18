/*
 * drivers/misc/tegra-cec/tegra_cec.c
 *
 * Copyright (c) 2012-2016, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/ktime.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/clk/tegra.h>
#include <linux/of.h>

#include "tegra_cec.h"

#include <linux/tegra-powergate.h>

static ssize_t cec_logical_addr_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t cec_logical_addr_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static DEVICE_ATTR(cec_logical_addr_config, S_IWUSR | S_IRUGO,
		cec_logical_addr_show, cec_logical_addr_store);

static int tegra_cec_open(struct inode *inode, struct file *file)
{
	struct miscdevice *miscdev = file->private_data;
	struct tegra_cec *cec = container_of(miscdev,
		struct tegra_cec, misc_dev);
	int ret = 0;

	dev_dbg(cec->dev, "%s\n", __func__);

	ret = wait_event_interruptible(cec->init_waitq,
	    atomic_read(&cec->init_done) == 1);
	if (ret)
		return ret;
	file->private_data = cec;

	return ret;
}

static int tegra_cec_release(struct inode *inode, struct file *file)
{
	struct tegra_cec *cec = file->private_data;

	dev_dbg(cec->dev, "%s\n", __func__);

	return 0;
}

static inline void tegra_cec_native_tx(const struct tegra_cec *cec, u32 block)
{
	writel(block, cec->cec_base + TEGRA_CEC_TX_REGISTER);
	writel(TEGRA_CEC_INT_STAT_TX_REGISTER_EMPTY,
		cec->cec_base + TEGRA_CEC_INT_STAT);
}

static
int tegra_cec_native_write_l(struct tegra_cec *cec, const u8 *buf, size_t cnt)
{
	int ret;
	size_t i;
	u32 start, mode, eom;
	u32 mask;

	/*
	 * In case previous transmission was interrupted by signal,
	 *  driver will try to complete the frame anyway.  However,
	 *  this means we have to wait for it to finish before beginning
	 *  subsequent transmission.
	 */
	ret = wait_event_interruptible(cec->tx_waitq, cec->tx_wake == 1);
	if (ret)
		return ret;

	mode = TEGRA_CEC_LADDR_MODE(buf[0]) << TEGRA_CEC_TX_REG_ADDR_MODE_SHIFT;

	cec->tx_wake = 0;
	cec->tx_error = 0;
	cec->tx_buf_cur = 0;
	cec->tx_buf_cnt = cnt;

	for (i = 0; i < cnt; i++) {
		start = i == 0 ? (1 << TEGRA_CEC_TX_REG_START_BIT_SHIFT) : 0;
		eom   = i == cnt-1 ? (1 << TEGRA_CEC_TX_REG_EOM_SHIFT) : 0;
		cec->tx_buf[i] = start | mode | eom | buf[i];
	}

	mask = readl(cec->cec_base + TEGRA_CEC_INT_MASK);
	writel(mask | TEGRA_CEC_INT_MASK_TX_REGISTER_EMPTY,
		cec->cec_base + TEGRA_CEC_INT_MASK);

	ret = wait_event_interruptible(cec->tx_waitq, cec->tx_wake == 1);
	if (!ret)
		ret = cec->tx_error;

	return ret;
}

static ssize_t tegra_cec_write(struct file *file, const char __user *buf,
	size_t count, loff_t *ppos)
{
	u8 tx_buf[TEGRA_CEC_FRAME_MAX_LENGTH];
	struct tegra_cec *cec = file->private_data;
	ssize_t ret;

	if (count == 0 || count > TEGRA_CEC_FRAME_MAX_LENGTH)
		return -EMSGSIZE;

	ret = wait_event_interruptible(cec->init_waitq,
	    atomic_read(&cec->init_done) == 1);
	if (ret)
		return ret;

	if (copy_from_user(tx_buf, buf, count))
		return -EFAULT;

	mutex_lock(&cec->tx_lock);
	ret = tegra_cec_native_write_l(cec, tx_buf, count);
	mutex_unlock(&cec->tx_lock);
	if (ret)
		return ret;
	else
		return count;
}

static ssize_t tegra_cec_read(struct file *file, char  __user *buffer,
	size_t count, loff_t *ppos)
{
	struct tegra_cec *cec = file->private_data;
	ssize_t ret;

	count = sizeof(cec->rx_buffer);

	ret = wait_event_interruptible(cec->init_waitq,
	    atomic_read(&cec->init_done) == 1);
	if (ret)
		return ret;

	if (cec->rx_wake == 0)
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;

	ret = wait_event_interruptible(cec->rx_waitq, cec->rx_wake == 1);
	if (ret)
		return ret;

	if (copy_to_user(buffer, &(cec->rx_buffer), count))
		return -EFAULT;

	cec->rx_buffer = 0x0;
	cec->rx_wake = 0;
	return count;
}

static inline void tegra_cec_error_recovery(struct tegra_cec *cec)
{
	u32 hw_ctrl;

	hw_ctrl = readl(cec->cec_base + TEGRA_CEC_HW_CONTROL);
	writel(0x0, cec->cec_base + TEGRA_CEC_HW_CONTROL);
	writel(0xFFFFFFFF, cec->cec_base + TEGRA_CEC_INT_STAT);
	writel(hw_ctrl, cec->cec_base + TEGRA_CEC_HW_CONTROL);
}

static irqreturn_t tegra_cec_irq_handler(int irq, void *data)
{
	struct device *dev = data;
	struct tegra_cec *cec = dev_get_drvdata(dev);
	u32 status, mask;

	status = readl(cec->cec_base + TEGRA_CEC_INT_STAT);
	mask = readl(cec->cec_base + TEGRA_CEC_INT_MASK);

	status &= mask;

	if (!status)
		goto out;

	if (status & TEGRA_CEC_INT_STAT_TX_REGISTER_UNDERRUN) {
		dev_err(dev, "tegra_cec: TX underrun, interrupt timing issue!\n");

		tegra_cec_error_recovery(cec);
		writel(mask & ~TEGRA_CEC_INT_MASK_TX_REGISTER_EMPTY,
			cec->cec_base + TEGRA_CEC_INT_MASK);

		cec->tx_error = -EIO;
		cec->tx_wake = 1;

		wake_up_interruptible(&cec->tx_waitq);

		goto out;
	} else if ((status & TEGRA_CEC_INT_STAT_TX_ARBITRATION_FAILED) ||
		   (status & TEGRA_CEC_INT_STAT_TX_BUS_ANOMALY_DETECTED)) {
		tegra_cec_error_recovery(cec);
		writel(mask & ~TEGRA_CEC_INT_MASK_TX_REGISTER_EMPTY,
			cec->cec_base + TEGRA_CEC_INT_MASK);

		cec->tx_error = -ECOMM;
		cec->tx_wake = 1;

		wake_up_interruptible(&cec->tx_waitq);

		goto out;
	} else if (status & TEGRA_CEC_INT_STAT_TX_FRAME_TRANSMITTED) {
		writel((TEGRA_CEC_INT_STAT_TX_FRAME_TRANSMITTED),
			cec->cec_base + TEGRA_CEC_INT_STAT);

		if (status & TEGRA_CEC_INT_STAT_TX_FRAME_OR_BLOCK_NAKD) {
			tegra_cec_error_recovery(cec);

			cec->tx_error = TEGRA_CEC_LADDR_MODE(cec->tx_buf[0]) ?
					-ECONNRESET : -EHOSTUNREACH;
		}
		cec->tx_wake = 1;

		wake_up_interruptible(&cec->tx_waitq);

		goto out;
	} else if (status & TEGRA_CEC_INT_STAT_TX_FRAME_OR_BLOCK_NAKD)
		dev_warn(dev, "tegra_cec: TX NAKed on the fly!\n");

	if (status & TEGRA_CEC_INT_STAT_TX_REGISTER_EMPTY) {
		if (cec->tx_buf_cur == cec->tx_buf_cnt)
			writel(mask & ~TEGRA_CEC_INT_MASK_TX_REGISTER_EMPTY,
				cec->cec_base + TEGRA_CEC_INT_MASK);
		else
			tegra_cec_native_tx(cec,
				cec->tx_buf[cec->tx_buf_cur++]);
	}

	if (status & (TEGRA_CEC_INT_STAT_RX_REGISTER_OVERRUN |
		TEGRA_CEC_INT_STAT_RX_BUS_ANOMALY_DETECTED |
		TEGRA_CEC_INT_STAT_RX_START_BIT_DETECTED |
		TEGRA_CEC_INT_STAT_RX_BUS_ERROR_DETECTED)) {
		writel((TEGRA_CEC_INT_STAT_RX_REGISTER_OVERRUN |
			TEGRA_CEC_INT_STAT_RX_BUS_ANOMALY_DETECTED |
			TEGRA_CEC_INT_STAT_RX_START_BIT_DETECTED |
			TEGRA_CEC_INT_STAT_RX_BUS_ERROR_DETECTED),
			cec->cec_base + TEGRA_CEC_INT_STAT);
	} else if (status & TEGRA_CEC_INT_STAT_RX_REGISTER_FULL) {
		writel(TEGRA_CEC_INT_STAT_RX_REGISTER_FULL,
			cec->cec_base + TEGRA_CEC_INT_STAT);
		cec->rx_buffer = readw(cec->cec_base + TEGRA_CEC_RX_REGISTER);
		cec->rx_wake = 1;
		wake_up_interruptible(&cec->rx_waitq);
	}

out:
	return IRQ_HANDLED;
}

static const struct file_operations tegra_cec_fops = {
	.owner = THIS_MODULE,
	.open = tegra_cec_open,
	.release = tegra_cec_release,
	.read = tegra_cec_read,
	.write = tegra_cec_write,
};

static void tegra_cec_init(struct tegra_cec *cec)
{
	int power_tmp = 0, is_tegra186 = 0, ret;

	cec->rx_wake = 0;
	cec->tx_wake = 1;
	cec->tx_buf_cnt = 0;
	cec->tx_buf_cur = 0;
	cec->tx_error = 0;

	dev_notice(cec->dev, "%s started\n", __func__);

	is_tegra186 = of_device_is_compatible(cec->dev->of_node, "nvidia,tegra186-cec");
	if (is_tegra186 && !tegra_powergate_is_powered(TEGRA_POWERGATE_DISA)) {
		ret  = tegra_unpowergate_partition_with_clk_on(TEGRA_POWERGATE_DISA);
		if (ret) {
			dev_err(cec->dev, "Fail to unpowergate DISP.\n");
			return;
		}
		power_tmp = 1;
		dev_warn(cec->dev, "Unpowergate DISP temporarily.\n");
	}

	writel(0x00, cec->cec_base + TEGRA_CEC_HW_CONTROL);
	writel(0x00, cec->cec_base + TEGRA_CEC_INT_MASK);
	writel(0xffffffff, cec->cec_base + TEGRA_CEC_INT_STAT);

#ifdef CONFIG_PM
	if (wait_event_interruptible_timeout(cec->suspend_waitq,
				atomic_xchg(&cec->init_cancel, 0) == 1,
				msecs_to_jiffies(1000)) > 0)
		return;
#else
	msleep(1000);
#endif

	writel(0x00, cec->cec_base + TEGRA_CEC_SW_CONTROL);

	cec->logical_addr = TEGRA_CEC_HWCTRL_RX_LADDR_UNREG;
	writel(TEGRA_CEC_HWCTRL_RX_LADDR(cec->logical_addr) |
		TEGRA_CEC_HWCTRL_TX_NAK_MODE |
		TEGRA_CEC_HWCTRL_TX_RX_MODE,
		cec->cec_base + TEGRA_CEC_HW_CONTROL);

	writel((1U << 31) | 0x20, cec->cec_base + TEGRA_CEC_INPUT_FILTER);

	writel((0x7a << TEGRA_CEC_RX_TIMING_0_RX_START_BIT_MAX_LO_TIME_MASK) |
	   (0x6d << TEGRA_CEC_RX_TIMING_0_RX_START_BIT_MIN_LO_TIME_MASK) |
	   (0x93 << TEGRA_CEC_RX_TIMING_0_RX_START_BIT_MAX_DURATION_MASK) |
	   (0x86 << TEGRA_CEC_RX_TIMING_0_RX_START_BIT_MIN_DURATION_MASK),
	   cec->cec_base + TEGRA_CEC_RX_TIMING_0);

	writel((0x35 << TEGRA_CEC_RX_TIMING_1_RX_DATA_BIT_MAX_LO_TIME_MASK) |
	   (0x21 << TEGRA_CEC_RX_TIMING_1_RX_DATA_BIT_SAMPLE_TIME_MASK) |
	   (0x56 << TEGRA_CEC_RX_TIMING_1_RX_DATA_BIT_MAX_DURATION_MASK) |
	   (0x40 << TEGRA_CEC_RX_TIMING_1_RX_DATA_BIT_MIN_DURATION_MASK),
	   cec->cec_base + TEGRA_CEC_RX_TIMING_1);

	writel((0x50 << TEGRA_CEC_RX_TIMING_2_RX_END_OF_BLOCK_TIME_MASK),
	   cec->cec_base + TEGRA_CEC_RX_TIMING_2);

	writel((0x74 << TEGRA_CEC_TX_TIMING_0_TX_START_BIT_LO_TIME_MASK) |
	   (0x8d << TEGRA_CEC_TX_TIMING_0_TX_START_BIT_DURATION_MASK) |
	   (0x08 << TEGRA_CEC_TX_TIMING_0_TX_BUS_XITION_TIME_MASK) |
	   (0x71 << TEGRA_CEC_TX_TIMING_0_TX_BUS_ERROR_LO_TIME_MASK),
	   cec->cec_base + TEGRA_CEC_TX_TIMING_0);

	writel((0x2f << TEGRA_CEC_TX_TIMING_1_TX_LO_DATA_BIT_LO_TIME_MASK) |
	   (0x13 << TEGRA_CEC_TX_TIMING_1_TX_HI_DATA_BIT_LO_TIME_MASK) |
	   (0x4b << TEGRA_CEC_TX_TIMING_1_TX_DATA_BIT_DURATION_MASK) |
	   (0x21 << TEGRA_CEC_TX_TIMING_1_TX_ACK_NAK_BIT_SAMPLE_TIME_MASK),
	   cec->cec_base + TEGRA_CEC_TX_TIMING_1);

	writel((0x07 << TEGRA_CEC_TX_TIMING_2_BUS_IDLE_TIME_ADDITIONAL_FRAME_MASK) |
	   (0x05 << TEGRA_CEC_TX_TIMING_2_BUS_IDLE_TIME_NEW_FRAME_MASK) |
	   (0x03 << TEGRA_CEC_TX_TIMING_2_BUS_IDLE_TIME_RETRY_FRAME_MASK),
	   cec->cec_base + TEGRA_CEC_TX_TIMING_2);

	writel(TEGRA_CEC_INT_MASK_TX_REGISTER_UNDERRUN |
		TEGRA_CEC_INT_MASK_TX_FRAME_OR_BLOCK_NAKD |
		TEGRA_CEC_INT_MASK_TX_ARBITRATION_FAILED |
		TEGRA_CEC_INT_MASK_TX_BUS_ANOMALY_DETECTED |
		TEGRA_CEC_INT_MASK_TX_FRAME_TRANSMITTED |
		TEGRA_CEC_INT_MASK_RX_REGISTER_FULL |
		TEGRA_CEC_INT_MASK_RX_REGISTER_OVERRUN,
		cec->cec_base + TEGRA_CEC_INT_MASK);

	atomic_set(&cec->init_done, 1);
	wake_up_interruptible(&cec->init_waitq);

	if (is_tegra186 && power_tmp == 1) {
		ret = tegra_powergate_partition_with_clk_off(TEGRA_POWERGATE_DISA);
		if (ret) {
			dev_err(cec->dev, "Fail to powergate DISP.\n");
			return;
		}
		power_tmp = 0;
		dev_warn(cec->dev, "Powergate DISP.\n");
	}

	dev_notice(cec->dev, "%s Done.\n", __func__);
}

static void tegra_cec_init_worker(struct work_struct *work)
{
	struct tegra_cec *cec = container_of(work, struct tegra_cec, work);

	tegra_cec_init(cec);
}

static ssize_t cec_logical_addr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tegra_cec *cec = dev_get_drvdata(dev);

	if (!atomic_read(&cec->init_done))
		return -EAGAIN;

	if (buf)
		return sprintf(buf, "0x%x\n", (u32)cec->logical_addr);
	return 1;
}

static ssize_t cec_logical_addr_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret;
	u32 state;
	u16 addr;
	struct tegra_cec *cec;

	if (!buf || !count)
		return -EINVAL;

	cec = dev_get_drvdata(dev);
	if (!atomic_read(&cec->init_done))
		return -EAGAIN;

	ret = kstrtou16(buf, 0, &addr);
	if (ret)
		return ret;


	dev_info(dev, "tegra_cec: set logical address: %x\n", (u32)addr);
	cec->logical_addr = addr;
	state = readl(cec->cec_base + TEGRA_CEC_HW_CONTROL);
	state &= ~TEGRA_CEC_HWCTRL_RX_LADDR_MASK;
	state |= TEGRA_CEC_HWCTRL_RX_LADDR(cec->logical_addr);
	writel(state, cec->cec_base + TEGRA_CEC_HW_CONTROL);

	return count;
}

static int tegra_cec_probe(struct platform_device *pdev)
{
	struct tegra_cec *cec;
	struct resource *res;
#if defined(CONFIG_TEGRA_DISPLAY)
	struct device_node *np = pdev->dev->of_node;
#endif
	int ret = 0;

	cec = devm_kzalloc(&pdev->dev, sizeof(struct tegra_cec), GFP_KERNEL);

	if (!cec)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (!res) {
		dev_err(&pdev->dev,
		    "Unable to allocate resources for device.\n");
		ret = -EBUSY;
		goto cec_error;
	}

	if (!devm_request_mem_region(&pdev->dev, res->start, resource_size(res),
		pdev->name)) {
		dev_err(&pdev->dev,
			"Unable to request mem region for device.\n");
		ret = -EBUSY;
		goto cec_error;
	}

	cec->tegra_cec_irq = platform_get_irq(pdev, 0);

	if (cec->tegra_cec_irq <= 0) {
		ret = -EBUSY;
		goto cec_error;
	}

	cec->cec_base = devm_ioremap_nocache(&pdev->dev, res->start,
		resource_size(res));

	if (!cec->cec_base) {
		dev_err(&pdev->dev, "Unable to grab IOs for device.\n");
		ret = -EBUSY;
		goto cec_error;
	}

	dev_info(&pdev->dev, "dt=%d start=0x%08llX end=0x%08llX irq=%d\n",
		(pdev->dev.of_node != NULL),
		res->start, res->end,
		cec->tegra_cec_irq);

	atomic_set(&cec->init_done, 0);
	mutex_init(&cec->tx_lock);

#if defined(CONFIG_TEGRA_DISPLAY)
    if (np)
        cec->clk = of_clk_get_by_name(np, "cec");
#else
	cec->clk = clk_get(&pdev->dev, "cec");
#endif

	if (IS_ERR_OR_NULL(cec->clk)) {
		dev_err(&pdev->dev, "can't get clock for CEC\n");
		ret = -ENOENT;
		goto clk_error;
	}

	ret = clk_prepare_enable(cec->clk);
	dev_info(&pdev->dev, "Enable clock result: %d.\n", ret);

	/* set context info. */
	cec->dev = &pdev->dev;
	init_waitqueue_head(&cec->rx_waitq);
	init_waitqueue_head(&cec->tx_waitq);
	init_waitqueue_head(&cec->init_waitq);

#ifdef CONFIG_PM
	init_waitqueue_head(&cec->suspend_waitq);
	atomic_set(&cec->init_cancel, 0);
#endif

	platform_set_drvdata(pdev, cec);
	/* clear out the hardware. */

	INIT_WORK(&cec->work, tegra_cec_init_worker);
	schedule_work(&cec->work);

	device_init_wakeup(&pdev->dev, 1);

	cec->misc_dev.minor = MISC_DYNAMIC_MINOR;
	cec->misc_dev.name = TEGRA_CEC_NAME;
	cec->misc_dev.fops = &tegra_cec_fops;
	cec->misc_dev.parent = &pdev->dev;

	if (misc_register(&cec->misc_dev)) {
		printk(KERN_WARNING "Couldn't register device , %s.\n", TEGRA_CEC_NAME);
		goto cec_error;
	}

	ret = devm_request_irq(&pdev->dev, cec->tegra_cec_irq,
		tegra_cec_irq_handler, 0x0, "cec_irq", &pdev->dev);

	if (ret) {
		dev_err(&pdev->dev,
			"Unable to request interrupt for device (err=%d).\n", ret);
		goto cec_error;
	}

	ret = sysfs_create_file(
		&pdev->dev.kobj, &dev_attr_cec_logical_addr_config.attr);
	dev_info(&pdev->dev, "cec_add_sysfs ret=%d\n", ret);
	if (ret != 0) {
		dev_err(&pdev->dev, "Failed to add sysfs: %d\n", ret);
		goto cec_error;
	}

	dev_notice(&pdev->dev, "probed\n");

	return 0;

cec_error:
	clk_disable(cec->clk);
	clk_put(cec->clk);
clk_error:
	return ret;
}

static int tegra_cec_remove(struct platform_device *pdev)
{
	struct tegra_cec *cec = platform_get_drvdata(pdev);

	clk_disable(cec->clk);
	clk_put(cec->clk);

	misc_deregister(&cec->misc_dev);
	cancel_work_sync(&cec->work);

	return 0;
}

#ifdef CONFIG_PM
static int tegra_cec_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct tegra_cec *cec = platform_get_drvdata(pdev);

	atomic_set(&cec->init_cancel, 1);
	wmb();

	wake_up_interruptible(&cec->suspend_waitq);

	/* cancel the work queue */
	cancel_work_sync(&cec->work);

	atomic_set(&cec->init_done, 0);
	atomic_set(&cec->init_cancel, 0);

	clk_disable(cec->clk);

	dev_notice(&pdev->dev, "suspended\n");
	return 0;
}

static int tegra_cec_resume(struct platform_device *pdev)
{
	struct tegra_cec *cec = platform_get_drvdata(pdev);

	dev_notice(&pdev->dev, "Resuming\n");

	clk_enable(cec->clk);
	schedule_work(&cec->work);

	return 0;
}
#endif

static struct of_device_id tegra_cec_of_match[] = {
	{ .compatible = "nvidia,tegra114-cec", },
	{ .compatible = "nvidia,tegra124-cec", },
	{ .compatible = "nvidia,tegra210-cec", },
	{ .compatible = "nvidia,tegra186-cec", },
	{},
};

static struct platform_driver tegra_cec_driver = {
	.driver = {
		.name = TEGRA_CEC_NAME,
		.owner = THIS_MODULE,

		.of_match_table = of_match_ptr(tegra_cec_of_match),
	},
	.probe = tegra_cec_probe,
	.remove = tegra_cec_remove,

#ifdef CONFIG_PM
	.suspend = tegra_cec_suspend,
	.resume = tegra_cec_resume,
#endif
};

module_platform_driver(tegra_cec_driver);
