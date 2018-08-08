/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2016 - 2018 Intel Corporation
 *
 * Author: Tommi Franttila <tommi.franttila@intel.com>
 *
 */

#ifndef __CRLMODULE_ov5670_CONFIGURATION_H_
#define __CRLMODULE_ov5670_CONFIGURATION_H_

#include "crlmodule-nvm.h"
#include "crlmodule-sensor-ds.h"

static struct crl_register_write_rep ov5670_pll_840mbps[] = {
	{ 0x030a, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0300, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x0301, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0302, CRL_REG_LEN_08BIT, 0x78 },
	{ 0x0304, CRL_REG_LEN_08BIT, 0x03 },
	{ 0x0303, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0305, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0306, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0312, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x030b, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x030c, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x030d, CRL_REG_LEN_08BIT, 0x1e },
	{ 0x030f, CRL_REG_LEN_08BIT, 0x06 },
	{ 0x030e, CRL_REG_LEN_08BIT, 0x00 },
};

static struct crl_register_write_rep ov5670_powerup_regset[] = {
	{ 0x0103, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0100, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3000, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3002, CRL_REG_LEN_08BIT, 0x21 },
	{ 0x3005, CRL_REG_LEN_08BIT, 0xf0 },
	{ 0x3007, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3015, CRL_REG_LEN_08BIT, 0x0f },
	{ 0x3018, CRL_REG_LEN_08BIT, 0x32 },
	{ 0x301a, CRL_REG_LEN_08BIT, 0xf0 },
	{ 0x301b, CRL_REG_LEN_08BIT, 0xf0 },
	{ 0x301c, CRL_REG_LEN_08BIT, 0xf0 },
	{ 0x301d, CRL_REG_LEN_08BIT, 0xf0 },
	{ 0x301e, CRL_REG_LEN_08BIT, 0xf0 },
	{ 0x3021, CRL_REG_LEN_08BIT, 0x03 },
	{ 0x3030, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3031, CRL_REG_LEN_08BIT, 0x0a },
	{ 0x303c, CRL_REG_LEN_08BIT, 0xff },
	{ 0x303e, CRL_REG_LEN_08BIT, 0xff },
	{ 0x3040, CRL_REG_LEN_08BIT, 0xf0 },
	{ 0x3041, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3042, CRL_REG_LEN_08BIT, 0xf0 },
	{ 0x3106, CRL_REG_LEN_08BIT, 0x11 },
	{ 0x3500, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3502, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3503, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x3504, CRL_REG_LEN_08BIT, 0x03 },
	{ 0x3505, CRL_REG_LEN_08BIT, 0x83 },
	{ 0x3508, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x3509, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x350e, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x350f, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3510, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3511, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x3512, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3601, CRL_REG_LEN_08BIT, 0xc8 },
	{ 0x3610, CRL_REG_LEN_08BIT, 0x88 },
	{ 0x3612, CRL_REG_LEN_08BIT, 0x48 },
	{ 0x3614, CRL_REG_LEN_08BIT, 0x5b },
	{ 0x3615, CRL_REG_LEN_08BIT, 0x96 },
	{ 0x3621, CRL_REG_LEN_08BIT, 0xd0 },
	{ 0x3622, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3623, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3633, CRL_REG_LEN_08BIT, 0x13 },
	{ 0x3634, CRL_REG_LEN_08BIT, 0x13 },
	{ 0x3635, CRL_REG_LEN_08BIT, 0x13 },
	{ 0x3636, CRL_REG_LEN_08BIT, 0x13 },
	{ 0x3645, CRL_REG_LEN_08BIT, 0x13 },
	{ 0x3646, CRL_REG_LEN_08BIT, 0x82 },
	{ 0x3650, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3652, CRL_REG_LEN_08BIT, 0xff },
	{ 0x3655, CRL_REG_LEN_08BIT, 0x20 },
	{ 0x3656, CRL_REG_LEN_08BIT, 0xff },
	{ 0x365a, CRL_REG_LEN_08BIT, 0xff },
	{ 0x365e, CRL_REG_LEN_08BIT, 0xff },
	{ 0x3668, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x366a, CRL_REG_LEN_08BIT, 0x07 },
	{ 0x366e, CRL_REG_LEN_08BIT, 0x10 },
	{ 0x366d, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x366f, CRL_REG_LEN_08BIT, 0x80 },
	{ 0x3700, CRL_REG_LEN_08BIT, 0x28 },
	{ 0x3701, CRL_REG_LEN_08BIT, 0x10 },
	{ 0x3702, CRL_REG_LEN_08BIT, 0x3a },
	{ 0x3703, CRL_REG_LEN_08BIT, 0x19 },
	{ 0x3704, CRL_REG_LEN_08BIT, 0x10 },
	{ 0x3705, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3706, CRL_REG_LEN_08BIT, 0x66 },
	{ 0x3707, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x3708, CRL_REG_LEN_08BIT, 0x34 },
	{ 0x3709, CRL_REG_LEN_08BIT, 0x40 },
	{ 0x370a, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x370b, CRL_REG_LEN_08BIT, 0x1b },
	{ 0x3714, CRL_REG_LEN_08BIT, 0x24 },
	{ 0x371a, CRL_REG_LEN_08BIT, 0x3e },
	{ 0x3733, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3734, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x373a, CRL_REG_LEN_08BIT, 0x05 },
	{ 0x373b, CRL_REG_LEN_08BIT, 0x06 },
	{ 0x373c, CRL_REG_LEN_08BIT, 0x0a },
	{ 0x373f, CRL_REG_LEN_08BIT, 0xa0 },
	{ 0x3755, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3758, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x375b, CRL_REG_LEN_08BIT, 0x0e },
	{ 0x3766, CRL_REG_LEN_08BIT, 0x5f },
	{ 0x3768, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3769, CRL_REG_LEN_08BIT, 0x22 },
	{ 0x3773, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x3774, CRL_REG_LEN_08BIT, 0x1f },
	{ 0x3776, CRL_REG_LEN_08BIT, 0x06 },
	{ 0x37a0, CRL_REG_LEN_08BIT, 0x88 },
	{ 0x37a1, CRL_REG_LEN_08BIT, 0x5c },
	{ 0x37a7, CRL_REG_LEN_08BIT, 0x88 },
	{ 0x37a8, CRL_REG_LEN_08BIT, 0x70 },
	{ 0x37aa, CRL_REG_LEN_08BIT, 0x88 },
	{ 0x37ab, CRL_REG_LEN_08BIT, 0x48 },
	{ 0x37b3, CRL_REG_LEN_08BIT, 0x66 },
	{ 0x37c2, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x37c5, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x37c8, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3800, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3801, CRL_REG_LEN_08BIT, 0x0c },
	{ 0x3802, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3803, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x3804, CRL_REG_LEN_08BIT, 0x0a },
	{ 0x3805, CRL_REG_LEN_08BIT, 0x33 },
	{ 0x3806, CRL_REG_LEN_08BIT, 0x07 },
	{ 0x3807, CRL_REG_LEN_08BIT, 0xa3 },
	{ 0x3811, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x3813, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x3815, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x3816, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3817, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3818, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3819, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3820, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3822, CRL_REG_LEN_08BIT, 0x48 },
	{ 0x3826, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3827, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x3830, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x3836, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x3837, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3838, CRL_REG_LEN_08BIT, 0x10 },
	{ 0x3841, CRL_REG_LEN_08BIT, 0xff }, /* Auto size function enabled */
	{ 0x3846, CRL_REG_LEN_08BIT, 0x48 },
	{ 0x3861, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3862, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x3863, CRL_REG_LEN_08BIT, 0x06 },
	{ 0x3a11, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x3a12, CRL_REG_LEN_08BIT, 0x78 },
	{ 0x3b00, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3b02, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3b03, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3b04, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3b05, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3c00, CRL_REG_LEN_08BIT, 0x89 },
	{ 0x3c01, CRL_REG_LEN_08BIT, 0xab },
	{ 0x3c02, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x3c03, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3c04, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3c05, CRL_REG_LEN_08BIT, 0x03 },
	{ 0x3c06, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3c07, CRL_REG_LEN_08BIT, 0x05 },
	{ 0x3c0c, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3c0d, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3c0e, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3c0f, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3c40, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3c41, CRL_REG_LEN_08BIT, 0xa3 },
	{ 0x3c43, CRL_REG_LEN_08BIT, 0x7d },
	{ 0x3c45, CRL_REG_LEN_08BIT, 0xd7 },
	{ 0x3c47, CRL_REG_LEN_08BIT, 0xfc },
	{ 0x3c50, CRL_REG_LEN_08BIT, 0x05 },
	{ 0x3c52, CRL_REG_LEN_08BIT, 0xaa },
	{ 0x3c54, CRL_REG_LEN_08BIT, 0x71 },
	{ 0x3c56, CRL_REG_LEN_08BIT, 0x80 },
	{ 0x3d85, CRL_REG_LEN_08BIT, 0x17 },
	{ 0x3d8d, CRL_REG_LEN_08BIT, 0xea },
	{ 0x3f03, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3f0a, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3f0b, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4001, CRL_REG_LEN_08BIT, 0x60 },
	{ 0x4009, CRL_REG_LEN_08BIT, 0x0d },
	{ 0x4017, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x4020, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4021, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4022, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4023, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4024, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4025, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4026, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4027, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4028, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4029, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x402a, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x402b, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x402c, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x402d, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x402e, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x402f, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4040, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4041, CRL_REG_LEN_08BIT, 0x03 },
	{ 0x4042, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4043, CRL_REG_LEN_08BIT, 0x7A },
	{ 0x4044, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4045, CRL_REG_LEN_08BIT, 0x7A },
	{ 0x4046, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4047, CRL_REG_LEN_08BIT, 0x7A },
	{ 0x4048, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4049, CRL_REG_LEN_08BIT, 0x7A },
	{ 0x4303, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4307, CRL_REG_LEN_08BIT, 0x30 },
	{ 0x4500, CRL_REG_LEN_08BIT, 0x58 },
	{ 0x4501, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x4502, CRL_REG_LEN_08BIT, 0x40 },
	{ 0x4503, CRL_REG_LEN_08BIT, 0x10 },
	{ 0x4508, CRL_REG_LEN_08BIT, 0xaa },
	{ 0x4509, CRL_REG_LEN_08BIT, 0xaa },
	{ 0x450a, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x450b, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4700, CRL_REG_LEN_08BIT, 0xa4 },
	{ 0x4800, CRL_REG_LEN_08BIT, 0x4c },
	{ 0x4816, CRL_REG_LEN_08BIT, 0x53 },
	{ 0x481f, CRL_REG_LEN_08BIT, 0x40 },
	{ 0x4837, CRL_REG_LEN_08BIT, 0x13 },
	{ 0x5000, CRL_REG_LEN_08BIT, 0x56 },
	{ 0x5001, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x5002, CRL_REG_LEN_08BIT, 0x28 },
	{ 0x5004, CRL_REG_LEN_08BIT, 0x0c },
	{ 0x5006, CRL_REG_LEN_08BIT, 0x0c },
	{ 0x5007, CRL_REG_LEN_08BIT, 0xe0 },
	{ 0x5008, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x5009, CRL_REG_LEN_08BIT, 0xb0 },
	{ 0x5901, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x5a01, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x5a03, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x5a04, CRL_REG_LEN_08BIT, 0x0c },
	{ 0x5a05, CRL_REG_LEN_08BIT, 0xe0 },
	{ 0x5a06, CRL_REG_LEN_08BIT, 0x09 },
	{ 0x5a07, CRL_REG_LEN_08BIT, 0xb0 },
	{ 0x5a08, CRL_REG_LEN_08BIT, 0x06 },
	{ 0x5e00, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3734, CRL_REG_LEN_08BIT, 0x40 },
	{ 0x5b00, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x5b01, CRL_REG_LEN_08BIT, 0x10 },
	{ 0x5b02, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x5b03, CRL_REG_LEN_08BIT, 0xdb },
	{ 0x3d8c, CRL_REG_LEN_08BIT, 0x71 },
	{ 0x370b, CRL_REG_LEN_08BIT, 0x05 },
	{ 0x3618, CRL_REG_LEN_08BIT, 0x2a },
	{ 0x5780, CRL_REG_LEN_08BIT, 0x3e },
	{ 0x5781, CRL_REG_LEN_08BIT, 0x0f },
	{ 0x5782, CRL_REG_LEN_08BIT, 0x44 },
	{ 0x5783, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x5784, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x5785, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x5786, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x5787, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x5788, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x5789, CRL_REG_LEN_08BIT, 0x0f },
	{ 0x578a, CRL_REG_LEN_08BIT, 0xfd },
	{ 0x578b, CRL_REG_LEN_08BIT, 0xf5 },
	{ 0x578c, CRL_REG_LEN_08BIT, 0xf5 },
	{ 0x578d, CRL_REG_LEN_08BIT, 0x03 },
	{ 0x578e, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x578f, CRL_REG_LEN_08BIT, 0x0c },
	{ 0x5790, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x5791, CRL_REG_LEN_08BIT, 0x06 },
	{ 0x5792, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x5793, CRL_REG_LEN_08BIT, 0x52 },
	{ 0x5794, CRL_REG_LEN_08BIT, 0xa3 },
	{ 0x3503, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x380e, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x380f, CRL_REG_LEN_08BIT, 0x60 },
	{ 0x3002, CRL_REG_LEN_08BIT, 0x61 },
	{ 0x3010, CRL_REG_LEN_08BIT, 0x40 },
	{ 0x300D, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x5045, CRL_REG_LEN_08BIT, 0x05 },
	{ 0x5048, CRL_REG_LEN_08BIT, 0x10 },
	{ 0x3610, CRL_REG_LEN_08BIT, 0xa8 },
	{ 0x3733, CRL_REG_LEN_08BIT, 0x10 },
	{ 0x3734, CRL_REG_LEN_08BIT, 0x40 },
};

