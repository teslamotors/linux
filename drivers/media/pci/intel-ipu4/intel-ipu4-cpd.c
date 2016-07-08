/*
 * Copyright (c) 2015--2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/dma-mapping.h>
#include <linux/module.h>

#include "intel-ipu4.h"
#include "intel-ipu4-cpd.h"

#include <ia_css_fw_pkg_release.h>

/* 15 entries + header*/
#define MAX_PKG_DIR_ENT_CNT		16
/* 2 qword per entry/header */
#define PKG_DIR_ENT_LEN			2
/* PKG_DIR size in bytes */
#define PKG_DIR_SIZE			((MAX_PKG_DIR_ENT_CNT) *	\
					 (PKG_DIR_ENT_LEN) * sizeof(u64))
#define PKG_DIR_ID_SHIFT		48
#define PKG_DIR_ID_MASK			0x7f
#define PKG_DIR_VERSION_SHIFT		32
#define PKG_DIR_SIZE_MASK		0xfffff
/* _IUPKDR_ */
#define PKG_DIR_HDR_MARK		0x5f4955504b44525f

/* $CPD */
#define CPD_HDR_MARK			0x44504324

/* Maximum size is 2K DWORDs */
#define MAX_MANIFEST_SIZE		(2 * 1024 * sizeof(u32))

/* Maximum size is 64k */
#define MAX_METADATA_SIZE		(64 * 1024)

#define MAX_COMPONENT_ID		127
#define MAX_COMPONENT_VERSION		0xffff

#define CPD_MANIFEST_IDX	0
#define CPD_METADATA_IDX	1
#define CPD_MODULEDATA_IDX	2

#define intel_ipu4_cpd_get_entries(cpd)					\
	((struct intel_ipu4_cpd_ent *)					\
	 ((struct intel_ipu4_cpd_hdr *)cpd + 1))
#define intel_ipu4_cpd_get_entry(cpd, idx)	\
	(&intel_ipu4_cpd_get_entries(cpd)[idx])
#define intel_ipu4_cpd_get_manifest(cpd)	\
	intel_ipu4_cpd_get_entry(cpd, CPD_MANIFEST_IDX)
#define intel_ipu4_cpd_get_metadata(cpd)	\
	intel_ipu4_cpd_get_entry(cpd, CPD_METADATA_IDX)
#define intel_ipu4_cpd_get_moduledata(cpd)		\
	intel_ipu4_cpd_get_entry(cpd, CPD_MODULEDATA_IDX)

static const struct intel_ipu4_cpd_metadata_cmpnt *
intel_ipu4_cpd_metadata_get_cmpnt(
	struct intel_ipu4_device *isp, const void *metadata,
	unsigned metadata_size, u8 idx)
{
	const struct intel_ipu_cpd_metadata_extn *extn;
	const struct intel_ipu4_cpd_metadata_cmpnt *cmpnts;
	int cmpnt_count;

	extn = metadata;
	cmpnts = metadata + sizeof(*extn);
	cmpnt_count = (metadata_size - sizeof(*extn)) / sizeof(*cmpnts);

	if (idx > MAX_COMPONENT_ID || idx >= cmpnt_count) {
		dev_err(&isp->pdev->dev, "Component index out of range (%d)\n",
			idx);
		return ERR_PTR(-EINVAL);
	}

	return &cmpnts[idx];
}

static u32 intel_ipu4_cpd_metadata_cmpnt_version(
					struct intel_ipu4_device *isp,
					const void *metadata,
					unsigned metadata_size,
					u8 idx)
{
	const struct intel_ipu4_cpd_metadata_cmpnt *cmpnt =
		intel_ipu4_cpd_metadata_get_cmpnt(
			isp, metadata, metadata_size, idx);

	if (IS_ERR(cmpnt))
		return PTR_ERR(cmpnt);

	return cmpnt->ver;
}

