/*
 * drivers/thermal/tegra_aotag.c
 *
 * TEGRA AOTAG (Always-On Thermal Alert Generator) driver.
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/thermal.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/tegra-fuse.h>

#define aotag_pdev_print(level, pdev, format, arg...)	\
	dev_##level((struct device *)&(pdev->dev), format, ## arg)

#define aotag_print(level, format, arg...)	\
	pr_##level("tegra_aotag: " format, ## arg)


#define	OF_PROPERTY_READ(type, np, property, ptr)	\
	({\
	 int _ret;\
	 _ret = of_property_read_##type(np, property, ptr);\
	 if (_ret < 0) {\
		aotag_print(alert, "no property %s", property); } \
	 (_ret);\
	})

#define MAX_SENSORS	(4)
#define SENSOR_TYPE_LEN	(32)
#define DEFAULT_SHUTDOWN_TEMP	(104000)

#define reg_read(pdev, device, reg)	\
	readl((PDEV2REG_INFO(pdev))->device##_base + reg)

#define reg_write(pdev, device, val, reg) \
	writel(val, (PDEV2REG_INFO(pdev))->device##_base + reg)

#define PDEV2DEVICE(pdev)	\
	(&(pdev->dev))

#define PDEV2PLAT_INFO(pdev)	\
	(pdev->dev.platform_data)

#define PDEV2SENSOR_INFO(pdev)	\
	(&(((struct aotag_plat_info_t *)\
	    (pdev->dev.platform_data))->sensor_info))

#define PDEV2REG_INFO(pdev)	\
	(&(((struct aotag_plat_info_t *)(pdev->dev.platform_data))->reg_info))

#define PDEV2FUSE_INFO(pdev)	\
	(&(((struct aotag_plat_info_t *)(pdev->dev.platform_data))->\
	   sensor_info.fuse_info))

#define PDEV2TZDEVICE(pdev)	\
	((((struct aotag_plat_info_t *)(pdev->dev.platform_data))->\
	   sensor_info.tzd))

#define PDEV2OF_NODE(pdev)	\
	(pdev->dev.of_node)

#define SENSOR_INFO2COMMON_PARAMS(sensor)	\
	(&(sensor->sensor_common_params))

#define SENSOR_INFO2FUSE_INFO(sensor)	\
	(&(sensor->fuse_info))

#define FILL_REG_OR(value, mask, addr)	\
	do {\
		value = value<<(mask##_POS_START); \
		*(addr) |= (value & mask##_MASK);\
	} while (0)

#define GET_REG(pvalue, mask, addr)	\
	do {\
		*(u32 *)(pvalue) = *(addr) & mask##_MASK;\
		*(u32 *)(pvalue) = *(u32 *)(pvalue) >> (mask##_POS_START);\
	} while (0)

#define MASK(start, end)	\
	(((0xFFFFFFFF)<<start) & (u32)(((u64)1<<(end+1))-1))

/*
 * Forward declarations
 */
static int aotag_get_temp_generic(void*, long*);
static int aotag_get_trend_generic(void*, long*);

/*
 * Register definitions
 */
#define TSENSOR_COMMON_FUSE_ADDR	(0x280)
#define AOTAG_FUSE_ADDR			(0x2D4)

#define PMC_R_OBS_AOTAG			(0x017)
#define PMC_R_OBS_AOTAG_CAPTURE		(0x017)
#define PMC_RST_STATUS			(0x1B4)

#define PMC_AOTAG_CFG			(0x484)
#define CFG_DISABLE_CLK_POS		(0)
#define CFG_SW_TRIGGER_POS		(2)
#define CFG_USE_EXT_TRIGGER_POS		(3)
#define CFG_ENABLE_SW_DEBUG_POS		(4)
#define CFG_TAG_EN_POS			(5)
#define CFG_THERMTRIP_EN_POS		(6)

#define PMC_AOTAG_THRESH1_CFG		(0x488)
#define PMC_AOTAG_THRESH2_CFG		(0x48C)

