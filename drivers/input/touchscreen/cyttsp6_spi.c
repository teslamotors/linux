/*
 * cyttsp6_spi.c
 * Cypress TrueTouch(TM) Standard Product V4 SPI Module.
 * For use with Cypress touchscreen controllers.
 * Supported parts include:
 * CY8CTMA46X
 * CY8CTMA1036/768
 * CYTMA445
 *
 * Copyright (C) 2012-2015 Cypress Semiconductor
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#include "cyttsp6_regs.h"
#include <linux/spi/spi.h>
#include <linux/version.h>

#define CY_SPI_WR_OP		0x00 /* r/~w */
#define CY_SPI_RD_OP		0x01
#define CY_SPI_A8_BIT		0x02
#define CY_SPI_WR_HEADER_BYTES	2
#define CY_SPI_RD_HEADER_BYTES	1
#define CY_SPI_SYNC_BYTE	0
#define CY_SPI_SYNC_ACK		0x62
#define CY_SPI_MAX_HEADER_BYTES	\
		max(CY_SPI_WR_HEADER_BYTES, CY_SPI_RD_HEADER_BYTES)
#define CY_SPI_CMD_BYTES	CY_SPI_MAX_HEADER_BYTES
#define CY_SPI_DATA_SIZE	(2 * 256)
#define CY_SPI_DATA_BUF_SIZE	(CY_SPI_CMD_BYTES + CY_SPI_DATA_SIZE)
#define CY_SPI_BITS_PER_WORD	8
#define CY_SPI_NUM_RETRY	3

#ifdef VERBOSE_DEBUG
static char b[CY_SPI_DATA_SIZE * 3 + 1];

static void _cyttsp6_spi_pr_buf(struct device *dev, u8 *buf,
			int size, char const *info)
{
	unsigned i, k;

	for (i = k = 0; i < size; i++, k += 3)
		snprintf(b + k, sizeof(b) - k, "%02x ", buf[i]);
	dev_dbg(dev, "%s: %s\n", info, b);
}
#else
#define _cyttsp6_spi_pr_buf(a, b, c, d) do { } while (0)
#endif

