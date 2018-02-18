/*
 * escore-spi.c  --  SPI interface for Audience earSmart chips
 *
 * Copyright 2011 Audience, Inc.
 *
 * Author: Greg Clemson <gclemson@audience.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include "escore.h"
#include "escore-spi.h"

static struct spi_device *escore_spi;

static u32 escore_cpu_to_spi(struct escore_priv *escore, u32 resp)
{
	return cpu_to_be32(resp);
}

static u32 escore_spi_to_cpu(struct escore_priv *escore, u32 resp)
{
	return be32_to_cpu(resp);
}

static int escore_spi_read(struct escore_priv *escore, void *buf, int len)
{
	int rc;

	rc = spi_read(escore_spi, buf, len);
	if (rc < 0) {
		dev_err(&escore_spi->dev, "%s(): error %d reading SR\n",
			__func__, rc);
		ESCORE_FW_RECOVERY_FORCED_OFF(escore);
	}

	return rc;
}

static int escore_spi_read_streaming(struct escore_priv *escore,
				void *buf, int len)
{
	int rc;
	int rdcnt, rd_len, rem_len, pk_cnt;
	u16 data;

	pk_cnt = len/ESCORE_SPI_PACKET_LEN;
	rd_len = ESCORE_SPI_PACKET_LEN;
	rem_len = len % ESCORE_SPI_PACKET_LEN;

	/*
	 * While doing CVQ streaming, userspace is slower in reading data from
	 * kernel which causes circular buffer overrun in kernel. SPI read is
	 * not blocking call and will always return (with either valid data or
	 * 0's). This causes kernel thread much faster. This delay is added for
	 * CVQ streaming only for bug #24910.
	 */
	if (escore->es_streaming_mode == ES_CVQ_STREAMING)
		usleep_range(20000, 20000);

	for (rdcnt = 0; rdcnt < pk_cnt; rdcnt++) {
		rc = escore_spi_read(escore, (char *)(buf + (rdcnt * rd_len)),
				rd_len);
		if (rc < 0) {
			dev_err(escore->dev,
				"%s(): Read Data Block error %d\n",
				__func__, rc);
			return rc;
		}

		usleep_range(ES_SPI_STREAM_READ_DELAY,
				ES_SPI_STREAM_READ_DELAY);
	}

	if (rem_len) {
		rc = escore_spi_read(escore, (char *) (buf + (rdcnt * rd_len)),
				rem_len);
	}

	for (rdcnt = 0; rdcnt < len; rdcnt += 2) {
		data = *((u16 *)buf);
		data = be16_to_cpu(data);
		memcpy(buf, (char *)&data, sizeof(data));
		buf += 2;
	}

	return rdcnt;
}

static int escore_spi_write(struct escore_priv *escore,
			    const void *buf, int len)
{
	int rc;
	int rem = 0;
	char align[4] = {0};

	/* Check if length is 4 byte aligned or not
	   If not aligned send extra bytes after write */
	if ((len > 4) && (len % 4 != 0)) {
		rem = len % 4;
		dev_info(escore->dev,
			"Alignment required 0x%x bytes\n", rem);
	}

	rc = spi_write(escore_spi, buf, len);

	if (rem != 0)
		rc = spi_write(escore_spi, align, 4 - rem);
	if (rc < 0)
		ESCORE_FW_RECOVERY_FORCED_OFF(escore);

	return rc;
}

