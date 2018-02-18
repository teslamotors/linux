/*
 * SPI proxy driver for NVIDIA's Tegra186 AON SPI Controller.
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi-tegra.h>
#include <linux/tegra-aon.h>
#include <linux/mailbox_client.h>

#include "aon-spi-messages.h"

#define SPI_DMA_TIMEOUT		(msecs_to_jiffies(5000))
#define MAX_HOLD_CYCLES		16
#define SPI_DEFAULT_SPEED	25000000

#define MAX_CHIP_SELECT		4
#define SPI_DEF_CHIPSELECT	0

#define SPI_DEFAULT_RX_TAP_DELAY 10

/* block period in ms */
#define TX_BLOCK_PERIOD 200

struct tegra_aon_spi_chipdata {
	bool set_rx_tap_delay;
};

struct tegra_spi_data {
	struct device				*dev;
	struct spi_master			*master;
	u32					spi_max_frequency;
	u8					def_chip_select;
	const struct tegra_aon_spi_chipdata	*chip_data;
	struct completion			*xfer_completion;
	struct mbox_client			cl;
	struct mbox_chan			*mbox;
	struct tegra_spi_device_controller_data cdata[MAX_CHIP_SELECT];
	struct aon_spi_response			*spi_resp;
	struct aon_spi_request			*spi_req;
};


static struct tegra_spi_device_controller_data
	*tegra_spi_get_cdata_dt(struct spi_device *spi,
			struct tegra_spi_data *tspi)
{
	struct tegra_spi_device_controller_data *cdata;
	struct device_node *slave_np, *data_np;

	slave_np = spi->dev.of_node;
	if (!slave_np) {
		dev_dbg(&spi->dev, "device node not found\n");
		return NULL;
	}

	data_np = of_get_child_by_name(slave_np, "controller-data");
	if (!data_np) {
		dev_dbg(&spi->dev, "child node 'controller-data' not found\n");
		return NULL;
	}

	cdata = &tspi->cdata[spi->chip_select];
	memset(cdata, 0, sizeof(*cdata));

	of_property_read_u32(data_np, "nvidia,cs-setup-clk-count",
			     &cdata->cs_setup_clk_count);
	of_property_read_u32(data_np, "nvidia,cs-hold-clk-count",
			     &cdata->cs_hold_clk_count);
	of_property_read_u32(data_np, "nvidia,cs-inactive-cycles",
			     &cdata->cs_inactive_cycles);
	of_node_put(data_np);

	return cdata;
}

static void tegra_aon_spi_mbox_rcv_msg(struct mbox_client *cl, void *mssg)
{
	struct tegra_aon_mbox_msg *msg = mssg;
	struct spi_master *master = dev_get_drvdata(cl->dev);
	struct tegra_spi_data *tspi = spi_master_get_devdata(master);

	memcpy(tspi->spi_resp, msg->data, sizeof(*tspi->spi_resp));
	complete(tspi->xfer_completion);
}

static int ivc_aon_spi_send_req(struct tegra_spi_data *tspi, int len)
{
	int status;
	struct tegra_aon_mbox_msg msg;

	msg.length = len;
	msg.data = (void *)tspi->spi_req;
	status = mbox_send_message(tspi->mbox, (void *)&msg);
	if (status < 0) {
		dev_err(tspi->dev, "mbox_send_message() failed with %d\n",
			status);
	} else {
		status = wait_for_completion_timeout(tspi->xfer_completion,
						     SPI_DMA_TIMEOUT);
		if (status == 0) {
			dev_err(tspi->dev, "Timeout waiting for ipc response\n");
			return -ETIMEDOUT;
		} else {
			status = tspi->spi_resp->status;
		}
	}

	return status;
}

static int do_ivc_aon_spi_init(struct tegra_spi_data *tspi)
{
	int len, status;

	tspi->spi_req->req_type = AON_SPI_REQUEST_TYPE_INIT;
	len = sizeof(tspi->spi_req->req_type);
	status = ivc_aon_spi_send_req(tspi, len);
	if (status)
		return -EIO;

	return 0;
}

static int do_ivc_aon_spi_setup(struct spi_device *spi,
				struct tegra_spi_data *tspi)
{
	int status;
	struct tegra_spi_device_controller_data *cdata = spi->controller_data;
	struct aon_spi_request *req = tspi->spi_req;
	int len;