static int cyttsp6_spi_xfer(u8 op, struct device *dev, u16 reg, u8 *buf,
	int length)
{
	struct spi_device *spi = to_spi_device(dev);
	struct spi_message msg;
	struct spi_transfer xfer[2];
	u8 wr_hdr_buf[CY_SPI_MAX_HEADER_BYTES];
	u8 rd_hdr_buf[CY_SPI_MAX_HEADER_BYTES];
	int rc;

	memset(wr_hdr_buf, 0, CY_SPI_MAX_HEADER_BYTES);
	memset(rd_hdr_buf, 0, CY_SPI_MAX_HEADER_BYTES);
	memset(xfer, 0, sizeof(xfer));

	spi_message_init(&msg);

	/* Header buffer */
	xfer[0].tx_buf = wr_hdr_buf;
	xfer[0].rx_buf = rd_hdr_buf;

	switch (op) {
	case CY_SPI_WR_OP:
		if (length + CY_SPI_WR_HEADER_BYTES > CY_SPI_DATA_SIZE) {
			dev_vdbg(dev,
				"%s: length+%d=%d is greater than SPI max=%d\n",
				__func__, CY_SPI_WR_HEADER_BYTES,
				length + CY_SPI_WR_HEADER_BYTES,
				CY_SPI_DATA_SIZE);
			rc = -EINVAL;
			goto cyttsp6_spi_xfer_exit;
		}

		/* Header byte 0 */
		if (reg > 255)
			wr_hdr_buf[0] = CY_SPI_WR_OP + CY_SPI_A8_BIT;
		else
			wr_hdr_buf[0] = CY_SPI_WR_OP;

		/* Header byte 1 */
		wr_hdr_buf[1] = reg % 256;

		xfer[0].len = CY_SPI_WR_HEADER_BYTES;

		spi_message_add_tail(&xfer[0], &msg);

		/* Data buffer */
		if (buf) {
			xfer[1].tx_buf = buf;
			xfer[1].len = length;

			spi_message_add_tail(&xfer[1], &msg);
		}
		break;

	case CY_SPI_RD_OP:
		if (!buf) {
			dev_err(dev, "%s: No read buffer\n", __func__);
			rc = -EINVAL;
			goto cyttsp6_spi_xfer_exit;
		}

		if ((length + CY_SPI_RD_HEADER_BYTES) > CY_SPI_DATA_SIZE) {
			dev_vdbg(dev,
				"%s: length+%d=%d is greater than SPI max=%d\n",
				__func__, CY_SPI_RD_HEADER_BYTES,
				length + CY_SPI_RD_HEADER_BYTES,
				CY_SPI_DATA_SIZE);
			rc = -EINVAL;
			goto cyttsp6_spi_xfer_exit;
		}

		/* Header byte 0 */
		wr_hdr_buf[0] = CY_SPI_RD_OP;

		xfer[0].len = CY_SPI_RD_HEADER_BYTES;

		spi_message_add_tail(&xfer[0], &msg);

		/* Data buffer */
		xfer[1].rx_buf = buf;
		xfer[1].len = length;

		spi_message_add_tail(&xfer[1], &msg);
		break;

	default:
		dev_dbg(dev, "%s: bad op code=%d\n", __func__, op);
		rc = -EINVAL;
		goto cyttsp6_spi_xfer_exit;
	}

	rc = spi_sync(spi, &msg);
	if (rc < 0) {
		dev_vdbg(dev, "%s: spi_sync() error %d, len=%d, op=%d\n",
			__func__, rc, xfer[0].len, op);
		/*
		 * do not return here since probably a bad ACK sequence
		 * let the following ACK check handle any errors and
		 * allow silent retries
		 */
	}

	if (rd_hdr_buf[CY_SPI_SYNC_BYTE] != CY_SPI_SYNC_ACK) {
		/* signal ACK error so silent retry */
		rc = 1;

		switch (op) {
		case CY_SPI_WR_OP:
			_cyttsp6_spi_pr_buf(dev, wr_hdr_buf,
				CY_SPI_WR_HEADER_BYTES,
				"spi_wr_buf HEAD");
			if (buf)
				_cyttsp6_spi_pr_buf(dev, buf,
					length, "spi_wr_buf DATA");
			break;

		case CY_SPI_RD_OP:
			_cyttsp6_spi_pr_buf(dev, rd_hdr_buf,
				CY_SPI_RD_HEADER_BYTES, "spi_rd_buf HEAD");
			_cyttsp6_spi_pr_buf(dev, buf, length,
				"spi_rd_buf DATA");
			break;

		default:
			/*
			 * should not get here due to error check
			 * in first switch
			 */
			break;
		}
	}

cyttsp6_spi_xfer_exit:
	return rc;
}

static s32 cyttsp6_spi_read_block_data(struct device *dev, u16 addr, int length,
	void *data, int max_xfer)
{
	int retry = 0;
	int trans_len;
	int rc = -EINVAL;

	while (length > 0) {
		trans_len = min(length, max_xfer);

		/* Write address */
		rc = cyttsp6_spi_xfer(CY_SPI_WR_OP, dev, addr, NULL, 0);
		if (rc < 0) {
			dev_err(dev, "%s: Fail write address r=%d\n",
				__func__, rc);
			return rc;
		}

		/* Read data */
		rc = cyttsp6_spi_xfer(CY_SPI_RD_OP, dev, addr, data, trans_len);
		if (rc < 0) {
			dev_err(dev, "%s: Fail read r=%d\n", __func__, rc);
			goto exit;
		} else if (rc > 0) {
			/* Perform retry or fail */
			if (retry++ < CY_SPI_NUM_RETRY) {
				dev_dbg(dev, "%s: ACK error, retry %d\n",
					__func__, retry);
				continue;
			} else {
				dev_err(dev, "%s: ACK error\n", __func__);
				rc = -EIO;
				goto exit;
			}
		}

		length -= trans_len;
		data += trans_len;
		addr += trans_len;
	}

exit:
	return rc;
}

static s32 cyttsp6_spi_write_block_data(struct device *dev, u16 addr,
	int length, const void *data, int max_xfer)
{
	int retry = 0;
	int trans_len;
	int rc = -EINVAL;

	while (length > 0) {
		trans_len = min(length, max_xfer);

		rc = cyttsp6_spi_xfer(CY_SPI_WR_OP, dev, addr, (void *)data,
				trans_len);
		if (rc < 0) {
			dev_err(dev, "%s: Fail write r=%d\n", __func__, rc);
			goto exit;
		} else if (rc > 0) {
			/* Perform retry or fail */
			if (retry++ < CY_SPI_NUM_RETRY) {
				dev_dbg(dev, "%s: ACK error, retry %d\n",
					__func__, retry);
				continue;
			} else {
				dev_err(dev, "%s: ACK error\n", __func__);
				rc = -EIO;
				goto exit;
			}
		}

		length -= trans_len;
		data += trans_len;
		addr += trans_len;
	}

exit:
	return rc;
}