static int escore_spi_high_bw_write(struct escore_priv *escore,
			    const void *buf, int len)
{
	int rc = 0;
	int rem = 0;
	char align[4] = {0};

#ifdef CONFIG_SND_SOC_ES_SPI_WRITE_DMA_MODE
	int dma_bytes = 0;
	int non_dma_bytes = 0, room = 0, bytes = 0;
	unsigned long pfn = 0;
	const void *current_vaddr =  NULL;
	void *kaddr = NULL;
	int offset = 0;
#endif
	/* Check if length is 4 byte aligned or not
	   If not aligned send extra bytes after write */
	if ((len > 4) && (len % 4 != 0)) {
		rem = len % 4;
		dev_info(escore->dev,
			"Alignment required 0x%x bytes\n", rem);
	}

/* Buffer data can be transmitted by the SPI controller
 * using either DMA mode or polling mode.
 * In DMA mode the SPI controller directly takes data from memory without
 * any interference from processor whereas in polling mode processor is
 * utilized for transferring buffer data from memory to SPI controller.
 * As per the implementation of SPI controller a threshold limit is
 * defined which decides the mode of transfer.
 * So when number of bytes to be transferred exceeds threshold limit
 * DMA mode is utilized, otherwise polling mode is utilized.
 */
#ifdef CONFIG_SND_SOC_ES_SPI_WRITE_DMA_MODE
	if (is_vmalloc_or_module_addr(buf)) {

		/* If the buffer is not page aligned,
		 * transfer the number of bytes from the page offset of buffer
		 * using DMA mode if number of bytes are greater than
		 * ES_SPI_DMA_MIN_BYTES otherwise use non dma mode.
		 * Go to next page if still remaining bytes of data exist.
		 */
		if (((unsigned long)buf & (~PAGE_MASK))) {
			current_vaddr = buf;
			offset = (unsigned long)current_vaddr & (~PAGE_MASK);
			bytes = len;
			room = PAGE_SIZE - offset;
			bytes = ((bytes < room) ? bytes : room);

			/* DMA mode requires buffer address which has
			 * a direct mapping with physical address,So
			 * deduce the physical page frame number from vmalloc
			 * address and from that deduce the virtual address.
			 */
			if (bytes >= ES_SPI_DMA_MIN_BYTES) {
				pfn = vmalloc_to_pfn(current_vaddr);
				kaddr = pfn_to_kaddr(pfn);
				kaddr += offset;
			} else {

			/* Since ES_SPI_DMA_MIN_BYTES
			 * may vary across different platforms,
			 * For number of bytes < ES_SPI_DMA_MIN_BYTES
			 * the SPI controller may use either DMA mode
			 * or polling mode.
			 * So copy the buffer from vmalloc address
			 * to a kernel logical address so that it can be
			 * utilized for both DMA and polling mode.
			 */
				kaddr = kzalloc(bytes, GFP_KERNEL | GFP_DMA);
				memcpy(kaddr, current_vaddr, bytes);
			}
			rc = spi_write(escore_spi, kaddr, bytes);

			if (bytes < ES_SPI_DMA_MIN_BYTES)
				kfree(kaddr);

			if (rc < 0) {
				dev_err(escore->dev,
					"%s(): SPI Write failed rc = %d\n",
					__func__, rc);
				goto spi_write_err;
			}

			len -= bytes;

			if (len == 0) {
				if (rem)
					rc = spi_write(escore_spi,
							align, 4 - rem);
				goto spi_write_err;
			}

			current_vaddr += bytes;
			buf = current_vaddr;
		}

		non_dma_bytes = ((len % PAGE_SIZE < ES_SPI_DMA_MIN_BYTES) ?
				len % PAGE_SIZE : 0);
		dma_bytes = len - non_dma_bytes;
		current_vaddr = buf;

		while  (dma_bytes > 0) {
			pfn = vmalloc_to_pfn(current_vaddr);
			kaddr = pfn_to_kaddr(pfn);
			rc = spi_write(escore_spi, kaddr,
					(dma_bytes > PAGE_SIZE) ?
					PAGE_SIZE : dma_bytes);
			if (rc < 0) {
				dev_err(escore->dev,
					"%s(): SPI Write failed rc = %d\n",
					__func__, rc);
				goto spi_write_err;
			}
			current_vaddr += PAGE_SIZE;
			dma_bytes -= PAGE_SIZE;
		}

		if (non_dma_bytes) {
			kaddr = kzalloc(non_dma_bytes, GFP_KERNEL | GFP_DMA);
			memcpy(kaddr, current_vaddr, non_dma_bytes);
			rc = spi_write(escore_spi, kaddr, non_dma_bytes);
			kfree(kaddr);
			if (rc < 0) {
				dev_err(escore->dev,
					"%s(): SPI Write failed rc = %d\n",
					__func__, rc);
				goto spi_write_err;

			}
		}
		len = 0;
	}
#endif
	if (len) {
		rc = spi_write(escore_spi, buf, len);
		if (rc < 0) {
			dev_err(escore->dev,
				"%s(): SPI Write failed rc = %d\n",
				__func__, rc);
			goto spi_write_err;
		}
	}

	if (rem != 0)
		rc = spi_write(escore_spi, align, 4 - rem);
spi_write_err:
	if (rc < 0)
		ESCORE_FW_RECOVERY_FORCED_OFF(escore);

	return rc;
}

