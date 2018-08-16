/**
* Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2018, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
*/

#ifndef __IA_CSS_ISYSAPI_PROXY_REGION_DEFS_H
#define __IA_CSS_ISYSAPI_PROXY_REGION_DEFS_H

#include "ia_css_isysapi_proxy_region_types.h"

/*
 * Definitions for IPU4_B0_PROXY_INT
 */

#if defined(IPU4_B0_PROXY_INT)

/**
 * enum ipu4_b0_ia_css_proxy_write_region. Provides the list of regions for ipu4B0 that
 * can be accessed (for writing purpose) through the proxy interface
 */
enum ipu4_b0_ia_css_proxy_write_region {
	IPU4_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_0_ERROR_FILL_RATE = 0,
	IPU4_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_1_ERROR_FILL_RATE,
	IPU4_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_2_ERROR_FILL_RATE,
	IPU4_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_3_ERROR_FILL_RATE,
	IPU4_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_4_ERROR_FILL_RATE,
	IPU4_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_5_ERROR_FILL_RATE,
	IPU4_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_6_ERROR_FILL_RATE,
	IPU4_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_7_ERROR_FILL_RATE,
	IPU4_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_8_ERROR_FILL_RATE,
	IPU4_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_9_ERROR_FILL_RATE,
	IPU4_B0_IA_CSS_PROXY_WRITE_REGION_GDA_IRQ_URGENT_THRESHOLD,
	IPU4_B0_IA_CSS_PROXY_WRITE_REGION_GDA_IRQ_CRITICAL_THRESHOLD,
	N_IPU4_B0_IA_CSS_PROXY_WRITE_REGION
};

struct ia_css_proxy_write_region_description ipu4_b0_reg_write_desc[N_IPU4_B0_IA_CSS_PROXY_WRITE_REGION] = {
	/* base_addr, 										offset */
	{0x64128,	/*input_system_csi2_logic_s2m_a_stream2mmio_err_mode_dc_ctrl_reg_id*/		4},	/*IPU4_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_0_ERROR_FILL_RATE*/
	{0x65128,	/*input_system_csi2_logic_s2m_b_stream2mmio_err_mode_dc_ctrl_reg_id*/		4},	/*IPU4_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_1_ERROR_FILL_RATE*/
	{0x66128,	/*input_system_csi2_logic_s2m_c_stream2mmio_err_mode_dc_ctrl_reg_id*/		4},	/*IPU4_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_2_ERROR_FILL_RATE*/
	{0x67128,	/*input_system_csi2_logic_s2m_d_stream2mmio_err_mode_dc_ctrl_reg_id*/		4},	/*IPU4_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_3_ERROR_FILL_RATE*/
	{0x6C128,	/*input_system_csi2_3ph_logic_s2m_a_stream2mmio_err_mode_dc_ctrl_reg_id*/	4},	/*IPU4_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_4_ERROR_FILL_RATE*/
	{0x6C928,	/*input_system_csi2_3ph_logic_s2m_b_stream2mmio_err_mode_dc_ctrl_reg_id*/	4},	/*IPU4_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_5_ERROR_FILL_RATE*/
	{0x6D128,	/*input_system_csi2_3ph_logic_s2m_0_stream2mmio_err_mode_dc_ctrl_reg_id*/	4},	/*IPU4_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_6_ERROR_FILL_RATE*/
	{0x6D928,	/*input_system_csi2_3ph_logic_s2m_1_stream2mmio_err_mode_dc_ctrl_reg_id*/	4},	/*IPU4_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_7_ERROR_FILL_RATE*/
	{0x6E128,	/*input_system_csi2_3ph_logic_s2m_2_stream2mmio_err_mode_dc_ctrl_reg_id*/	4},	/*IPU4_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_8_ERROR_FILL_RATE*/
	{0x6E928,	/*input_system_csi2_3ph_logic_s2m_3_stream2mmio_err_mode_dc_ctrl_reg_id*/	4},	/*IPU4_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_9_ERROR_FILL_RATE*/
	{0x7800C,	/*input_system_unis_logic_gda_irq_urgent_threshold*/				4},	/*IPU4_B0_IA_CSS_PROXY_WRITE_REGION_GDA_IRQ_URGENT_THRESHOLD*/
	{0x78010,	/*input_system_unis_logic_gda_irq_critical_threshold*/				4}	/*IPU4_B0_IA_CSS_PROXY_WRITE_REGION_GDA_IRQ_CRITICAL_THRESHOLD*/
};

#endif /*defined(IPU4_B0_PROXY_INT)*/

/*
 * Definitions for IPU4P_A0_PROXY_INT
 */

#if defined(IPU4P_A0_PROXY_INT)

/**
 * enum ipu4p_a0_ia_css_proxy_write_region. Provides the list of regions for ipu4pA0 that
 * can be accessed (for writing purpose) through the proxy interface
 */
enum ipu4p_a0_ia_css_proxy_write_region {
	N_IPU4P_A0_IA_CSS_PROXY_WRITE_REGION
};

#define IPU4P_A0_NO_PROXY_WRITE_REGION_AVAILABLE

#ifndef IPU4P_A0_NO_PROXY_WRITE_REGION_AVAILABLE
struct ia_css_proxy_write_region_description ipu4p_a0_reg_write_desc[N_IPU4P_A0_IA_CSS_PROXY_WRITE_REGION] = {
}
#endif /*IPU4P_A0_NO_PROXY_WRITE_REGION_AVAILABLE*/

#endif /*defined(IPU4P_A0_PROXY_INT)*/

/*
 * Definitions for IPU4P_B0_PROXY_INT
 */

#if defined(IPU4P_B0_PROXY_INT)

/**
 * enum ipu4p_b0_ia_css_proxy_write_region. Provides the list of regions for ipu4pB0 that
 * can be accessed (for writing purpose) through the proxy interface
 */
enum ipu4p_b0_ia_css_proxy_write_region {
	IPU4P_B0_IA_CSS_PROXY_WRITE_REGION_GDA_IWAKE_THRESHOLD = 0,
	IPU4P_B0_IA_CSS_PROXY_WRITE_REGION_GDA_ENABLE_IWAKE,
	N_IPU4P_B0_IA_CSS_PROXY_WRITE_REGION
};

struct ia_css_proxy_write_region_description ipu4p_b0_reg_write_desc[N_IPU4P_B0_IA_CSS_PROXY_WRITE_REGION] = {
	/* base_addr, max_offset */
		/*input_system_unis_logic_gda_iwake_threshold*/
	{0x78014, 4}, /*IPU4P_B0_IA_CSS_PROXY_WRITE_REGION_GDA_IWAKE_THRESHOLD*/
		/*input_system_unis_logic_gda_enable_iwake*/
	{0x7801C, 4} /*IPU4P_B0_IA_CSS_PROXY_WRITE_REGION_GDA_ENABLE_IWAKE*/
};

#endif /*defined(IPU4P_B0_PROXY_INT)*/


#endif /* __IA_CSS_ISYSAPI_PROXY_REGION_DEFS_H */
