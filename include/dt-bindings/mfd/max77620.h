/*
 * This header provides macros for MAXIM MAX77620 device bindings.
 *
 * Copyright (c) 2016, NVIDIA Corporation.
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 */

#ifndef _DT_BINDINGS_MFD_MAX77620_H
#define _DT_BINDINGS_MFD_MAX77620_H

/* FPS event source */
#define MAX77620_FPS_EVENT_SRC_EN0		0
#define MAX77620_FPS_EVENT_SRC_EN1		1
#define MAX77620_FPS_EVENT_SRC_SW		2
#define MAX77620_FPS_EVENT_SRC_RSVD	3

/* PMIC state when FPS inactive state */
#define MAX77620_FPS_INACTIVE_STATE_SLEEP	0
#define MAX77620_FPS_INACTIVE_STATE_LOW_POWER	1

/* FPS time period */
#define FPS_TIME_PERIOD_40US	0
#define FPS_TIME_PERIOD_80US	1
#define FPS_TIME_PERIOD_160US	2
#define FPS_TIME_PERIOD_320US	3
#define FPS_TIME_PERIOD_640US	4
#define FPS_TIME_PERIOD_1280US	5
#define FPS_TIME_PERIOD_2560US	6
#define FPS_TIME_PERIOD_5120US	7
#define FPS_TIME_PERIOD_DEF	8

/* FPS source */
#define MAX77620_FPS_SRC_0			0
#define MAX77620_FPS_SRC_1			1
#define MAX77620_FPS_SRC_2			2
#define MAX77620_FPS_SRC_NONE			3
#define MAX77620_FPS_SRC_DEF			4

#define	FPS_POWER_PERIOD_0	0
#define	FPS_POWER_PERIOD_1	1
#define	FPS_POWER_PERIOD_2	2
#define	FPS_POWER_PERIOD_3	3
#define	FPS_POWER_PERIOD_4	4
#define	FPS_POWER_PERIOD_5	5
#define	FPS_POWER_PERIOD_6	6
#define	FPS_POWER_PERIOD_7	7
#define	FPS_POWER_PERIOD_DEF	8

#endif
