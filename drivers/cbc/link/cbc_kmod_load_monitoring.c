/*
 * carrier boards communicatons core.
 * demultiplexes the cbc protocol.
 *
 * Copryright (C) 2014 Intel Corporation
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
#include "link/cbc_kmod_load_monitoring.h"
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/stat.h>

struct cbc_load_monitoring {
	u8 tx_queue_average;
	u8 tx_queue_max;
	u8 tx_queue_overload;
	u8 tx_uart_average;
	u8 tx_uart_max;
	u8 rx_uart_average;
	u8 rx_uart_max;
};

static struct cbc_load_monitoring load_monitoring;


/* callbacks for the cbc-core */

#if (CBC_LOAD_MONITOR_ENABLE_FRAME_QUEUE_UTILIZATION == 1U)
static void on_cbc_load_queue_utilization_cb(enum  cbc_frame_queue_utilization tx_average_utilization
		, enum cbc_frame_queue_utilization tx_max_utilization)
{
	load_monitoring.tx_queue_average = tx_average_utilization;
	load_monitoring.tx_queue_max = tx_max_utilization;
}


static void on_cbc_load_queue_overload_cb(void)
{
	if (load_monitoring.tx_queue_overload < 0xFF)
		load_monitoring.tx_queue_overload++;
}

#endif

#if (CBC_LOAD_MONITOR_ENABLE_UART_TRANSMISSION_MONITOR == 1U)
static void on_cbc_load_tx_uart_cb(u8 tx_average_load, u8 tx_max_load)
{
	load_monitoring.tx_uart_average = tx_average_load;
	load_monitoring.tx_uart_max = tx_max_load;
}
#endif

#if (CBC_LOAD_MONITOR_ENABLE_UART_RECEPTION_MONITOR == 1U)
static void on_cbc_load_rx_uart_cb(u8 rx_average_load, u8 rx_max_load)
{
	load_monitoring.rx_uart_average = rx_average_load;
	load_monitoring.rx_uart_max = rx_max_load;
}
#endif




/* sys files for the values */

static ssize_t load_mon_reset_store(struct class *cls, struct class_attribute *attr,
									const char *buf, size_t count)
{
	pr_info("reset load monitoring counters\n");
	/* reset counters */
#if (CBC_LOAD_MONITOR_ENABLE_UART_RECEPTION_MONITOR == 1U)
	cbc_load_monitor_reset_checksum_errors_on_receive();
	cbc_load_monitor_reset_sequence_counter_errors_on_receive();
#endif /* (CBC_LOAD_MONITOR_ENABLE_UART_RECEPTION_MONITOR == 1U) */
	load_monitoring.tx_queue_overload = 0;
	return count;
}


static ssize_t load_mon_tx_queue_average_show(struct class *cls, struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", load_monitoring.tx_queue_average);
}


static ssize_t load_mon_tx_queue_max_show(struct class *cls, struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", load_monitoring.tx_queue_max);
}


static ssize_t load_mon_tx_queue_overload_show(struct class *cls, struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", load_monitoring.tx_queue_overload);
}


static ssize_t load_mon_tx_uart_average_show(struct class *cls, struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", load_monitoring.tx_uart_average);
}


static ssize_t load_mon_tx_uart_max_show(struct class *cls, struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", load_monitoring.tx_uart_max);
}


static ssize_t load_mon_rx_uart_average_show(struct class *cls, struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", load_monitoring.rx_uart_average);
}


static ssize_t load_mon_rx_uart_max_show(struct class *cls, struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", load_monitoring.rx_uart_max);
}


static ssize_t load_mon_rx_checksum_errors_show(struct class *cls, struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", cbc_load_monitor_get_checksum_errors_on_receive());
}


static ssize_t load_mon_rx_sequence_counter_errors_show(struct class *cls, struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", cbc_load_monitor_get_sequence_counter_errors_on_receive());
}

struct class_attribute load_mon_reset =
			__ATTR(load_monitoring_reset
					, 0220
					, NULL
					, load_mon_reset_store);
