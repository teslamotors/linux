/*
 * 3-axis accelerometer driver supporting SPI Bosch-Sensortec accelerometer chip
 * Copyright © 2015 Pengutronix, Markus Pargmann <mpa@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/device.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/acpi.h>
#include <linux/regmap.h>
#include <linux/spi/spi.h>

#include "smi130-accel.h"

static int smi130_accel_probe(struct spi_device *spi)
{
	struct regmap *regmap;
	const struct spi_device_id *id = spi_get_device_id(spi);

	regmap = devm_regmap_init_spi(spi, &smi130_regmap_conf);
	if (IS_ERR(regmap)) {
		dev_err(&spi->dev, "Failed to initialize spi regmap\n");
		return PTR_ERR(regmap);
	}

	return smi130_accel_core_probe(&spi->dev, regmap, id->name);
}

static int smi130_accel_remove(struct spi_device *spi)
{
	return smi130_accel_core_remove(&spi->dev);
}

static const struct spi_device_id smi130_accel_id[] = {
	{"smi130_accel",	smi130},
	{}
};
MODULE_DEVICE_TABLE(spi, smi130_accel_id);

static struct spi_driver smi130_accel_driver = {
	.driver = {
		.name	= "smi130_accel_spi",
	},
	.probe		= smi130_accel_probe,
	.remove		= smi130_accel_remove,
	.id_table	= smi130_accel_id,
};
module_spi_driver(smi130_accel_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("SMI130 SPI accelerometer driver");