static int intel_ipu4_cpd_metadata_get_cmpnt_id(
					struct intel_ipu4_device *isp,
					const void *metadata,
					unsigned metadata_size,
					u8 idx)
{
	const struct intel_ipu4_cpd_metadata_cmpnt *cmpnt =
		intel_ipu4_cpd_metadata_get_cmpnt(
			isp, metadata, metadata_size, idx);

	if (IS_ERR(cmpnt))
		return PTR_ERR(cmpnt);

	return cmpnt->id;
}

static u32 intel_ipu4_cpd_metadata_get_cmpnt_icache_base_offs(
	struct intel_ipu4_device *isp,
	const void *metadata,
	unsigned metadata_size,
	u8 idx)
{
	const struct intel_ipu4_cpd_metadata_cmpnt *cmpnt =
		intel_ipu4_cpd_metadata_get_cmpnt(
			isp, metadata, metadata_size, idx);

	if (IS_ERR(cmpnt))
		return PTR_ERR(cmpnt);

	return cmpnt->icache_base_offs;
}

static u32 intel_ipu4_cpd_metadata_get_cmpnt_entry_point(
	struct intel_ipu4_device *isp,
	const void *metadata,
	unsigned metadata_size,
	u8 idx)
{
	const struct intel_ipu4_cpd_metadata_cmpnt *cmpnt =
		intel_ipu4_cpd_metadata_get_cmpnt(
			isp, metadata, metadata_size, idx);

	if (IS_ERR(cmpnt))
		return PTR_ERR(cmpnt);

	return cmpnt->entry_point;
}

static int intel_ipu4_cpd_parse_module_data(struct intel_ipu4_device *isp,
					    const void *module_data,
					    unsigned module_data_size,
					    dma_addr_t dma_addr_module_data,
					    u64 *pkg_dir,
					    const void *metadata,
					    unsigned metadata_size)
{
	const struct intel_ipu4_cpd_module_data_hdr *module_data_hdr;
	const struct intel_ipu4_cpd_hdr *dir_hdr;
	const struct intel_ipu4_cpd_ent *dir_ent;
	int i;

	if (!module_data)
		return -EINVAL;

	module_data_hdr = module_data;
	dir_hdr = module_data + module_data_hdr->hdr_len;
	dir_ent = (struct intel_ipu4_cpd_ent *) (dir_hdr + 1);

	pkg_dir[0] = PKG_DIR_HDR_MARK;
	/* pkg_dir entry count = component count + pkg_dir header */
	pkg_dir[1] = dir_hdr->ent_cnt + 1;

	for (i = 0; i < dir_hdr->ent_cnt; i++, dir_ent++) {
		u64 *p = &pkg_dir[PKG_DIR_ENT_LEN + i * PKG_DIR_ENT_LEN];
		int ver, id;

		*p++ = dma_addr_module_data + dir_ent->offset;

		id = intel_ipu4_cpd_metadata_get_cmpnt_id(isp, metadata,
							  metadata_size, i);
		if (id < 0 || id > MAX_COMPONENT_ID) {
			dev_err(&isp->pdev->dev, "Failed to parse component id\n");
			return -EINVAL;
		}
		ver = intel_ipu4_cpd_metadata_cmpnt_version(isp, metadata,
						       metadata_size, i);
		if (ver < 0 || ver > MAX_COMPONENT_VERSION) {
			dev_err(&isp->pdev->dev, "Failed to parse component version\n");
			return -EINVAL;
		}

		/*
		 * PKG_DIR Entry (type == id)
		 * 63:56	55	54:48	47:32	31:24	23:0
		 * Rsvd		Rsvd	Type	Version	Rsvd	Size
		 */
		*p = dir_ent->len | (u64) id << PKG_DIR_ID_SHIFT |
			(u64) ver << PKG_DIR_VERSION_SHIFT;
	}

	return 0;
}

