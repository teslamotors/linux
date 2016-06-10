/*
 * Copyright (c) 2016 Intel Corporation.
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
#ifndef INTEL_IPU4_PSYS_ABI_H
#define INTEL_IPU4_PSYS_ABI_H

/* ia_css_psys_process_group_cmd_impl.h */
#define	N_PADDING_UINT8_IN_PROCESS_STRUCT		8
#define	N_PADDING_UINT8_IN_PROCESS_GROUP_STRUCT		7

/* ia_css_terminal_base_types.h */
#define N_PADDING_UINT8_IN_TERMINAL_STRUCT		5

/* ia_css_terminal_types.h */
#define N_PADDING_UINT8_IN_PARAM_PAYLOAD_STRUCT		4
#define N_PADDING_UINT8_IN_PARAM_TERMINAL_STRUCT	6

/* ia_css_psys_terminal.c */
#define	N_PADDING_UINT8_IN_DATA_TERMINAL_STRUCT		4

/* ia_css_program_group_data.h */
#define	N_PADDING_UINT8_IN_FRAME_DESC_STRUCT		3
#define IA_CSS_N_FRAME_PLANES				6

/* ia_css_psys_process_group_cmd_impl.h */
struct __packed ia_css_process_group {
	u64 token;
	u64 private_token;
	u32 size;
	u32 pg_load_start_ts;
	u32 pg_load_cycles;
	u32 pg_init_cycles;
	u32 pg_processing_cycles;
	u32 ID;
	u32 state;
	u32 ipu_virtual_address;
	u32 resource_bitmap;
	u16 fragment_count;
	u16 fragment_state;
	u16 fragment_limit;
	u16 processes_offset;
	u16 terminals_offset;
	u8 process_count;
	u8 terminal_count;
	u8 subgraph_count;
	u8 padding[N_PADDING_UINT8_IN_PROCESS_GROUP_STRUCT];
};

struct ia_css_process {
	u64 kernel_bitmap;
	u32 size;
	u32 ID;
	u32 state;
	s16 parent_offset;
	u16 cell_dependencies_offset;
	u16 terminal_dependencies_offset;
	u16 int_mem_offset[VIED_NCI_N_MEM_TYPE_ID];
	u16 ext_mem_offset[VIED_NCI_N_DATA_MEM_TYPE_ID];
	u16 dev_chn_offset[VIED_NCI_N_DEV_CHN_ID];
	u8 cell_id;
	u8 int_mem_id[VIED_NCI_N_MEM_TYPE_ID];
	u8 ext_mem_id[VIED_NCI_N_DATA_MEM_TYPE_ID];
	u8 cell_dependency_count;
	u8 terminal_dependency_count;
	u8 padding[N_PADDING_UINT8_IN_PROCESS_STRUCT];
};

/* ia_css_psys_init.h */
struct ia_css_psys_srv_init {
	void *host_ddr_pkg_dir;
	dma_addr_t ddr_pkg_dir_address;
	u32 pkg_dir_size;

	u32 icache_prefetch_sp;
	u32 icache_prefetch_isp;
};

/* ia_css_psys_transport.h */
struct __packed ia_css_psys_cmd {
	u16 command;
	u16 msg;
	u32 process_group;
};


struct __packed ia_css_psys_event {
	u16 status;
	u16 command;
	u32 process_group;
	u64 token;
};

/* ia_css_program_group_param_types.h */
struct ia_css_terminal {
	u32 terminal_type;
	s16 parent_offset;
	u16 size;
	u16 tm_index;
	u8 ID;
	u8 padding[N_PADDING_UINT8_IN_TERMINAL_STRUCT];
};

/* ia_css_terminal_types.h */
struct ia_css_param_payload {
	u64 host_buffer;
	u32 buffer;
	u8 padding[N_PADDING_UINT8_IN_PARAM_PAYLOAD_STRUCT];
};

struct ia_css_param_terminal {
	struct ia_css_terminal base;
	struct ia_css_param_payload param_payload;
	u16 param_section_desc_offset;
	u8 padding[N_PADDING_UINT8_IN_PARAM_TERMINAL_STRUCT];
};

struct ia_css_frame {
	u32 buffer_state;
	u32 access_type;
	u32 pointer_state;
	u32 access_scope;
	u32 data;
	u32 data_bytes;
};

/* ia_css_program_group_data.h */
struct ia_css_frame_descriptor {
	u32 frame_format_type;
	u32 plane_count;
	u32 plane_offsets[IA_CSS_N_FRAME_PLANES];
	u32 stride[1];
	u16 dimension[2];
	u16 size;
	u8 bpp;
	u8 bpe;
	u8 is_compressed;
	u8 padding[N_PADDING_UINT8_IN_FRAME_DESC_STRUCT];
};