static int escore_spi_cmd(struct escore_priv *escore,
			  u32 cmd, u32 *resp)
{
	int err = 0;
	int sr = cmd & BIT(28);
	int retry = ES_SPI_MAX_RETRIES;
	u32 resp32;

	dev_dbg(escore->dev,
			"%s: cmd=0x%08x  sr=0x%08x\n", __func__, cmd, sr);
	INC_DISABLE_FW_RECOVERY_USE_CNT(escore);

	if ((escore->cmd_compl_mode == ES_CMD_COMP_INTR) && !sr)
		escore_set_api_intr_wait(escore);
	*resp = 0;
	cmd = cpu_to_be32(cmd);
	err = escore_spi_write(escore, &cmd, sizeof(cmd));
	if (err || sr)
		goto cmd_exit;

	if (escore->cmd_compl_mode == ES_CMD_COMP_INTR) {
		pr_debug("%s(): Waiting for API interrupt. Jiffies:%lu",
				__func__, jiffies);
		err = escore_api_intr_wait_completion(escore);
		if (err)
			goto cmd_exit;
	}

	usleep_range(ES_SPI_RETRY_DELAY, ES_SPI_RETRY_DELAY + 50);

	do {
		--retry;
		if (escore->cmd_compl_mode != ES_CMD_COMP_INTR) {
			usleep_range(ES_SPI_1MS_DELAY,
					ES_SPI_1MS_DELAY + 50);
		}

		err = escore_spi_read(escore, &resp32, sizeof(resp32));
		dev_dbg(escore->dev, "%s: err=%d\n", __func__, err);
		*resp = (be32_to_cpu(resp32));
		dev_dbg(escore->dev, "%s: *resp=0x%08x\n", __func__, *resp);
		if (err) {
			dev_dbg(escore->dev,
				"%s: escore_spi_read() failure, err=%d\n",
				__func__, err);
		}
		if (*resp == 0) {
			if (retry == 0) {
				err = -ETIMEDOUT;
				dev_err(escore->dev,
					"%s: response retry timeout, err=%d\n",
					__func__, err);
				break;
			} else {
				continue;
			}
		} else if ((*resp >> 16) == 0) {
			/* Fw SPI interface is 16bit. So in the 32bit
			 * read of command's response, sometimes fw
			 * sends first 16bit in first 32bit read and
			 * second 16bit in second 32bit. So this is
			 * special condition handling to make response.
			 */
			err = escore_spi_read(escore, &resp32, sizeof(resp32));
			if (err) {
				dev_dbg(escore->dev,
					"%s: escore_spi_read() failure, err=%d\n",
					__func__, err);
			} else {
				*resp = *resp << 16;
				*resp |= (be32_to_cpu(resp32) >> 16);
			}
		}

		if ((*resp & ES_ILLEGAL_CMD) == ES_ILLEGAL_CMD) {
			dev_err(escore->dev, "%s: illegal command 0x%08x\n",
				__func__, be32_to_cpu(cmd));
			err = -EINVAL;
			goto cmd_exit;
		} else {
			goto cmd_exit;
		}

	} while (retry != 0);

cmd_exit:
	update_cmd_history(be32_to_cpu(cmd), *resp);
	DEC_DISABLE_FW_RECOVERY_USE_CNT(escore);
	if (err && ((*resp & ES_ILLEGAL_CMD) != ES_ILLEGAL_CMD))
		ESCORE_FW_RECOVERY_FORCED_OFF(escore);

	return err;
}

static int escore_spi_setup(u32 speed)
{
	int status;

	/* set SPI speed to
	 *	- 12MHz for fw download
	 *		- <= 4MHz for rest operations */
	escore_spi->max_speed_hz = speed;
	status = spi_setup(escore_spi);
	if (status < 0) {
		pr_err("%s : can't setup %s, status %d\n", __func__,
				(char *)dev_name(&escore_spi->dev), status);
	} else {
		pr_info("\nSPI speed changed to %dHz\n",
				escore_spi->max_speed_hz);
		usleep_range(1000, 1005);
	}
	return status;
}