static int cyttsp6_spi_write(struct device *dev, u16 addr, u8 *wr_buf,
	const void *buf, int size, int max_xfer)
{
	int rc;

	pm_runtime_get_noresume(dev);
	rc = cyttsp6_spi_write_block_data(dev, addr, size, buf, max_xfer);
	pm_runtime_put_noidle(dev);

	return rc;
}

static int cyttsp6_spi_read(struct device *dev, u16 addr, void *buf, int size,
	int max_xfer)
{
	int rc;

	pm_runtime_get_noresume(dev);
	rc = cyttsp6_spi_read_block_data(dev, addr, size, buf, max_xfer);
	pm_runtime_put_noidle(dev);

	return rc;
}

static struct cyttsp6_bus_ops cyttsp6_spi_bus_ops = {
	.write = cyttsp6_spi_write,
	.read = cyttsp6_spi_read,
};

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_DEVICETREE_SUPPORT
static struct of_device_id cyttsp6_spi_of_match[] = {
	{ .compatible = "cy,cyttsp6_spi_adapter", }, { }
};
MODULE_DEVICE_TABLE(of, cyttsp6_spi_of_match);
#endif

static int cyttsp6_spi_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_DEVICETREE_SUPPORT
	const struct of_device_id *match;
#endif
	int rc;

	/* Set up SPI*/
	spi->bits_per_word = CY_SPI_BITS_PER_WORD;
	spi->mode = SPI_MODE_0;
	rc = spi_setup(spi);
	if (rc < 0) {
		dev_err(dev, "%s: SPI setup error %d\n", __func__, rc);
		return rc;
	}

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_DEVICETREE_SUPPORT
	match = of_match_device(of_match_ptr(cyttsp6_spi_of_match), dev);
	if (match) {
		rc = cyttsp6_devtree_create_and_get_pdata(dev);
		if (rc < 0)
			return rc;
	}
#endif

	rc = cyttsp6_probe(&cyttsp6_spi_bus_ops, &spi->dev, spi->irq,
			CY_SPI_DATA_BUF_SIZE);

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_DEVICETREE_SUPPORT
	if (rc && match)
		cyttsp6_devtree_clean_pdata(dev);
#endif

	return rc;
}

static int cyttsp6_spi_remove(struct spi_device *spi)
{
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_DEVICETREE_SUPPORT
	struct device *dev = &spi->dev;
	const struct of_device_id *match;
#endif
	struct cyttsp6_core_data *cd = spi_get_drvdata(spi);

	cyttsp6_release(cd);

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_DEVICETREE_SUPPORT
	match = of_match_device(of_match_ptr(cyttsp6_spi_of_match), dev);
	if (match)
		cyttsp6_devtree_clean_pdata(dev);
#endif

	return 0;
}

static const struct spi_device_id cyttsp6_spi_id[] = {
	{ CYTTSP6_SPI_NAME, 0 },  { }
};
MODULE_DEVICE_TABLE(spi, cyttsp6_spi_id);

static struct spi_driver cyttsp6_spi_driver = {
	.driver = {
		.name = CYTTSP6_SPI_NAME,
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
		.pm = &cyttsp6_pm_ops,
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_DEVICETREE_SUPPORT
		.of_match_table = cyttsp6_spi_of_match,
#endif
	},
	.probe = cyttsp6_spi_probe,
	.remove = cyttsp6_spi_remove,
	.id_table = cyttsp6_spi_id,
};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0))
module_spi_driver(cyttsp6_spi_driver);
#else
static int __init cyttsp6_spi_init(void)
{
	int err = spi_register_driver(&cyttsp6_spi_driver);

	pr_info("%s: Cypress TTSP SPI Driver (Built %s) rc=%d\n",
		 __func__, CY_DRIVER_VERSION, err);

	return err;
}
module_init(cyttsp6_spi_init);

static void __exit cyttsp6_spi_exit(void)
{
	spi_unregister_driver(&cyttsp6_spi_driver);
}
module_exit(cyttsp6_spi_exit);
#endif

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cypress TrueTouch(R) Standard Product SPI driver");
MODULE_AUTHOR("Cypress Semiconductor <ttdrivers@cypress.com>");
