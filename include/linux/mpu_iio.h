/*
* Copyright (C) 2012 Invensense, Inc.
* Copyright (c) 2013-2016, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __MPU_H_
#define __MPU_H_

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/ioctl.h>
#endif

/* Mount matrices for mount orientation.
 * MTMAT_XXX_CCW_YYY
 *     XXX : mount position. TOP for top and BOT for bottom.
 *     YYY : couter-clockwise rotation angle in degree.
 */
#define MTMAT_TOP_CCW_0		{  1,  0,  0,  0,  1,  0,  0,  0,  1 }
#define MTMAT_TOP_CCW_90	{  0, -1,  0,  1,  0,  0,  0,  0,  1 }
#define MTMAT_TOP_CCW_180	{ -1,  0,  0,  0, -1,  0,  0,  0,  1 }
#define MTMAT_TOP_CCW_270	{  0,  1,  0, -1,  0,  0,  0,  0,  1 }
#define MTMAT_BOT_CCW_0		{ -1,  0,  0,  0,  1,  0,  0,  0, -1 }
#define MTMAT_BOT_CCW_90	{  0, -1,  0, -1,  0,  0,  0,  0, -1 }
#define MTMAT_BOT_CCW_180	{  1,  0,  0,  0, -1,  0,  0,  0, -1 }
#define MTMAT_BOT_CCW_270	{  0,  1,  0,  1,  0,  0,  0,  0, -1 }

enum secondary_slave_type {
	SECONDARY_SLAVE_TYPE_NONE,
	SECONDARY_SLAVE_TYPE_ACCEL,
	SECONDARY_SLAVE_TYPE_COMPASS,
	SECONDARY_SLAVE_TYPE_PRESSURE,

	SECONDARY_SLAVE_TYPE_TYPES
};

enum ext_slave_id {
	ID_INVALID = 0,
	GYRO_ID_MPU3050,
	GYRO_ID_MPU6050A2,
	GYRO_ID_MPU6050B1,
	GYRO_ID_MPU6050B1_NO_ACCEL,
	GYRO_ID_ITG3500,

	ACCEL_ID_LIS331,
	ACCEL_ID_LSM303DLX,
	ACCEL_ID_LIS3DH,
	ACCEL_ID_KXSD9,
	ACCEL_ID_KXTF9,
	ACCEL_ID_BMA150,
	ACCEL_ID_BMA222,
	ACCEL_ID_BMA250,
	ACCEL_ID_ADXL34X,
	ACCEL_ID_MMA8450,
	ACCEL_ID_MMA845X,
	ACCEL_ID_MPU6050,

	COMPASS_ID_AK8963,
	COMPASS_ID_AK8975,
	COMPASS_ID_AK8972,
	COMPASS_ID_AMI30X,
	COMPASS_ID_AMI306,
	COMPASS_ID_YAS529,
	COMPASS_ID_YAS530,
	COMPASS_ID_HMC5883,
	COMPASS_ID_LSM303DLH,
	COMPASS_ID_LSM303DLM,
	COMPASS_ID_MMC314X,
	COMPASS_ID_HSCDTD002B,
	COMPASS_ID_HSCDTD004A,
	COMPASS_ID_MLX90399,
	COMPASS_ID_AK09911,

	PRESSURE_ID_BMP085,
	PRESSURE_ID_BMP280,

	ID_INVALID_END /* always last, leave auto-assigned */
};

#define INV_PROD_KEY(ver, rev) (ver * 100 + rev)

#define NVI_CONFIG_BOOT_AUTO		(0) /* auto detect connection to MPU */
#define NVI_CONFIG_BOOT_MPU		(1) /* connected to MPU */
#define NVI_CONFIG_BOOT_HOST		(2) /* connected to host */
#define NVI_CONFIG_BOOT_MASK		(0x03)

/**
 * struct mpu_platform_data - Platform data for the mpu driver
 * @int_config:		Bits [7:3] of the int config register.
 * @level_shifter:	0: VLogic, 1: VDD
 * @orientation:	Orientation matrix of the gyroscope
 * @sec_slave_type:     secondary slave device type, can be compass, accel, etc
 * @sec_slave_id:       id of the secondary slave device
 * @secondary_i2c_address: secondary device's i2c address
 * @secondary_orientation: secondary device's orientation matrix
 * @key:                key for MPL library.
 * @nvi_config: the selection determines the device behavior.
 *              Select from the NVI_CONFIG_BOOT_ defines.
 *
 * Contains platform specific information on how to configure the MPU3050 to
 * work on this platform.  The orientation matricies are 3x3 rotation matricies
 * that are applied to the data to rotate from the mounting orientation to the
 * platform orientation.  The values must be one of 0, 1, or -1 and each row and
 * column should have exactly 1 non-zero value.
 */
