/******************************************************************************
 * Intel mei_dal Linux driver
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2016-2017 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * Contact Information:
 *	Intel Corporation.
 *	linux-mei@linux.intel.com
 *	http://www.intel.com
 *
 * BSD LICENSE
 *
 * Copyright(c) 2016-2017 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
#define pr_fmt(fmt) KBUILD_MODNAME ":%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/atomic.h>
#include <linux/module.h>
#include <linux/mei_cl_bus.h>
#include <linux/mei.h>
#include <linux/sched.h>
#include <linux/dal.h>

#include "bh_external.h"
#include "bh_errcode.h"
#include "acp_parser.h"
#include "dal_dev.h"

static DEFINE_MUTEX(dal_kdi_lock);

/**
 * to_kdi_err- converts error number to kdi error
 *
 * Beihai errors (>0) are converted to DAL_KDI errors (those errors came
 * from DAL FW)
 * system errors and success value (<=0) stay as is
 *
 * @err: error code to convert (either bh err or system err)
 *
 * Return: the converted kdi error number or system error
 */
static int to_kdi_err(int err)
{
	if (err)
		pr_debug("got error: %d\n", err);

	if (err <= 0)
		return err;

	/* err > 0: is error from DAL FW */
	switch (err) {
	case BPE_INTERNAL_ERROR:
		return DAL_KDI_STATUS_INTERNAL_ERROR;
	case BPE_INVALID_PARAMS:
	case BHE_INVALID_PARAMS:
		return DAL_KDI_STATUS_INVALID_PARAMS;
	case BHE_INVALID_HANDLE:
		return DAL_KDI_STATUS_INVALID_HANDLE;
	case BPE_NOT_INIT:
		return DAL_KDI_STATUS_NOT_INITIALIZED;
	case BPE_OUT_OF_MEMORY:
	case BHE_OUT_OF_MEMORY:
		return DAL_KDI_STATUS_OUT_OF_MEMORY;
	case BHE_INSUFFICIENT_BUFFER:
	case BHE_APPLET_SMALL_BUFFER:
		return DAL_KDI_STATUS_BUFFER_TOO_SMALL;
	case BPE_OUT_OF_RESOURCE:
	case BHE_VM_INSTANCE_INIT_FAIL:
		return DAL_KDI_STATUS_OUT_OF_RESOURCE;
	case BHE_SESSION_NUM_EXCEED:
		return DAL_KDI_STATUS_MAX_SESSIONS_REACHED;
	case BHE_UNCAUGHT_EXCEPTION:
		return DAL_KDI_STATUS_UNCAUGHT_EXCEPTION;
	case BHE_WD_TIMEOUT:
		return DAL_KDI_STATUS_WD_TIMEOUT;
	case BHE_APPLET_CRASHED:
		return DAL_KDI_STATUS_APPLET_CRASHED;
	case BHE_TA_PACKAGE_HASH_VERIFY_FAIL:
		return DAL_KDI_STATUS_INVALID_ACP;
	case BHE_PACKAGE_NOT_FOUND:
		return DAL_KDI_STATUS_TA_NOT_FOUND;
	case BHE_PACKAGE_EXIST:
		return DAL_KDI_STATUS_TA_EXIST;
	default:
		return DAL_KDI_STATUS_INTERNAL_ERROR;
	}
}

/**
 * dal_kdi_send - a callback which is called from bhp to send msg over mei
 *
 * @dev_idx: DAL device type
 * @buf: message buffer
 * @len: buffer length
 * @seq: message sequence
 *
 * Return: 0 on success
 *         -EINVAL on incorrect input
 *         -ENODEV when the device can't be found
 *         -EFAULT if client is NULL
 *         <0 on dal_write failure
 */
