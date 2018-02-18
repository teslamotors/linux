/*
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include <asm/io.h>
#include "i2c-t18x-slv-common.h"
#include "i2c-t18x-slave.h"

static struct i2cslv_cntlr *i2cslv;

#if defined(DEBUG)
void debug_reg(void)
{
	dev_dbg(i2cslv->dev, "I2C_I2C_SL_INT_SOURCE_0 %X\n",
		(unsigned int)tegra_i2cslv_readl(i2cslv, I2C_SLV_INT_SOURCE));
	dev_dbg(i2cslv->dev, "I2C_INTERRUPT_STATUS_REGISTER_0 %X\n",
		(unsigned int)tegra_i2cslv_readl(i2cslv, I2C_INT_STATUS));
	dev_dbg(i2cslv->dev, "I2C_I2C_SL_STATUS_0 %X\n",
		(unsigned int)tegra_i2cslv_readl(i2cslv, I2C_SLV_STATUS));
	dev_dbg(i2cslv->dev, "I2C_INT_SOURCE %X\n",
		(unsigned int)tegra_i2cslv_readl(i2cslv, I2C_INT_SOURCE));
	dev_dbg(i2cslv->dev, "I2C_I2C_SL_CNFG_0 %X\n",
		(unsigned int)tegra_i2cslv_readl(i2cslv, I2C_SLV_CNFG));
	dev_dbg(i2cslv->dev, "I2C_SLV_ADDR1 %X\n",
		(unsigned int)tegra_i2cslv_readl(i2cslv, I2C_SLV_ADDR1));
	dev_dbg(i2cslv->dev, "I2C_INTERRUPT_MASK_REGISTER_0 %X\n",
		(unsigned int)tegra_i2cslv_readl(i2cslv, I2C_INT_MASK));
	dev_dbg(i2cslv->dev, "I2C_I2C_SL_INT_MASK_0 %X\n",
		(unsigned int)tegra_i2cslv_readl(i2cslv, I2C_SLV_INT_MASK));
}
EXPORT_SYMBOL(debug_reg);
#endif

/* i2cslv_load_config - To activate the i2c slave configuration
 *
 * After configuring I2C slave controller, To activate the configuration,
 * We need to set I2C_CONFIG_LOAD_SLV bit in I2C_CONFIG_LOAD reg. Once the
 * configuration is active, HW will clear the bit.
 * This needs to call after doing all required slave registers configuration.
 */
static int i2cslv_load_config(void)
{
	int ret = 0;
	unsigned long i2c_load_config_reg = 0;
	int i2c_retries = I2C_RETRIES;

	i2c_load_config_reg = tegra_i2cslv_readl(i2cslv, I2C_CONFIG_LOAD);
	i2c_load_config_reg |= I2C_CONFIG_LOAD_SLV;

	tegra_i2cslv_writel(i2cslv, i2c_load_config_reg, I2C_CONFIG_LOAD);

	do {
		if (tegra_i2cslv_readl(i2cslv, I2C_CONFIG_LOAD) &
			I2C_CONFIG_LOAD_SLV)
			continue;
		else
			break;
	} while (--i2c_retries);

	if (!i2c_retries) {
		dev_err(i2cslv->dev, "ERR unable to load i2cslv config\n");
		ret = -1;
	}

	return ret;
}

/* handle_slave_rx - To get the data byte fron bus and provide the data to
 *                   client driver
 * After receving master tx event, This function reads the data from bus and
 * pass it to client driver for processing.
 * In case of I2C_SLV_END_TRANS interrupt, It clears the interrupts and
 * notifies the client driver.All this happens in interrupt context only.
 */
static void handle_slave_rx(const unsigned long i2c_int_src,
			    const unsigned long i2c_slv_src)
{
	unsigned char udata;

	if (i2c_slv_src & I2C_SLV_END_TRANS) {
		dev_dbg(i2cslv->dev, "End of Transfer\n");

		/* clear the interrupts to release the SCL line */
		tegra_i2cslv_writel(i2cslv, I2C_SLV_END_TRANS, I2C_SLV_STATUS);
		tegra_i2cslv_writel(i2cslv, I2C_SLV_SL_IRQ, I2C_SLV_STATUS);
		goto end_trnsfr;
	}

	udata = (unsigned char)tegra_i2cslv_readl(i2cslv, I2C_SLV_SLV_RCVD);
	/* Send the received data to client driver */
	tegra_i2cslv_sendbyte_to_client(i2cslv, udata);
	/* clear the interrupt to release the SCL line */
	tegra_i2cslv_writel(i2cslv, I2C_SLV_SL_IRQ, I2C_SLV_STATUS);

	return;

end_trnsfr:
	i2cslv->slave_rx_in_progress = false;
	tegra_i2cslv_sendbyte_end_to_client(i2cslv);
}

