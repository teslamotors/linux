/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2016 - 2018 Intel Corporation */

#ifndef UAPI_LINUX_CRLMODULE_H
#define UAPI_LINUX_CRLMODULE_H

#define REGS_BUF_SIZE			32
/**
 * struct crl_registers_info - the information for register.
 * @start_address: sensor i2c start address.
 * @number:        the number of registers to be read/write.
 * @len:           register length, 1:8bit, 2:16bit, 3:24bit, 4:32bit.
 * @regs:          the array of registers.
 */
struct crl_registers_info {
	unsigned int start_address;
	unsigned int number;
	unsigned int len;
	unsigned int regs[REGS_BUF_SIZE];
};

#define CRL_G_REGISTERS			_IOWR('C', 1, struct crl_registers_info)
#define CRL_S_REGISTERS			_IOW('C', 2, struct crl_registers_info)

#define V4L2_CID_CRLMODULE_BASE		(V4L2_CID_USER_BASE + 0x2050)

#define V4L2_CID_FRAME_LENGTH_LINES (V4L2_CID_CRLMODULE_BASE + 1)
#define V4L2_CID_LINE_LENGTH_PIXELS (V4L2_CID_CRLMODULE_BASE + 2)
#define CRL_CID_SENSOR_THERMAL_DATA (V4L2_CID_CRLMODULE_BASE + 3)

/*
 * Select sensor mode directly, driver programs media pad
 * formats as in configuration file
 */
#define CRL_CID_SENSOR_MODE (V4L2_CID_CRLMODULE_BASE + 4)

/* IMX230 HDR specific controls */
#define CRL_CID_IMX230_HDR_MODE		(V4L2_CID_CRLMODULE_BASE + 5)
#define CRL_CID_IMX230_HDR_ET_RATIO	(V4L2_CID_CRLMODULE_BASE + 6)

/* Set multi-exposure frame in HDR with different exposure value */
#define CRL_CID_EXPOSURE_SHS1		(V4L2_CID_CRLMODULE_BASE + 8)
#define CRL_CID_EXPOSURE_SHS2		(V4L2_CID_CRLMODULE_BASE + 9)
#define CRL_CID_EXPOSURE_SHS3		(V4L2_CID_CRLMODULE_BASE + 10)
#define CRL_CID_EXPOSURE_RHS1		(V4L2_CID_CRLMODULE_BASE + 11)
#define CRL_CID_EXPOSURE_RHS2		(V4L2_CID_CRLMODULE_BASE + 12)

/* Switch to enable/disable PDAF settings */
#define CRL_CID_SENSOR_PDAF		(V4L2_CID_CRLMODULE_BASE + 13)

/* Set multi-digital gain */
#define CRL_CID_DIGITAL_GAIN_L		(V4L2_CID_CRLMODULE_BASE + 14)
#define CRL_CID_DIGITAL_GAIN_S		(V4L2_CID_CRLMODULE_BASE + 15)
#define CRL_CID_DIGITAL_GAIN_VS		(V4L2_CID_CRLMODULE_BASE + 16)

/* Get sensor bit linear */
#define CRL_CID_SENSOR_BIT_LINEAR	(V4L2_CID_CRLMODULE_BASE + 17)

/* set sensor msb align*/
#define CRL_CID_MSB_ALIGN		(V4L2_CID_CRLMODULE_BASE + 18)

/* enable/disable auto exposure */
#define CRL_CID_AUTO_EXPOSURE_DEBUG	(V4L2_CID_CRLMODULE_BASE + 19)

/* set analog gain for HDR frames */
#define CRL_CID_ANALOG_GAIN_L		(V4L2_CID_CRLMODULE_BASE + 20)
#define CRL_CID_ANALOG_GAIN_S		(V4L2_CID_CRLMODULE_BASE + 21)
#define CRL_CID_ANALOG_GAIN_VS		(V4L2_CID_CRLMODULE_BASE + 22)

/* Set exposure mode: Linear mode or 2-/3-/4-HDR mode */
#define CRL_CID_EXPOSURE_MODE		(V4L2_CID_CRLMODULE_BASE + 23)

/* Set HDR mode exposure ratio */
#define CRL_CID_EXPOSURE_HDR_RATIO	(V4L2_CID_CRLMODULE_BASE + 24)

#endif /* UAPI_LINUX_CRLMODULE_H */