void *intel_ipu4_cpd_create_pkg_dir(struct intel_ipu4_bus_device *adev,
						const void *src,
						dma_addr_t dma_addr_src,
						dma_addr_t *dma_addr,
						unsigned *pkg_dir_size)
{
	struct intel_ipu4_device *isp = adev->isp;
	const struct intel_ipu4_cpd_hdr *hdr = src;
	const struct intel_ipu4_cpd_ent *ent, *man_ent, *met_ent;
	u64 *pkg_dir;
	unsigned man_sz, met_sz, hdr_sz;
	void *pkg_dir_pos;
	int ret;

	hdr_sz = hdr->hdr_len;

	man_ent = intel_ipu4_cpd_get_manifest(src);
	man_sz = man_ent->len;

	met_ent = intel_ipu4_cpd_get_metadata(src);
	met_sz = met_ent->len;

	*pkg_dir_size = PKG_DIR_SIZE + man_sz + met_sz;
	pkg_dir = dma_alloc_attrs(&adev->dev, *pkg_dir_size, dma_addr,
				  GFP_KERNEL, NULL);
	if (!pkg_dir)
		return pkg_dir;

	/*
	 * pkg_dir entry/header:
	 * qword | 63:56 | 55   | 54:48 | 47:32 | 31:24 | 23:0
	 * N         Address/Offset/"_IUPKDR_"
	 * N + 1 | rsvd  | rsvd | type  | ver   | rsvd  | size
	 *
	 * We can ignore other fields that size in N + 1 qword as they
	 * are 0 anyway. Just setting size for now.
	 */

	ent = intel_ipu4_cpd_get_moduledata(src);

	ret = intel_ipu4_cpd_parse_module_data(isp, src + ent->offset,
					       ent->len,
					       dma_addr_src + ent->offset,
					       pkg_dir,
					       src + met_ent->offset,
					       met_ent->len);
	if (ret) {
		dev_err(&isp->pdev->dev,
			"Unable to parse module data section!\n");
		dma_free_attrs(&isp->psys->dev, *pkg_dir_size, pkg_dir,
			       *dma_addr, NULL);
		return NULL;
	}

	/* Copy manifest after pkg_dir */
	pkg_dir_pos = pkg_dir + PKG_DIR_ENT_LEN * MAX_PKG_DIR_ENT_CNT;
	memcpy(pkg_dir_pos, src + man_ent->offset, man_sz);

	/* Copy metadata after manifest */
	pkg_dir_pos += man_sz;
	memcpy(pkg_dir_pos, src + met_ent->offset, met_sz);

	dma_sync_single_range_for_device(&adev->dev, *dma_addr,
					  0, *pkg_dir_size, DMA_TO_DEVICE);

	return pkg_dir;
}
EXPORT_SYMBOL_GPL(intel_ipu4_cpd_create_pkg_dir);

void intel_ipu4_cpd_free_pkg_dir(struct intel_ipu4_bus_device *adev,
				 u64 *pkg_dir,
				 dma_addr_t dma_addr,
				 unsigned pkg_dir_size)
{
	dma_free_attrs(&adev->dev, pkg_dir_size, pkg_dir, dma_addr, NULL);
}
EXPORT_SYMBOL_GPL(intel_ipu4_cpd_free_pkg_dir);

u32 intel_ipu4_cpd_get_pg_icache_base(struct intel_ipu4_device *isp,
				      u8 idx,
				      const void *cpd_file,
				      unsigned cpd_file_size)
{
	const struct intel_ipu4_cpd_ent *metadata =
		intel_ipu4_cpd_get_metadata(cpd_file);

	return intel_ipu4_cpd_metadata_get_cmpnt_icache_base_offs(
		isp, cpd_file + metadata->offset, metadata->len, idx);
}
EXPORT_SYMBOL_GPL(intel_ipu4_cpd_get_pg_icache_base);

