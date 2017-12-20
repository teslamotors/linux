/*
 * drivers/video/tegra/host/t20/syncpt_t20.h
 *
 * Tegra Graphics Host Syncpoints for T20
 *
 * Copyright (c) 2010-2011, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __NVHOST_SYNCPT_T20_H
#define __NVHOST_SYNCPT_T20_H

#define NVSYNCPT_DISP0_A		     (8)
#define NVSYNCPT_DISP1_A		     (9)
#define NVSYNCPT_AVP_0			     (10)
#define NVSYNCPT_CSI_VI_0		     (11)
#define NVSYNCPT_CSI_VI_1		     (12)
#define NVSYNCPT_VI_ISP_0		     (13)
#define NVSYNCPT_VI_ISP_1		     (14)
#define NVSYNCPT_VI_ISP_2		     (15)
#define NVSYNCPT_VI_ISP_3		     (16)
#define NVSYNCPT_VI_ISP_4		     (17)
#define NVSYNCPT_2D_0			     (18)
#define NVSYNCPT_2D_1			     (19)
#define NVSYNCPT_DISP0_B		     (20)
#define NVSYNCPT_DISP1_B		     (21)
#define NVSYNCPT_3D			     (22)
#define NVSYNCPT_MPE			     (23)
#define NVSYNCPT_DISP0_C		     (24)
#define NVSYNCPT_DISP1_C		     (25)
#define NVSYNCPT_VBLANK0		     (26)
#define NVSYNCPT_VBLANK1		     (27)
#define NVSYNCPT_MPE_EBM_EOF		     (28)
#define NVSYNCPT_MPE_WR_SAFE		     (29)
#define NVSYNCPT_DSI			     (31)


/*#define NVSYNCPT_2D_CHANNEL2_0    (20) */
/*#define NVSYNCPT_2D_CHANNEL2_1    (21) */
/*#define NVSYNCPT_2D_TINYBLT_WAR		     (30)*/
/*#define NVSYNCPT_2D_TINYBLT_RESTORE_CLASS_ID (30)*/

/* sync points that are wholly managed by the client */
#define NVSYNCPTS_CLIENT_MANAGED ( \
	BIT(NVSYNCPT_DISP0_A) | BIT(NVSYNCPT_DISP1_A) | \
	BIT(NVSYNCPT_DISP0_B) | BIT(NVSYNCPT_DISP1_B) | \
	BIT(NVSYNCPT_DISP0_C) | BIT(NVSYNCPT_DISP1_C) | \
	BIT(NVSYNCPT_DSI) | \
	BIT(NVSYNCPT_VBLANK0) | BIT(NVSYNCPT_VBLANK1) | \
	BIT(NVSYNCPT_CSI_VI_0) | BIT(NVSYNCPT_CSI_VI_1) | \
	BIT(NVSYNCPT_VI_ISP_1) | BIT(NVSYNCPT_VI_ISP_2) | \
	BIT(NVSYNCPT_VI_ISP_3) | BIT(NVSYNCPT_VI_ISP_4) | \
	BIT(NVSYNCPT_MPE_EBM_EOF) | BIT(NVSYNCPT_MPE_WR_SAFE) | \
	BIT(NVSYNCPT_2D_1) | BIT(NVSYNCPT_AVP_0))


#define NVWAITBASE_2D_0 (1)
#define NVWAITBASE_2D_1 (2)
#define NVWAITBASE_3D   (3)
#define NVWAITBASE_MPE  (4)

struct nvhost_master;
int nvhost_t20_init_syncpt(struct nvhost_master *host);

#endif /* __NVHOST_SYNCPT_T20_H */