static struct crl_register_write_rep ov5670_mode_1944[] = {
	/* Auto size function in use, but no cropping in this mode */
	{ 0x3808, CRL_REG_LEN_08BIT, 0x0a },
	{ 0x3809, CRL_REG_LEN_08BIT, 0x20 },
	{ 0x380a, CRL_REG_LEN_08BIT, 0x07 },
	{ 0x380b, CRL_REG_LEN_08BIT, 0x98 },
	{ 0x3821, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4600, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x4601, CRL_REG_LEN_08BIT, 0x03 },
};

static struct crl_register_write_rep ov5670_mode_1940[] = {
	/* Auto size function in use, cropping from the centre of the image */
	{ 0x3808, CRL_REG_LEN_08BIT, 0x0a },
	{ 0x3809, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x380a, CRL_REG_LEN_08BIT, 0x07 },
	{ 0x380b, CRL_REG_LEN_08BIT, 0x94 },
	{ 0x3821, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4600, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x4601, CRL_REG_LEN_08BIT, 0x00 },
};

static struct crl_register_write_rep ov5670_mode_1458[] = {
	/* Auto size function in use, cropping from the centre of the image */
	{ 0x3808, CRL_REG_LEN_08BIT, 0x0a },
	{ 0x3809, CRL_REG_LEN_08BIT, 0x20 },
	{ 0x380a, CRL_REG_LEN_08BIT, 0x05 },
	{ 0x380b, CRL_REG_LEN_08BIT, 0xB2 },
	{ 0x3821, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4600, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x4601, CRL_REG_LEN_08BIT, 0x03 },
};