#define PMC_AOTAG_THRESH3_CFG		(0x490)
#define THRESH3_CFG_POS_START		(0)
#define THRESH3_CFG_POS_END		(14)
#define THRESH3_CFG_MASK		(MASK(THRESH3_CFG_POS_START,\
						THRESH3_CFG_POS_END))

#define PMC_AOTAG_STATUS		(0x494)
#define PMC_AOTAG_SECURITY		(0x498)

#define PMC_TSENSOR_CONFIG0		(0x49C)
#define CONFIG0_STOP_POS		(0)
#define CONFIG0_RO_SEL_POS		(1)
#define CONFIG0_STATUS_CLR_POS		(5)
#define CONFIG0_TALL_POS_START		(8)
#define CONFIG0_TALL_POS_END		(27)
#define CONFIG0_TALL_SIZE		(CONFIG0_TALL_POS_END - \
					CONFIG0_TALL_POS + 1)
#define CONFIG0_TALL_MASK		(MASK(CONFIG0_TALL_POS_START, \
						CONFIG0_TALL_POS_END))

#define PMC_TSENSOR_CONFIG1		(0x4A0)
#define CONFIG1_TEMP_ENABLE_POS		(31)
#define CONFIG1_TEN_COUNT_POS_START	(24)
#define CONFIG1_TEN_COUNT_POS_END	(29)
#define CONFIG1_TEN_COUNT_MASK		(MASK(CONFIG1_TEN_COUNT_POS_START, \
						CONFIG1_TEN_COUNT_POS_END))
#define CONFIG1_TIDDQ_EN_POS_START	(15)
#define CONFIG1_TIDDQ_EN_POS_END	(20)
#define CONFIG1_TIDDQ_EN_MASK		(MASK(CONFIG1_TIDDQ_EN_POS_START, \
						CONFIG1_TIDDQ_EN_POS_END))
#define CONFIG1_TSAMPLE_POS_START	(0)
#define CONFIG1_TSAMPLE_POS_END		(9)
#define CONFIG1_TSAMPLE_MASK		(MASK(CONFIG1_TSAMPLE_POS_START, \
						CONFIG1_TSAMPLE_POS_END))

#define PMC_TSENSOR_CONFIG2		(0x4A4)
#define CONFIG2_THERM_A_POS_START	(16)
#define CONFIG2_THERM_A_POS_END		(31)
#define CONFIG2_THERM_A_MASK		(MASK(CONFIG2_THERM_A_POS_START, \
						CONFIG2_THERM_A_POS_END))

#define CONFIG2_THERM_B_POS_START	(0)
#define CONFIG2_THERM_B_POS_END		(15)
#define CONFIG2_THERM_B_MASK		(MASK(CONFIG2_THERM_B_POS_START, \
						CONFIG2_THERM_B_POS_END))

#define PMC_TSENSOR_STATUS0		(0x4A8)
#define STATUS0_CAPTURE_VALID_POS	(31)
#define STATUS0_CAPTURE_POS_START	(0)
#define STATUS0_CAPTURE_POS_END		(15)
#define STATUS0_CAPTURE_MASK		(MASK(STATUS0_CAPTURE_POS_START, \
					STATUS0_CAPTURE_POS_END))

#define PMC_TSENSOR_STATUS1		(0x4AC)
#define STATUS1_TEMP_POS		(31)
#define STATUS1_TEMP_POS_START		(0)
#define STATUS1_TEMP_POS_END		(15)
#define STATUS1_TEMP_MASK		(MASK(STATUS1_TEMP_POS_START, \
					STATUS1_TEMP_POS_END))

#define STATUS1_TEMP_VALID_POS_START	(31)
#define STATUS1_TEMP_VALID_POS_END	(31)
#define STATUS1_TEMP_VALID_MASK		(MASK(STATUS1_TEMP_VALID_POS_START, \
					STATUS1_TEMP_VALID_POS_END))

#define STATUS1_TEMP_ABS_POS_START	(8)
#define STATUS1_TEMP_ABS_POS_END	(15)
#define STATUS1_TEMP_ABS_MASK		(MASK(STATUS1_TEMP_ABS_POS_START, \
					STATUS1_TEMP_ABS_POS_END))