u32 intel_ipu4_cpd_get_pg_entry_point(struct intel_ipu4_device *isp,
				      u8 idx,
				      const void *cpd_file,
				      unsigned cpd_file_size)
{
	const struct intel_ipu4_cpd_ent *metadata =
		intel_ipu4_cpd_get_metadata(cpd_file);

	return intel_ipu4_cpd_metadata_get_cmpnt_entry_point(
		isp, cpd_file + metadata->offset, metadata->len, idx);
}
EXPORT_SYMBOL_GPL(intel_ipu4_cpd_get_pg_entry_point);

static int intel_ipu4_cpd_validate_cpd(struct intel_ipu4_device *isp,
				       const void *cpd,
				       unsigned long cpd_size,
				       unsigned long data_size)
{
	const struct intel_ipu4_cpd_hdr *cpd_hdr = cpd;
	struct intel_ipu4_cpd_ent *ent;
	unsigned int i;

	/* Ensure cpd hdr is within moduledata */
	if (cpd_size < sizeof(*cpd_hdr)) {
		dev_err(&isp->pdev->dev, "Invalid CPD moduledata size\n");
		return -EINVAL;
	}

	/* Sanity check for CPD header */
	if ((cpd_size - sizeof(*cpd_hdr)) / sizeof(*ent) < cpd_hdr->ent_cnt) {
		dev_err(&isp->pdev->dev, "Invalid CPD header\n");
		return -EINVAL;
	}

	/* Ensure that all entries are within moduledata */
	ent = (struct intel_ipu4_cpd_ent *)(cpd_hdr + 1);
	for (i = 0; i < cpd_hdr->ent_cnt; i++, ent++) {
		if (data_size < ent->offset ||
		    data_size - ent->offset < ent->len) {
			dev_err(&isp->pdev->dev, "Invalid CPD entry (%d)\n", i);
			return -EINVAL;
		}
	}

	return 0;
}

static int intel_ipu4_cpd_validate_moduledata(struct intel_ipu4_device *isp,
				     const void *moduledata,
				     u32 moduledata_size)
{
	const struct intel_ipu4_cpd_module_data_hdr *mod_hdr = moduledata;
	int rval;

	/* Ensure moduledata hdr is within moduledata */
	if (moduledata_size < sizeof(*mod_hdr) ||
	    moduledata_size < mod_hdr->hdr_len) {
		dev_err(&isp->pdev->dev, "Invalid moduledata size\n");
		return -EINVAL;
	}

	if (IA_CSS_FW_PKG_RELEASE != mod_hdr->fw_pkg_date) {
		dev_err(&isp->pdev->dev,
			"Moduledata and library version mismatch (%x != %x)\n",
			mod_hdr->fw_pkg_date, IA_CSS_FW_PKG_RELEASE);
		return -EINVAL;
	}

	dev_info(&isp->pdev->dev, "CSS release: %x\n", IA_CSS_FW_PKG_RELEASE);

	rval = intel_ipu4_cpd_validate_cpd(isp, moduledata +
					   mod_hdr->hdr_len,
					   moduledata_size -
					   mod_hdr->hdr_len,
					   moduledata_size);
	if (rval) {
		dev_err(&isp->pdev->dev, "Invalid CPD in moduledata\n");
		return -EINVAL;
	}

	return 0;
}

static int intel_ipu4_cpd_validate_metadata(struct intel_ipu4_device *isp,
				     const void *metadata,
				     u32 metadata_size)
{
	const struct intel_ipu_cpd_metadata_extn *extn = metadata;

	/* Sanity check for metadata size */
	if (metadata_size < sizeof(*extn) ||
	    metadata_size > MAX_METADATA_SIZE) {
		dev_err(&isp->pdev->dev, "%s: Invalid metadata\n", __func__);
		return -EINVAL;
	}

	/* Validate extension and image types */
	if (extn->extn_type != INTEL_IPU4_CPD_METADATA_EXTN_TYPE_IUNIT ||
	    extn->img_type !=
	    INTEL_IPU4_CPD_METADATA_IMAGE_TYPE_MAIN_FIRMWARE) {
		dev_err(&isp->pdev->dev, "Invalid metadata descriptor img_type (%d)\n",
			extn->img_type);
		return -EINVAL;
	}

	/* Validate metadata size multiple of metadata components */
	if ((metadata_size - sizeof(*extn)) %
	    sizeof(struct intel_ipu4_cpd_metadata_cmpnt)) {
		dev_err(&isp->pdev->dev, "%s: Invalid metadata size\n",
			__func__);
		return -EINVAL;
	}

	return 0;
}

