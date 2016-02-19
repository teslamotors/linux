/*
 * ADC1x8S102 SPI ADC driver
 *
 * Copyright(c) 2013 Intel Corporation.
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


#ifndef __LINUX_PLATFORM_DATA_ADC1x8S102_H__
#define __LINUX_PLATFORM_DATA_ADC1x8S102_H__


/**
 * struct adc1x8s102_platform_data - Platform data for the adc1x8s102 ADC driver
 * @ext_vin: External input voltage range for all voltage input channels
 *	This is the voltage level of pin VA in millivolts
 **/
struct adc1x8s102_platform_data {
	u16  ext_vin;
};

#endif /* __LINUX_PLATFORM_DATA_ADC1x8S102_H__ */