#define STATUS1_TEMP_FRAC_POS_START	(7)
#define STATUS1_TEMP_FRAC_POS_END	(7)
#define STATUS1_TEMP_FRAC_MASK		(MASK(STATUS1_TEMP_FRAC_POS_START, \
					STATUS1_TEMP_FRAC_POS_END))

#define STATUS1_TEMP_SIGN_POS_START	(0)
#define STATUS1_TEMP_SIGN_POS_END	(0)
#define STATUS1_TEMP_SIGN_MASK		(MASK(STATUS1_TEMP_SIGN_POS_START, \
					STATUS1_TEMP_SIGN_POS_END))

#define PMC_TSENSOR_STATUS2		(0x4B0)
#define PMC_TSENSOR_PDIV0		(0x4B4)
#define TSENSOR_PDIV_POS_START		(12)
#define TSENSOR_PDIV_POS_END		(15)
#define TSENSOR_PDIV_MASK		(MASK(TSENSOR_PDIV_POS_START, \
						TSENSOR_PDIV_POS_END))

#define PMC_AOTAG_INTR_EN		(0x4B8)
#define PMC_AOTAG_INTR_DIS		(0x4BC)

#define AOTAG_FUSE_CALIB_CP_POS_START	(0)
#define AOTAG_FUSE_CALIB_CP_POS_END	(12)
#define AOTAG_FUSE_CALIB_CP_MASK	(MASK(AOTAG_FUSE_CALIB_CP_POS_START, \
						AOTAG_FUSE_CALIB_CP_POS_END))

#define AOTAG_FUSE_CALIB_FT_POS_START	(13)
#define AOTAG_FUSE_CALIB_FT_POS_END	(25)
#define AOTAG_FUSE_CALIB_FT_MASK	(MASK(AOTAG_FUSE_CALIB_FT_POS_START, \
						AOTAG_FUSE_CALIB_FT_POS_END))
/*
 * Data definitions
 */

static struct of_device_id tegra_aotag_of_match[] = {
	{ .compatible = "nvidia,tegra21x-aotag" },
	{ },
};
MODULE_DEVICE_TABLE(of, tegra_aotag_of_match);

struct aotag_reg_info_t {
	void __iomem *aotag_base;
	void __iomem *pmc_base;
};

struct sensor_common_params_t {
	u32 tall;
	u32 tiddq;
	u32 count;
	u32 tsample;
	u32 pdiv;
	u32 tsamp_ate;
	u32 pdiv_ate;
};

struct fuse_info_t {
	s32 common_fuse_rev;
	s32 nominal_temp_cp;
	s32 nominal_temp_ft;
	s32 avg_ts_reading_cp;
	s32 avg_ts_reading_ft;
	s32 shifted_temp_cp;
	s32 shifted_temp_ft;
	s32 actual_temp_cp;
	s32 actual_temp_ft;
	s32 actual_ts_reading_cp;
	s32 actual_ts_reading_ft;
	s32 compensation_a;
	s32 compensation_b;
};

struct aotag_sensor_info_t {
	u32 id;
	u32 advertised_id;
	const char *name;
	struct sensor_common_params_t sensor_common_params;
	struct thermal_of_sensor_ops sops;
	struct thermal_zone_device *tzd;
	struct fuse_info_t fuse_info;
	s32 therm_a;
	s32 therm_b;

	u32 last_raw_capture;
	long last_temp_capture;
	long shutdown_temp;
};

struct aotag_plat_info_t {
	struct aotag_reg_info_t reg_info;
	struct aotag_sensor_info_t sensor_info;
};

/*
 * Helper Functions
 */