static struct crl_register_write_rep ov5670_mode_1456[] = {
	/* Auto size function in use, cropping from the centre of the image */
	{ 0x3808, CRL_REG_LEN_08BIT, 0x0a },
	{ 0x3809, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x380a, CRL_REG_LEN_08BIT, 0x05 },
	{ 0x380b, CRL_REG_LEN_08BIT, 0xB0 },
	{ 0x3821, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4600, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x4601, CRL_REG_LEN_08BIT, 0x00 },
};

static struct crl_register_write_rep ov5670_mode_1152[] = {
	/* Auto size function in use, cropping from the centre of the image */
	{ 0x3808, CRL_REG_LEN_08BIT, 0x07 },
	{ 0x3809, CRL_REG_LEN_08BIT, 0xC0 },
	{ 0x380a, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x380b, CRL_REG_LEN_08BIT, 0x80 },
	{ 0x3821, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4600, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4601, CRL_REG_LEN_08BIT, 0xc6 },
};

static struct crl_register_write_rep ov5670_mode_1080[] = {
	/* Auto size function in use, cropping from the centre of the image */
	{ 0x3808, CRL_REG_LEN_08BIT, 0x07 },
	{ 0x3809, CRL_REG_LEN_08BIT, 0x80 },
	{ 0x380a, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x380b, CRL_REG_LEN_08BIT, 0x38 },
	{ 0x3821, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4600, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4601, CRL_REG_LEN_08BIT, 0xc0 },
};