static int escore_spi_set_bits_per_word(u8 bits)
{
	int status;

	/* set SPI bits per word to
	 *	- 16 bits per word for before fw download
	 *	- 32 bits per word for rest operations */
	switch (bits) {
	case ES_SPI_8BITS_PER_WORD:
	case ES_SPI_16BITS_PER_WORD:
	case ES_SPI_32BITS_PER_WORD:
		break;
	default:
		pr_err("%s : Unsupported SPI bits per word %d\n",
				__func__, bits);
		return -EINVAL;
	}
	escore_spi->bits_per_word = bits;
	status = spi_setup(escore_spi);
	if (status < 0) {
		pr_err("%s : can't setup %s, status %d\n", __func__,
				(char *)dev_name(&escore_spi->dev), status);
	} else {
		pr_debug("%s(): SPI bits per word changed to %d\n",
				__func__, escore_spi->bits_per_word);
	}
	return status;
}

static int escore_spi_datablock_read(struct escore_priv *escore, void *buf,
		size_t len)
{
	int rc;
	int rdcnt = 0;
	u16 temp;

	rc = escore_spi_read(escore, (char *)buf, len);
	if (rc < 0) {
		dev_err(escore->dev, "%s(): Read Data Block error %d\n",
				__func__, rc);
		return rc;
	}

	for (rdcnt = 0; rdcnt < len; rdcnt += 2) {
		temp = *((u16 *)buf);
		temp = be16_to_cpu(temp);
		memcpy(buf, (char *)&temp, sizeof(temp));
		buf += 2;
	}

	return rc;
}

static int escore_spi_boot_setup(struct escore_priv *escore)
{
	u16 boot_cmd = ES_SPI_BOOT_CMD;
	u16 boot_ack;
	u16 sbl_sync_cmd = ES_SPI_SYNC_CMD;
	u16 sbl_sync_ack;
	int retry = 5;
	int iteration_count = 0;
	int rc;
	u8 bits_per_word = ES_SPI_16BITS_PER_WORD;

	pr_debug("%s(): prepare for fw download\n", __func__);
	rc = escore_spi_set_bits_per_word(bits_per_word);
	if (rc < 0) {
		pr_err("%s(): Set SPI %d bits per word failed rc = %d\n",
		       __func__, bits_per_word, rc);
		return rc;
	}
	msleep(20);
	sbl_sync_cmd = cpu_to_be16(sbl_sync_cmd);

	rc = escore_spi_write(escore, &sbl_sync_cmd, sizeof(sbl_sync_cmd));

	if (rc < 0) {
		pr_err("%s(): firmware load failed sync write %d\n",
		       __func__, rc);
		goto escore_spi_boot_setup_failed;
	}

	usleep_range(4000, 4050);

	rc = escore_spi_read(escore, &sbl_sync_ack, sizeof(sbl_sync_ack));
	if (rc < 0) {
		pr_err("%s(): firmware load failed sync ack %d\n",
		       __func__, rc);
		goto escore_spi_boot_setup_failed;
	}

	sbl_sync_ack = be16_to_cpu(sbl_sync_ack);
	pr_debug("%s(): SBL SYNC ACK = 0x%08x\n", __func__, sbl_sync_ack);
	if (sbl_sync_ack != ES_SPI_SYNC_ACK) {
		pr_err("%s(): sync ack pattern fail 0x%x\n", __func__,
				sbl_sync_ack);
		rc = -EIO;
		goto escore_spi_boot_setup_failed;
	}

	usleep_range(4000, 4050);
	pr_debug("%s(): write ES_BOOT_CMD = 0x%04x\n", __func__, boot_cmd);
	boot_cmd = cpu_to_be16(boot_cmd);
	rc = escore_spi_write(escore, &boot_cmd, sizeof(boot_cmd));
	if (rc < 0) {
		pr_err("%s(): firmware load failed boot write %d\n",
		       __func__, rc);
		goto escore_spi_boot_setup_failed;
	}
	do {
		usleep_range(ES_SPI_1MS_DELAY, ES_SPI_1MS_DELAY + 5);
		rc = escore_spi_read(escore, &boot_ack, sizeof(boot_ack));
		if (rc < 0) {
			pr_err("%s(): firmware load failed boot ack %d\n",
			       __func__, rc);
		}

		boot_ack = be16_to_cpu(boot_ack);
		pr_debug("%s(): BOOT ACK = 0x%08x\n", __func__, boot_ack);
		if (boot_ack != ES_SPI_BOOT_ACK) {
			pr_err("%s():boot ack pattern fail 0x%08x", __func__,
					boot_ack);
			rc = -EIO;
			iteration_count++;
		}
	} while (rc && retry--);
	if (rc < 0) {
		pr_err("%s(): boot ack pattern fail after %d iterations\n",
				__func__, iteration_count);
		goto escore_spi_boot_setup_failed;
	}
	rc = escore_spi_setup(escore->pdata->spi_fw_download_speed);
escore_spi_boot_setup_failed:
	return rc;
}

