/*
 * This header provides macros for ams AS3722 device bindings.
 *
 * Copyright (c) 2013, NVIDIA Corporation.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 *
 */

#ifndef __DT_BINDINGS_AS3722_H__
#define __DT_BINDINGS_AS3722_H__

/* External control pins */
#define AS3722_EXT_CONTROL_PIN_ENABLE1 1
#define AS3722_EXT_CONTROL_PIN_ENABLE2 2
#define AS3722_EXT_CONTROL_PIN_ENABLE3 3

#define AS3722_BBCCUR_50UA		0
#define AS3722_BBCCUR_200UA		1
#define AS3722_BBCCUR_100UA		2
#define AS3722_BBCCUR_400UA		3
#define AS3722_BBCRESOFF_ENABLE		0
#define AS3722_BBCRESOFF_BYPASS		1
#define AS3722_BBCMODE_OFF		0
#define AS3722_BBCMODE_ACTIVE		1
#define AS3722_BBCMODE_ACT_STBY		2
#define AS3722_BBCMODE_ACT_STBY_OFF	3

/* Interrupt numbers for AS3722 */
#define AS3722_IRQ_LID			0
#define AS3722_IRQ_ACOK			1
#define AS3722_IRQ_ENABLE1		2
#define AS3722_IRQ_OCCUR_ALARM_SD0	3
#define AS3722_IRQ_ONKEY_LONG_PRESS	4
#define AS3722_IRQ_ONKEY		5
#define AS3722_IRQ_OVTMP		6
#define AS3722_IRQ_LOWBAT		7
#define AS3722_IRQ_SD0_LV		8
#define AS3722_IRQ_SD1_LV		9
#define AS3722_IRQ_SD2_LV		10
#define AS3722_IRQ_PWM1_OV_PROT		11
#define AS3722_IRQ_PWM2_OV_PROT		12
#define AS3722_IRQ_ENABLE2		13
#define AS3722_IRQ_SD6_LV		14
#define AS3722_IRQ_RTC_REP		15
#define AS3722_IRQ_RTC_ALARM		16
#define AS3722_IRQ_GPIO1		17
#define AS3722_IRQ_GPIO2		18
#define AS3722_IRQ_GPIO3		19
#define AS3722_IRQ_GPIO4		20
#define AS3722_IRQ_GPIO5		21
#define AS3722_IRQ_WATCHDOG		22
#define AS3722_IRQ_ENABLE3		23
#define AS3722_IRQ_TEMP_SD0_SHUTDOWN	24
#define AS3722_IRQ_TEMP_SD1_SHUTDOWN	25
#define AS3722_IRQ_TEMP_SD2_SHUTDOWN	26
#define AS3722_IRQ_TEMP_SD0_ALARM	27
#define AS3722_IRQ_TEMP_SD1_ALARM	28
#define AS3722_IRQ_TEMP_SD6_ALARM	29
#define AS3722_IRQ_OCCUR_ALARM_SD6	30
#define AS3722_IRQ_ADC			31

/* Power Good OC Mask macro */
#define AS3722_OC_PG_MASK_AC_OK            0x1
#define AS3722_OC_PG_MASK_GPIO3            0x2
#define AS3722_OC_PG_MASK_GPIO4            0x4
#define AS3722_OC_PG_MASK_GPIO5            0x8
#define AS3722_OC_PG_MASK_PWRGOOD_SD0      0x10
#define AS3722_OC_PG_MASK_OVCURR_SD0       0x20
#define AS3722_OC_PG_MASK_POWERGOOD_SD6    0x40
#define AS3722_OC_PG_MASK_OVCURR_SD6       0x80

/* Thermal sensor zones */
#define AS3722_SD0_SENSOR              0
#define AS3722_SD1_SENSOR              1
#define AS3722_SD6_SENSOR              2

/* SDx temperature ADC channel */
#define AS3722_TEMP1_SD0               16
#define AS3722_TEMP2_SD0               17
#define AS3722_TEMP3_SD0               18
#define AS3722_TEMP4_SD0               19
#define AS3722_TEMP_SD1                20
#define AS3722_TEMP1_SD6               21
#define AS3722_TEMP2_SD6               22

#endif /* __DT_BINDINGS_AS3722_H__ */