static struct crl_register_write_rep ov5670_streamon_regs[] = {
	{ 0x0100, CRL_REG_LEN_08BIT, 0x01 }
};

static struct crl_register_write_rep ov5670_streamoff_regs[] = {
	/* MIPI stream off when current frame finish */
	{ 0x4202, CRL_REG_LEN_08BIT, 0x0f },
	/* Wait to finish the current frame */
	{ 0x0000, CRL_REG_LEN_DELAY, 0x40 },
	/* Sensor to standby */
	{ 0x0100, CRL_REG_LEN_08BIT, 0x00 },
};

static struct crl_register_write_rep ov5670_data_fmt_width10[] = {
	{ 0x3031, CRL_REG_LEN_08BIT, 0x0a }
};

static struct crl_arithmetic_ops ov5670_vflip_ops[] = {
	{
		.op = CRL_BITWISE_LSHIFT,
		.operand.entity_val = 1,
	},
};

static struct crl_arithmetic_ops ov5670_swap_flip_ops[] = {
	{
		.op = CRL_BITWISE_LSHIFT,
		.operand.entity_val = 5,
	},
};

static struct crl_arithmetic_ops ov5670_hflip_ops[] = {
	{
		.op = CRL_BITWISE_LSHIFT,
		.operand.entity_val = 1,
	},
};

static struct crl_arithmetic_ops ov5670_hblank_ops[] = {
	{
		.op = CRL_BITWISE_RSHIFT,
		.operand.entity_val = 1,
	},
};

static struct crl_arithmetic_ops ov5670_exposure_ops[] = {
	{
		.op = CRL_BITWISE_LSHIFT,
		.operand.entity_val = 4,
	},
};

static struct crl_dynamic_register_access ov5670_v_flip_regs[] = {
	{
		.address = 0x3820,
		.len = CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE,
		.ops_items = ARRAY_SIZE(ov5670_vflip_ops),
		.ops = ov5670_vflip_ops,
		.mask = 0x2,
	},
	{
		.address = 0x450B,
		.len = CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE,
		.ops_items = ARRAY_SIZE(ov5670_swap_flip_ops),
		.ops = ov5670_swap_flip_ops,
		.mask = 0x20,
	},
};

static struct crl_dynamic_register_access ov5670_h_flip_regs[] = {
	{
		.address = 0x3821,
		.len = CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE,
		.ops_items = ARRAY_SIZE(ov5670_hflip_ops),
		.ops = ov5670_hflip_ops,
		.mask = 0x2,
	},
};