int dal_kdi_send(unsigned int dev_idx, const unsigned char *buf,
		 size_t len, u64 seq)
{
	enum dal_dev_type mei_device;
	struct dal_device *ddev;
	struct dal_client *dc;
	struct device *dev;
	ssize_t wr;
	int ret;

	if (!buf)
		return -EINVAL;

	if (dev_idx >= DAL_MEI_DEVICE_MAX)
		return -EINVAL;

	if (!len)
		return 0;

	if (len > DAL_MAX_BUFFER_SIZE)
		return -EMSGSIZE;

	mei_device = (enum dal_dev_type)dev_idx;
	dev = dal_find_dev(mei_device);
	if (!dev) {
		dev_dbg(dev, "can't find device\n");
		return -ENODEV;
	}

	ddev = to_dal_device(dev);
	dc = ddev->clients[DAL_INTF_KDI];
	if (!dc) {
		dev_dbg(dev, "client is NULL\n");
		ret = -EFAULT;
		goto out;
	}

	/* copy data to client object */
	memcpy(dc->write_buffer, buf, len);
	wr = dal_write(dc, len, seq);
	if (wr > 0)
		ret = 0;
	else
		ret = wr;
out:
	put_device(dev);
	return ret;
}

/**
 * dal_kdi_recv - a callback which is called from bhp to recv msg from DAL FW
 *
 * @dev_idx: DAL device type
 * @buf: buffer of received message
 * @count: input and output param -
 *       - input: buffer length
 *       - output: size of the received message
 *
 * Return: 0 on success
 *         -EINVAL on incorrect input
 *         -ENODEV when the device can't be found
 *         -EFAULT when client is NULL or copy failed
 *         -EMSGSIZE when buffer is too small
 *         <0 on dal_wait_for_read failure
 */
int dal_kdi_recv(unsigned int dev_idx, unsigned char *buf, size_t *count)
{
	enum dal_dev_type mei_device;
	struct dal_device *ddev;
	struct dal_client *dc;
	struct device *dev;
	int ret;
	size_t len;

	if (!buf || !count)
		return -EINVAL;

	if (dev_idx >= DAL_MEI_DEVICE_MAX)
		return -EINVAL;

	mei_device = (enum dal_dev_type)dev_idx;
	dev = dal_find_dev(mei_device);
	if (!dev)
		return -ENODEV;

	ddev = to_dal_device(dev);
	dc = ddev->clients[DAL_INTF_KDI];
	if (!dc) {
		dev_dbg(dev, "client is NULL\n");
		ret = -EFAULT;
		goto out;
	}

	ret = dal_wait_for_read(dc);

	if (ret)
		goto out;

	if (kfifo_is_empty(&dc->read_queue)) {
		*count = 0;
		goto out;
	}

	ret = kfifo_out(&dc->read_queue, &len, sizeof(len));
	if (ret != sizeof(len)) {
		dev_err(&ddev->dev, "could not copy buffer: cannot fetch size\n");
		ret = -EFAULT;
		goto out;
	}

	if (len > *count) {
		dev_dbg(&ddev->dev, "could not copy buffer: src size = %zd > dest size = %zd\n",
			len, *count);
		ret = -EMSGSIZE;
		goto out;
	}

	ret = kfifo_out(&dc->read_queue, buf, len);
	if (ret != len) {
		dev_err(&ddev->dev, "could not copy buffer: src size = %zd, dest size = %d\n",
			len, ret);
		ret = -EFAULT;
	}

	*count = len;
	ret = 0;
out:
	put_device(dev);
	return ret;
}

/**
 * dal_create_session - create session to an installed trusted application.
 *
 * @session_handle: output param to hold the session handle
 * @ta_id: trusted application (ta) id
 * @acp_pkg: acp file of the ta
 * @acp_pkg_len: acp file length
 * @init_param:	init parameters to the session (optional)
 * @init_param_len: length of the init parameters
 *
 * Return: 0 on success
 *         <0 on system failure
 *         >0 on DAL FW failure
 */
