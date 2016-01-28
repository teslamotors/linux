/*
 * Copyright (c) 2014 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include "libcsspsys2600.h"

/* PSYS library symbols */
EXPORT_SYMBOL_GPL(ia_css_data_terminal_get_frame);
EXPORT_SYMBOL_GPL(ia_css_data_terminal_set_connection_type);
EXPORT_SYMBOL_GPL(ia_css_frame_set_data_bytes);
EXPORT_SYMBOL_GPL(ia_css_process_group_abort);
EXPORT_SYMBOL_GPL(ia_css_process_group_attach_buffer);
EXPORT_SYMBOL_GPL(ia_css_process_group_create);
EXPORT_SYMBOL_GPL(ia_css_process_group_destroy);
EXPORT_SYMBOL_GPL(ia_css_process_group_detach_buffer);
EXPORT_SYMBOL_GPL(ia_css_process_group_get_process);
EXPORT_SYMBOL_GPL(ia_css_process_group_get_process_count);
EXPORT_SYMBOL_GPL(ia_css_process_group_get_resource_bitmap);
EXPORT_SYMBOL_GPL(ia_css_process_group_get_terminal);
EXPORT_SYMBOL_GPL(ia_css_process_group_get_terminal_count);
EXPORT_SYMBOL_GPL(ia_css_process_group_get_size);
EXPORT_SYMBOL_GPL(ia_css_process_group_reset);
EXPORT_SYMBOL_GPL(ia_css_process_group_resume);
EXPORT_SYMBOL_GPL(ia_css_process_group_start);
EXPORT_SYMBOL_GPL(ia_css_process_group_submit);
EXPORT_SYMBOL_GPL(ia_css_process_group_suspend);
EXPORT_SYMBOL_GPL(ia_css_process_group_set_ipu_vaddress);
EXPORT_SYMBOL_GPL(ia_css_process_group_set_resource_bitmap);
EXPORT_SYMBOL_GPL(ia_css_process_group_set_token);
EXPORT_SYMBOL_GPL(ia_css_program_group_manifest_get_program_count);
EXPORT_SYMBOL_GPL(ia_css_program_group_manifest_get_program_manifest);
EXPORT_SYMBOL_GPL(ia_css_process_get_program_ID);
EXPORT_SYMBOL_GPL(ia_css_process_get_cell);
EXPORT_SYMBOL_GPL(ia_css_process_set_cell);
EXPORT_SYMBOL_GPL(ia_css_process_set_dev_chn);
EXPORT_SYMBOL_GPL(ia_css_process_clear_cell);
EXPORT_SYMBOL_GPL(ia_css_program_manifest_get_cell_type_ID);
EXPORT_SYMBOL_GPL(ia_css_program_manifest_get_dev_chn_size);
EXPORT_SYMBOL_GPL(ia_css_has_program_manifest_fixed_cell);
EXPORT_SYMBOL_GPL(ia_css_program_manifest_get_program_ID);
EXPORT_SYMBOL_GPL(ia_css_psys_close);
EXPORT_SYMBOL_GPL(ia_css_psys_event_queue_receive);
EXPORT_SYMBOL_GPL(ia_css_psys_open);
EXPORT_SYMBOL_GPL(ia_css_sizeof_process_group);
EXPORT_SYMBOL_GPL(ia_css_sizeof_psys);
EXPORT_SYMBOL_GPL(ia_css_terminal_get_type);
EXPORT_SYMBOL_GPL(ia_css_process_group_stop);
EXPORT_SYMBOL_GPL(ia_css_pkg_dir_get_num_entries);
EXPORT_SYMBOL_GPL(ia_css_pkg_dir_entry_get_size);
EXPORT_SYMBOL_GPL(ia_css_pkg_dir_entry_get_address_lo);
EXPORT_SYMBOL_GPL(ia_css_pkg_dir_entry_get_type);
EXPORT_SYMBOL_GPL(ia_css_pkg_dir_verify_header);
EXPORT_SYMBOL_GPL(ia_css_pkg_dir_get_entry);
EXPORT_SYMBOL_GPL(ia_css_client_pkg_get_pg_manifest_offset_size);
EXPORT_SYMBOL_GPL(ia_css_psys_specify);
EXPORT_SYMBOL_GPL(vied_nci_cell_get_type);
EXPORT_SYMBOL_GPL(psys_syscom);
/*Implement SMIF/Vied subsystem access here */

unsigned long long _hrt_master_port_subsystem_base_address;

/* end of platform stubs */

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Intel intel_ipu4 psys library");
