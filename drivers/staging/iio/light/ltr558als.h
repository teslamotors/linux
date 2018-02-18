/* Lite-On LTR-558ALS Linux Driver
 *
 * Copyright (c) 2012-2014, NVIDIA Corporation.  All Rights reserved.
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

#ifndef _LTR558_H
#define _LTR558_H


/* LTR-558 Registers */
#define LTR558_ALS_CONTR	0x80
#define LTR558_PS_CONTR		0x81
#define LTR558_PS_LED		0x82
#define LTR558_PS_N_PULSES	0x83
#define LTR558_PS_MEAS_RATE	0x84
#define LTR558_ALS_MEAS_RATE	0x85
#define LTR558_MANUFACTURER_ID	0x87

#define LTR558_INTERRUPT	0x8F
#define LTR558_PS_THRES_UP_0	0x90
#define LTR558_PS_THRES_UP_1	0x91
#define LTR558_PS_THRES_LOW_0	0x92
#define LTR558_PS_THRES_LOW_1	0x93

#define LTR558_ALS_THRES_UP_0	0x97
#define LTR558_ALS_THRES_UP_1	0x98
#define LTR558_ALS_THRES_LOW_0	0x99
#define LTR558_ALS_THRES_LOW_1	0x9A

#define LTR558_INTERRUPT_PERSIST 0x9E

/* 558's Read Only Registers */
#define LTR558_ALS_DATA_CH1_0	0x88
#define LTR558_ALS_DATA_CH1_1	0x89
#define LTR558_ALS_DATA_CH0_0	0x8A
#define LTR558_ALS_DATA_CH0_1	0x8B
#define LTR558_ALS_PS_STATUS	0x8C
#define LTR558_PS_DATA_0	0x8D
#define LTR558_PS_DATA_1	0x8E

/* ALS PS STATUS 0x8C */
#define STATUS_ALS_GAIN_RANGE1	0x10
#define STATUS_ALS_INT_TRIGGER	0x08
#define STATUS_ALS_NEW_DATA		0x04
#define STATUS_PS_INT_TRIGGER	0x02
#define STATUS_PS_NEW_DATA		0x01

/* Basic Operating Modes */
#define MODE_ALS_ON_Range1	0x0B
#define MODE_ALS_ON_Range2	0x03
#define MODE_ALS_StdBy		0x00

#define MODE_PS_ON_Gain1	0x03
#define MODE_PS_ON_Gain4	0x07
#define MODE_PS_ON_Gain8	0x0B
#define MODE_PS_ON_Gain16	0x0F
#define MODE_PS_StdBy		0x00

#define PS_LED_CUR_LEVEL_5	0x00
#define PS_LED_CUR_LEVEL_10	0x01
#define PS_LED_CUR_LEVEL_20	0x02
#define PS_LED_CUR_LEVEL_50	0x03
#define PS_LED_CUR_LEVEL_100	0x07

#define PS_LED_CUR_DUTY_25	0x00
#define PS_LED_CUR_DUTY_50	0x08
#define PS_LED_CUR_DUTY_75	0x10
#define PS_LED_CUR_DUTY_100	0x18

#define PS_LED_PMF_30KHZ	0x0
#define PS_LED_PMF_40KHZ	0x20
#define PS_LED_PMF_50KHZ	0x40
#define PS_LED_PMF_60KHZ	0x60
#define PS_LED_PMF_70KHZ	0x80
#define PS_LED_PMF_80KHZ	0xA0
#define PS_LED_PMF_90KHZ	0xC0
#define PS_LED_PMF_100KHZ	0xE0

#define PS_N_PULSES_1		0x01
#define PS_N_PULSES_2		0x02
#define PS_N_PULSES_3		0x03
#define PS_N_PULSES_4		0x04
#define PS_N_PULSES_5		0x05
#define PS_N_PULSES_6		0x06
#define PS_N_PULSES_7		0x07
#define PS_N_PULSES_8		0x08
#define PS_N_PULSES_9		0x09
#define PS_N_PULSES_10		0x0A
#define PS_N_PULSES_11		0x0B
#define PS_N_PULSES_12		0x0C
#define PS_N_PULSES_13		0x0D
#define PS_N_PULSES_14		0x0E
#define PS_N_PULSES_15		0x0F

#define PS_MEAS_RATE_50MS	0x00
#define PS_MEAS_RATE_70MS	0x01
#define PS_MEAS_RATE_100MS	0x02
#define PS_MEAS_RATE_200MS	0x03
#define PS_MEAS_RATE_500MS	0x04
#define PS_MEAS_RATE_1000MS	0x05
#define PS_MEAS_RATE_2000MS	0x07

#define PS_RANGE1	1
#define PS_RANGE4	2
#define PS_RANGE8	4
#define PS_RANGE16	8

#define ALS_RANGE1_320	1
#define ALS_RANGE2_64K	2

#define LTR_MANUFACTURER_ID	0x05

/* Power On response time in ms */
#define PON_DELAY	600
#define WAKEUP_DELAY	10

#endif