int dal_create_session(u64 *session_handle,  const char *ta_id,
		       const u8 *acp_pkg, size_t acp_pkg_len,
		       const u8 *init_param, size_t init_param_len)
{
	struct ac_ins_jta_pack_ext pack;
	char *ta_pkg;
	int ta_pkg_size;
	int ret;

	if (!ta_id || !acp_pkg || !acp_pkg_len || !session_handle)
		return -EINVAL;

	/* init_param are optional, if they exists the length shouldn't be 0 */
	if (!init_param && init_param_len != 0) {
		pr_debug("INVALID_PARAMS init_param %p init_param_len %zu\n",
			 init_param, init_param_len);
		return -EINVAL;
	}

	mutex_lock(&dal_kdi_lock);

	ret = acp_pload_ins_jta(acp_pkg, acp_pkg_len, &pack);
	if (ret) {
		pr_debug("acp_pload_ins_jta() return %d\n", ret);
		goto out;
	}

	ta_pkg = pack.ta_pack;
	if (!ta_pkg) {
		ret = -EINVAL;
		goto out;
	}

	ta_pkg_size = ta_pkg - (char *)acp_pkg;

	if (ta_pkg_size < 0 || (unsigned int)ta_pkg_size > acp_pkg_len) {
		ret = -EINVAL;
		goto out;
	}

	ta_pkg_size = acp_pkg_len - ta_pkg_size;

	ret = bh_ta_session_open(session_handle, ta_id, ta_pkg, ta_pkg_size,
				 init_param, init_param_len);

	if (ret)
		pr_debug("bh_ta_session_open failed = %d\n", ret);

out:
	mutex_unlock(&dal_kdi_lock);

	return to_kdi_err(ret);
}
EXPORT_SYMBOL(dal_create_session);

/**
 * dal_send_and_receive - send and receive data to/from ta
 *
 * @session_handle: session handle
 * @command_id: command id
 * @input: message to be sent
 * @input_len: sent message size
 * @output: output param to hold a pointer to the buffer which
 *          will contain the received message.
 *          This buffer is allocated by DAL KDI module and freed by the user
 * @output_len: input and output param -
 *              - input: the expected maximum length of the received message
 *              - output: size of the received message
 * @response_code: output param to hold the return value from the applet
 *
 * Return: 0 on success
 *         <0 on system failure
 *         >0 on DAL FW failure
 */
int dal_send_and_receive(u64 session_handle, int command_id, const u8 *input,
			 size_t input_len, u8 **output, size_t *output_len,
			 int *response_code)
{
	int ret;

	mutex_lock(&dal_kdi_lock);

	ret = bh_ta_session_command(session_handle, command_id,
				    input, input_len,
				    (void **)output, output_len,
				    response_code);

	if (ret)
		pr_debug("bh_ta_session_command failed, status = %d\n", ret);

	mutex_unlock(&dal_kdi_lock);

	return to_kdi_err(ret);
}
EXPORT_SYMBOL(dal_send_and_receive);

/**
 * dal_close_session - close ta session
 *
 * @session_handle: session handle
 *
 * Return: 0 on success
 *         <0 on system failure
 *         >0 on DAL FW failure
 */
int dal_close_session(u64 session_handle)
{
	int ret;

	mutex_lock(&dal_kdi_lock);

	ret = bh_ta_session_close(session_handle);

	if (ret)
		pr_debug("hp_close_ta_session failed = %d\n", ret);

	mutex_unlock(&dal_kdi_lock);

	return to_kdi_err(ret);
}
EXPORT_SYMBOL(dal_close_session);

/**
 * dal_set_ta_exclusive_access - set client to be owner of the ta,
 *                               so no one else (especially user space client)
 *                               will be able to open session to it
 *
 * @ta_id: trusted application (ta) id
 *
 * Return: 0 on success
 *         -ENODEV when the device can't be found
 *         -ENOMEM on memory allocation failure
 *         -EPERM when ta is owned by another client
 *         -EEXIST when ta is already owned by current client
 */
int dal_set_ta_exclusive_access(const uuid_t *ta_id)
{
	struct dal_device *ddev;
	struct device *dev;
	struct dal_client *dc;
	int ret;

	mutex_lock(&dal_kdi_lock);

	dev = dal_find_dev(DAL_MEI_DEVICE_IVM);
	if (!dev) {
		dev_dbg(dev, "can't find device\n");
		ret = -ENODEV;
		goto unlock;
	}

	ddev = to_dal_device(dev);
	dc = ddev->clients[DAL_INTF_KDI];

	ret = dal_access_policy_add(ddev, ta_id, dc);

	put_device(dev);
unlock:
	mutex_unlock(&dal_kdi_lock);
	return ret;
}
EXPORT_SYMBOL(dal_set_ta_exclusive_access);