struct crl_register_write_rep ov5670_poweroff_regset[] = {
	{ 0x0103, CRL_REG_LEN_08BIT, 0x01  },
};

static struct crl_dynamic_register_access ov5670_ana_gain_global_regs[] = {
	{
		.address = 0x3508,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0x7ff,
	},
};

static struct crl_dynamic_register_access ov5670_exposure_regs[] = {
	{
		.address = 0x3500,
		.len = CRL_REG_LEN_24BIT,
		.ops_items = ARRAY_SIZE(ov5670_exposure_ops),
		.ops = ov5670_exposure_ops,
		.mask = 0x0ffff0,
	},
};

static struct crl_dynamic_register_access ov5670_vblank_regs[] = {
	{
		.address = 0x380E,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	},
};

static struct crl_dynamic_register_access ov5670_hblank_regs[] = {
	{
		.address = 0x380C,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = ARRAY_SIZE(ov5670_hblank_ops),
		.ops = ov5670_hblank_ops,
		.mask = 0xffff,
	},
};

static struct crl_sensor_detect_config ov5670_sensor_detect_regset[] = {
	{
		.reg = { 0x300B, CRL_REG_LEN_08BIT, 0x000000ff  },
		.width = 7,
	},
	{
		.reg = { 0x300C, CRL_REG_LEN_08BIT, 0x000000ff  },
		.width = 7,
	},
};

static const s64 ov5670_op_sys_clock[] =  { 420000000, };

static struct crl_pll_configuration ov5670_pll_configurations[] = {
	{
		.input_clk = 24000000,
		.op_sys_clk = 420000000,
		.bitsperpixel = 10,
		.pixel_rate_csi = 240000000,
		.pixel_rate_pa = 199180800,
		.csi_lanes = 2,
		.comp_items = 0,
		.ctrl_data = 0,
		.pll_regs_items = ARRAY_SIZE(ov5670_pll_840mbps),
		.pll_regs = ov5670_pll_840mbps,
	},
};

static struct crl_subdev_rect_rep ov5670_1944_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect = { 0, 0, 2592, 1944 },
		.out_rect = { 0, 0, 2592, 1944 },
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect = { 0, 0, 2592, 1944 },
		.out_rect = { 0, 0, 2592, 1944 },
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.in_rect = { 0, 0, 2592, 1944 },
		.out_rect = { 0, 0, 2592, 1944 },
	},
};

static struct crl_subdev_rect_rep ov5670_1940_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect = { 0, 0, 2592, 1944 },
		.out_rect = { 16, 2, 2560, 1940 },
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect = { 0, 0, 2560, 1940 },
		.out_rect = { 0, 0, 2560, 1940 },
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.in_rect = { 0, 0, 2560, 1940 },
		.out_rect = { 0, 0, 2560, 1940 },
	},
};

static struct crl_subdev_rect_rep ov5670_1458_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect = { 0, 0, 2592, 1944 },
		.out_rect = { 0, 244, 2592, 1458 },
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect = { 0, 0, 2592, 1458 },
		.out_rect = { 0, 0, 2592, 1458 },
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.in_rect = { 0, 0, 2592, 1458 },
		.out_rect = { 0, 0, 2592, 1458 },
	},
};

static struct crl_subdev_rect_rep ov5670_1456_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect = { 0, 0, 2592, 1944 },
		.out_rect = { 16, 244, 2560, 1456 },
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect = { 0, 0, 2560, 1456 },
		.out_rect = { 0, 0, 2560, 1456 },
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.in_rect = { 0, 0, 2560, 1456 },
		.out_rect = { 0, 0, 2560, 1456 },
	},
};

static struct crl_subdev_rect_rep ov5670_1152_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect = { 0, 0, 2592, 1944 },
		.out_rect = { 304, 396, 1984, 1152 },
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect = { 0, 0, 1984, 1152 },
		.out_rect = { 0, 0, 1984, 1152 },
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.in_rect = { 0, 0, 1984, 1152 },
		.out_rect = { 0, 0, 1984, 1152 },
	},
};

static struct crl_subdev_rect_rep ov5670_1080_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect = { 0, 0, 2592, 1944 },
		.out_rect = { 336, 432, 1920, 1080 },
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect = { 0, 0, 1920, 1080 },
		.out_rect = { 0, 0, 1920, 1080 },
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.in_rect = { 0, 0, 1920, 1080 },
		.out_rect = { 0, 0, 1920, 1080 },
	},
};