static int aotag_update_shutdown_temp(struct platform_device *pdev)
{
	int ret = 0;
	u32 reg;
	long temp;
	struct thermal_zone_device *ptzd = PDEV2TZDEVICE(pdev);
	struct aotag_sensor_info_t *ps_info = PDEV2SENSOR_INFO(pdev);

	if (IS_ERR_OR_NULL(ptzd)) {
		ret = -ENODEV;
		goto out;
	}

	ret = -EINVAL;
	if (ptzd->ops->get_crit_temp)
		ret = ptzd->ops->get_crit_temp(ptzd, &ps_info->shutdown_temp);

	if (unlikely(ret)) {
		aotag_pdev_print(warn, pdev,
				"Could not query shutdown/critical temp\n");
	}

	reg = 0;
	temp = (ps_info->shutdown_temp)/1000;
	FILL_REG_OR(temp, THRESH3_CFG, &reg);
	reg_write(pdev, pmc, reg, PMC_AOTAG_THRESH3_CFG);
	aotag_pdev_print(info, pdev, "shutdown temperature - %d\n",
			reg_read(pdev, pmc, PMC_AOTAG_THRESH3_CFG));
out:
	return ret;
}

static int aotag_trip_update(void *data, int trip)
{
	int ret = 0;
	struct platform_device *pdev;

	pdev = (struct platform_device *)data;

	aotag_update_shutdown_temp(pdev);

	return ret;
}

static int aotag_get_temp_generic(void *data, long *temp)
{
	int ret = 0;
	u32 regval = 0;
	u32 abs = 0;
	u32 fraction = 0;
	u32 valid = 0;
	u32 sign = 0;
	struct platform_device *pdev;

	if (unlikely(!data)) {
		aotag_print(alert, " Invalid data pointer\n");
		*temp = (-66);
		ret = -EINVAL;
		goto out;
	}
	pdev = (struct platform_device *)data;

	regval = reg_read(pdev, pmc, PMC_TSENSOR_STATUS1);
	/* aotag_pdev_print(info, pdev, " TEMP - 0x%x\n", regval); */
	GET_REG(&valid, STATUS1_TEMP_VALID, &regval);
	if (!valid) {
		*temp = -125;
		aotag_pdev_print(info, pdev, "Invalid temp readout\n");
		goto out;
	}
	GET_REG(&abs, STATUS1_TEMP_ABS, &regval);
	GET_REG(&fraction, STATUS1_TEMP_FRAC, &regval);
	GET_REG(&sign, STATUS1_TEMP_SIGN, &regval);
	/* aotag_pdev_print(info, pdev, "abs %d, frac %d, sign %d\n", abs,
			fraction, sign); */
	*temp = (abs*1000) + (fraction*500);
	if (sign)
		*temp = (-1) * (*temp);
	/* aotag_print(info, " temp - %ld\n", *temp); */
out:
	return ret;
}

static int aotag_get_trend_generic(void *data, long *trend)
{
	*trend = 0;
	return 0;
}

static int aotag_init(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *pmc_np = NULL;
	struct aotag_plat_info_t *pplat = NULL;
	struct device *dev = PDEV2DEVICE(pdev);
	struct device_node *np = PDEV2OF_NODE(pdev);

	pmc_np = of_parse_phandle(np, "parent-block", 0);
	if (unlikely(!pmc_np)) {
		aotag_pdev_print(alert, pdev,
				"PMC handle not found.\n");
		ret = -EINVAL;
		goto out;
	}

	pplat = devm_kzalloc(dev, sizeof(*pplat), GFP_KERNEL);
	if (unlikely(!pplat)) {
		aotag_pdev_print(alert, pdev,
				"Unable to allocate platform memory\n");
		ret = -ENOMEM;
		goto out;
	}

	dev->platform_data = pplat;

	/*
	 * reading PMC reg base here from the already parsed pmc DT node,
	 * rather than adding this (an extra) value to the DT.
	 */
	pplat->reg_info.pmc_base = of_iomap(pmc_np, 0);
	if (!pplat->reg_info.pmc_base) {
		ret = -ENOMEM;
		aotag_pdev_print(alert, pdev, " - unable to map PMC\n");
		goto out;
	}

	/*
	 * Initialize plat data with defaults
	 */
	pplat->sensor_info.shutdown_temp = DEFAULT_SHUTDOWN_TEMP;

out:
	return ret;
}

