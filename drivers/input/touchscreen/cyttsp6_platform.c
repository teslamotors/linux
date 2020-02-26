/*
 * cyttsp6_platform.c
 * Cypress TrueTouch(TM) Standard Product V4 Platform Module.
 * For use with Cypress touchscreen controllers.
 * Supported parts include:
 * CY8CTMA46X
 * CY8CTMA1036/768
 * CYTMA445
 *
 * Copyright (C) 2013-2015 Cypress Semiconductor
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#include "cyttsp6_regs.h"

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_PLATFORM_FW_UPGRADE
#include "cyttsp6_img.h"
static struct cyttsp6_touch_firmware cyttsp6_firmware = {
	.img = cyttsp6_img,
	.size = ARRAY_SIZE(cyttsp6_img),
	.ver = cyttsp6_ver,
	.vsize = ARRAY_SIZE(cyttsp6_ver),
};
#else
static struct cyttsp6_touch_firmware cyttsp6_firmware = {
	.img = NULL,
	.size = 0,
	.ver = NULL,
	.vsize = 0,
};
#endif

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_PLATFORM_TTCONFIG_UPGRADE
#include "cyttsp6_params.h"
static struct touch_settings cyttsp6_sett_param_regs = {
	.data = (uint8_t *)&cyttsp6_param_regs[0],
	.size = ARRAY_SIZE(cyttsp6_param_regs),
	.tag = 0,
};

static struct touch_settings cyttsp6_sett_param_size = {
	.data = (uint8_t *)&cyttsp6_param_size[0],
	.size = ARRAY_SIZE(cyttsp6_param_size),
	.tag = 0,
};

static struct cyttsp6_touch_config cyttsp6_ttconfig = {
	.param_regs = &cyttsp6_sett_param_regs,
	.param_size = &cyttsp6_sett_param_size,
	.fw_ver = ttconfig_fw_ver,
	.fw_vsize = ARRAY_SIZE(ttconfig_fw_ver),
};
#else
static struct cyttsp6_touch_config cyttsp6_ttconfig = {
	.param_regs = NULL,
	.param_size = NULL,
	.fw_ver = NULL,
	.fw_vsize = 0,
};
#endif

static inline void cyttsp6_gpio_lock_as_irq(int irq_gpio, struct irq_data *d){
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 2, 0))
	return;
#else
	struct gpio_chip *chip = gpio_to_chip(irq_gpio);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0))
	gpiochip_lock_as_irq(chip,d->hwirq);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0))
	gpio_lock_as_irq(chip,d->hwirq);
#endif
#endif
}

static inline void cyttsp6_gpio_unlock_as_irq(int irq_gpio, struct irq_data *d){
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 2, 0))
	return;
#else
	struct gpio_chip *chip = gpio_to_chip(irq_gpio);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0))
	gpiochip_unlock_as_irq(chip,d->hwirq);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0))
	gpio_unlock_as_irq(chip,d->hwirq);
#endif
#endif
}

struct cyttsp6_loader_platform_data _cyttsp6_loader_platform_data = {
	.fw = &cyttsp6_firmware,
	.ttconfig = &cyttsp6_ttconfig,
	.flags = CY_LOADER_FLAG_CALIBRATE_AFTER_FW_UPGRADE,
};

int cyttsp6_xres(struct cyttsp6_core_platform_data *pdata,
		struct device *dev)
{

	int rst_gpio = pdata->rst_gpio;
	int rc = 0;

	/*
	 *	The M3 display panel inverts the XRES signal output from the bmp.
	 *	Note that we originally extended GPIO timeouts to account for the long ser/deser path. 
 	 *	We found this increased boot time so we increased it slightly from the default
	 * 	values (20ms and 40ms).
	*/

	dev_dbg(dev, "%s: setting XRES direction to output=%d\n", __func__, 0);
	rc = gpio_direction_output(rst_gpio, 0);
	if (rc < 0)
		return rc;
	msleep(50);
	dev_dbg(dev, "%s: setting XRES gpio=%d\n", __func__, 1);
	gpio_set_value(rst_gpio, 1);
	msleep(100);
	dev_dbg(dev, "%s: setting XRES gpio=%d\n", __func__, 0);
	gpio_set_value(rst_gpio, 0);
	msleep(50);
	dev_info(dev,
		"%s: RESET CYTTSP gpio=%d r=%d\n", __func__,
		pdata->rst_gpio, rc);
   
	return rc;

}

int _cyttsp6_gpio_request(struct device *dev, int gpio)
{
	int rc;

	rc = gpio_request(gpio, NULL);
	if(rc < 0) {
		gpio_free(gpio);
		rc = gpio_request(gpio,NULL);
	}
	if(rc < 0) {
		dev_err(dev,"%s: Fail request gpio=%d\n",__func__,gpio);
	}
	return rc;
}