struct mpu_platform_data {
	__u8 int_config;
	__u8 level_shifter;
	__s8 orientation[9];
	enum secondary_slave_type sec_slave_type;
	enum ext_slave_id sec_slave_id;
	__u16 secondary_i2c_addr;
	__s8 secondary_orientation[9];
	__u8 key[16];
	enum secondary_slave_type aux_slave_type;
	enum ext_slave_id aux_slave_id;
	__u16 aux_i2c_addr;

#ifdef CONFIG_DTS_INV_MPU_IIO
	int (*power_on)(struct mpu_platform_data *);
	int (*power_off)(struct mpu_platform_data *);
	struct regulator *vdd_ana;
	struct regulator *vdd_i2c;
#endif
	__u8 nvi_config;
};


/**
 * struct nvi_mpu_port: Allows an external driver to use the MPU
 *    auxiliary I2C master by providing the details for the I2C
 *    polling of a device connected to the MPU.
 * - addr: The 6:0 I2C address of the device connected to
 *      the MPU.
 *      7:7 = 0 if the port is to do write transactions.
 *          = 1 if the port is to do read transactions.
 * - reg: The device register the I2C transaction will
 *      use.
 * - ctrl: The number of consecutive registers to read in
 *      3:0. If the port is to do write transactions then this
 *      value must be 1.  See MPU documentation for the other
 *      bits in I2C_SLVx_CTRL that can be applied by this byte.
 * - dmp_ctrl: When the DMP in enabled, the number of
 *      consecutive registers to read in 3:0. If the port is to
 *      do write transactions then this value must be 1.  See
 *      MPU documentation for the other bits in I2C_SLVx_CTRL
 *      that can be applied by this byte.
 * - data_out: The data byte written if the port is configured
 *      to do writes (addr 7:7 = 0).
 * - delay_ms: The polling delay time between I2C transactions
 *      in ms.  Note that the MPU HW only supports one delay
 *      time so the longest delay of all the MPU ports enabled
 *      is used.
 * - delay_us: The delay at which the read data is reported.
 * - shutdown_bypass: set if a connection to the host is needed
 *      when the system is shutdown.  The MPU API will be
 *      disabled as part of its shutdown but it will enable the
 *      bypass if this is true.
 * - *handler: The pointer to the function called when the data
 *      is available.  This can be NULL if the port is
 *      configured for write transactions.
 *      The function is called with the following parameters:
 *      - *data: The pointer to the data to read.
 *      - length: The number of bytes to be read (same value as
 *           length above).
 *      - timestamp: The timestamp of when the data was polled.
 *      - *pointer: A generic pointer defined next below.  Note
 *           that this can be NULL if this will be a write I2C
 *           transaction.
 * - *ext_driver: A generic pointer that can be used by the
 *      external driver.  Note that this is specifically for the
 *      external driver and not used by the MPU.
 * - matrix: device orientation on platform.
 *      Needed by the DMP.
 * - type: Define if device is to be used by the MPU DMP.
 * - id: Define if device is to be used by the MPU DMP.
 * - asa: compass axis sensitivity adjustment.
 *      Needed by the DMP.
 */
struct nvi_mpu_port {
	u8 addr;
	u8 reg;
	u8 ctrl;
	u8 dmp_ctrl;
	u8 data_out;
	unsigned int delay_ms;
	unsigned long delay_us;
	bool shutdown_bypass;
	void (*handler)(u8 *data, unsigned int len,
			long long timestamp, void *ext_driver);
	void *ext_driver;
	signed char matrix[9];
	enum secondary_slave_type type;
	enum ext_slave_id id;
	u64 q30[3];
};