int escore_spi_boot_finish(struct escore_priv *escore)
{
	u32 sync_cmd = (ES_SYNC_CMD << 16) | ES_SYNC_POLLING;
	u32 sync_ack;
	int rc = 0;
	int sync_retry = ES_SYNC_MAX_RETRY;
	u8 bits_per_word = ES_SPI_32BITS_PER_WORD;

	rc = escore_spi_set_bits_per_word(bits_per_word);
	if (rc < 0) {
		pr_err("%s(): Set SPI %d bits per word failed rc = %d\n",
		       __func__, bits_per_word, rc);
		return rc;
	}

	rc = escore_spi_setup(escore->pdata->spi_operational_speed);
	msleep(50);
	/* sometimes earSmart chip sends success in second sync command */
	do {
		pr_debug("%s(): write ES_SYNC_CMD = 0x%08x\n",
				__func__, sync_cmd);
		rc = escore_spi_cmd(escore, sync_cmd, &sync_ack);
		pr_debug("%s(): rc=%d, sync_ack = 0x%08x\n", __func__, rc,
			sync_ack);
		if (rc < 0) {
			pr_err("%s(): firmware load failed sync write %d\n",
			       __func__, rc);
			continue;
		}
		if (sync_ack != ES_SYNC_ACK) {
			pr_err("%s(): firmware load failed sync ack pattern",
					__func__);
			rc = -EIO;
		} else {
			pr_info("%s(): firmware load success", __func__);
			break;
		}
	} while (sync_retry--);
	return rc;
}

static int escore_spi_abort_config(struct escore_priv *escore)
{
	int rc;

	rc = escore_spi_setup(escore->pdata->spi_operational_speed);
	if (rc < 0)
		pr_err("%s(): config spi failed, rc = %d\n", __func__, rc);

	return rc;
}

static int escore_spi_wdb_config(struct escore_priv *escore)
{
	return escore_spi_setup(escore->pdata->spi_wdb_speed);
}

static int escore_spi_reset_wdb_config(struct escore_priv *escore)
{
	return escore_spi_setup(escore->pdata->spi_operational_speed);
}

static int escore_spi_populate_dt_data(struct escore_priv *escore)
{
	struct esxxx_platform_data *pdata;
	struct device_node *node = escore_spi->dev.of_node;
	int rc = -EINVAL;

	/* No node means  no dts provision, like Panda board */
	if (node == NULL)
		return 0;
	pdata = escore->pdata;
	if (pdata == NULL)
		return rc;

	/*
	rc = of_property_read_u32(node, "adnc,spi-fw-download-speed",
			&pdata->spi_fw_download_speed);
	if (rc || pdata->spi_fw_download_speed == 0) {
		pr_err("%s, Error in parsing adnc,spi-fw-download-speed\n",
				__func__);
		return rc;
	}

	rc = of_property_read_u32(node, "adnc,spi-operational-speed",
			&pdata->spi_operational_speed);
	if (rc || pdata->spi_operational_speed == 0) {
		pr_err("%s, Error in parsing adnc,spi-operational-speed\n",
				__func__);
		return rc;
	}
	*/

	pdata->spi_fw_download_speed = 9600000;
	pdata->spi_operational_speed = 4800000;

	rc = of_property_read_u32(node, "adnc,spi-wdb-speed",
			&pdata->spi_wdb_speed);
	if (rc || pdata->spi_wdb_speed == 0) {
		pr_err("%s, Error in parsing adnc,spi-wdb-speed\n", __func__);
		pr_debug("%s, Use operational speed for writing data block\n",
				__func__);
		pdata->spi_wdb_speed = pdata->spi_operational_speed;
		rc = 0;
	}

	return rc;

}
static void escore_spi_setup_pri_intf(struct escore_priv *escore)
{
	escore->bus.ops.read = escore_spi_read;
	escore->bus.ops.write = escore_spi_write;
	escore->bus.ops.cmd = escore_spi_cmd;
	escore->streamdev = es_spi_streamdev;
	escore->bus.ops.cpu_to_bus = escore_cpu_to_spi;
	escore->bus.ops.bus_to_cpu = escore_spi_to_cpu;
}