static int aotag_parse_sensor_params(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np =  PDEV2OF_NODE(pdev);
	struct aotag_sensor_info_t *psensor_info = PDEV2SENSOR_INFO(pdev);
	struct fuse_info_t *pfuse = PDEV2FUSE_INFO(pdev);
	struct sensor_common_params_t *pcparams =
		SENSOR_INFO2COMMON_PARAMS(psensor_info);

	/*
	 * Parse Common sensor data
	 */
	ret =
		OF_PROPERTY_READ(u32, np, "sensor-params-tall",
				&pcparams->tall) ||
		OF_PROPERTY_READ(u32, np, "sensor-params-tiddq",
				&pcparams->tiddq) ||
		OF_PROPERTY_READ(u32, np, "sensor-params-ten-count",
				&pcparams->count) ||
		OF_PROPERTY_READ(u32, np, "sensor-params-tsample",
				&pcparams->tsample) ||
		OF_PROPERTY_READ(u32, np, "sensor-params-pdiv",
				&pcparams->pdiv) ||
		OF_PROPERTY_READ(u32, np, "sensor-params-tsamp-ate",
				&pcparams->tsamp_ate) ||
		OF_PROPERTY_READ(u32, np, "sensor-params-pdiv-ate",
				&pcparams->pdiv_ate) ||
		OF_PROPERTY_READ(u32, np, "sensor-id",
				&psensor_info->id) ||
		OF_PROPERTY_READ(u32, np, "advertised-sensor-id",
				&psensor_info->advertised_id) ||
		OF_PROPERTY_READ(string, np, "sensor-name",
				&psensor_info->name) ||
		OF_PROPERTY_READ(s32, np, "sensor-nominal-temp-cp",
				&pfuse->nominal_temp_cp);
		OF_PROPERTY_READ(s32, np, "sensor-nominal-temp-ft",
				&pfuse->nominal_temp_ft);
		OF_PROPERTY_READ(s32, np, "sensor-compensation-a",
				&pfuse->compensation_a);
		OF_PROPERTY_READ(s32, np, "sensor-compensation-b",
				&pfuse->compensation_b);

	if (ret < 0)
		goto out;

	aotag_pdev_print(info, pdev, "nominal ft -- %d\n",
			pfuse->nominal_temp_ft);
	aotag_pdev_print(info, pdev, "tall -- %d\n", pcparams->tall);
	aotag_pdev_print(info, pdev, "tiddq -- %d\n", pcparams->tiddq);
	aotag_pdev_print(info, pdev, "ten-count -- %d\n", pcparams->count);
	aotag_pdev_print(info, pdev, "tsample -- %d\n", pcparams->tsample);
	aotag_pdev_print(info, pdev, "pdiv -- %d\n", pcparams->pdiv);
	aotag_pdev_print(info, pdev, "nominal cp -- %d\n",
			pfuse->nominal_temp_cp);
	aotag_pdev_print(info, pdev, "pdiv-ate -- %d\n", pcparams->pdiv_ate);
	aotag_pdev_print(info, pdev, "tsamp-ate -- %d\n", pcparams->tsamp_ate);
	aotag_pdev_print(info, pdev, "compensation A,B -- %d,%d\n",
			pfuse->compensation_a, pfuse->compensation_b);


	psensor_info->sops.get_temp = aotag_get_temp_generic;
	psensor_info->sops.get_trend = aotag_get_trend_generic;
	psensor_info->sops.trip_update = aotag_trip_update;
	aotag_pdev_print(info, pdev,
			"sensor found :ID %d, Name: %s\n",
			psensor_info->id,
			psensor_info->name);

out:
	if (ret < 0)
		aotag_pdev_print(alert, pdev, "sensors params failed\n");

	return ret;
}

