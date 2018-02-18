/* Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
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


#ifndef _NVS_LIGHT_H_
#define _NVS_LIGHT_H_

#include <linux/of.h>
#include <linux/nvs.h>

#define RET_POLL_NEXT			(-1)
#define RET_NO_CHANGE			(0)
#define RET_HW_UPDATE			(1)

#define NVS_LIGHT_STRING		"light"

/* nvs_light_threshold_calibrate_ data uses upper byte for multiple purposes if
 * nld_thr is enabled.
 * bit 31:30 reserved.
 * bit 29 selects which nld_thresh threshold (0 = lo/hi, 1 = i_lo/i_hi).
 * bit 28:24 selects which nld_tbl index entry the threshold is for.
 */
#define NVS_LIGHT_THRESH_CMD_SHIFT	(24)
#define NVS_LIGHT_THRESH_CMD_NDX_MSK	(0x1F)
#define NVS_LIGHT_THRESH_CMD_SEL_MSK	(0x20)

/**
 * struct nvs_light_dynamic - the structure that allows the NVS
 * light module to control dynamic resolution/range.
 * @resolution_ival: resolution.
 * @max_range_ival: max_range.
 * @milliamp: milliamp.
 * @delay_min_ms: minimum delay time in ms.
 * @driver_data: private data for driver.
 *
 * Driver includes a pointer to this table to enable dynamic
 * resolution/max_range.
 * The data must be from low to high max_range.
 */
struct nvs_light_dynamic {
	struct nvs_float resolution;
	struct nvs_float max_range;
	struct nvs_float milliamp;
	unsigned int delay_min_ms;	/* minimum ms delay time */
	unsigned int driver_data;	/* private data for driver */
};

/**
 * struct nld_thresh - nvs_light_dynamic table thresholds.  Each
 * entry in the nld table has its own low/high thresholds.
 * @lo: low threshold within a table entry.
 * @hi: high threshold within a table entry.
 * @i_lo: low threshold to switch to another table entry.
 * @i_hi: high threshold to switch to another table entry.
 *
 * Driver includes a pointer to this array to enable this
 * WAR feature.  The array is populated when nvs_light_of_dt is
 * called and the standard sensor_cfg thresh_lo/hi is not used
 * (as normally global thresholds). The feature is disabled if
 * there isn't a device tree entry for at least one of the
 * thresholds.
 *
 * Consider a scenario where an ALS sensor has been compromised
 * in its design, e.g. putting tint over the sensor so that the
 * HW value read from the sensor never reads its full value or
 * the HW value read by the sensor never goes to 0 in a
 * lightless environment, etc.  For whatever reason where the
 * normal algorithm of placing the global thresholds around the
 * current HW value and if near the "edge" (HW value near 0 or
 * the hw_mask) of the resolution/max_range of a nld entry,
 * switching to the next entry, doesn't work, then this feature
 * allows more specific thresholds to work around the issue(s).
 * The lo/hi thresholds are used to set the "window" around the
 * current HW value and the regular algorithms apply using these
 * specific thresholds for the specific nld table entry.
 * The i_lo/i_hi thresholds are absolute HW values only used for
 * the specific nld table entry to determine when to switch to
 * the next nld table entry.  They can be thought of as
 * thresholds around the entire HW range for an nld table entry.
 * For example, let's say table entry 1 will only get a HW
 * reading from 4 to 1000 but the integration time is 3 seconds
 * and we would like to improve on the response (integration)
 * time by using the next entry in the table as much as possible
 * while tolerating the extra delay for more accuracy in low
 * light conditions. By setting the i_lo to 5 and i_hi to 999
 * the next corresponding table entry will be switched to when
 * the HW reads values < 5 or > 999 (hw values of 0 and hw_mask
 * will also trigger the switch).
 *
 * Note: lo/hi are relative thresholds based on the current HW
 *       value and the normal threshold algorithms apply.
 *       i_lo/i_hi are absolute threshold HW values and force
 *       switching to the next nld table entry when reached.
 *       Both work together as individual mechanisms.
 *
 * Note: nvs_light_of_dt will use the string
 *       %s_nld_thr_lo_%u and %s_nld_thr_hi_%u to search the DT
 *       for the lo/hi values where %s is typically "light" and
 *       %u is the index number into the nvs_light_dynamic
 *       table.
 *       For the i_lo/i_hi, the following string is used:
 *       %s_nld_thr_i_lo_%u and s_nld_thr_i_hi_%u
 *       If a device tree entry is not found the corresponding
 *       sensor_cfg thresh_lo/hi value is used instead.  This
 *       way only the table entry/ies with "issues" need the
 *       specific threshold(s) override.
 *       This goes for the i_lo/i_hi absolute values as well; if
 *       no DT entry is found the absolute values will be
 *       calculated based on sensor_cfg thresh_lo/hi values as
 *       would normally be calculated.
 *       If there isn't any DT thresholds for this feature then
 *       nvs_light_of_dt will disable this feature if
 *       nld_tbl_n > 0, by setting nld_thr to NULL.
 *
 * Note: If the driver supports it, the nld table thresholds can
 *       be changed at runtime by writing to the sysfs threshold
 *       nodes where bit 29 selects the threshold (0 = lo/hi and
 *       1 = i_lo/i_hi) and 28:24 of the value specifies the
 *       table index.
 */