static int escore_spi_setup_high_bw_intf(struct escore_priv *escore)
{
	int rc;

	escore->boot_ops.setup = escore_spi_boot_setup;
	escore->boot_ops.finish = escore_spi_boot_finish;
	escore->bus.ops.high_bw_write = escore_spi_high_bw_write;
	escore->bus.ops.high_bw_read = escore_spi_read;
	escore->bus.ops.high_bw_cmd = escore_spi_cmd;
	escore->boot_ops.escore_abort_config = escore_spi_abort_config;
	escore->bus.ops.rdb = escore_spi_datablock_read;
	escore->bus.ops.cpu_to_high_bw_bus = escore_cpu_to_spi;
	escore->bus.ops.high_bw_bus_to_cpu = escore_spi_to_cpu;

	rc = escore->probe(escore->dev);
	if (rc)
		goto out;

	rc = escore_spi_populate_dt_data(escore);
	if (rc)
		goto out;

	if (escore->pdata->spi_wdb_speed !=
			escore->pdata->spi_operational_speed) {
		escore->datablock_dev.wdb_config = escore_spi_wdb_config;
		escore->datablock_dev.wdb_reset_config =
			escore_spi_reset_wdb_config;
	}

	mutex_lock(&escore->access_lock);
	rc = escore->boot_ops.bootup(escore);
	mutex_unlock(&escore->access_lock);

out:
	return rc;
}

static int escore_spi_probe(struct spi_device *spi)
{
	int rc;
	struct escore_priv *escore = &escore_priv;

	dev_set_drvdata(&spi->dev, &escore_priv);

	escore_spi = spi;

	if (escore->pri_intf == ES_SPI_INTF)
		escore->bus.setup_prim_intf = escore_spi_setup_pri_intf;
	if (escore->high_bw_intf == ES_SPI_INTF)
		escore->bus.setup_high_bw_intf = escore_spi_setup_high_bw_intf;

	rc = escore_probe(escore, &spi->dev, ES_SPI_INTF, ES_CONTEXT_PROBE);
	return rc;
}

int escore_spi_wait(struct escore_priv *escore)
{
	/* For SPI there is no wait function available. It should always return
	 * true so that next read request is made in streaming case.
	 */
	return 1;
}

static int escore_spi_close(struct escore_priv *escore)
{
	int rc;
	char buf[64] = {0};
	u32 sync_cmd = (ES_SYNC_CMD << 16) | ES_SYNC_POLLING;
	u32 sync_ack;

	rc = escore_spi_write(escore, buf, sizeof(buf));
	if (rc) {
		dev_err(escore->dev, "%s:spi write error %d\n",
				__func__, rc);
		return rc;
	}

	/* Sending SYNC command */
	rc = escore_spi_cmd(escore, sync_cmd, &sync_ack);
	if (rc) {
		dev_err(escore->dev, "%s: Failed to send SYNC cmd, error %d\n",
				__func__, rc);
		return rc;
	}

	if (sync_ack != ES_SYNC_ACK) {
		dev_warn(escore->dev, "%s:Invalis SYNC_ACK received %x\n",
				__func__, sync_ack);
		rc = -EIO;
	}
	return rc;
}

struct es_stream_device es_spi_streamdev = {
	.read = escore_spi_read_streaming,
	.wait = escore_spi_wait,
	.close = escore_spi_close,
	.intf = ES_SPI_INTF,
};

static int escore_spi_remove(struct spi_device *spi)
{
	snd_soc_unregister_codec(&spi->dev);
	return 0;
}

int __init escore_spi_init(void)
{
	return spi_register_driver(&escore_spi_driver);
}

void __exit escore_spi_exit(void)
{
	spi_unregister_driver(&escore_spi_driver);
}

static const struct spi_device_id escore_spi_id[] = {
	{ "earSmart-codec", 0},
	{ }
};
MODULE_DEVICE_TABLE(spi, escore_id);

struct spi_driver escore_spi_driver = {
	.driver = {
		.name   = "earSmart-codec",
		.bus    = &spi_bus_type,
		.owner  = THIS_MODULE,
		.pm = &escore_pm_ops,
	},
	.probe  = escore_spi_probe,
	.remove = escore_spi_remove,
	.id_table = escore_spi_id,
};

MODULE_DESCRIPTION("Audience earSmart SPI core driver");
MODULE_AUTHOR("Greg Clemson <gclemson@audience.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:earSmart-codec");