/* handle_slave_tx - To get the data byte fron client driver and send it to
 *                   master over bus.
 * After receving master rx event, This function reads the data from client
 * driver and pass it to master driver.
 * In case of I2C_SLV_END_TRANS interrupt, It clears the interrupts and
 * notifies the client driver.All this happens in interrupt context only.
 */

static void handle_slave_tx(const unsigned long i2c_int_src,
			    const unsigned long i2c_slv_src)
{
	unsigned char udata;

	if (i2c_slv_src & I2C_SLV_END_TRANS) {
		dev_dbg(i2cslv->dev, "End of Transfer\n");

		/* clear the interrupt to release the SCL line */
		tegra_i2cslv_writel(i2cslv, I2C_SLV_END_TRANS, I2C_SLV_STATUS);
		tegra_i2cslv_writel(i2cslv, I2C_SLV_SL_IRQ, I2C_SLV_STATUS);

		goto end_trnsfr;
	}

	/* clear the interrupt to release the SCL line */
	tegra_i2cslv_writel(i2cslv, I2C_SLV_SL_IRQ, I2C_SLV_STATUS);
	/* Get the data byte fron client driver */
	udata = tegra_i2cslv_getbyte_from_client(i2cslv);
	tegra_i2cslv_writel(i2cslv, udata, I2C_SLV_SLV_RCVD);

	return;

end_trnsfr:
	i2cslv->slave_tx_in_progress = false;
	tegra_i2cslv_getbyte_end_from_client(i2cslv);
}

/* tegra_i2cslv_isr - Interrupt handler for slave controller
 *
 * This is the core part of slave driver. It maintain the state machine of
 * i2c slave controller.
 */