/**
 * Expected use of the nvi_mpu_ routines are as follows:
 * - nvi_mpu_dev_valid: Use to validate whether a device is
 *      connected to the MPU.
 * - nvi_mpu_port_alloc: Request a connection to the device.  If
 *      successful, a port number will be returned to identify
 *      the connection.  The port number is then used for all
 *      further communication with the connection.
 * - nvi_mpu_port_free: Use to close the port connection.
 * - nvi_mpu_enable: Use to enable/disable a port.
 *      The enable and FIFO enable is disabled by default so
 *      this will be required after a port is assigned.
 * - nvi_mpu_delay_us: Use to set the sampling rate in
 *      microseconds.  The fastest rate of all the enabled MPU
 *      devices will be used that does not exceed the
 *      nvi_mpu_delay_ms setting of an enabled device.
 * - nvi_mpu_delay_ms: Use to change the port polling delay at
 *      runtime. There is only one HW delay so the delay used
 *      will be the longest delay of all the enabled ports.
 *      This is separate from the sampling rate
 *      (nvi_mpu_delay_us).  See function notes below.
 * - nvi_mpu_data_out: Use to change the data written at runtime
 *      for ports that are configured as I2C write transactions.
 * - nvi_mpu_bypass request/release: Use to connect/disconnect
 *      the MPU host from the device.  When bypass is enabled,
 *      the connection from the device to the MPU will then be
 *      connected to the host (that the MPU is connected to).
 *      This is a global connection switch affecting all ports
 *      so a mechanism is in place of whether the request is
 *      honored or not.  See the function notes for
 *      nvi_mpu_bypass_request.
 */

/**
 * Use to validate a device connected to the MPU I2C master.
 * The function works by doing a single byte read or write to
 * the device and detecting a NACK.  Typically, the call would
 * be set up to read a byte ID of the device.
 * @param struct nvi_mpu_port *nmp
 *           Only the following is needed in nmp:
 *           - addr
 *           - reg
 *           - ctrl
 *           - data_out if a write transaction
 * @param *data: pointer for read data.  Can be NULL if write.
 * @return int error
 *            Possible return value or errors are:
 *            - 0: device is connected to MPU.
 *            - -EAGAIN: MPU is not initialized yet.
 *            - -EPERM: MPU is shutdown.  MPU API won't be
 *                 available until a system restart.
 *            - -EBUSY: MPU is busy with another request.
 *            - -ENODEV: The device is not connected to the MPU.
 *            - -EINVAL: Problem with input parameters.
 *            - -EIO: The device is connected but responded with
 *                 a NACK.
 */
int nvi_mpu_dev_valid(struct nvi_mpu_port *nmp, u8 *data);

/**
 * Request a port.
 * @param struct nvi_mpu_port *nmp
 *           - addr: device I2C address 6:0.
 *                7:7 = 0 if the port is to do writes.
 *                7:7 = 1 if the port is to do reads.
 *           - reg: the starting register to write or read.
 *           - ctrl: number of bytes to read.  Use 1 if port
 *                is configured to do writes.
 *           - data_out: only valid if port is configured to do
 *                writes.
 *           - delay: polling delay
 *           - handler: function to call when data is read. This
 *                should be NULL if the port is configured to do
 *                writes.
 *           - ext_driver: this pointer is passed in handler for
 *                use by external driver.  This should be NULL
 *                if the port is configured for writes.
 * @param port: request a specific port (0 to 3).
 *            If port is -1 then the returned port ID will be
 *            automatically selected.
 *            Requesting a specific port is used when the device
 *            is expected to work with the Invensense DMP, since
 *            the DMP FW is pathetically designed and has strict
 *            limitations of how it will work with auxiliary
 *            devices.
 * @return int error/port ID
 *            if return >= 0 then this is the port ID.  The ID
 *            will have a value of 0 to 3 (HW has 4 ports).
 *            Possible errors are:
 *            - -EAGAIN: MPU is not initialized yet.
 *            - -EPERM: MPU is shutdown.  MPU API won't be
 *                 available until a system restart.
 *            - -EBUSY: MPU is busy with another request.
 *            - -ENODEV: A port is not available.  The only way
 *                 to resolve this error is for a port to be
 *                 freed.
 *            - -EINVAL: Problem with input parameters.
 */
int nvi_mpu_port_alloc(struct nvi_mpu_port *nmp, int port);

/**
 * Remove a port.
 * @param port
 * @return int error
 *            Possible errors are:
 *            - -EAGAIN: MPU is not initialized yet.
 *            - -EPERM: MPU is shutdown.  MPU API won't be
 *                 available until a system restart.
 *            - -EBUSY: MPU is busy with another request.
 *            - -EINVAL: Problem with input parameters.
 */
int nvi_mpu_port_free(int port);

/**
 * Enable/disable ports.  Use of a port mask (port_mask) allows
 * enabling/disabling multiple ports at the same time.
 * @param port_mask
 * @param enable
 * @return int error
 *            Possible errors are:
 *            - -EAGAIN: MPU is not initialized yet.
 *            - -EPERM: MPU is shutdown.  MPU API won't be
 *                 available until a system restart.
 *            - -EBUSY: MPU is busy with another request.
 *            - -EINVAL: Problem with input parameters.
 */