	dev_dbg(tspi->dev, "%s -> invocation\n", __func__);
	req->req_type = AON_SPI_REQUEST_TYPE_SETUP;
	if (cdata) {
		req->data.setup.cs_setup_clk_count = cdata->cs_setup_clk_count;
		req->data.setup.cs_hold_clk_count = cdata->cs_hold_clk_count;
		req->data.setup.cs_inactive_cycles = cdata->cs_inactive_cycles;
	}
	req->data.setup.set_rx_tap_delay = tspi->chip_data->set_rx_tap_delay;
	req->data.setup.chip_select = spi->chip_select;
	spi->max_speed_hz = spi->max_speed_hz ? : tspi->spi_max_frequency;
	req->data.setup.spi_max_clk_rate = spi->max_speed_hz;
	/* TODO: No-Dma support to be added for core driver */
	req->data.setup.spi_no_dma = false;
	len = (sizeof(req->req_type) + sizeof(struct aon_spi_setup_request));
	status = ivc_aon_spi_send_req(tspi, len);
	if (status)
		return -EIO;

	return 0;
}

#if defined(ENABLE_DUMPS)
static void tegra_spi_dump_buf(u8 *buffer, int len, const char *name)
{
	int j;

	pr_dbg("%s:::", name);
	for (j = 0; j < len; j++)
		pr_dbg("%x ", *(buffer + j));
	pr_dbg("\n");
}
#else
static void tegra_spi_dump_buf(u8 *buffer, int len, const char *name)
{
}
#endif

static int do_aon_ivc_spi_xfer(struct spi_device *spi,
					struct spi_transfer *spi_xfer,
					struct tegra_spi_data *tspi,
					enum aon_spi_xfer_flag xfer_type)
{
	int length = 0, status = 0;
	struct aon_spi_request *req = tspi->spi_req;
	struct aon_spi_response *resp = tspi->spi_resp;

	req->req_type = AON_SPI_REQUEST_TYPE_XFER;

	req->data.xfer.xfers.flags = xfer_type;
	if (spi_xfer->tx_buf)
		req->data.xfer.xfers.flags |= AON_SPI_XFER_FLAG_WRITE;
	if (spi_xfer->rx_buf)
		req->data.xfer.xfers.flags |= AON_SPI_XFER_FLAG_READ;

	req->data.xfer.xfers.length = spi_xfer->len;
	req->data.xfer.xfers.rx_buf_offset = 0;
	req->data.xfer.xfers.tx_buf_offset = 0;
	req->data.xfer.xfers.chip_select = spi->chip_select;
	req->data.xfer.xfers.tx_nbits = spi_xfer->tx_nbits;
	req->data.xfer.xfers.rx_nbits = spi_xfer->rx_nbits;
	req->data.xfer.xfers.bits_per_word = spi_xfer->bits_per_word;
	req->data.xfer.xfers.mode = spi->mode;
	req->data.xfer.xfers.spi_clk_rate = spi_xfer->speed_hz;
	/* per-word bits-on-wire */

	if (spi_xfer->tx_buf) {
		memcpy(req->data.xfer.data, spi_xfer->tx_buf, spi_xfer->len);
		tegra_spi_dump_buf(req->data.xfer.data, spi_xfer->len,
				   "reqdata");
	}
	/* alignmemt + data */
	length = (TEGRA_IVC_ALIGN + spi_xfer->len + 1);
	if (spi_xfer->len > AON_SPI_MAX_DATA_SIZE) {
		dev_err(tspi->dev, "length %d greater than max length\n",
			spi_xfer->len);
		return -E2BIG;
	}
	status = ivc_aon_spi_send_req(tspi, length);
	if (status) {
		dev_err(tspi->dev, "Error in transfer\n");
		return -EIO;
	}
	if (spi_xfer->rx_buf) {
		memcpy(spi_xfer->rx_buf, resp->data.xfer.data, spi_xfer->len);
		tegra_spi_dump_buf(spi_xfer->rx_buf, spi_xfer->len, "rxdata");
	}

	return 0;
}

static int tegra_spi_transfer_one_message(struct spi_master *master,
			struct spi_message *msg)
{
	struct tegra_spi_data *tspi = spi_master_get_devdata(master);
	struct spi_transfer *xfer;
	int ret = -1;
	int count = 0;
	u16 flags = 0;