static void aotag_unregister_sensors(struct platform_device *pdev)
{
	struct device *dev = PDEV2DEVICE(pdev);
	struct aotag_sensor_info_t *ps_info = PDEV2SENSOR_INFO(pdev);
	struct thermal_zone_device *ptz = ps_info->tzd;
	thermal_zone_of_sensor_unregister(dev, ptz);
}
static int aotag_register_sensors(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = PDEV2DEVICE(pdev);
	struct aotag_sensor_info_t *ps_info = PDEV2SENSOR_INFO(pdev);
	struct thermal_zone_device *ptz = NULL;

	aotag_pdev_print(info, pdev,
			"Registering sensor %d\n", ps_info->id);

	ptz = NULL;
	ptz = thermal_zone_of_sensor_register2(
			dev,
			ps_info->id,
			pdev,
			&ps_info->sops);
	if (IS_ERR(ptz)) {
		aotag_pdev_print(alert, pdev,
				"Failed to register sensor %d\n",
				ps_info->id);
		ret = PTR_ERR(ptz);
		goto out;
	}
	ps_info->tzd = ptz;
	aotag_pdev_print(alert, pdev, "Bound to TZ : ID %d\n",
			ps_info->tzd->id);
out:
	return ret;
}

static int aotag_read_fuses(struct platform_device *pdev)
{
	int ret = 0;
	struct aotag_sensor_info_t *ps_info = PDEV2SENSOR_INFO(pdev);
	struct sensor_common_params_t *pcparams =
		SENSOR_INFO2COMMON_PARAMS(ps_info);
	struct fuse_info_t *pfuse = SENSOR_INFO2FUSE_INFO(ps_info);
	u32 fuse_val = 0;
	s32 calib, delta_ts_reading, delta_temp;
	u32 reg;
	int fuse_rev = 0;
	s64 numerator, denominator;


	fuse_rev = tegra_fuse_calib_base_get_cp(NULL, NULL);
	if (0 > fuse_rev) {
		ret = fuse_rev;
		aotag_pdev_print(alert, pdev, "Invalid fuse rev\n");
		goto out;
	}

	pfuse->common_fuse_rev = fuse_rev;

	aotag_pdev_print(info, pdev, "reading fuse value for sensor %d\n",
			ps_info->id);

	/*
	 * Read average raw calibration readings and temperature shifts
	 */
	if ((0 > tegra_fuse_calib_base_get_cp(&(pfuse->avg_ts_reading_cp),
					&(pfuse->shifted_temp_cp))) ||
			(0 > tegra_fuse_calib_base_get_ft(
					&(pfuse->avg_ts_reading_ft),
					&(pfuse->shifted_temp_ft)))) {
		aotag_pdev_print(alert, pdev,
				"failed to read fuse values\n");
		ret = -EINVAL;
		goto out;
	}

	aotag_pdev_print(info, pdev, "base-cp:%x shifted-cp:%x\n",
			pfuse->avg_ts_reading_cp,
			pfuse->shifted_temp_cp);
	aotag_pdev_print(info, pdev, "base-ft:%x shifted-ft:%x\n",
			pfuse->avg_ts_reading_ft,
			pfuse->shifted_temp_ft);

	pfuse->actual_temp_cp =
		(1000*pfuse->nominal_temp_cp) + (500*pfuse->shifted_temp_cp);
	pfuse->actual_temp_ft =
		(1000*pfuse->nominal_temp_ft) + (500*pfuse->shifted_temp_ft);

	pfuse->actual_temp_cp /= 100;
	pfuse->actual_temp_ft /= 100;

	aotag_pdev_print(info, pdev, "actual_aotag_cp %d, ft %d\n",
			pfuse->actual_temp_cp, pfuse->actual_temp_ft);


	/*
	 * Now read the reading delta's per sensor that happened during
	 * calibration
	 */
	ret = tegra_fuse_get_tsensor_calib(ps_info->advertised_id,
			&fuse_val);
	if (ret) {
		aotag_pdev_print(alert, pdev,
				"unable to read fuse tsensor\n");
		goto out;
	}

	aotag_pdev_print(info, pdev,
			"- Tsens calib value - 0x%x\n", fuse_val);

	aotag_pdev_print(info, pdev, "CPmask %x FTmask %x\n",
			AOTAG_FUSE_CALIB_CP_MASK,
			AOTAG_FUSE_CALIB_FT_MASK);
	GET_REG(&calib, AOTAG_FUSE_CALIB_CP, &fuse_val);
	aotag_pdev_print(info, pdev, "Tsense calib CP - 0x%x\n", calib);

	if (calib & 0x1000) {	/* negative value */
		calib = (calib ^ 0x1FFF) + 1;
		pfuse->actual_ts_reading_cp =
			(pfuse->avg_ts_reading_cp * 64) - calib;
	} else {
		pfuse->actual_ts_reading_cp =
			((pfuse->avg_ts_reading_cp) * 64) + calib;
	}
	aotag_pdev_print(info, pdev, "Tsense Actual CP d.%d\n",
			pfuse->actual_ts_reading_cp);

	GET_REG(&calib, AOTAG_FUSE_CALIB_FT, &fuse_val);
	aotag_pdev_print(info, pdev, "Tsense calib FT - 0x%x\n", calib);
	if (calib & 0x1000) {	/* negative value */
		calib = (calib ^ 0x1FFF) + 1;
		pfuse->actual_ts_reading_ft =
			(pfuse->avg_ts_reading_ft * 32) - calib;
	} else {
		pfuse->actual_ts_reading_ft =
			((pfuse->avg_ts_reading_ft) * 32) + calib;
	}
	aotag_pdev_print(info, pdev, "Tsense Actual FT d.%d\n",
			pfuse->actual_ts_reading_ft);

	delta_ts_reading =
		pfuse->actual_ts_reading_ft - pfuse->actual_ts_reading_cp;
	delta_temp = pfuse->actual_temp_ft - pfuse->actual_temp_cp;
	aotag_pdev_print(info, pdev,
			"delta ts %d, temp %d\n", delta_ts_reading, delta_temp);

	/*
	 * THERM_A
	 */
	numerator =
		(((s64)(delta_temp))<<13) *
		pcparams->tsamp_ate * pcparams->pdiv;
	denominator =
		(s64)(delta_ts_reading) *
		pcparams->tsample * pcparams->pdiv * 10;
	do_div(numerator, denominator);
	ps_info->therm_a = numerator;
	ps_info->therm_a = (ps_info->therm_a) * (pfuse->compensation_a)/10000;

	/*
	 * THERM_B
	 */
	numerator = (
			((s64)(pfuse->actual_ts_reading_ft *
				pfuse->actual_temp_cp)) -
			((s64)(pfuse->actual_ts_reading_cp *
				pfuse->actual_temp_ft)));
	denominator = (s64)delta_ts_reading * 10;
	if (numerator > 0) {
		do_div(numerator, denominator);
		ps_info->therm_b = numerator;
	} else {
		numerator *= (-1);
		do_div(numerator, denominator);
		ps_info->therm_b = (-1) * numerator;
	}

	if (ps_info->therm_b > 0) {
		ps_info->therm_b = ((ps_info->therm_b * pfuse->compensation_a)
				+ pfuse->compensation_b)/10000;
	} else {
		ps_info->therm_b = (-1) *
			(((-1) * ((ps_info->therm_b * pfuse->compensation_a)
				  + pfuse->compensation_b))/10000);
	}
	/*
	 * Now write the thermA,B registers
	 */
	aotag_pdev_print(alert, pdev, "thermA-%d, thermB-%d\n",
			ps_info->therm_a, ps_info->therm_b);

	/* convert to 0.5C precision */
	ps_info->therm_a *= 2;
	ps_info->therm_b *= 2;

	reg = 0;
	FILL_REG_OR(ps_info->therm_a, CONFIG2_THERM_A, &reg);
	FILL_REG_OR(ps_info->therm_b, CONFIG2_THERM_B, &reg);
	reg_write(pdev, pmc, reg, PMC_TSENSOR_CONFIG2);

out:
	return ret;
}
static int aotag_hw_init(struct platform_device *pdev)
{
	int ret = 0;
	unsigned long reg = 0;
	struct aotag_sensor_info_t *ps_info = PDEV2SENSOR_INFO(pdev);
	struct sensor_common_params_t *pcparams =
		SENSOR_INFO2COMMON_PARAMS(ps_info);

	/*
	 * init CONFIG_0 registers
	 */
	reg = 0;
	clear_bit(CONFIG0_STOP_POS, &reg);
	clear_bit(CONFIG0_RO_SEL_POS, &reg);
	clear_bit(CONFIG0_STATUS_CLR_POS, &reg);
	FILL_REG_OR(pcparams->tall, CONFIG0_TALL, &reg);
	reg_write(pdev, pmc, reg, PMC_TSENSOR_CONFIG0);

	/*
	 * init CONFIG_1 registers
	 */
	reg = 0;
	FILL_REG_OR(pcparams->count, CONFIG1_TEN_COUNT, &reg);
	FILL_REG_OR(pcparams->tiddq, CONFIG1_TIDDQ_EN, &reg);
	pcparams->tsample--;
	FILL_REG_OR(pcparams->tsample, CONFIG1_TSAMPLE, &reg);
	pcparams->tsample++;
	set_bit(CONFIG1_TEMP_ENABLE_POS, &reg);
	reg_write(pdev, pmc, reg, PMC_TSENSOR_CONFIG1);

	/*
	 * init CONFIG_2 registers
	 */
	reg = 0;
	FILL_REG_OR(pcparams->pdiv, TSENSOR_PDIV, &reg);
	reg_write(pdev, pmc, reg, PMC_TSENSOR_PDIV0);

	/*
	 * Enable AOTAG + THERMTRIP
	 */
	ret = aotag_update_shutdown_temp(pdev);
	if (unlikely(ret)) {
		aotag_pdev_print(alert, pdev,
				"failed to get shutdown temp, using default\n");
	}

	reg = 0;
	set_bit(CFG_TAG_EN_POS, &reg);
	clear_bit(CFG_DISABLE_CLK_POS, &reg);
	set_bit(CFG_THERMTRIP_EN_POS, &reg);
	reg_write(pdev, pmc, reg, PMC_AOTAG_CFG);
	aotag_pdev_print(info, pdev, "AOTAG EN %x\n",
			reg_read(pdev, pmc, PMC_AOTAG_CFG));

	return ret;
}