struct ia_css_stream {
	u64 dummy;
};

/* ia_css_psys_terminal.c */
struct ia_css_data_terminal {
	struct ia_css_terminal base;
	struct ia_css_frame_descriptor frame_descriptor;
	struct ia_css_frame frame;
	struct ia_css_stream stream;
	u32 frame_format_type;
	u32 connection_type;
	u16 fragment_descriptor_offset;
	u8 kernel_id;
	u8 subgraph_id;
	u8 padding[N_PADDING_UINT8_IN_DATA_TERMINAL_STRUCT];
};

struct intel_ipu4_psys_kcmd;
struct intel_ipu4_psys;

struct intel_ipu4_psys_abi {
	int (*pg_start)(struct intel_ipu4_psys_kcmd *kcmd);
	int (*pg_disown)(struct intel_ipu4_psys_kcmd *kcmd);
	int (*pg_submit)(struct intel_ipu4_psys_kcmd *kcmd);
	int (*pg_abort)(struct intel_ipu4_psys_kcmd *kcmd);
	struct intel_ipu4_psys_kcmd *(*pg_rcv)(
		struct intel_ipu4_psys *psys,
		u32 *status);
	int (*terminal_set)(struct ia_css_terminal *terminal,
			    int terminal_idx,
			    struct intel_ipu4_psys_kcmd *kcmd,
			    u32 buffer,
			    unsigned size);
	void (*pg_dump)(struct intel_ipu4_psys *psys,
			 struct intel_ipu4_psys_kcmd *kcmd,
			 const char *note);
	int (*pg_get_id)(struct intel_ipu4_psys_kcmd *kcmd);
	int (*pg_get_terminal_count)(struct intel_ipu4_psys_kcmd *kcmd);
	int (*pg_get_size)(struct intel_ipu4_psys_kcmd *kcmd);
	int (*pg_set_ipu_vaddress)(struct intel_ipu4_psys_kcmd *kcmd,
				   dma_addr_t vaddress);
	void (*pg_set_token)(struct intel_ipu4_psys_kcmd *kcmd,
					    u64 token);
	void *(*pg_get_terminal)(
		struct intel_ipu4_psys_kcmd *kcmd,
		int index);
	int (*open)(struct intel_ipu4_psys *psys);
	int (*close)(struct intel_ipu4_psys *psys);
};

void intel_ipu4_psys_abi_init_ext(struct intel_ipu4_psys_abi *abi);
void intel_ipu4_psys_abi_cleanup_ext(struct intel_ipu4_psys_abi *abi);
int intel_ipu4_psys_abi_pg_start(struct intel_ipu4_psys_kcmd *kcmd);
int intel_ipu4_psys_abi_pg_disown(struct intel_ipu4_psys_kcmd *kcmd);
int intel_ipu4_psys_abi_pg_abort(struct intel_ipu4_psys_kcmd *kcmd);
int intel_ipu4_psys_abi_pg_submit(struct intel_ipu4_psys_kcmd *kcmd);
struct intel_ipu4_psys_kcmd *intel_ipu4_psys_abi_rcv_kcmd(
	struct intel_ipu4_psys *psys,
	u32 *status);
int intel_ipu4_psys_abi_terminal_set(struct ia_css_terminal *terminal,
				     int terminal_idx,
				     struct intel_ipu4_psys_kcmd *kcmd,
				     u32 buffer,
				     unsigned size);
void intel_ipu4_psys_abi_pg_dump(struct intel_ipu4_psys *psys,
					struct intel_ipu4_psys_kcmd *kcmd,
					const char *note);
int intel_ipu4_psys_abi_pg_get_id(struct intel_ipu4_psys_kcmd *kcmd);
int intel_ipu4_psys_abi_pg_get_terminal_count(
	struct intel_ipu4_psys_kcmd *kcmd);
int intel_ipu4_psys_abi_pg_get_size(struct intel_ipu4_psys_kcmd *kcmd);
int intel_ipu4_psys_abi_pg_set_ipu_vaddress(struct intel_ipu4_psys_kcmd *kcmd,
					    dma_addr_t vaddress);
struct ia_css_terminal *intel_ipu4_psys_abi_pg_get_terminal(
	struct intel_ipu4_psys_kcmd *kcmd,
	int index);
void intel_ipu4_psys_abi_pg_set_token(struct intel_ipu4_psys_kcmd *kcmd,
				      u64 token);
int intel_ipu4_psys_abi_open(struct intel_ipu4_psys *psys);
int intel_ipu4_psys_abi_close(struct intel_ipu4_psys *psys);

#endif /* INTEL_IPU4_PSYS_ABI_H */