int cyttsp6_init(struct cyttsp6_core_platform_data *pdata,
		int on, struct device *dev)
{
	int rst_gpio = pdata->rst_gpio;
	int irq_gpio = pdata->irq_gpio;
	int err_gpio = pdata->err_gpio;
	int rc = 0;

	if (on) {
		rc = _cyttsp6_gpio_request(dev,rst_gpio);
		if (rc < 0) {
			dev_err(dev,"%s: Fail request rst_gpio\n",__func__);
			return rc;
		}
		rc = gpio_direction_output(rst_gpio, 1);
		if (rc < 0) {
			pr_err("%s: Fail set output gpio=%d\n",__func__,
					rst_gpio);
			goto free_rst;
		}
		rc = _cyttsp6_gpio_request(dev, irq_gpio);
		if (rc < 0) {
			dev_err(dev,"%s: Fail request irq_gpio\n",__func__);
			goto free_rst;
		}
		rc = gpio_direction_input(irq_gpio);
		if (rc < 0) {
			pr_err("%s: Fail set input gpio=%d\n",__func__,
					irq_gpio);
			goto free_irq;
		}
		rc = _cyttsp6_gpio_request(dev,err_gpio);
		if (rc < 0) {
			dev_err(dev,"%s: Fail request irq_gpio\n",__func__);
			goto free_irq;
		}
		rc = gpio_direction_input(err_gpio);
		if (rc < 0) {
			pr_err("%s: Fail set error gpio=%d\n",__func__,
					err_gpio);
			goto free_err;
		}

		dev_info(dev, "%s: INIT CYTTSP RST gpio=%d and "
				"IRQ gpio=%d ERR gpio=%d\n",__func__, rst_gpio,
			       irq_gpio, err_gpio);
		return rc;
	}

free_err:
	gpio_free(err_gpio);
free_irq:
	gpio_free(irq_gpio);
free_rst:
	gpio_free(rst_gpio);

	return rc;
}

static int cyttsp6_wakeup(struct cyttsp6_core_platform_data *pdata,
		struct device *dev, atomic_t *ignore_irq)
{
	int irq_gpio = pdata->irq_gpio;
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct irq_data *d = irq_get_irq_data(cd->irq);
	int rc = 0;

	if (ignore_irq)
		atomic_set(ignore_irq, 1);
	if(!d){
		dev_err(dev,"%s:",__func__);
		return -EINVAL;
	}

	cyttsp6_gpio_unlock_as_irq(irq_gpio, d);
	rc = gpio_direction_output(irq_gpio, 0);
	if (rc < 0) {
		if (ignore_irq)
			atomic_set(ignore_irq, 0);
		dev_err(dev,
			"%s: Fail set output gpio=%d\n",
			__func__, irq_gpio);
	} else {
		if (ignore_irq)
			atomic_set(ignore_irq, 0);
		rc = gpio_direction_input(irq_gpio);
		if (rc < 0) {
			dev_err(dev,
				"%s: Fail set input gpio=%d\n",
				__func__, irq_gpio);
		}
		else{
			cyttsp6_gpio_lock_as_irq(irq_gpio,d);
		}
	}

	dev_info(dev,
		"%s: WAKEUP CYTTSP gpio=%d r=%d\n", __func__,
		irq_gpio, rc);

	return rc;
}

static int cyttsp6_sleep(struct cyttsp6_core_platform_data *pdata,
		struct device *dev, atomic_t *ignore_irq)
{
	return 0;
}

int cyttsp6_power(struct cyttsp6_core_platform_data *pdata,
		int on, struct device *dev, atomic_t *ignore_irq)
{
	if (on)
		return cyttsp6_wakeup(pdata, dev, ignore_irq);

	return cyttsp6_sleep(pdata, dev, ignore_irq);
}

int cyttsp6_irq_stat(struct cyttsp6_core_platform_data *pdata,
		struct device *dev)
{
	return gpio_get_value(pdata->irq_gpio);
}

int cyttsp6_error_stat(struct cyttsp6_core_platform_data *pdata,
		struct device *dev)
{
	return gpio_get_value(pdata->err_gpio);
}

#ifdef CYTTSP6_DETECT_HW
int cyttsp6_detect(struct cyttsp6_core_platform_data *pdata,
		struct device *dev, cyttsp6_platform_read read)
{
	int retry = 3;
	int rc;
	char buf[1];

	while (retry--) {
		/* Perform reset, wait for 100 ms and perform read */
		dev_vdbg(dev, "%s: Performing a reset\n", __func__);
		pdata->xres(pdata, dev);
		msleep(100);
		rc = read(dev, 0, buf, 1);
		if (!rc)
			return 0;

		dev_vdbg(dev, "%s: Read unsuccessful, try=%d\n",
			__func__, 3 - retry);
	}

	return rc;
}
#endif
