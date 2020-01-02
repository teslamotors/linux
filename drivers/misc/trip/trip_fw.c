/*
 * Copyright (C) 2018 Tesla, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#include <linux/crc32.h>
#include <linux/dma-mapping.h>
#include <linux/cdev.h>

#include "trip_ioctl.h"
#include "trip_fw_fmt.h"
#include "trip_fw.h"
#include "trip_reg.h"
#include "trip.h"
#include "trip_dma.h"

static inline uint32_t unpack32(const uint8_t *ofs)
{
	return le32_to_cpu(*((const uint32_t *)ofs));
}

static inline uint64_t unpack64(const uint8_t *ofs)
{
	return le64_to_cpu(*((const uint64_t *)ofs));
}

static int sanity_check_firmware(struct platform_device *pdev,
				 const struct firmware *fw)
{
	uint64_t filesize, tcp_size, tcp_ofs;
	uint32_t crc, n_tcpd;

	/* Make sure buffer is big enough for a header at least */
	if (fw->size < TRIP_FW_HDR_LEN) {
		dev_err(&pdev->dev, "Trip firmware too short\n");
		return -EINVAL;
	}

	/* Check the magic */
	if (unpack32(fw->data + TRIP_FW_HDR_MAGIC_OFS) != TRIP_FW_HDR_MAGIC) {
		dev_err(&pdev->dev, "Trip firmware has incorrect magic\n");
		return -EINVAL;
	}

	/* Make sure the included filesize is correct. */
	filesize = unpack64(fw->data + TRIP_FW_HDR_FILESIZE_OFS);
	if (filesize != fw->size) {
		dev_err(&pdev->dev, "Trip firmware has incorrect size\n");
		return -EINVAL;
	}

	/* Compute CRC with CRC header field set to zero */
	crc = crc32_le_combine(
		crc32_le(~0, fw->data, TRIP_FW_HDR_CRC32_OFS),
		crc32_le(0, fw->data + TRIP_FW_HDR_VERSION_STR_OFS,
			 fw->size - TRIP_FW_HDR_VERSION_STR_OFS),
			fw->size - TRIP_FW_HDR_VERSION_STR_OFS + 4);
	crc ^= 0xffffffffu;
	if (crc != unpack32(fw->data + TRIP_FW_HDR_CRC32_OFS)) {
		dev_err(&pdev->dev, "Trip firmware has incorrect checksum %08x expecting %08x\n",
			crc, unpack32(fw->data + TRIP_FW_HDR_CRC32_OFS));
		return -EINVAL;
	}

	/* Check computed filesize */
	n_tcpd = unpack32(fw->data + TRIP_FW_HDR_N_TCP_OFS);
	tcp_ofs = ALIGN(TRIP_FW_HDR_LEN +
			 ((uint64_t)n_tcpd) * TRIP_FW_TCPD_LEN,
			 TRIP_TCP_BLOB_ALIGN);
	tcp_size = unpack64(fw->data + TRIP_FW_HDR_TCP_SIZE_OFS);

	if (tcp_ofs > fw->size || tcp_size != fw->size - tcp_ofs) {
		dev_err(&pdev->dev, "Trip firmware size error\n");
		return -EINVAL;
	}

	return 0;
}

static int parse_header(struct platform_device *pdev, const uint8_t *buf,
			struct trip_fw_info *fw_info)
{
	struct tripdev *td = platform_get_drvdata(pdev);
	struct trip_fw_hdr *hdr = &fw_info->hdr;

	memcpy(hdr->version, buf + TRIP_FW_HDR_VERSION_STR_OFS,
			TRIP_FW_HDR_VERSION_STR_LEN);
	/* Must be zero-terminated and padded */
	if (hdr->version[TRIP_FW_HDR_VERSION_STR_LEN-1] != 0) {
		dev_err(&pdev->dev, "Trip firmware version fmt error\n");
		return -EINVAL;
	}

	hdr->timestamp = unpack64(buf + TRIP_FW_HDR_TIMESTAMP_OFS);

	hdr->features = unpack32(buf + TRIP_FW_HDR_FEATURES_OFS);

	hdr->load_addr = unpack64(buf + TRIP_FW_HDR_LOAD_ADDR_OFS);
	if (hdr->load_addr > (uint64_t) td->sram_len) {
		dev_err(&pdev->dev, "Trip firmware has invalid load addr\n");
		return -EINVAL;
	}

	hdr->tcp_size = unpack64(buf + TRIP_FW_HDR_TCP_SIZE_OFS);
	if (hdr->tcp_size > (uint64_t) td->sram_len - hdr->load_addr) {
		dev_err(&pdev->dev, "Trip firmware has invalid program len\n");
		return -EINVAL;
	}

	hdr->spare_addr = unpack64(buf + TRIP_FW_HDR_SPARE_ADDR_OFS);
	if (hdr->spare_addr > (uint64_t) td->sram_len - 12) {
		dev_err(&pdev->dev, "Trip firmware has invalid spare addr\n");
		return -EINVAL;
	}
	if (hdr->spare_addr > hdr->load_addr - 12 &&
			hdr->spare_addr < hdr->load_addr + hdr->tcp_size) {
		dev_err(&pdev->dev, "Trip firmware has invalid spare addr.\n");
		return -EINVAL;
	}

	fw_info->n_tcpd = unpack32(buf + TRIP_FW_HDR_N_TCP_OFS);

	return 0;
}

static int parse_tcpd(struct platform_device *pdev, const uint8_t *buf,
			struct trip_fw_info *fw_info, int num)
{
	struct trip_fw_tcp_desc *tcpd = &fw_info->tcpd[num];