static struct crl_mode_rep ov5670_modes[] = {
	{
		.sd_rects_items = ARRAY_SIZE(ov5670_1944_rects),
		.sd_rects = ov5670_1944_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 2592,
		.height = 1944,
		.min_llp = 3360,
		.min_fll = 1976,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(ov5670_mode_1944),
		.mode_regs = ov5670_mode_1944,
	},
	{
		.sd_rects_items = ARRAY_SIZE(ov5670_1940_rects),
		.sd_rects = ov5670_1940_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 2560,
		.height = 1940,
		.min_llp = 3366,
		.min_fll = 1972,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(ov5670_mode_1940),
		.mode_regs = ov5670_mode_1940,
	},
	{
		.sd_rects_items = ARRAY_SIZE(ov5670_1458_rects),
		.sd_rects = ov5670_1458_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 2592,
		.height = 1458,
		.min_llp = 4455,
		.min_fll = 1490,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(ov5670_mode_1458),
		.mode_regs = ov5670_mode_1458,
	},
	{
		.sd_rects_items = ARRAY_SIZE(ov5670_1456_rects),
		.sd_rects = ov5670_1456_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 2560,
		.height = 1456,
		.min_llp = 4461,
		.min_fll = 1488,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(ov5670_mode_1456),
		.mode_regs = ov5670_mode_1456,
	},

	{
		.sd_rects_items = ARRAY_SIZE(ov5670_1152_rects),
		.sd_rects = ov5670_1152_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 1984,
		.height = 1152,
		.min_llp = 2803,
		.min_fll = 1184,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(ov5670_mode_1152),
		.mode_regs = ov5670_mode_1152,
	},
	{
		.sd_rects_items = ARRAY_SIZE(ov5670_1080_rects),
		.sd_rects = ov5670_1080_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 1920,
		.height = 1080,
		.min_llp = 2985,
		.min_fll = 1112,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(ov5670_mode_1080),
		.mode_regs = ov5670_mode_1080,
	},
};

static struct crl_sensor_subdev_config ov5670_sensor_subdevs[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.name = "ov5670 scaler",
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.name = "ov5670 binner",
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.name = "ov5670 pixel array",
	},
};

static struct crl_sensor_limits ov5670_sensor_limits = {
	.x_addr_min = 0,
	.y_addr_min = 0,
	.x_addr_max = 2592,
	.y_addr_max = 1944,
	.min_frame_length_lines = 160,
	.max_frame_length_lines = 65535,
	.min_line_length_pixels = 2700,
	.max_line_length_pixels = 32752,
	.scaler_m_min = 16,
	.scaler_m_max = 255,
	.scaler_n_min = 16,
	.scaler_n_max = 16,
	.min_even_inc = 1,
	.max_even_inc = 1,
	.min_odd_inc = 1,
	.max_odd_inc = 3,
};

static struct crl_flip_data ov5670_flip_configurations[] = {
	{
		.flip = CRL_FLIP_DEFAULT_NONE,
		.pixel_order = CRL_PIXEL_ORDER_BGGR,
	},
	{
		.flip = CRL_FLIP_VFLIP,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
	},
	{
		.flip = CRL_FLIP_HFLIP,
		.pixel_order = CRL_PIXEL_ORDER_GBRG,
	},
	{
		.flip = CRL_FLIP_HFLIP_VFLIP,
		.pixel_order = CRL_PIXEL_ORDER_RGGB,
	},
};

static struct crl_csi_data_fmt ov5670_crl_csi_data_fmt[] = {
	{
		.code = MEDIA_BUS_FMT_SGRBG10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
		.bits_per_pixel = 10,
		.regs_items = 1,
		.regs = ov5670_data_fmt_width10,
	},
	{
		.code = MEDIA_BUS_FMT_SRGGB10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_RGGB,
		.regs_items = 1,
		.bits_per_pixel = 10,
		.regs = ov5670_data_fmt_width10,
	},
	{
		.code = MEDIA_BUS_FMT_SBGGR10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_BGGR,
		.regs_items = 1,
		.bits_per_pixel = 10,
		.regs = ov5670_data_fmt_width10,
	},
	{
		.code = MEDIA_BUS_FMT_SGBRG10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_GBRG,
		.regs_items = 1,
		.bits_per_pixel = 10,
		.regs = ov5670_data_fmt_width10,
	},
};

