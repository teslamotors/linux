/* Copyright (c) 2014-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#ifndef _NVS_PROXIMITY_H_
#define _NVS_PROXIMITY_H_

#include <linux/of.h>
#include <linux/nvs.h>

#define RET_POLL_NEXT			(-1)
#define RET_NO_CHANGE			(0)
#define RET_HW_UPDATE			(1)

#define NVS_PROXIMITY_STRING		"proximity"

/**
 * struct nvs_proximity - the common structure between the
 * proximity driver and the NVS common module for proximity.
 * @timestamp: Driver writes the timestamp HW was read.
 * @timestamp_report: NVS writes the timestamp used to report.
 * @proximity: NVS writes the proximity value that is reported.
 * @hw: Driver writes the value read from HW.
 * @hw_mask: Driver writes the HW value mask (maximum value).
 * @hw_thresh_lo: NVS writes the low threshold value needed.
 * @hw_thresh_hi: NVS writes the high threshold value needed.
 * @hw_limit_lo: NVS determines if HW at low limit.  Driver can
 *               use this for dynamic resolution if disabled in
 *               NVS.  See nld_tbl.
 * @hw_limit_hi: NVS determines if HW at high limit.  Driver can
 *               use this for dynamic resolution if disabled in
 *               NVS.  See nld_tbl.
 * @thresh_valid_lo: NVS determines if cfg.thresh_lo valid.
 * @thresh_valid_hi: NVS determines if cfg.thresh_hi valid.
 * @thresholds_valid: NVS determines if both thresholds valid.
 * @calibration_en: NVS determines if calibration is enabled.
 * @dynamic_resolution_dis: Driver can set this to true if the
 *                          resolution is static.
 * Note: dynamic resolution allows floating point to be
 *       calculated in the kernel by shifting up to integer the
 *       floating point significant amount. This allows
 *       real-time resolution changes without the NVS HAL having
 *       to synchronize to the actual resolution per data value.
 *       The scale.fval must be a 10 base value, e.g. 0.1, 0.01,
 *       ... 0.000001, etc. as the significant amount.  The NVS
 *       HAL will then convert the integer float-data to a float
 *       value by multiplying it with scale.
 * @proximity_reverse_range_dis: Driver sets this if the
 *                               proximity range is not
 *                               reversed.
 * Note: Typically, the proximity HW value gets larger the
 *       closer an object gets.  By default NVS reverses this by
 *       subtracting the value from the maximum possible value.
 *       The driver can disable this feature by setting the
 *       proximity_reverse_range_dis to true.
 * @proximity_binary_en: NVS determines if binary reporting is
 *                       enabled.
 * @proximity_binary_hw: NVS determines if HW binary reporting
 *                       is enabled via device tree, or driver
 *                       enables this by setting hw_mask to 1.
 * @poll_delay_ms: NVS writes the poll time needed if polling.
 * @delay_us: Driver writes the requested sample time.
 * @report: NVS writes the report count.
 * @cfg: Driver writes the sensor_cfg structure pointer.
 * @nvs_data: Driver writes the private pointer for handler.
 * @handler: Driver writes the handler pointer.
 */
struct nvs_proximity {
	s64 timestamp;			/* sample timestamp */
	s64 timestamp_report;		/* last reported timestamp */
	u32 proximity;			/* proximity */
	u32 hw;				/* HW proximity value */
	u32 hw_mask;			/* HW proximity mask */
	u32 hw_thresh_lo;		/* HW low threshold value */
	u32 hw_thresh_hi;		/* HW high threshold value */
	bool hw_limit_lo;		/* hw < hw_thresh_lo or hw = 0 */
	bool hw_limit_hi;		/* hw > hw_thresh_hi or hw = hw_mask */
	bool thresh_valid_lo;		/* valid cfg.thresh_lo */
	bool thresh_valid_hi;		/* valid cfg.thresh_hi */
	bool thresholds_valid;		/* both thresholds valid */
	bool calibration_en;		/* if calibration enabled */
	bool dynamic_resolution_dis;	/* disable float significance */
	bool proximity_reverse_range_dis; /* if proximity range not reversed */
	bool proximity_binary_en;	/* if binary proximity enabled */
	bool proximity_binary_hw;	/* if HW binary proximity enabled */
	unsigned int poll_delay_ms;	/* HW polling delay (ms) */
	unsigned int delay_us;		/* OS requested sample delay */
	unsigned int report;		/* report count */
	struct sensor_cfg *cfg;		/* pointer to sensor configuration */
	void *nvs_st;			/* NVS state data for NVS handler */
	int (*handler)(void *handle, void *buffer, s64 ts);
};

int nvs_proximity_enable(struct nvs_proximity *np);
int nvs_proximity_read(struct nvs_proximity *np);
int nvs_proximity_of_dt(struct nvs_proximity *np, const struct device_node *dn,
			const char *dev_name);
void nvs_proximity_threshold_calibrate_lo(struct nvs_proximity *np, int lo);
void nvs_proximity_threshold_calibrate_hi(struct nvs_proximity *np, int hi);
ssize_t nvs_proximity_dbg(struct nvs_proximity *np, char *buf);

#endif /* _NVS_PROXIMITY_H_ */