int nvi_mpu_enable(unsigned int port_mask, bool enable);

/**
 * Use to change the ports polling delay in milliseconds.
 * A delay value of 0 disables the delay for that port.  The
 * hardware only supports one delay value so the largest request
 * of all the enabled ports is used. The polling delay is in
 * addition to the sampling delay (nvi_mpu_delay_us).  This is
 * typically used to guarantee a delay after an I2C write to a
 * device to allow the device to process the request and be read
 * by another port before another write at the sampling delay.
 * @param port
 * @param delay_ms
 * @return int error
 *            Possible errors are:
 *            - -EAGAIN: MPU is not initialized yet.
 *            - -EPERM: MPU is shutdown.  MPU API won't be
 *                 available until a system restart.
 *            - -EBUSY: MPU is busy with another request.
 *            - -EINVAL: Problem with input parameters.
 */
int nvi_mpu_delay_ms(int port, u8 delay_ms);

/**
 * Use to change the data written to the sensor.
 * @param port
 * @param data_out is the new data to be written
 * @return int error
 *            Possible errors are:
 *            - -EAGAIN: MPU is not initialized yet.
 *            - -EPERM: MPU is shutdown.  MPU API won't be
 *                 available until a system restart.
 *            - -EBUSY: MPU is busy with another request.
 *            - -EINVAL: Problem with input parameters.
 */
int nvi_mpu_data_out(int port, u8 data_out);

/**
 * batch mode.
 * @param port
 * @param period_us
 * @param timeout_us
 * @return int error
 *            Possible errors are:
 *            - -EAGAIN: MPU is not initialized yet.
 *            - -EPERM: MPU is shutdown.  MPU API won't be
 *                 available until a system restart.
 *            - -EBUSY: MPU is busy with another request.
 *            - -EINVAL: timeout_us not supported if > 0.
 */
int nvi_mpu_batch(int port, unsigned int period_us, unsigned int timeout_us);

/**
 * batch flush.
 * @param port
 * @return int error
 *            Possible errors are:
 *            - -EAGAIN: MPU is not initialized yet.
 *            - -EPERM: MPU is shutdown.  MPU API won't be
 *                 available until a system restart.
 *            - -EBUSY: MPU is busy with another request.
 *            - -EINVAL: Problem with input parameters.
 */
int nvi_mpu_flush(int port);

/**
 * batch fifo.
 * @param port
 * @param reserve
 * @param max
 * @return int error
 *            Possible errors are:
 *            - -EAGAIN: MPU is not initialized yet.
 *            - -EPERM: MPU is shutdown.  MPU API won't be
 *                 available until a system restart.
 *            - -EBUSY: MPU is busy with another request.
 *            - -EINVAL: Problem with input parameters.
 */
int nvi_mpu_fifo(int port, unsigned int *reserve, unsigned int *max);

/**
 * Enable/disable the MPU bypass mode.  When enabled, the MPU
 * will connect its auxiliary I2C ports to the host.  This is
 * typically used to initialize a device that requires more I2C
 * transactions than the automated port polling can offer.
 * EVERY nvi_mpu_bypass_request call must be balanced with a
 * nvi_mpu_bypass_release call!
 * A bypass request does not need a following ~enable call.  The
 * release call will automatically handle the correct bypass
 * enable setting. The request locks the bypass setting if
 * successful.  The release unlocks and restores the setting if
 * need be.  Although odd, the purpose of the request call with
 * the enable cleared to false is to allow an external driver to
 * access its device that would normally conflict with a device
 * behind the MPU.  Note that this call must not be a permanent
 * solution, i.e. delayed or no release call.
 * When the MPU is in a shutdown state the return error will be
 * -EPERM and bypass will be enabled to allow access from the
 * host to the devices connected to the MPU for their own
 * shutdown needs.
 * @param enable
 * @return int error: calls that return with an error must not
 *            be balanced with a release call.
 *            Possible errors are:
 *            - -EAGAIN: MPU is not initialized yet.
 *            - -EPERM: MPU is shutdown.  MPU API won't be
 *                 available until a system restart.
 *            - -EBUSY: MPU is busy with another request.
 */
int nvi_mpu_bypass_request(bool enable);

/**
 * See the nvi_mpu_bypass_request notes.
 * @return int 0: Always returns 0.  The call return should be
 *         void but for backward compatibility it returns 0.
 */
int nvi_mpu_bypass_release(void);

#endif	/* __MPU_H_ */