/*
 * Driver Initialization code begins here
 */

static int __init tegra_aotag_probe(struct platform_device *pdev)
{
	int ret = 0;

	if (unlikely(!pdev)) {
		aotag_print(alert, "Probe failed\n");
		return -EINVAL;
	}

	if (unlikely(!(pdev->dev.of_node))) {
		ret = -EINVAL;
		aotag_pdev_print(alert, pdev, "OF node not found\n");
		goto out;
	}

	aotag_pdev_print(info, pdev, "AOTAG probe started\n");
	ret = aotag_init(pdev);
	if (unlikely(ret)) {
		aotag_pdev_print(alert, pdev, "failed aotag init\n");
		goto out;
	}

	ret = aotag_parse_sensor_params(pdev);
	if (unlikely(ret)) {
		aotag_pdev_print(alert, pdev, "failed sensor params read\n");
		goto out;
	}

	ret = aotag_read_fuses(pdev);
	if (unlikely(ret)) {
		aotag_pdev_print(alert, pdev, "failed fuse init\n");
		goto out;
	}

	ret = aotag_register_sensors(pdev);
	if (unlikely(ret)) {
		aotag_pdev_print(alert, pdev, "failed sensor registration\n");
		goto out;
	}

	ret = aotag_hw_init(pdev);
	if (unlikely(ret)) {
		aotag_pdev_print(alert, pdev, "failed hardware init\n");
		aotag_unregister_sensors(pdev);
		/* unregister the sensor if HW init fails */
		goto out;
	}

out:
	aotag_pdev_print(alert, pdev, "Probe done %s:%d\n",
			(ret ? "[FAILURE]" : "[SUCCESS]"), ret);
	return ret;
}

static struct platform_driver tegra_aotag_driver = {
	/*
	 * not using the .probe here to allow __init usage.
	 */
	.driver = {
		.name = "tegra_aotag",
		.owner = THIS_MODULE,
		.of_match_table = tegra_aotag_of_match,
	},
};

static int __init tegra_aotag_driver_init(void)
{
	return platform_driver_probe(&tegra_aotag_driver, tegra_aotag_probe);
}
module_init(tegra_aotag_driver_init);

MODULE_AUTHOR("NVIDIA Corp.");
MODULE_DESCRIPTION("Tegra AOTAG driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:tegra-aotag");