struct class_attribute load_mon_attr_tx_queue_avg =
			__ATTR(tx_queue_average
					, 0444
					, load_mon_tx_queue_average_show
					, NULL);
struct class_attribute load_mon_attr_tx_queue_max =
			__ATTR(tx_queue_max
					, 0444
					, load_mon_tx_queue_max_show
					, NULL);
struct class_attribute load_mon_attr_tx_overloads =
			__ATTR(tx_overloads
					, 0444
					, load_mon_tx_queue_overload_show
					, NULL);
struct class_attribute load_mon_attr_tx_uart_avg =
			__ATTR(tx_uart_average
					, 0444
					, load_mon_tx_uart_average_show
					, NULL);
struct class_attribute load_mon_attr_tx_uart_max =
			__ATTR(tx_uart_max
					, 0444
					, load_mon_tx_uart_max_show
					, NULL);
struct class_attribute load_mon_attr_rx_uart_avg =
			__ATTR(rx_uart_average
					, 0444
					, load_mon_rx_uart_average_show
					, NULL);
struct class_attribute load_mon_attr_rx_uart_max =
			__ATTR(rx_uart_max
					, 0444
					, load_mon_rx_uart_max_show
					, NULL);
struct class_attribute load_mon_attr_rx_checksum_errors =
			__ATTR(rx_checksum_errors
					, S_IRUGO
					, load_mon_rx_checksum_errors_show
					, NULL);
struct class_attribute load_mon_attr_rx_sequence_counter_errors =
			__ATTR(rx_sequence_counter_errors
					, 0444
					, load_mon_rx_sequence_counter_errors_show
					, NULL);


int cbc_kmod_load_monitoring_init(struct class *cls)
{
	/* register callbacks */
	int res = 0;
	enum cbc_error cbc_res = e_cbc_error_ok;

	cbc_res = cbc_load_monitor_set_callbacks(
#if (CBC_LOAD_MONITOR_ENABLE_FRAME_QUEUE_UTILIZATION == 1U)
				  &on_cbc_load_queue_utilization_cb,
				  &on_cbc_load_queue_overload_cb
# if ((CBC_LOAD_MONITOR_ENABLE_UART_TRANSMISSION_MONITOR == 1U)\
|| (CBC_LOAD_MONITOR_ENABLE_UART_RECEPTION_MONITOR == 1U))
				  ,
# endif
#endif
#if (CBC_LOAD_MONITOR_ENABLE_UART_TRANSMISSION_MONITOR == 1U)
				  &on_cbc_load_tx_uart_cb
# if (CBC_LOAD_MONITOR_ENABLE_UART_RECEPTION_MONITOR == 1U)
				  ,
# endif
#endif
#if (CBC_LOAD_MONITOR_ENABLE_UART_RECEPTION_MONITOR == 1U)
				  &on_cbc_load_rx_uart_cb
#endif
			  );

	if (cbc_res != e_cbc_error_ok)
		res = -EINVAL;

	if (!res)
		res = class_create_file(cls, &load_mon_reset);
	if (!res)
		res = class_create_file(cls, &load_mon_attr_tx_queue_avg);
	if (!res)
		res = class_create_file(cls, &load_mon_attr_tx_queue_max);
	if (!res)
		res = class_create_file(cls, &load_mon_attr_tx_overloads);
	if (!res)
		res = class_create_file(cls, &load_mon_attr_tx_uart_avg);
	if (!res)
		res = class_create_file(cls, &load_mon_attr_tx_uart_max);
	if (!res)
		res = class_create_file(cls, &load_mon_attr_rx_uart_avg);
	if (!res)
		res = class_create_file(cls, &load_mon_attr_rx_uart_max);
	if (!res)
		res = class_create_file(cls, &load_mon_attr_rx_checksum_errors);
	if (!res)
		res = class_create_file(cls, &load_mon_attr_rx_sequence_counter_errors);

	return res;
}




