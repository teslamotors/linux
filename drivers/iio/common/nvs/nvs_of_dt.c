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


#include <linux/nvs.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/export.h>


const char * const nvs_float_significances[] = {
	"micro",
	"nano",
};
EXPORT_SYMBOL(nvs_float_significances);

/**
 * nvs_of_dt - NVS sensor device tree configuration.
 * @np: mandatory device node pointer.
 * @cfg: sensor's configuration pointer.
 *       This can be called early with cfg == NULL to determine
 *       if the device is available based on returned -ENODEV or
 *       -EINVAL.
 * @dev_name: name of sensor device.  If this is NULL then the
 *            sensor configuration's name member is used.
 *
 * Returns: -EINVAL = np and/or cfg are NULL.
 *          -ENODEV = if device is not available.
 *          0 = no default configuration changes were made.
 *          >0 = number of default configuration changes made.
 *               Note this does not include expected device tree
 *               configuration settings such as matrix and
 *               calibration.  Also not included is whether the
 *               device was disabled via the NVS mechanism.
 */
int nvs_of_dt(const struct device_node *np, struct sensor_cfg *cfg,
	      const char *dev_name)
{
	s32 s32tmp = 0;
	u32 u32tmp = 0;
	char str[256];
	const char *charp;
	int lenp;
	unsigned int i;
	unsigned int cfg_changes = 0;

	if (np == NULL)
		return -EINVAL;

	if (!of_device_is_available(np))
		return -ENODEV;

	if (cfg == NULL)
		return -EINVAL;

	if (dev_name == NULL)
		dev_name = cfg->name;
	if (sprintf(str, "%s_disable", dev_name) > 0) {
		if (!of_property_read_u32(np, str, (u32 *)&i)) {
			if (i)
				cfg->snsr_id = -1;
		}
	}
	if (sprintf(str, "%s_float_significance", dev_name) > 0) {
		if (!of_property_read_string((struct device_node *)np,
					     str, &charp)) {
			u32tmp = ARRAY_SIZE(nvs_float_significances);
			for (i = 0; i < u32tmp; i++) {
				if (!strcasecmp(charp,
						nvs_float_significances[i])) {
					if (cfg->float_significance != i) {
						cfg->float_significance = i;
						cfg_changes++;
					}
					break;
				}
			}
		}
	}

	if (sprintf(str, "%s_flags", dev_name) > 0) {
		if (!of_property_read_u32(np, str, &u32tmp)) {
			i = cfg->flags & SENSOR_FLAG_READONLY_MASK;
			u32tmp &= ~SENSOR_FLAG_READONLY_MASK;
			i |= u32tmp;
			if (cfg->flags != i) {
				cfg->flags = i;
				cfg_changes++;
			}
		}
	}
	if (sprintf(str, "%s_kbuffer_size", dev_name) > 0) {
		if (!of_property_read_s32(np, str, &s32tmp)) {
			if (cfg->kbuf_sz != s32tmp) {
				cfg->kbuf_sz = s32tmp;
				cfg_changes++;
			}
		}
	}
	if (sprintf(str, "%s_max_range_ival", dev_name) > 0) {
		if (!of_property_read_s32(np, str, &s32tmp)) {
			if (cfg->max_range.ival != s32tmp) {
				cfg->max_range.ival = s32tmp;
				cfg_changes++;
			}
		}
	}
	if (sprintf(str, "%s_max_range_fval", dev_name) > 0) {
		if (!of_property_read_s32(np, str, &s32tmp)) {
			if (cfg->max_range.fval != s32tmp) {
				cfg->max_range.fval = s32tmp;
				cfg_changes++;
			}
		}
	}
	if (sprintf(str, "%s_resolution_ival", dev_name) > 0) {
		if (!of_property_read_s32(np, str, &s32tmp)) {
			if (cfg->resolution.ival != s32tmp) {
				cfg->resolution.ival = s32tmp;
				cfg_changes++;
			}
		}
	}
	if (sprintf(str, "%s_resolution_fval", dev_name) > 0) {
		if (!of_property_read_s32(np, str, &s32tmp)) {
			if (cfg->resolution.fval != s32tmp) {
				cfg->resolution.fval = s32tmp;
				cfg_changes++;
			}
		}
	}
	if (sprintf(str, "%s_milliamp_ival", dev_name) > 0) {
		if (!of_property_read_s32(np, str, &s32tmp)) {
			if (cfg->milliamp.ival != s32tmp) {
				cfg->milliamp.ival = s32tmp;
				cfg_changes++;
			}
		}
	}
	if (sprintf(str, "%s_milliamp_fval", dev_name) > 0) {
		if (!of_property_read_s32(np, str, &s32tmp)) {
			if (cfg->milliamp.fval != s32tmp) {
				cfg->milliamp.fval = s32tmp;
				cfg_changes++;
			}
		}
	}
	if (sprintf(str, "%s_delay_us_min", dev_name) > 0) {
		if (!of_property_read_s32(np, str, &s32tmp)) {
			if (cfg->delay_us_min != s32tmp) {
				cfg->delay_us_min = s32tmp;
				cfg_changes++;
			}
		}
	}
	if (sprintf(str, "%s_delay_us_max", dev_name) > 0) {
		if (!of_property_read_s32(np, str, &s32tmp)) {
			if (cfg->delay_us_max != s32tmp) {
				cfg->delay_us_max = s32tmp;
				cfg_changes++;
			}
		}
	}
	if (sprintf(str, "%s_fifo_reserved_event_count", dev_name) > 0) {
		if (!of_property_read_u32(np, str, &u32tmp)) {
			if (cfg->fifo_rsrv_evnt_cnt != u32tmp) {
				cfg->fifo_rsrv_evnt_cnt = u32tmp;
				cfg_changes++;
			}
		}
	}
	if (sprintf(str, "%s_fifo_max_event_count", dev_name) > 0) {
		if (!of_property_read_u32(np, str, &u32tmp)) {
			if (cfg->fifo_max_evnt_cnt != u32tmp) {
				cfg->fifo_max_evnt_cnt = u32tmp;
				cfg_changes++;
			}
		}
	}
	if (sprintf(str, "%s_matrix", dev_name) > 0) {
		charp = of_get_property(np, str, &lenp);
		if (charp && lenp == sizeof(cfg->matrix))
			memcpy(&cfg->matrix, charp, lenp);
	}
	if (sprintf(str, "%s_uncalibrated_lo", dev_name) > 0)
		of_property_read_s32(np, str, (s32 *)&cfg->uncal_lo);
	if (sprintf(str, "%s_uncalibrated_hi", dev_name) > 0)
		of_property_read_s32(np, str, (s32 *)&cfg->uncal_hi);
	if (sprintf(str, "%s_calibrated_lo", dev_name) > 0)
		of_property_read_s32(np, str, (s32 *)&cfg->cal_lo);
	if (sprintf(str, "%s_calibrated_hi", dev_name) > 0)
		of_property_read_s32(np, str, (s32 *)&cfg->cal_hi);
	if (sprintf(str, "%s_threshold_lo", dev_name) > 0) {
		if (!of_property_read_s32(np, str, &s32tmp)) {
			if (cfg->thresh_lo != s32tmp) {
				cfg->thresh_lo = s32tmp;
				cfg_changes++;
			}
		}
	}
	if (sprintf(str, "%s_threshold_hi", dev_name) > 0) {
		if (!of_property_read_s32(np, str, &s32tmp)) {
			if (cfg->thresh_hi != s32tmp) {
				cfg->thresh_hi = s32tmp;
				cfg_changes++;
			}
		}
	}
	if (sprintf(str, "%s_report_count", dev_name) > 0) {
		if (!of_property_read_s32(np, str, &s32tmp)) {
			if (cfg->report_n != s32tmp) {
				cfg->report_n = s32tmp;
				cfg_changes++;
			}
		}
	}
	if (sprintf(str, "%s_scale_ival", dev_name) > 0) {
		if (!of_property_read_s32(np, str, &s32tmp)) {
			if (cfg->scale.ival != s32tmp) {
				cfg->scale.ival = s32tmp;
				cfg_changes++;
			}
		}
	}
	if (sprintf(str, "%s_scale_fval", dev_name) > 0) {
		if (!of_property_read_s32(np, str, &s32tmp)) {
			if (cfg->scale.fval != s32tmp) {
				cfg->scale.fval = s32tmp;
				cfg_changes++;
			}
		}
	}
	if (sprintf(str, "%s_offset_ival", dev_name) > 0) {
		if (!of_property_read_s32(np, str, &s32tmp)) {
			if (cfg->offset.ival != s32tmp) {
				cfg->offset.ival = s32tmp;
				cfg_changes++;
			}
		}
	}
	if (sprintf(str, "%s_offset_fval", dev_name) > 0) {
		if (!of_property_read_s32(np, str, &s32tmp)) {
			if (cfg->offset.fval != s32tmp) {
				cfg->offset.fval = s32tmp;
				cfg_changes++;
			}
		}
	}
	for (i = 0; i < cfg->ch_n_max; i++) {
		if (sprintf(str, "%s_scale_ival_ch%u", dev_name, i) > 0) {
			if (!of_property_read_s32(np, str, &s32tmp)) {
				if (cfg->scales[i].ival != s32tmp) {
					cfg->scales[i].ival = s32tmp;
					cfg_changes++;
				}
			}
		}
		if (sprintf(str, "%s_scale_fval_ch%u", dev_name, i) > 0) {
			if (!of_property_read_s32(np, str, &s32tmp)) {
				if (cfg->scales[i].fval != s32tmp) {
					cfg->scales[i].fval = s32tmp;
					cfg_changes++;
				}
			}
		}
		if (sprintf(str, "%s_offset_ival_ch%u", dev_name, i) > 0) {
			if (!of_property_read_s32(np, str, &s32tmp)) {
				if (cfg->offsets[i].ival != s32tmp) {
					cfg->offsets[i].ival = s32tmp;
					cfg_changes++;
				}
			}
		}
		if (sprintf(str, "%s_offset_fval_ch%u", dev_name, i) > 0) {
			if (!of_property_read_s32(np, str, &s32tmp)) {
				if (cfg->offsets[i].fval != s32tmp) {
					cfg->offsets[i].fval = s32tmp;
					cfg_changes++;
				}
			}
		}
	}

	return cfg_changes;
}