static irqreturn_t tegra_i2cslv_isr(int irq, void *context_data)
{
	unsigned char udata;
	unsigned long i2c_int_src = tegra_i2cslv_readl(i2cslv, I2C_INT_SOURCE);
	unsigned long i2c_slv_int_src = tegra_i2cslv_readl(i2cslv,
							I2C_SLV_INT_SOURCE);
	unsigned long i2c_slv_sts = tegra_i2cslv_readl(i2cslv, I2C_SLV_STATUS);
	unsigned long reg;

#if defined(DEBUG)
	debug_reg();
#endif
	/*
	 * Packet transfer error is seen at the end of transfer.
	 * It is not required by byte mode implementation and is
	 * most valid in case of fifo mode. As suggested by HW team
	 * we should clear it when it occurs.
	 */
	if (unlikely(i2c_int_src & I2C_INT_PACKET_XFER_ERR)) {
		dev_dbg(i2cslv->dev, " Packet Transfer error\n");
		tegra_i2cslv_writel(i2cslv, I2C_INT_PACKET_XFER_ERR,
				    I2C_INT_STATUS);
		if (!(I2C_SLV_SL_IRQ & i2c_slv_sts))
			return IRQ_HANDLED;
	}

	if (unlikely(i2c_int_src & I2C_INT_SLV_WR2RD_INT)) {

		dev_dbg(i2cslv->dev, "Master WR2RD\n");

		/* Master send repeatedstart condition and changed the xfer
		 * direction from write to read. In response, Slave will change
		 * its direction fron rx to tx, if Rx xfer is in progress.
		 */
		if (unlikely(WARN_ON(!i2cslv->slave_rx_in_progress))) {
			/* Master is misbehaving. */
			dev_err(i2cslv->dev,
				"Got WR2RD while slave rx not in progress\n");
			goto err;
		}

		if (i2cslv->slave_rx_in_progress) {
			i2cslv->slave_rx_in_progress = false;
			tegra_i2cslv_sendbyte_end_to_client(i2cslv);

			udata = tegra_i2cslv_getbyte_from_client(i2cslv);
			i2cslv->slave_tx_in_progress = true;
			tegra_i2cslv_writel(i2cslv, udata, I2C_SLV_SLV_RCVD);
		}

		/* Clearing the bit. If Slave Rx xfer is not in progress
		 * we can ignore this interrupt.
		 */
		tegra_i2cslv_writel(i2cslv, I2C_INT_SLV_WR2RD_INT,
					I2C_INT_STATUS);
		/* FIXME: What if SL_IRQ is set here */
		return IRQ_HANDLED;
	}

	/*
	 * ERR: if tx is in progress and RNW = 0.
	 * ERR: if tx is in progress and we receive a new transfer.
	 * FIXME: Add more cases for wr2wr and rd2rd xfer.
	 */
	if (unlikely(WARN_ON((i2cslv->slave_tx_in_progress
			 && (!(i2c_slv_sts & I2C_SLV_STATUS_RNW)))
		     || (i2cslv->slave_tx_in_progress
			 && (i2c_slv_sts & I2C_SLV_RCVD))))) {
		dev_err(i2cslv->dev, "error in transfer\n");
		goto err;
	}

	/* Check for the SL_IRQ */
	if (unlikely(WARN_ON(!(i2c_slv_int_src & I2C_SLV_SL_IRQ)))) {
		dev_err(i2cslv->dev, "SL_IRQ is not set\n");
		goto err;
	}

	if (i2cslv->slave_tx_in_progress)
		handle_slave_tx(i2c_int_src, i2c_slv_int_src);
	else if (i2cslv->slave_rx_in_progress &&
					!(i2c_slv_int_src & I2C_SLV_RCVD))
		/* In case of master wr2rd xfer, I2C_SLV_RCVD new xfer bit check
		 * is required to identify the xfer direction change.
		 */
		handle_slave_rx(i2c_int_src, i2c_slv_int_src);

	/*
	 * The RCVD bit marks the start of transfer.
	 * The RCVD register contains the received address.
	 * The RNW bit is used to determine the direction of transaction.
	 */
	if (i2c_slv_int_src & I2C_SLV_RCVD) {
		tegra_i2cslv_writel(i2cslv, I2C_SLV_RCVD, I2C_SLV_STATUS);
		tegra_i2cslv_writel(i2cslv, I2C_SLV_SL_IRQ, I2C_SLV_STATUS);

		/* In case of WR2RD, Return from here. Controller will
		 * then generate WR2RD interrupt. Write the data to SLV_RCVD
		 * while handling WR2RD interrupt.
		 */
		if (i2cslv->slave_rx_in_progress)
			return IRQ_HANDLED;

		/* if RNW, master issued read. */
		if (i2c_slv_sts & I2C_SLV_STATUS_RNW) {
			i2cslv->slave_rx_in_progress = false;
			i2cslv->slave_tx_in_progress = true;
			udata = tegra_i2cslv_getbyte_from_client(i2cslv);
			tegra_i2cslv_writel(i2cslv, udata, I2C_SLV_SLV_RCVD);
		} else {
			i2cslv->slave_rx_in_progress = true;
			i2cslv->slave_tx_in_progress = false;
		}
	}

	return IRQ_HANDLED;

err:
	dev_err(i2cslv->dev, "I2C_I2C_SL_INT_SOURCE_0 %X\n",
		(unsigned int)tegra_i2cslv_readl(i2cslv, I2C_SLV_INT_SOURCE));
	dev_err(i2cslv->dev, "I2C_INTERRUPT_STATUS_REGISTER_0 %X\n",
		(unsigned int)tegra_i2cslv_readl(i2cslv, I2C_INT_STATUS));
	dev_err(i2cslv->dev, "I2C_I2C_SL_STATUS_0 %X\n",
		(unsigned int)tegra_i2cslv_readl(i2cslv, I2C_SLV_STATUS));
	dev_err(i2cslv->dev, "I2C_INT_SOURCE %X\n",
		(unsigned int)tegra_i2cslv_readl(i2cslv, I2C_INT_SOURCE));
	dev_err(i2cslv->dev, "I2C_I2C_SL_CNFG_0 %X\n",
		(unsigned int)tegra_i2cslv_readl(i2cslv, I2C_SLV_CNFG));
	dev_err(i2cslv->dev, "I2C_SLV_ADDR1 %X\n",
		(unsigned int)tegra_i2cslv_readl(i2cslv, I2C_SLV_ADDR1));
	dev_err(i2cslv->dev, "I2C_INTERRUPT_MASK_REGISTER_0 %X\n",
		(unsigned int)tegra_i2cslv_readl(i2cslv, I2C_INT_MASK));
	dev_err(i2cslv->dev, "I2C_I2C_SL_INT_MASK_0 %X\n",
		(unsigned int)tegra_i2cslv_readl(i2cslv, I2C_SLV_INT_MASK));

	/* Reset the internal state of controller and fifo. This will not
	 * reset the registers configuration
	 */
	reg = tegra_i2cslv_readl(i2cslv, I2C_SLV_RESET_CNTRL);
	reg |= I2C_SLV_SOFT_RESET;
	tegra_i2cslv_writel(i2cslv, reg, I2C_SLV_RESET_CNTRL);
	udelay(2);
	/* Clear it for normal operation */
	reg &= ~(I2C_SLV_SOFT_RESET);
	tegra_i2cslv_writel(i2cslv, reg, I2C_SLV_RESET_CNTRL);

	return IRQ_HANDLED;
}