	msg->status = 0;
	msg->actual_length = 0;

	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		flags |= AON_SPI_XFER_HANDLE_CACHE;
		if (count == 0)
			flags |= AON_SPI_XFER_FIRST_MSG;
		if (list_is_last(&xfer->transfer_list, &msg->transfers))
			flags |= AON_SPI_XFER_LAST_MSG;

		ret = do_aon_ivc_spi_xfer(msg->spi, xfer, tspi, flags);
		if (!ret)
			msg->actual_length += xfer->len;
		count++;
	}

	msg->status = ret;
	spi_finalize_current_message(master);

	return ret;
}

static int tegra_aon_spi_setup(struct spi_device *spi)
{
	int ret;
	struct tegra_spi_data *tspi = spi_master_get_devdata(spi->master);
	struct tegra_spi_device_controller_data *cdata = spi->controller_data;

	dev_dbg(tspi->dev, "setup %d bpw, %scpol, %scpha, %dHz\n",
		spi->bits_per_word,
		spi->mode & SPI_CPOL ? "" : "~",
		spi->mode & SPI_CPHA ? "" : "~",
		spi->max_speed_hz);

	if (!cdata) {
		cdata = tegra_spi_get_cdata_dt(spi, tspi);
		if (!cdata)
			dev_err(&spi->dev, "Controller data not found\n");
		else
			spi->controller_data = cdata;
	}

	/* Set speed to the spi max fequency if spi device has not set */
	spi->max_speed_hz = spi->max_speed_hz ? : tspi->spi_max_frequency;

	ret = do_ivc_aon_spi_setup(spi, tspi);
	if (ret) {
		dev_err(&spi->dev, "SPI aon client setup failed %d\n", ret);
		ret = -EIO;
	}

	return ret;
}

static const struct tegra_aon_spi_chipdata tegra186_aon_spi_chipdata = {
	.set_rx_tap_delay = false,
};

static const struct of_device_id tegra_aon_spi_of_match[] = {
	{
		.compatible = "nvidia,tegra186-aon-spi",
		.data       = &tegra186_aon_spi_chipdata,
	},
	{}
};
MODULE_DEVICE_TABLE(of, tegra_aon_spi_of_match);

static int tegra_aon_spi_probe(struct platform_device *pdev)
{
	struct spi_master *master;
	struct tegra_spi_data *tspi;
	struct device_node *np = pdev->dev.of_node;
	const unsigned int *prop;
	const struct tegra_aon_spi_chipdata *chip_data = NULL;
	int ret = 0;
	int bus_num;

	chip_data = of_device_get_match_data(&pdev->dev);
	if (!chip_data) {
		dev_err(&pdev->dev, "No platform/chip data, exiting\n");
		return -ENODEV;
	}
	bus_num = of_alias_get_id(pdev->dev.of_node, "spi");
	if (bus_num < 0) {
		dev_warn(&pdev->dev,
			 "Dynamic bus number will be registerd\n");
		bus_num = -1;
	}

	master = spi_alloc_master(&pdev->dev, sizeof(*tspi));
	if (!master)
		return -ENOMEM;

	/* the spi->mode bits understood by this driver: */
	master->mode_bits = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH | SPI_LSB_FIRST |
		SPI_TX_DUAL | SPI_RX_DUAL;
	/* supported bpw 4-32 */
	master->bits_per_word_mask = (u32) ~(BIT(0)|BIT(1)|BIT(2));
	master->setup = tegra_aon_spi_setup;
	master->transfer_one_message = tegra_spi_transfer_one_message;
	master->num_chipselect = MAX_CHIP_SELECT;
	master->bus_num = bus_num;

	dev_set_drvdata(&pdev->dev, master);
	tspi = spi_master_get_devdata(master);
	tspi->master = master;
	tspi->dev = &pdev->dev;
	tspi->cl.dev = &pdev->dev;
	tspi->cl.tx_block = true;
	tspi->cl.tx_tout = TX_BLOCK_PERIOD;
	tspi->cl.knows_txdone = false;
	tspi->cl.rx_callback = tegra_aon_spi_mbox_rcv_msg;
	tspi->mbox = mbox_request_channel(&tspi->cl, 0);
	if (IS_ERR(tspi->mbox)) {
		dev_warn(&pdev->dev,
			 "can't get mailbox channel (%d)\n",
			 (int)PTR_ERR(tspi->mbox));
		ret = PTR_ERR(tspi->mbox);
		goto exit_free_master;
	}
	dev_dbg(&pdev->dev, "tspi->mbox = %p\n", tspi->mbox);
	tspi->def_chip_select = SPI_DEF_CHIPSELECT;
	tspi->dev = &pdev->dev;
	tspi->chip_data = chip_data;
	tspi->xfer_completion = devm_kzalloc(&pdev->dev,
				sizeof(struct completion), GFP_KERNEL);
	if (!tspi->xfer_completion) {
		ret = -ENOMEM;
		goto exit_free_mbox;
	}
	init_completion(tspi->xfer_completion);

	prop = of_get_property(np, "spi-max-frequency", NULL);
	if (prop)
		tspi->spi_max_frequency = be32_to_cpup(prop);
	if (!tspi->spi_max_frequency) {
		tspi->spi_max_frequency = SPI_DEFAULT_SPEED;
		dev_dbg(&pdev->dev, "Max frequency set to %d\n",
			SPI_DEFAULT_SPEED);
	}

	tspi->spi_req = devm_kzalloc(tspi->dev, sizeof(*tspi->spi_req),
							GFP_KERNEL);
	if (!tspi->spi_req) {
		ret = -ENOMEM;
		goto exit_free_mbox;
	}
	tspi->spi_resp = devm_kzalloc(tspi->dev, sizeof(*tspi->spi_resp),
							GFP_KERNEL);
	if (!tspi->spi_resp) {
		ret = -ENOMEM;
		goto exit_free_mbox;
	}

	ret = do_ivc_aon_spi_init(tspi);
	if (ret) {
		dev_err(&pdev->dev, "SPI aon init failed %d\n", ret);
		ret = -EIO;
		goto exit_free_mbox;
	}
	master->dev.of_node = pdev->dev.of_node;
	ret = devm_spi_register_master(&pdev->dev, master);
	if (ret < 0) {
		dev_err(&pdev->dev, "can not register to master err %d\n", ret);
		goto exit_free_mbox;
	}
	dev_dbg(&pdev->dev, "tegra_aon_spi_driver_probe() OK\n");

	return ret;

exit_free_mbox:
	mbox_free_channel(tspi->mbox);
exit_free_master:
	spi_master_put(master);
	dev_err(&pdev->dev, "tegra_aon_spi_driver_probe() FAILED\n");

	return ret;
}