static struct crl_v4l2_ctrl ov5670_v4l2_ctrls[] = {
	{
		.sd_type = CRL_SUBDEV_TYPE_SCALER,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_IDLE,
		.ctrl_id = V4L2_CID_LINK_FREQ,
		.name = "V4L2_CID_LINK_FREQ",
		.type = CRL_V4L2_CTRL_TYPE_MENU_INT,
		.data.v4l2_int_menu.def = 0,
		.data.v4l2_int_menu.max =
			ARRAY_SIZE(ov5670_pll_configurations) - 1,
		.data.v4l2_int_menu.menu = ov5670_op_sys_clock,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.regs_items = 0,
		.regs = 0,
		.dep_items = 0,
		.dep_ctrls = 0,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_GET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_PIXEL_RATE,
		.name = "V4L2_CID_PIXEL_RATE_PA",
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.data.std_data.min = 0,
		.data.std_data.max = INT_MAX,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.regs_items = 0,
		.regs = 0,
		.dep_items = 0,
		.dep_ctrls = 0,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_SCALER,
		.op_type = CRL_V4L2_CTRL_GET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_PIXEL_RATE,
		.name = "V4L2_CID_PIXEL_RATE_CSI",
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.data.std_data.min = 0,
		.data.std_data.max = INT_MAX,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.regs_items = 0,
		.regs = 0,
		.dep_items = 0,
		.dep_ctrls = 0,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_ANALOGUE_GAIN,
		.name = "V4L2_CID_ANALOGUE_GAIN",
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.data.std_data.min = 0,
		.data.std_data.max = 4096,
		.data.std_data.step = 1,
		.data.std_data.def = 128,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(ov5670_ana_gain_global_regs),
		.regs = ov5670_ana_gain_global_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_EXPOSURE,
		.name = "V4L2_CID_EXPOSURE",
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.data.std_data.min = 0,
		.data.std_data.max = 65500,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(ov5670_exposure_regs),
		.regs = ov5670_exposure_regs,
		.dep_items = 0, /* FLL is changed automatically */
		.dep_ctrls = 0,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_HFLIP,
		.name = "V4L2_CID_HFLIP",
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.data.std_data.min = 0,
		.data.std_data.max = 1,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(ov5670_h_flip_regs),
		.regs = ov5670_h_flip_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_VFLIP,
		.name = "V4L2_CID_VFLIP",
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.data.std_data.min = 0,
		.data.std_data.max = 1,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(ov5670_v_flip_regs),
		.regs = ov5670_v_flip_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_FRAME_LENGTH_LINES,
		.name = "Frame Length Lines",
		.type = CRL_V4L2_CTRL_TYPE_CUSTOM,
		.data.std_data.min = 160,
		.data.std_data.max = 65535,
		.data.std_data.step = 1,
		.data.std_data.def = 2474,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(ov5670_vblank_regs),
		.regs = ov5670_vblank_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_LINE_LENGTH_PIXELS,
		.name = "Line Length Pixels",
		.type = CRL_V4L2_CTRL_TYPE_CUSTOM,
		.data.std_data.min = 1024,
		.data.std_data.max = 65520,
		.data.std_data.step = 1,
		.data.std_data.def = 3880,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(ov5670_hblank_regs),
		.regs = ov5670_hblank_regs,
		.dep_items = 0,
		.dep_ctrls = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
	},
};

#define ov5670_OTP_START_ADDR 0x7010
#define ov5670_OTP_END_ADDR 0x7063

#define ov5670_OTP_LEN (ov5670_OTP_END_ADDR - ov5670_OTP_START_ADDR + 1)
#define ov5670_OTP_L_ADDR(x) (x & 0xff)
#define ov5670_OTP_H_ADDR(x) ((x >> 8) & 0xff)

static struct crl_register_write_rep ov5670_nvm_preop_regset[] = {
	/* Start streaming */
	{ 0x0100, CRL_REG_LEN_08BIT, 0x01 },
	/* Manual mode, program disable */
	{ 0x3D84, CRL_REG_LEN_08BIT, 0xC0 },
	/* Manual OTP start address for access */
	{ 0x3D88, CRL_REG_LEN_08BIT, ov5670_OTP_H_ADDR(ov5670_OTP_START_ADDR)},
	{ 0x3D89, CRL_REG_LEN_08BIT, ov5670_OTP_L_ADDR(ov5670_OTP_START_ADDR)},
	/* Manual OTP end address for access */
	{ 0x3D8A, CRL_REG_LEN_08BIT, ov5670_OTP_H_ADDR(ov5670_OTP_END_ADDR)},
	{ 0x3D8B, CRL_REG_LEN_08BIT, ov5670_OTP_L_ADDR(ov5670_OTP_END_ADDR)},
	/* OTP load enable */
	{ 0x3D81, CRL_REG_LEN_08BIT, 0x01 },
	/* Wait for the data to load into the buffer */
	{ 0x0000, CRL_REG_LEN_DELAY, 0x05 },
};

static struct crl_register_write_rep ov5670_nvm_postop_regset[] = {
	{ 0x0100, CRL_REG_LEN_08BIT, 0x00 }, /* Stop streaming */
};

static struct crl_nvm_blob ov5670_nvm_blobs[] = {
	{CRL_I2C_ADDRESS_NO_OVERRIDE, ov5670_OTP_START_ADDR, ov5670_OTP_LEN },
};

static struct crl_arithmetic_ops ov5670_frame_desc_width_ops[] = {
	{
		.op = CRL_ASSIGNMENT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_VAR_REF,
		.operand.entity_val = CRL_VAR_REF_OUTPUT_WIDTH,
	},
};

static struct crl_arithmetic_ops ov5670_frame_desc_height_ops[] = {
	{
		.op = CRL_ASSIGNMENT,
		.operand.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST,
		.operand.entity_val = 1,
	},
};