/* i2cslv_init_config - To set the default configuration.
 */
static void i2cslv_init_config(void)
{
	unsigned long reg = 0;

	reg = tegra_i2cslv_readl(i2cslv, I2C_SLV_RESET_CNTRL);
	reg &= ~(I2C_SLV_SOFT_RESET);
	tegra_i2cslv_writel(i2cslv, reg, I2C_SLV_RESET_CNTRL);

	tegra_i2cslv_writel(i2cslv, I2C_REG_RESET, I2C_FIFO_CONTROL);
	tegra_i2cslv_writel(i2cslv, I2C_REG_RESET, I2C_INTERRUPT_SET_REGISTER);
	tegra_i2cslv_writel(i2cslv, I2C_REG_RESET, I2C_CLKEN_OVERRIDE);
	tegra_i2cslv_writel(i2cslv, I2C_REG_RESET, I2C_TLOW_SEXT);
	tegra_i2cslv_writel(i2cslv, I2C_REG_RESET, I2C_SL_INT_SET);
	tegra_i2cslv_writel(i2cslv, I2C_SL_DELAY_COUNT_RESET,
						I2C_SL_DELAY_COUNT);
}

static int i2cslv_init_cntlr(const unsigned long i2cslv_addr, bool is_seven_bit)
{
	int ret = 0;
	unsigned long reg = 0;

	i2cslv->div_clk = devm_clk_get(i2cslv->dev, "div-clk");
	if (IS_ERR(i2cslv->div_clk)) {
		dev_err(i2cslv->dev, "missing controller clock");
		return PTR_ERR(i2cslv->div_clk);
	}

	ret = clk_prepare_enable(i2cslv->div_clk);
	if (ret < 0) {
		dev_err(i2cslv->dev, "Enabling div clk failed, err %d\n", ret);
		return ret;
	}

	i2cslv->rstc = devm_reset_control_get(i2cslv->dev, "i2c");
	if (IS_ERR(i2cslv->rstc)) {
		ret = PTR_ERR(i2cslv->rstc);
		dev_err(i2cslv->dev, "Reset control is not found: %d\n", ret);
		return ret;
	}

	/* set the default values */
	i2cslv_init_config();

	/* set the slave address and address mode */
	if (is_seven_bit) {
		tegra_i2cslv_writel(i2cslv, i2cslv_addr & I2C_7BIT_ADDR_MASK,
				    I2C_SLV_ADDR1);

		/* program ADDR2 register for 7 bit. */
		reg = tegra_i2cslv_readl(i2cslv, I2C_SLV_ADDR2);
		reg &= ~(I2C_SLV_ADDR2_MASK);
		tegra_i2cslv_writel(i2cslv, reg, I2C_SLV_ADDR2);
	} else {
		dev_err(i2cslv->dev, "10 bit address not supported\n");
		return -EINVAL;
	}

	/* Unmask I2C_INT_PACKET_XFER_ERR Just to clear it */
	tegra_i2cslv_writel(i2cslv, (I2C_INT_SLV_WR2RD_INT |
					I2C_INT_PACKET_XFER_ERR), I2C_INT_MASK);

	tegra_i2cslv_writel(i2cslv, (I2C_SLV_END_TRANS | I2C_SLV_SL_IRQ |
					I2C_SLV_RCVD), I2C_SLV_INT_MASK);

	reg = tegra_i2cslv_readl(i2cslv, I2C_SLV_CNFG);
	reg |= (I2C_SLV_CNFG_NEW_SL | I2C_SLV_CNFG_ENABLE_SL);
	tegra_i2cslv_writel(i2cslv, reg, I2C_SLV_CNFG);

	ret = i2cslv_load_config();

	i2cslv->slave_rx_in_progress = false;
	i2cslv->slave_tx_in_progress = false;

	return ret;
}