static int tegra_aon_spi_remove(struct platform_device *pdev)
{
	struct spi_master *master = dev_get_drvdata(&pdev->dev);
	struct tegra_spi_data *tspi = spi_master_get_devdata(master);

	mbox_free_channel(tspi->mbox);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tegra_aon_spi_suspend(struct device *dev)
{
	struct spi_master *master = dev_get_drvdata(dev);
	struct tegra_spi_data *tspi = spi_master_get_devdata(master);
	int len, ret, status;

	ret = spi_master_suspend(master);
	if (ret)
		return ret;

	tspi->spi_req->req_type = AON_SPI_REQUEST_TYPE_SUSPEND;
	len = sizeof(tspi->spi_req->req_type);
	status = ivc_aon_spi_send_req(tspi, len);
	if (status)
		return -EBUSY;

	return 0;
}

static int tegra_aon_spi_resume(struct device *dev)
{
	struct spi_master *master = dev_get_drvdata(dev);
	struct tegra_spi_data *tspi = spi_master_get_devdata(master);
	int len, ret;

	tspi->spi_req->req_type = AON_SPI_REQUEST_TYPE_RESUME;
	len = sizeof(tspi->spi_req->req_type);
	ret = ivc_aon_spi_send_req(tspi, len);
	if (ret)
		return -EIO;

	return spi_master_resume(master);
}
#endif

static const struct dev_pm_ops tegra_aon_spi_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(tegra_aon_spi_suspend, tegra_aon_spi_resume)
};

static struct platform_driver tegra_aon_spi_driver = {
	.driver = {
		.name		= "tegra-aon-spi",
		.owner		= THIS_MODULE,
		.of_match_table	= of_match_ptr(tegra_aon_spi_of_match),
		.pm = &tegra_aon_spi_pm_ops,
	},
	.remove =	tegra_aon_spi_remove,
	.probe =	tegra_aon_spi_probe,
};
module_platform_driver(tegra_aon_spi_driver);

MODULE_DESCRIPTION("NVIDIA Tegra186 AON SPI Controller Driver");
MODULE_LICENSE("GPL v2");