static struct crl_frame_desc ov5670_frame_desc[] = {
	{
		.flags.entity_val = 0,
		.bpp.entity_type = CRL_DYNAMIC_VAL_OPERAND_TYPE_VAR_REF,
		.bpp.entity_val = CRL_VAR_REF_BITSPERPIXEL,
		.pixelcode.entity_val = MEDIA_BUS_FMT_FIXED,
		.length.entity_val = 0,
		.start_line.entity_val = 0,
		.start_pixel.entity_val = 0,
		.width = {
			.ops_items = ARRAY_SIZE(ov5670_frame_desc_width_ops),
			.ops = ov5670_frame_desc_width_ops,
		},
		.height = {
			.ops_items = ARRAY_SIZE(ov5670_frame_desc_height_ops),
			.ops = ov5670_frame_desc_height_ops,
		},
		.csi2_channel.entity_val = 0,
		.csi2_data_type.entity_val = 0x12,
	},
};

/* Power items, they are enabled in the order they are listed here */
static const struct crl_power_seq_entity ov5670_power_items[] = {
	{
		.type = CRL_POWER_ETY_REGULATOR_FRAMEWORK,
		.ent_name = "VANA",
		.val = 2800000,
		.delay = 0,
	},
	{
		.type = CRL_POWER_ETY_REGULATOR_FRAMEWORK,
		.ent_name = "VDIG",
		.val = 1200000,
		.delay = 0,
	},
	{
		.type = CRL_POWER_ETY_REGULATOR_FRAMEWORK,
		.ent_name = "VIO",
		.val = 1800000,
		.delay = 0,
	},
	{
		.type = CRL_POWER_ETY_REGULATOR_FRAMEWORK,
		.ent_name = "VAF",
		.val = 3000000,
		.delay = 2000,
	},
	{
		.type = CRL_POWER_ETY_CLK_FRAMEWORK,
		.val = 24000000,
	},
	{
		.type = CRL_POWER_ETY_GPIO_FROM_PDATA,
		.val = 1,
		.delay = 10700,
	},
	{
		.type = CRL_POWER_ETY_GPIO_FROM_PDATA_BY_NUMBER,
	},

};

static struct crl_sensor_configuration ov5670_crl_configuration = {

	.power_items = ARRAY_SIZE(ov5670_power_items),
	.power_entities = ov5670_power_items,

	.powerup_regs_items = ARRAY_SIZE(ov5670_powerup_regset),
	.powerup_regs = ov5670_powerup_regset,

	.poweroff_regs_items = 0,
	.poweroff_regs = 0,

	.id_reg_items = ARRAY_SIZE(ov5670_sensor_detect_regset),
	.id_regs = ov5670_sensor_detect_regset,

	.subdev_items = ARRAY_SIZE(ov5670_sensor_subdevs),
	.subdevs = ov5670_sensor_subdevs,

	.sensor_limits = &ov5670_sensor_limits,

	.pll_config_items = ARRAY_SIZE(ov5670_pll_configurations),
	.pll_configs = ov5670_pll_configurations,

	.modes_items = ARRAY_SIZE(ov5670_modes),
	.modes = ov5670_modes,

	.streamon_regs_items = ARRAY_SIZE(ov5670_streamon_regs),
	.streamon_regs = ov5670_streamon_regs,

	.streamoff_regs_items = ARRAY_SIZE(ov5670_streamoff_regs),
	.streamoff_regs = ov5670_streamoff_regs,

	.v4l2_ctrls_items = ARRAY_SIZE(ov5670_v4l2_ctrls),
	.v4l2_ctrl_bank = ov5670_v4l2_ctrls,

	.csi_fmts_items = ARRAY_SIZE(ov5670_crl_csi_data_fmt),
	.csi_fmts = ov5670_crl_csi_data_fmt,

	.flip_items = ARRAY_SIZE(ov5670_flip_configurations),
	.flip_data = ov5670_flip_configurations,

	.crl_nvm_info.nvm_flags = CRL_NVM_ADDR_MODE_16BIT,
	.crl_nvm_info.nvm_preop_regs_items =
		ARRAY_SIZE(ov5670_nvm_preop_regset),
	.crl_nvm_info.nvm_preop_regs = ov5670_nvm_preop_regset,
	.crl_nvm_info.nvm_postop_regs_items =
		ARRAY_SIZE(ov5670_nvm_postop_regset),
	.crl_nvm_info.nvm_postop_regs = ov5670_nvm_postop_regset,
	.crl_nvm_info.nvm_blobs_items = ARRAY_SIZE(ov5670_nvm_blobs),
	.crl_nvm_info.nvm_config = ov5670_nvm_blobs,

	.frame_desc_entries = ARRAY_SIZE(ov5670_frame_desc),
	.frame_desc_type = CRL_V4L2_MBUS_FRAME_DESC_TYPE_CSI2,
	.frame_desc = ov5670_frame_desc,
};

#endif  /* __CRLMODULE_ov5670_CONFIGURATION_H_ */