int intel_ipu4_cpd_validate_cpd_file(struct intel_ipu4_device *isp,
				     const void *cpd_file,
				     unsigned long cpd_file_size)
{
	const struct intel_ipu4_cpd_hdr *hdr = cpd_file;
	struct intel_ipu4_cpd_ent *ent;
	int rval;

	if (is_intel_ipu5_hw_a0(isp)) {
		dev_info(&isp->pdev->dev, "Now do not validata cpd file for ipu5 due to cpd not ready\n");
		return 0;
	}
	rval = intel_ipu4_cpd_validate_cpd(isp, cpd_file,
					   cpd_file_size,
					   cpd_file_size);
	if (rval) {
		dev_err(&isp->pdev->dev, "Invalid CPD in file\n");
		return -EINVAL;
	}

	/* Check for CPD file marker */
	if (hdr->hdr_mark != CPD_HDR_MARK) {
		dev_err(&isp->pdev->dev, "Invalid CPD header\n");
		return -EINVAL;
	}

	/* Sanity check for manifest size */
	ent = intel_ipu4_cpd_get_manifest(cpd_file);
	if (ent->len > MAX_MANIFEST_SIZE) {
		dev_err(&isp->pdev->dev, "Invalid manifest size\n");
		return -EINVAL;
	}

	/* Validate metadata */
	ent = intel_ipu4_cpd_get_metadata(cpd_file);
	rval = intel_ipu4_cpd_validate_metadata(isp, cpd_file + ent->offset,
						ent->len);
	if (rval) {
		dev_err(&isp->pdev->dev, "Invalid metadata\n");
		return rval;
	}

	/* Validate moduledata */
	ent = intel_ipu4_cpd_get_moduledata(cpd_file);
	rval = intel_ipu4_cpd_validate_moduledata(isp, cpd_file + ent->offset,
						ent->len);
	if (rval) {
		dev_err(&isp->pdev->dev, "Invalid moduledata\n");
		return rval;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(intel_ipu4_cpd_validate_cpd_file);

unsigned int intel_ipu4_cpd_pkg_dir_get_address(
	const u64 *pkg_dir, int pkg_dir_idx)
{
	return pkg_dir[++pkg_dir_idx * PKG_DIR_ENT_LEN];
}
EXPORT_SYMBOL_GPL(intel_ipu4_cpd_pkg_dir_get_address);

unsigned int intel_ipu4_cpd_pkg_dir_get_num_entries(
	const u64 *pkg_dir)
{
	return pkg_dir[1];
}
EXPORT_SYMBOL_GPL(intel_ipu4_cpd_pkg_dir_get_num_entries);

unsigned int intel_ipu4_cpd_pkg_dir_get_size(
	const u64 *pkg_dir, int pkg_dir_idx)
{
	return pkg_dir[++pkg_dir_idx * PKG_DIR_ENT_LEN + 1] & PKG_DIR_SIZE_MASK;
}
EXPORT_SYMBOL_GPL(intel_ipu4_cpd_pkg_dir_get_size);

unsigned int intel_ipu4_cpd_pkg_dir_get_type(
	const u64 *pkg_dir, int pkg_dir_idx)
{
	return pkg_dir[++pkg_dir_idx * PKG_DIR_ENT_LEN + 1] >>
		PKG_DIR_ID_SHIFT & PKG_DIR_ID_MASK;
}
EXPORT_SYMBOL_GPL(intel_ipu4_cpd_pkg_dir_get_type);

