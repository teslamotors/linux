/**
* Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2016, Intel Corporation.
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

#ifndef __IA_CSS_ISYSAPI_PROXY_REGIONS_DEFS_H__
#define __IA_CSS_ISYSAPI_PROXY_REGIONS_DEFS_H__

#include "ia_css_isysapi_proxy_region_types.h"

/*
 * Definitions for BXT_B0_PROXY_INT
 */

#if defined(BXT_B0_PROXY_INT)

/**
 * enum bxt_b0_ia_css_proxy_write_region. Provides the list of regions for bxtB0 that
 * can be accessed (for writing purpose) through the proxy interface
 */
enum bxt_b0_ia_css_proxy_write_region {
	BXT_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_0_ERROR_FILL_RATE = 0,
	BXT_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_1_ERROR_FILL_RATE,
	BXT_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_2_ERROR_FILL_RATE,
	BXT_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_3_ERROR_FILL_RATE,
	BXT_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_4_ERROR_FILL_RATE,
	BXT_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_5_ERROR_FILL_RATE,
	BXT_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_6_ERROR_FILL_RATE,
	BXT_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_7_ERROR_FILL_RATE,
	BXT_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_8_ERROR_FILL_RATE,
	BXT_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_9_ERROR_FILL_RATE,
	BXT_B0_IA_CSS_PROXY_WRITE_REGION_GDA_IRQ_URGENT_THRESHOLD,
	BXT_B0_IA_CSS_PROXY_WRITE_REGION_GDA_IRQ_CRITICAL_THRESHOLD,
	N_BXT_B0_IA_CSS_PROXY_WRITE_REGION
};

struct ia_css_proxy_write_region_description bxt_b0_reg_write_desc[N_BXT_B0_IA_CSS_PROXY_WRITE_REGION] = {
	/* base_addr, 										offset */
	{0x64128,	/*input_system_csi2_logic_s2m_a_stream2mmio_err_mode_dc_ctrl_reg_id*/		4},	/*BXT_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_0_ERROR_FILL_RATE*/
	{0x65128,	/*input_system_csi2_logic_s2m_b_stream2mmio_err_mode_dc_ctrl_reg_id*/		4},	/*BXT_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_1_ERROR_FILL_RATE*/
	{0x66128,	/*input_system_csi2_logic_s2m_c_stream2mmio_err_mode_dc_ctrl_reg_id*/		4},	/*BXT_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_2_ERROR_FILL_RATE*/
	{0x67128,	/*input_system_csi2_logic_s2m_d_stream2mmio_err_mode_dc_ctrl_reg_id*/		4},	/*BXT_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_3_ERROR_FILL_RATE*/
	{0x6C128,	/*input_system_csi2_3ph_logic_s2m_a_stream2mmio_err_mode_dc_ctrl_reg_id*/	4},	/*BXT_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_4_ERROR_FILL_RATE*/
	{0x6C928,	/*input_system_csi2_3ph_logic_s2m_b_stream2mmio_err_mode_dc_ctrl_reg_id*/	4},	/*BXT_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_5_ERROR_FILL_RATE*/
	{0x6D128,	/*input_system_csi2_3ph_logic_s2m_0_stream2mmio_err_mode_dc_ctrl_reg_id*/	4},	/*BXT_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_6_ERROR_FILL_RATE*/
	{0x6D928,	/*input_system_csi2_3ph_logic_s2m_1_stream2mmio_err_mode_dc_ctrl_reg_id*/	4},	/*BXT_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_7_ERROR_FILL_RATE*/
	{0x6E128,	/*input_system_csi2_3ph_logic_s2m_2_stream2mmio_err_mode_dc_ctrl_reg_id*/	4},	/*BXT_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_8_ERROR_FILL_RATE*/
	{0x6E928,	/*input_system_csi2_3ph_logic_s2m_3_stream2mmio_err_mode_dc_ctrl_reg_id*/	4},	/*BXT_B0_IA_CSS_PROXY_WRITE_REGION_STR2MMIO_MIPI_9_ERROR_FILL_RATE*/
	{0x7800C,	/*input_system_unis_logic_gda_irq_urgent_threshold*/				4},	/*BXT_B0_IA_CSS_PROXY_WRITE_REGION_GDA_IRQ_URGENT_THRESHOLD*/
	{0x78010,	/*input_system_unis_logic_gda_irq_critical_threshold*/				4}	/*BXT_B0_IA_CSS_PROXY_WRITE_REGION_GDA_IRQ_CRITICAL_THRESHOLD*/
};

#endif /*defined(BXT_B0_PROXY_INT)*/

/*
 * Definitions for CNL_A0_PROXY_INT
 */

#if defined(CNL_A0_PROXY_INT)

/**
 * enum cnl_a0_ia_css_proxy_write_region. Provides the list of regions for cnlA0 that
 * can be accessed (for writing purpose) through the proxy interface
 */
enum cnl_a0_ia_css_proxy_write_region {
	N_CNL_A0_IA_CSS_PROXY_WRITE_REGION
};

#define CNL_A0_NO_PROXY_WRITE_REGION_AVAILABLE

#ifndef CNL_A0_NO_PROXY_WRITE_REGION_AVAILABLE
struct ia_css_proxy_write_region_description cnl_a0_reg_write_desc[N_CNL_A0_IA_CSS_PROXY_WRITE_REGION] = {
}
#endif /*CNL_A0_NO_PROXY_WRITE_REGION_AVAILABLE*/

#endif /*defined(CNL_A0_PROXY_INT)*/

/*
 * Definitions for CNL_B0_PROXY_INT
 */

#if defined(CNL_B0_PROXY_INT)

/**
 * enum cnl_b0_ia_css_proxy_write_region. Provides the list of regions for cnlB0 that
 * can be accessed (for writing purpose) through the proxy interface
 */
enum cnl_b0_ia_css_proxy_write_region {
	N_CNL_B0_IA_CSS_PROXY_WRITE_REGION
};

#define CNL_B0_NO_PROXY_WRITE_REGION_AVAILABLE

#ifndef CNL_B0_NO_PROXY_WRITE_REGION_AVAILABLE
struct ia_css_proxy_write_region_description cnl_b0_reg_write_desc[N_CNL_B0_IA_CSS_PROXY_WRITE_REGION] = {
}
#endif /*CNL_A0_NO_PROXY_WRITE_REGION_AVAILABLE*/

#endif /*defined(CNL_B0_PROXY_INT)*/

/*
 * Definitions for GLV_A0_PROXY_INT
 */

#if defined(GLV_A0_PROXY_INT)

/**
 * enum glv_a0_ia_css_proxy_write_region. Provides the list of regions for glvA0 that
 * can be accessed (for writing purpose) through the proxy interface
 */
enum glv_a0_ia_css_proxy_write_region {
	N_GLV_A0_IA_CSS_PROXY_WRITE_REGION
};

#define GLV_A0_NO_PROXY_WRITE_REGION_AVAILABLE

#ifndef GLV_A0_NO_PROXY_WRITE_REGION_AVAILABLE
struct ia_css_proxy_write_region_description glv_a0_reg_write_desc[N_GLV_A0_IA_CSS_PROXY_WRITE_REGION] = {
}
#endif /*GLV_A0_NO_PROXY_WRITE_REGION_AVAILABLE*/

#endif /*defined(GLV_A0_PROXY_INT)*/

#endif /*__IA_CSS_ISYSAPI_PROXY_REGIONS_DEFS_H__*/