int i2cslv_register_client(struct i2cslv_client_ops *i2c_ops,
			   const unsigned long i2cslv_addr, bool is_seven_bit)
{
	int ret = 0;

	if (i2cslv->is_cntlr_intlzd) {
		dev_info(i2cslv->dev, "controller already initialized\n");
		return -EAGAIN;
	}

	if (!i2c_ops) {
		dev_err(i2cslv->dev, "i2c ops == NULL\n");
		return -EINVAL;
	}

	if (!(i2c_ops->slv_sendbyte_to_client) ||
			!(i2c_ops->slv_getbyte_from_client) ||
			!(i2c_ops->slv_sendbyte_end_to_client) ||
			!(i2c_ops->slv_getbyte_end_from_client)) {
		dev_err(i2cslv->dev, "i2c ops incomplete\n");
		return -EINVAL;
	}

	i2cslv->i2c_clnt_ops = i2c_ops;

	ret = i2cslv_init_cntlr(i2cslv_addr, is_seven_bit);
	if (ret < 0) {
		dev_err(i2cslv->dev, "i2c ops == NULL\n");
		return -EAGAIN;
	}

	i2cslv->is_cntlr_intlzd = true;

	return ret;
}
EXPORT_SYMBOL(i2cslv_register_client);

void i2cslv_unregister_client(void)
{
	tegra_i2cslv_writel(i2cslv, 0, I2C_INT_MASK);
	tegra_i2cslv_writel(i2cslv, 0, I2C_SLV_INT_MASK);

	i2cslv->is_cntlr_intlzd = false;

	i2cslv->i2c_clnt_ops = NULL;
}
EXPORT_SYMBOL(i2cslv_unregister_client);

static int tegra_i2cslv_probe(struct platform_device *pdev)
{
	int ret = 0;

	i2cslv = devm_kzalloc(&pdev->dev, sizeof(*i2cslv), GFP_KERNEL);
	if (!i2cslv)
		return -ENOMEM;

	i2cslv->dev = &pdev->dev;

	i2cslv->iomem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!i2cslv->iomem) {
		dev_err(&pdev->dev, "No IO memory resource\n");
		ret = -ENODEV;
		goto err_enmm;
	}

	i2cslv->base = devm_ioremap_resource(i2cslv->dev, i2cslv->iomem);
	if (!i2cslv->base) {
		dev_err(&pdev->dev,
			"Cannot request memregion/iomap dma address\n");
		ret = -EADDRNOTAVAIL;
		goto err_adrnvld;
	}

	i2cslv->irq = platform_get_irq(pdev, 0);

	ret = request_threaded_irq(i2cslv->irq, NULL,
				   tegra_i2cslv_isr, IRQF_ONESHOT,
				   dev_name(&pdev->dev), i2cslv);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register ISR for IRQ %d\n",
			i2cslv->irq);
		goto err_free_i2cslv;
	}

	i2cslv->is_cntlr_intlzd = false;

	return ret;

err_free_i2cslv:

err_adrnvld:
	release_mem_region(i2cslv->iomem->start, resource_size(i2cslv->iomem));
err_enmm:
	put_device(&pdev->dev);
	return ret;
}

static int tegra_i2cslv_remove(struct platform_device *pdev)
{
	struct i2cslv_cntlr *i2cslv = platform_get_drvdata(pdev);

	clk_disable_unprepare(i2cslv->div_clk);
	reset_control_put(i2cslv->rstc);
	return 0;
}

static struct of_device_id tegra_i2cslv_of_match[] = {
	{.compatible = "nvidia,tegra-i2cslv-t186",},
	{}
};

MODULE_DEVICE_TABLE(of, tegra_i2cslv_of_match);

static struct platform_driver tegra_i2cslv_driver = {
	.probe = tegra_i2cslv_probe,
	.remove = tegra_i2cslv_remove,
	.driver = {
		   .name = "tegra-i2cslv-t186",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(tegra_i2cslv_of_match),
		   },
};

static int __init tegra_i2cslv_init_driver(void)
{
	return platform_driver_register(&tegra_i2cslv_driver);
}

static void __exit tegra_i2cslv_exit_driver(void)
{
	platform_driver_unregister(&tegra_i2cslv_driver);
}

MODULE_AUTHOR("Ankit Gupta <guptaa@nvidia.com>");
MODULE_DESCRIPTION("NVIDIA Tegra186 i2c slave driver");
MODULE_LICENSE("GPL v2");
module_init(tegra_i2cslv_init_driver);
module_exit(tegra_i2cslv_exit_driver);