struct nld_thresh {
	unsigned int lo;		/* low threshold */
	unsigned int hi;		/* high threshold */
	unsigned int i_lo;		/* index absolute low threshold */
	unsigned int i_hi;		/* index absolute high threshold */
};

/**
 * struct nvs_light - the common structure between the ALS
 * driver and the NVS common module for light.
 * @timestamp: Driver writes the timestamp HW was read.
 * @timestamp_report: NVS writes the timestamp used to report.
 * @lux: NVS writes the lux value that is reported.
 * @lux_max: The maximum lux value allowed.
 * Note: This is a WAR for compromised sensor implementations as
 *       described in struct nld_thresh.
 *       When this is 0 this WAR feature is disabled which is
 *       the default. The device tree string is %s_lux_maximum
 *       where %s is typically "light" but may be the part name.
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
 * @nld_i_change: NVS determined dynamic resolution changed.
 *                Driver needs to update HW.
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
 * @poll_delay_ms: NVS writes the poll time needed if polling.
 * @delay_us: Driver writes the requested sample time.
 * @report: NVS writes the report count.
 * @nld_i: NVS writes index to nvs_light_dynamic table.
 * @nld_i_lo: Driver writes low index limit to nvs_light_dynamic
 *            table.
 *            Note: nvs_light_of_dt can be used.
 * @nld_i_hi: Driver writes high index limit to
 *            nvs_light_dynamic table.
 *            Note: nvs_light_of_dt can be used.
 * @nld_tbl_n: Driver writes ARRAY_SIZE of the nvs_light_dynamic
 *             table.  This allows runtime changes to nld_i_lo
 *             and nld_i_hi using resolution and max_range as
 *             well as nld_thr DT and runtime changes.
 *             Note: If this value is 0 then these features are
 *                   disabled - typically to lock values.
 * @nld_thr: See struct nld_thresh above.
 * @nld_tbl: Driver writes pointer to nvs_light_dynamic table.
 *           If this is NULL then dynamic resolution is disabled
 *           in NVS.
 * @cfg: Driver writes the sensor_cfg structure pointer.
 * @nvs_data: Driver writes the private pointer for handler.
 * @handler: Driver writes the handler pointer.
 */
struct nvs_light {
	s64 timestamp;			/* sample timestamp */
	s64 timestamp_report;		/* last reported timestamp */
	u32 lux;			/* calculated lux */
	u32 lux_max;			/* lux max limit WAR */
	u32 hw;				/* HW light value */
	u32 hw_mask;			/* HW light mask */
	u32 hw_thresh_lo;		/* HW low threshold value */
	u32 hw_thresh_hi;		/* HW high threshold value */
	bool hw_limit_lo;		/* hw < hw_thresh_lo or hw = 0 */
	bool hw_limit_hi;		/* hw > hw_thresh_hi or hw = hw_mask */
	bool thresh_valid_lo;		/* valid cfg.thresh_lo */
	bool thresh_valid_hi;		/* valid cfg.thresh_hi */
	bool thresholds_valid;		/* both thresholds valid */
	bool nld_i_change;		/* flag that dynamic index changed */
	bool calibration_en;		/* if calibration enabled */
	bool dynamic_resolution_dis;	/* disable float significance */
	unsigned int poll_delay_ms;	/* HW polling delay (ms) */
	unsigned int delay_us;		/* OS requested sample delay */
	unsigned int report;		/* report count */
	unsigned int nld_i;		/* index to light dynamic table */
	unsigned int nld_i_lo;		/* low index limit to dynamic table */
	unsigned int nld_i_hi;		/* high index limit to dynamic table */
	unsigned int nld_tbl_n;		/* nvs_light_dynamic tbl array size */
	struct nld_thresh *nld_thr;	/* ptr to nld_tbl thresholds */
	struct nvs_light_dynamic *nld_tbl; /* ptr to nvs_light_dynamic table */
	struct sensor_cfg *cfg;		/* pointer to sensor configuration */
	void *nvs_st;			/* NVS state data for NVS handler */
	int (*handler)(void *handle, void *buffer, s64 ts);
};

int nvs_light_read(struct nvs_light *nl);
int nvs_light_enable(struct nvs_light *nl);
int nvs_light_of_dt(struct nvs_light *nl, const struct device_node *np,
		    const char *dev_name);
int nvs_light_resolution(struct nvs_light *nl, int resolution);
int nvs_light_max_range(struct nvs_light *nl, int max_range);
int nvs_light_threshold_calibrate_lo(struct nvs_light *nl, int lo);
int nvs_light_threshold_calibrate_hi(struct nvs_light *nl, int hi);
ssize_t nvs_light_dbg(struct nvs_light *nl, char *buf);

#endif /* _NVS_LIGHT_H_ */
