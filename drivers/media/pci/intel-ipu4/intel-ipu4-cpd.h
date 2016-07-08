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

#ifndef INTEL_IPU4_CPD_H
#define INTEL_IPU4_CPD_H

#define INTEL_IPU4_CPD_SIZE_OF_FW_ARCH_VERSION		7
#define INTEL_IPU4_CPD_SIZE_OF_SYSTEM_VERSION		11
#define INTEL_IPU4_CPD_SIZE_OF_COMPONENT_NAME		12

#define INTEL_IPU4_CPD_METADATA_EXTN_TYPE_IUNIT	0x10

#define INTEL_IPU4_CPD_METADATA_IMAGE_TYPE_RESERVED		0
#define INTEL_IPU4_CPD_METADATA_IMAGE_TYPE_BOOTLOADER		1
#define INTEL_IPU4_CPD_METADATA_IMAGE_TYPE_MAIN_FIRMWARE	2

#define INTEL_IPU4_CPD_PKG_DIR_PSYS_SERVER_IDX	0
#define INTEL_IPU4_CPD_PKG_DIR_ISYS_SERVER_IDX	1

#define INTEL_IPU4_CPD_PKG_DIR_CLIENT_PG_TYPE	3

struct __packed intel_ipu4_cpd_module_data_hdr {
	u32	hdr_len;
	u32	endian;
	u32	fw_pkg_date;
	u32	hive_sdk_date;
	u32	compiler_date;
	u32	target_platform_type;
	u8	sys_ver[INTEL_IPU4_CPD_SIZE_OF_SYSTEM_VERSION];
	u8	fw_arch_ver[INTEL_IPU4_CPD_SIZE_OF_FW_ARCH_VERSION];
	u8	rsvd[2];
};

struct __packed intel_ipu4_cpd_hdr {
	u32 hdr_mark;
	u32 ent_cnt;
	u8 hdr_ver;
	u8 ent_ver;
	u8 hdr_len;
	u8 chksm;
	u32 name;
};

struct __packed intel_ipu4_cpd_ent {
	u8 name[INTEL_IPU4_CPD_SIZE_OF_COMPONENT_NAME];
	u32 offset;
	u32 len;
	u8 rsvd[4];
};

struct __packed intel_ipu4_cpd_metadata_cmpnt {
	u32 id;
	u32 size;
	u32 ver;
	u8 sha2_hash[32];
	u32 entry_point;
	u32 icache_base_offs;
	u8 attrs[16];
};

struct __packed intel_ipu_cpd_metadata_extn {
	u32 extn_type;
	u32 len;
	u32 img_type;
	u8 rsvd[16];
};

struct __packed intel_ipu_cpd_client_pkg_hdr {
	u32 prog_list_offs;
	u32 prog_list_size;
	u32 prog_desc_offs;
	u32 prog_desc_size;
	u32 pg_manifest_offs;
	u32 pg_manifest_size;
	u32 prog_bin_offs;
	u32 prog_bin_size;
};

void *intel_ipu4_cpd_create_pkg_dir(struct intel_ipu4_bus_device *adev,
						 const void *src,
						 dma_addr_t dma_addr_src,
						 dma_addr_t *dma_addr,
						 unsigned *pkg_dir_size);
void intel_ipu4_cpd_free_pkg_dir(struct intel_ipu4_bus_device *adev,
					      u64 *pkg_dir,
					      dma_addr_t dma_addr,
					      unsigned pkg_dir_size);
u32 intel_ipu4_cpd_get_pg_icache_base(struct intel_ipu4_device *isp,
				      u8 idx,
				      const void *cpd_file,
				      unsigned cpd_file_size);
u32 intel_ipu4_cpd_get_pg_entry_point(struct intel_ipu4_device *isp,
				      u8 idx,
				      const void *cpd_file,
				      unsigned cpd_file_size);
int intel_ipu4_cpd_validate_cpd_file(struct intel_ipu4_device *isp,
				     const void *cpd_file,
				     unsigned long cpd_file_size);
unsigned int intel_ipu4_cpd_pkg_dir_get_address(const u64 *pkg_dir,
						int pkg_dir_idx);
unsigned int intel_ipu4_cpd_pkg_dir_get_num_entries(const u64 *pkg_dir);
unsigned int intel_ipu4_cpd_pkg_dir_get_size(const u64 *pkg_dir,
					     int pkg_dir_idx);
unsigned int intel_ipu4_cpd_pkg_dir_get_type(const u64 *pkg_dir,
					     int pkg_dir_idx);

#endif /* INTEL_IPU4_CPD_H */