/**
 * dal_unset_ta_exclusive_access - unset client from owning ta
 *
 * @ta_id: trusted application (ta) id
 *
 * Return: 0 on success
 *         -ENODEV when the device can't be found
 *         -ENOENT when ta isn't found in exclusiveness ta list
 *         -EPERM when ta is owned by another client
 */
int dal_unset_ta_exclusive_access(const uuid_t *ta_id)
{
	struct dal_device *ddev;
	struct device *dev;
	struct dal_client *dc;
	int ret;

	mutex_lock(&dal_kdi_lock);

	dev = dal_find_dev(DAL_MEI_DEVICE_IVM);
	if (!dev) {
		dev_dbg(dev, "can't find device\n");
		ret = -ENODEV;
		goto unlock;
	}

	ddev = to_dal_device(dev);
	dc = ddev->clients[DAL_INTF_KDI];

	ret = dal_access_policy_remove(ddev, ta_id, dc);

	put_device(dev);
unlock:
	mutex_unlock(&dal_kdi_lock);
	return ret;
}
EXPORT_SYMBOL(dal_unset_ta_exclusive_access);

#define KDI_MAJOR_VER         "1"
#define KDI_MINOR_VER         "0"
#define KDI_HOTFIX_VER        "0"

#define KDI_VERSION KDI_MAJOR_VER "." \
		    KDI_MINOR_VER "." \
		    KDI_HOTFIX_VER

/**
 * dal_get_version_info - return DAL driver version
 *
 * @version_info: output param to hold DAL driver version information
 *
 * Return: 0 on success
 *         -EINVAL on incorrect input
 */
int dal_get_version_info(struct dal_version_info *version_info)
{
	if (!version_info)
		return -EINVAL;

	memset(version_info, 0, sizeof(*version_info));
	snprintf(version_info->version, DAL_VERSION_LEN, "%s", KDI_VERSION);

	return 0;
}
EXPORT_SYMBOL(dal_get_version_info);

/**
 * dal_kdi_add_dev - add new dal device (one of dal_dev_type)
 *
 * @dev: device object which is associated with dal device
 * @class_intf: class interface
 *
 * Return: 0 on success
 *         <0 on failure
 *
 * When new dal device is added, a new client is created for
 * this device in kernel space interface
 */
static int dal_kdi_add_dev(struct device *dev,
			   struct class_interface *class_intf)
{
	int ret;
	struct dal_device *ddev;

	ddev = to_dal_device(dev);
	mutex_lock(&ddev->context_lock);
	ret = dal_dc_setup(ddev, DAL_INTF_KDI);
	mutex_unlock(&ddev->context_lock);
	return ret;
}

/**
 * dal_kdi_rm_dev - rm dal device (one of dal_dev_type)
 *
 * @dev: device object which is associated with dal device
 * @class_intf: class interface
 *
 * Return: 0 on success
 *         <0 on failure
 */
static void dal_kdi_rm_dev(struct device *dev,
			   struct class_interface *class_intf)
{
	struct dal_device *ddev;

	ddev = to_dal_device(dev);
	mutex_lock(&ddev->context_lock);
	dal_dc_destroy(ddev, DAL_INTF_KDI);
	mutex_unlock(&ddev->context_lock);
}

/*
 * dal_kdi_interface handles addition/removal of dal devices
 */
static struct class_interface dal_kdi_interface __refdata = {
	.add_dev    = dal_kdi_add_dev,
	.remove_dev = dal_kdi_rm_dev,
};

/**
 * dal_kdi_init - initialize dal kdi
 *
 * Return: 0 on success
 *         <0 on failure
 */
int dal_kdi_init(void)
{
	int ret;

	bh_init_internal();

	dal_kdi_interface.class = dal_class;
	ret = class_interface_register(&dal_kdi_interface);
	if (ret) {
		pr_err("failed to register class interface = %d\n", ret);
		goto err;
	}

	return 0;

err:
	bh_deinit_internal();
	return ret;
}

/**
 * dal_kdi_exit - dal kdi exit function
 */
void dal_kdi_exit(void)
{
	bh_deinit_internal();
	class_interface_unregister(&dal_kdi_interface);
}