	memcpy(tcpd->version, buf + TRIP_FW_TCPD_VERSION_STR_OFS,
			TRIP_FW_TCPD_VERSION_STR_LEN);
	/* Must be zero-terminated and padded */
	if (tcpd->version[TRIP_FW_TCPD_VERSION_STR_LEN-1] != 0) {
		dev_err(&pdev->dev, "Trip desc %u firmware version error\n",
			num);
		return -EINVAL;
	}

	tcpd->start_addr = unpack64(buf + TRIP_FW_TCPD_START_ADDR_OFS);
	if (tcpd->start_addr < fw_info->hdr.load_addr ||
		tcpd->start_addr >=
			fw_info->hdr.load_addr + fw_info->hdr.tcp_size) {
		dev_err(&pdev->dev, "Trip desc %u firmware version error\n",
			num);
		return -EINVAL;
	}

	tcpd->data_bufsz = unpack64(buf + TRIP_FW_TCPD_DATA_BUFSZ_OFS);
	tcpd->weight_bufsz = unpack64(buf + TRIP_FW_TCPD_WEIGHT_BUFSZ_OFS);
	tcpd->out_bufsz = unpack64(buf + TRIP_FW_TCPD_OUT_BUFSZ_OFS);
	/* XXX - sanity check is within HW limits */

	memcpy(tcpd->function_uuid, buf + TRIP_FW_TCPD_FN_UUID_OFS,
		TRIP_FW_TCPD_FN_UUID_LEN);

	tcpd->runtime_ms = unpack32(buf + TRIP_FW_TCPD_RUNTIME_MS_OFS);

	tcpd->progress_wdt = unpack32(buf + TRIP_FW_TCPD_PROGRESS_WDT_OFS);
	tcpd->layer_wdt = unpack32(buf + TRIP_FW_TCPD_LAYER_WDT_OFS);
	tcpd->network_wdt = unpack32(buf + TRIP_FW_TCPD_NETWORK_WDT_OFS);
	/* XXX - santiy check within HW limits */

	return 0;
}

static int load_fw_tcp(struct platform_device *pdev, const uint8_t *buf,
			uint64_t load_addr, uint64_t tcp_size,
			uint64_t spare_addr)
{
	uint8_t *dmabuf;
	dma_addr_t iova;
	struct sg_table sgt;
	struct trip_ioc_xfer xfer;
	size_t bufsz;
	int ret;

	dev_dbg(&pdev->dev, "load_fw_tcp: loading 0x%llx bytes at load_addr 0x%llx.\n",
			tcp_size, load_addr);

	/* Allocate a sgt to track buffer */
	if (sg_alloc_table(&sgt, 1, GFP_KERNEL))
		return -ENOMEM;

	/* Copy data to a buffer more suited for DMA */
	bufsz = ALIGN(tcp_size, PAGE_SIZE);
	dmabuf = dma_alloc_coherent(&pdev->dev, bufsz, &iova, GFP_KERNEL);
	if (!dmabuf)
		return -ENOMEM;

	memcpy(dmabuf, buf, tcp_size);
	sg_set_buf(sgt.sgl, dmabuf, tcp_size);
	sg_dma_len(sgt.sgl) = tcp_size;
	sg_dma_address(sgt.sgl) = iova;

	xfer.sram_addr = load_addr;
	xfer.spare_sram_addr = spare_addr;
	xfer.spare_net = 0;

	ret = trip_sg_handle_dma(pdev, &xfer, &sgt, 1, 1);

	sg_free_table(&sgt);
	dma_free_coherent(&pdev->dev, bufsz, dmabuf, iova);

	return ret;
}

void trip_handle_firmware(const struct firmware *fw, void *ctx)
{
	struct platform_device *pdev = ctx;
	struct tripdev *td = platform_get_drvdata(pdev);
	struct trip_fw_info *fw_info = fw_info = &td->fw_info;
	uint64_t tcp_ofs;
	int ret, i;

	if (fw == NULL) {
		dev_err(&pdev->dev, "Failed to request firmware\n");
		return;
	}

	ret = sanity_check_firmware(pdev, fw);
	if (ret)
		goto out_err;

	ret = parse_header(pdev, fw->data, fw_info);
	if (ret)
		goto out_err;

	/* allocate and parse TCP descriptors */
	fw_info->tcpd = vmalloc(sizeof(*fw_info->tcpd) * fw_info->n_tcpd);
	if (fw_info->tcpd == NULL)
		goto out_err;
	for (i = 0; i < fw_info->n_tcpd; i++) {
		ret = parse_tcpd(pdev, fw->data + TRIP_FW_HDR_LEN +
				 (i * TRIP_FW_TCPD_LEN), fw_info, i);
		if (ret)
			goto out_err;
	}

	/* load the firmware blob into Trip SRAM */
	tcp_ofs = ALIGN(TRIP_FW_HDR_LEN +
			 ((uint64_t)fw_info->n_tcpd) * TRIP_FW_TCPD_LEN,
			 TRIP_TCP_BLOB_ALIGN);
	ret = load_fw_tcp(pdev, fw->data + tcp_ofs, fw_info->hdr.load_addr,
				fw_info->hdr.tcp_size,
				fw_info->hdr.spare_addr);

out_err:
	if (ret == 0)
		fw_info->fw_valid = true;
	else
		vfree(fw_info->tcpd);

	release_firmware(fw);
}

int trip_fw_load(struct platform_device *pdev, const char *filename,
		 struct trip_fw_info *fw_info)
{
	const struct firmware *firmware;
	int ret;

	/* special case for testing, also resume */
	if (fw_info->fw_valid)
		vfree(fw_info->tcpd);
	fw_info->fw_valid = false;

	ret = request_firmware(&firmware, filename, &pdev->dev);
	if (ret == 0)
		trip_handle_firmware(firmware, pdev);

	return ret;
}

