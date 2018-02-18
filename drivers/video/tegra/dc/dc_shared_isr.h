/*
 * drivers/video/tegra/dc/dc_shared_isr.c
 *
 * Copyright (c) 2016, NVIDIA CORPORATION, All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __DRIVERS_VIDEO_TEGRA_DC_DC_SHARED_ISR_H
#define __DRIVERS_VIDEO_TEGRA_DC_DC_SHARED_ISR_H

/* define to share the Tegra display IRQ with other kernel module */
#define  TEGRA_DC_USR_SHARED_IRQ

#define  TEGRA_DC_HEAD_STATUS_MAGIC1  0x12348765
struct tegra_dc_head_status  {
	int   magic;      /* magic number to match */
	int   irqnum;     /* IRQ line number */
	bool  init;       /* display initialized */
	bool  connected;  /* display is connected */
	bool  active;     /* display is active */
	char  dummy[53];  /* dummy padding */
};

int  tegra_dc_get_numof_dispheads(void);
int  tegra_dc_get_disphead_sts(int dcid, struct tegra_dc_head_status *pSts);
int  tegra_dc_register_isr_usr_cb(int dcid,
	int (*usr_isr)(int dcid, unsigned long irq_sts, void *usr_isr_pdt),
	void *usr_isr_pdt);
int  tegra_dc_unregister_isr_usr_cb(int dcid,
	int (*usr_isr)(int dcid, unsigned long irq_sts, void *usr_isr_pdt),
	void *usr_isr_pdt);

#endif
