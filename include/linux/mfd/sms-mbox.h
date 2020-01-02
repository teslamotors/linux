/*
 * Copyright (C) 2018 Tesla, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef	__SMS_MBOX_H
#define	__SMS_MBOX_H

struct trav_mbox;

int sms_mbox_get_voltage(struct trav_mbox *mbox, int comp_id);
int sms_mbox_set_voltage(struct trav_mbox *mbox, int comp_id, int uV);

int sms_mbox_get_time(struct trav_mbox *mbox, uint32_t *time_regs);
int sms_mbox_set_time(struct trav_mbox *mbox, uint32_t *time_regs);

int sms_mbox_reset_sms_comm(uint32_t region_id, uint32_t ringbuffer_base_addr,
	uint32_t ringbuffer_size);

#define SMS_MBOX_REGULATOR_NAME "sms-mbox-regulator"

#define SMS_MBOX_VOLTAGE_TURBO_INT_NAME "voltage_turbo_int"
#define SMS_MBOX_VOLTAGE_TURBO_CPU_NAME "voltage_turbo_cpu"
#define SMS_MBOX_VOLTAGE_TURBO_GPU_NAME "voltage_turbo_gpu"
#define SMS_MBOX_VOLTAGE_TURBO_TRIP0_NAME "voltage_turbo_trip0"
/* trip1 share volt with trip0 */

#define SMS_MBOX_VOLTAGE_TURBO_INT_COMP_ID 1
#define SMS_MBOX_VOLTAGE_TURBO_CPU_COMP_ID 2
#define SMS_MBOX_VOLTAGE_TURBO_GPU_COMP_ID 3
#define SMS_MBOX_VOLTAGE_TURBO_TRIP0_COMP_ID 4
/* trip1 share volt with trip0 */

#define SMS_MBOX_N_TEMPS	26

int sms_mbox_get_temps(struct trav_mbox *mbox, int16_t *temps);

#define SMS_MBOX_THERMAL_NAME "sms-mbox-thermal"
#define SMS_MBOX_RTC_NAME "sms-mbox-rtc"

#endif		/* __SMS_MBOX_H */
