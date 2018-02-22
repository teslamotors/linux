/******************************************************************************
 * Intel dal test Linux driver
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/gfp.h>
#include <linux/uuid.h>
#include <linux/ctype.h>
#include <linux/sizes.h>
#include <linux/atomic.h>

#include <linux/mei_cl_bus.h>
#include <linux/dal.h>

#include "uapi/kdi_cmd_defs.h"
#define KDI_MODULE "mei_dal"

/**
 * this is the max data size possible:
 * there is no actually max size for acp file,
 * but for testing 512k is good enough
 */
#define MAX_DATA_SIZE SZ_512K

#define KDI_TEST_OPENED 0

/**
 * struct dal_test_data - dal test cmd and response data
 *
 * @cmd_data_size: size of cmd got from user space
 * @cmd_data: the cmd got from user space
 * @cmd_lock: protects cmd_data buffer
 *
 * @resp_data_size: size of response from kdi
 * @resp_data: the response from kdi
 * @resp_lock: protects resp_data buffer
 */
struct dal_test_data {
	u32 cmd_data_size;
	u8 *cmd_data;
	struct mutex cmd_lock; /* protects cmd_data buffer */

	u32 resp_data_size;
	u8 *resp_data;
	struct mutex resp_lock; /* protects resp_data buffer */
};

/**
 * struct dal_test_device - dal test private data
 *
 * @dev: the device structure
 * @cdev: character device
 *
 * @kdi_test_status: status of test module
 * @data: cmd and response data
 */
static struct dal_test_device {
	struct device *dev;
	struct cdev cdev;

	unsigned long kdi_test_status;
	struct dal_test_data *data;
} dal_test_dev;

#ifdef CONFIG_MODULES
/**
 * dal_test_find_module - find the given module
 *
 * @mod_name: the module name to find
 *
 * Return: pointer to the module if it is found
 *         NULL otherwise
 */
static struct module *dal_test_find_module(const char *mod_name)
{
	struct module *mod;

	mutex_lock(&module_mutex);
	mod = find_module(mod_name);
	mutex_unlock(&module_mutex);

	return mod;
}

/**
 * dal_test_load_kdi - load kdi module
 *
 * @dev: dal test device
 *
 * Return: 0 on success
 *         <0 on failure
 */
static int dal_test_load_kdi(struct dal_test_device *dev)
{
	struct module *mod;

	/* load KDI if it wasn't loaded */
	request_module(KDI_MODULE);

	mod = dal_test_find_module(KDI_MODULE);
	if (!mod) {
		dev_err(dev->dev, "failed to find KDI module: %s\n",
			KDI_MODULE);
		return -ENODEV;
	}

	if (!try_module_get(mod)) {
		dev_err(dev->dev, "failed to get KDI module\n");
		return  -EFAULT;
	}

	return 0;
}

/**
 * dal_test_unload_kdi - unload kdi module
 *
 * @dev: dal test device
 *
 * Return: 0 on success
 *         <0 on failure
 */
static int dal_test_unload_kdi(struct dal_test_device *dev)
{
	struct module *mod;

	mod = dal_test_find_module(KDI_MODULE);
	if (!mod) {
		dev_err(dev->dev, "failed to find KDI module: %s\n",
			KDI_MODULE);
		return -ENODEV;
	}
	module_put(mod);

	return 0;
}
#else
static inline int dal_test_load_kdi(struct dal_test_device *dev) { return 0; }
static inline int dal_test_unload_kdi(struct dal_test_device *dev) { return 0; }
#endif

/**
 * dal_test_result_set - set data to the result buffer
 *
 * @test_data: test command and response buffers
 * @data:  new data
 * @size:  size of the data buffer
 */
static void dal_test_result_set(struct dal_test_data *test_data,
				void *data, u32 size)
{
	memcpy(test_data->resp_data, data, size);
	test_data->resp_data_size = size;
}

/**
 * dal_test_result_append - append data to the result buffer
 *
 * @test_data: test command and response buffers
 * @data:  new data
 * @size:  size of the data buffer
 */
static void dal_test_result_append(struct dal_test_data *test_data,
				   void *data, u32 size)
{
	size_t offset = test_data->resp_data_size;

	memcpy(test_data->resp_data + offset, data, size);
	test_data->resp_data_size += size;
}

/**
 * dal_test_send_and_recv - call send and receive function of kdi
 *
 * @dev: dal test device
 * @t_cmd: the command to send kdi
 * @t_data: test command and response buffers
 */
static void dal_test_send_and_recv(struct dal_test_device *dev,
				   struct kdi_test_command *t_cmd,
				   struct dal_test_data *t_data)
{
	struct send_and_rcv_cmd *cmd;
	struct send_and_rcv_resp resp;
	ssize_t data_size;
	size_t output_len;
	s32 response_code;
	u8 *input;
	u8 *output;
	s32 status;

	memset(&resp, 0, sizeof(resp));

	cmd = (struct send_and_rcv_cmd *)t_cmd->data;
	data_size = t_data->cmd_data_size - sizeof(t_cmd->cmd_id) -
		    sizeof(*cmd);
	if (data_size < 0) {
		dev_dbg(dev->dev, "malformed command struct: data_size = %zu\n",
			data_size);
		resp.test_mod_status = -EINVAL;

		mutex_lock(&t_data->resp_lock);
		dal_test_result_set(t_data, &resp, sizeof(resp));
		mutex_unlock(&t_data->resp_lock);
		return;
	}

	response_code = 0;
	output = NULL;
	input = (data_size) ? cmd->input : NULL;
	output_len = (cmd->is_output_len_ptr) ? cmd->output_buf_len : 0;

	dev_dbg(dev->dev, "call dal_send_and_receive: handle=%llu command_id=%d input_len=%zd\n",
		cmd->session_handle, cmd->command_id, data_size);

	status = dal_send_and_receive(cmd->session_handle, cmd->command_id,
				      input, data_size,
				      cmd->is_output_buf ? &output : NULL,
				      cmd->is_output_len_ptr ?
				      &output_len : NULL,
				      cmd->is_response_code_ptr ?
				      &response_code : NULL);

	dev_dbg(dev->dev, "dal_send_and_receive return: status=%d output_len=%zu response_code=%d\n",
		status, output_len, response_code);

	resp.output_len = (u32)output_len;
	resp.response_code = response_code;
	resp.status = status;
	resp.test_mod_status = 0;

	/* in case the call failed we don't copy the data */
	mutex_lock(&t_data->resp_lock);
	dal_test_result_set(t_data, &resp, sizeof(resp));
	if (output && resp.output_len)
		dal_test_result_append(t_data, output, resp.output_len);
	mutex_unlock(&t_data->resp_lock);

	kfree(output);
}

/**
 * dal_test_create_session - call create session function of kdi
 *
 * @dev: dal test device
 * @t_cmd: the command to send kdi
 * @t_data: test command and response buffers
 */
static void dal_test_create_session(struct dal_test_device *dev,
				    struct kdi_test_command *t_cmd,
				    struct dal_test_data *t_data)
{
	struct session_create_cmd *cmd;
	struct session_create_resp resp;
	u32 data_size;
	u64 handle;
	char *app_id;
	u8 *acp_pkg;
	u8 *init_params;
	u32 offset;
	s32 status;

	memset(&resp, 0, sizeof(resp));

	cmd  = (struct session_create_cmd *)t_cmd->data;
	data_size = t_data->cmd_data_size - sizeof(t_cmd->cmd_id) -
		    sizeof(*cmd);

	if (cmd->app_id_len + cmd->acp_pkg_len + cmd->init_param_len !=
	    data_size) {
		dev_dbg(dev->dev, "malformed command struct: data_size = %d\n",
			data_size);
		resp.test_mod_status = -EINVAL;

		mutex_lock(&t_data->resp_lock);
		dal_test_result_set(t_data, &resp, sizeof(resp));
		mutex_unlock(&t_data->resp_lock);
		return;
	}

	handle = 0;

	offset = 0;
	app_id = (cmd->app_id_len) ? cmd->data + offset : NULL;
	offset += cmd->app_id_len;

	acp_pkg = (cmd->acp_pkg_len) ? cmd->data + offset : NULL;
	offset += cmd->acp_pkg_len;

	init_params = (cmd->init_param_len) ? cmd->data + offset : NULL;
	offset += cmd->init_param_len;

	dev_dbg(dev->dev, "call dal_create_session params: app_id = %s, app_id len = %d, acp pkg len = %d, init params len = %d\n",
		app_id, cmd->app_id_len, cmd->acp_pkg_len, cmd->init_param_len);

	status = dal_create_session(cmd->is_session_handle_ptr ?
				    &handle : NULL,
				    app_id, acp_pkg,
				    cmd->acp_pkg_len,
				    init_params,
				    cmd->init_param_len);
	dev_dbg(dev->dev, "dal_create_session return: status = %d, handle = %llu\n",
		status, handle);

	resp.session_handle = handle;
	resp.status = status;
	resp.test_mod_status = 0;

	mutex_lock(&t_data->resp_lock);
	dal_test_result_set(t_data, &resp, sizeof(resp));
	mutex_unlock(&t_data->resp_lock);
}

/**
 * dal_test_close_session - call close session function of kdi
 *
 * @dev: dal test device
 * @t_cmd: the command to send kdi
 * @t_data: test command and response buffers
 */
static void dal_test_close_session(struct dal_test_device *dev,
				   struct kdi_test_command *t_cmd,
				   struct dal_test_data *t_data)
{
	struct session_close_cmd *cmd;
	struct session_close_resp resp;

	memset(&resp, 0, sizeof(resp));

	cmd  = (struct session_close_cmd *)t_cmd->data;
	if (t_data->cmd_data_size != sizeof(t_cmd->cmd_id) + sizeof(*cmd)) {
		dev_dbg(dev->dev, "malformed command struct\n");
		resp.test_mod_status = -EINVAL;

		mutex_lock(&t_data->resp_lock);
		dal_test_result_set(t_data, &resp, sizeof(resp));
		mutex_unlock(&t_data->resp_lock);
		return;
	}

	resp.status = dal_close_session(cmd->session_handle);
	resp.test_mod_status = 0;

	mutex_lock(&t_data->resp_lock);
	dal_test_result_set(t_data, &resp, sizeof(resp));
	mutex_unlock(&t_data->resp_lock);
}

/**
 * dal_test_version_info - call get version function of kdi
 *
 * @dev: dal test device
 * @t_cmd: the command to send kdi
 * @t_data: test command and response buffers
 */
static void dal_test_version_info(struct dal_test_device *dev,
				  struct kdi_test_command *t_cmd,
				  struct dal_test_data *t_data)
{
	struct version_get_info_cmd *cmd;
	struct version_get_info_resp resp;
	struct dal_version_info *version;

	memset(&resp, 0, sizeof(resp));

	cmd  = (struct version_get_info_cmd *)t_cmd->data;
	if (t_data->cmd_data_size != sizeof(t_cmd->cmd_id) + sizeof(*cmd)) {
		dev_dbg(dev->dev, "malformed command struct\n");
		resp.test_mod_status = -EINVAL;
		mutex_lock(&t_data->resp_lock);
		dal_test_result_set(t_data, &resp, sizeof(resp));
		mutex_unlock(&t_data->resp_lock);
		return;
	}

	version = (cmd->is_version_ptr) ?
		  (struct dal_version_info *)resp.kdi_version : NULL;

	resp.status = dal_get_version_info(version);
	resp.test_mod_status = 0;

	mutex_lock(&t_data->resp_lock);
	dal_test_result_set(t_data, &resp, sizeof(resp));
	mutex_unlock(&t_data->resp_lock);
}

/**
 * dal_test_set_ex_access - call set/remove access function of kdi
 *
 * @dev: dal test device
 * @t_cmd: the command to send kdi
 * @t_data: test command and response buffers
 * @set_access: true when calling set access function
 *              false when calling remove access function
 */
static void dal_test_set_ex_access(struct dal_test_device *dev,
				   struct kdi_test_command *t_cmd,
				   struct dal_test_data *t_data,
				   bool set_access)
{
	struct ta_access_set_remove_cmd *cmd;
	struct ta_access_set_remove_resp resp;
	u32 data_size;
	uuid_t app_uuid;
	char *app_id;
	s32 status;

	memset(&resp, 0, sizeof(resp));

	cmd  = (struct ta_access_set_remove_cmd *)t_cmd->data;
	data_size = t_data->cmd_data_size - sizeof(t_cmd->cmd_id) -
		    sizeof(*cmd);

	if (cmd->app_id_len != data_size) {
		dev_dbg(dev->dev, "malformed command struct\n");
		resp.test_mod_status = -EINVAL;

		mutex_lock(&t_data->resp_lock);
		dal_test_result_set(t_data, &resp, sizeof(resp));
		mutex_unlock(&t_data->resp_lock);
		return;
	}

	app_id = (cmd->app_id_len) ? cmd->data : NULL;

	status = dal_uuid_parse(app_id, &app_uuid);
	if (status < 0)
		goto out;

	if (set_access)
		status = dal_set_ta_exclusive_access(&app_uuid);
	else
		status = dal_unset_ta_exclusive_access(&app_uuid);

out:
	resp.status = status;
	resp.test_mod_status = 0;

	mutex_lock(&t_data->resp_lock);
	dal_test_result_set(t_data, &resp, sizeof(resp));
	mutex_unlock(&t_data->resp_lock);
}

/**
 * dal_test_kdi_command - parse and invoke the requested command
 *
 * @dev: dal test device
 */
static void dal_test_kdi_command(struct dal_test_device *dev)
{
	struct dal_test_data *test_data;
	struct kdi_test_command *cmd;
	s32 status;

	test_data = dev->data;
	cmd = (struct kdi_test_command *)test_data->cmd_data;

	if (test_data->cmd_data_size < sizeof(cmd->cmd_id)) {
		dev_dbg(dev->dev, "malformed command struct\n");
		status = -EINVAL;
		goto prep_err_test_mod;
	}

	switch (cmd->cmd_id) {
	case KDI_SESSION_CREATE: {
		dev_dbg(dev->dev, "KDI_CREATE_SESSION[%d]\n", cmd->cmd_id);
		dal_test_create_session(dev, cmd, test_data);
		break;
	}
	case KDI_SESSION_CLOSE: {
		dev_dbg(dev->dev, "KDI_CLOSE_SESSION[%d]\n", cmd->cmd_id);
		dal_test_close_session(dev, cmd, test_data);
		break;
	}
	case KDI_SEND_AND_RCV: {
		dev_dbg(dev->dev, "KDI_SEND_AND_RCV[%d]\n", cmd->cmd_id);
		dal_test_send_and_recv(dev, cmd, test_data);
		break;
	}
	case KDI_VERSION_GET_INFO: {
		dev_dbg(dev->dev, "KDI_GET_VERSION_INFO[%d]\n", cmd->cmd_id);
		dal_test_version_info(dev, cmd, test_data);
		break;
	}
	case KDI_EXCLUSIVE_ACCESS_SET:
	case KDI_EXCLUSIVE_ACCESS_REMOVE: {
		dev_dbg(dev->dev, "KDI_SET_EXCLUSIVE_ACCESS or KDI_REMOVE_EXCLUSIVE_ACCESS[%d]\n",
			cmd->cmd_id);
		dal_test_set_ex_access(dev, cmd, test_data,
				       cmd->cmd_id == KDI_EXCLUSIVE_ACCESS_SET);
		break;
	}
	default:
		dev_dbg(dev->dev, "unknown command %d\n", cmd->cmd_id);
		status = -EINVAL;
		goto prep_err_test_mod;
	}

	return;

prep_err_test_mod:
	mutex_lock(&test_data->resp_lock);
	dal_test_result_set(test_data, &status, sizeof(status));
	mutex_unlock(&test_data->resp_lock);
}

/**
 * dal_test_read - dal test read function
 *
 * @filp: pointer to file structure
 * @buff: pointer to user buffer
 * @count: buffer length
 * @offp: data offset in buffer
 *
 * Return: >=0 data length on success
 *         <0 on failure
 */
static ssize_t dal_test_read(struct file *filp, char __user *buff, size_t count,
			     loff_t *offp)
{
	struct dal_test_device *dev;
	struct dal_test_data *test_data;
	int ret;

	dev = filp->private_data;
	test_data = dev->data;

	mutex_lock(&test_data->resp_lock);

	if (test_data->resp_data_size > count) {
		ret = -EMSGSIZE;
		goto unlock;
	}

	dev_dbg(dev->dev, "copying %d bytes to userspace\n",
		test_data->resp_data_size);
	if (copy_to_user(buff, test_data->resp_data,
			 test_data->resp_data_size)) {
		dev_dbg(dev->dev, "copy_to_user failed\n");
		ret = -EFAULT;
		goto unlock;
	}
	ret = test_data->resp_data_size;

unlock:
	mutex_unlock(&test_data->resp_lock);

	return ret;
}

/**
 * dal_test_write - dal test write function
 *
 * @filp: pointer to file structure
 * @buff: pointer to user buffer
 * @count: buffer length
 * @offp: data offset in buffer
 *
 * Return: >=0 data length on success
 *         <0 on failure
 */
static ssize_t dal_test_write(struct file *filp, const char __user *buff,
			      size_t count, loff_t *offp)
{
	struct dal_test_device *dev;
	struct dal_test_data *test_data;

	dev = filp->private_data;
	test_data = dev->data;

	if (count > MAX_DATA_SIZE)
		return -EMSGSIZE;

	mutex_lock(&test_data->cmd_lock);

	if (copy_from_user(test_data->cmd_data, buff, count)) {
		mutex_unlock(&test_data->cmd_lock);
		dev_dbg(dev->dev, "copy_from_user failed\n");
		return -EFAULT;
	}

	test_data->cmd_data_size = count;
	dev_dbg(dev->dev, "write %zu bytes\n", count);

	dal_test_kdi_command(dev);

	mutex_unlock(&test_data->cmd_lock);

	return count;
}

/**
 * dal_test_open - dal test open function
 *
 * @inode: pointer to inode structure
 * @filp: pointer to file structure
 *
 * Return: 0 on success
 *         <0 on failure
 */
static int dal_test_open(struct inode *inode, struct file *filp)
{
	struct dal_test_device *dev;
	struct dal_test_data *test_data;
	int ret;

	dev = container_of(inode->i_cdev, struct dal_test_device, cdev);
	if (!dev)
		return -ENODEV;

	/* single open */
	if (test_and_set_bit(KDI_TEST_OPENED, &dev->kdi_test_status))
		return -EBUSY;

	test_data = kzalloc(sizeof(*test_data), GFP_KERNEL);
	if (!test_data) {
		ret = -ENOMEM;
		goto err_clear_bit;
	}

	test_data->cmd_data = kzalloc(MAX_DATA_SIZE, GFP_KERNEL);
	test_data->resp_data = kzalloc(MAX_DATA_SIZE, GFP_KERNEL);
	if (!test_data->cmd_data || !test_data->resp_data) {
		ret = -ENOMEM;
		goto err_free;
	}

	mutex_init(&test_data->cmd_lock);
	mutex_init(&test_data->resp_lock);

	ret = dal_test_load_kdi(dev);
	if (ret)
		goto err_free;

	dev->data = test_data;
	filp->private_data = dev;

	return nonseekable_open(inode, filp);

err_free:
	kfree(test_data->cmd_data);
	kfree(test_data->resp_data);
	kfree(test_data);

err_clear_bit:
	clear_bit(KDI_TEST_OPENED, &dev->kdi_test_status);

	return ret;
}

/**
 * dal_test_release - dal test release function
 *
 * @inode: pointer to inode structure
 * @filp: pointer to file structure
 *
 * Return: 0 on success
 *         <0 on failure
 */
static int dal_test_release(struct inode *inode, struct file *filp)
{
	struct dal_test_device *dev;
	struct dal_test_data *test_data;

	dev = filp->private_data;
	if (!dev)
		return -ENODEV;

	dal_test_unload_kdi(dev);

	test_data = dev->data;
	if (test_data) {
		kfree(test_data->cmd_data);
		kfree(test_data->resp_data);
		kfree(test_data);
	}

	clear_bit(KDI_TEST_OPENED, &dev->kdi_test_status);

	filp->private_data = NULL;

	return 0;
}

static const struct file_operations dal_test_fops = {
	.owner    = THIS_MODULE,
	.open     = dal_test_open,
	.release  = dal_test_release,
	.read     = dal_test_read,
	.write    = dal_test_write,
	.llseek   = no_llseek,
};

/**
 * dal_test_exit - destroy dal test device
 */
static void __exit dal_test_exit(void)
{
	struct dal_test_device *dev = &dal_test_dev;
	struct class *dal_test_class;
	static dev_t devt;

	dal_test_class = dev->dev->class;
	devt = dev->dev->devt;

	cdev_del(&dev->cdev);
	unregister_chrdev_region(devt, MINORMASK);
	device_destroy(dal_test_class, devt);
	class_destroy(dal_test_class);
}

/**
 * dal_test_init - initiallize dal test device
 *
 * Return: 0 on success
 *         <0 on failure
 */
static int __init dal_test_init(void)
{
	struct dal_test_device *dev = &dal_test_dev;
	struct class *dal_test_class;
	static dev_t devt;
	int ret;

	ret = alloc_chrdev_region(&devt, 0, 1, "mei_dal_test");
	if (ret)
		return ret;

	dal_test_class = class_create(THIS_MODULE, "mei_dal_test");
	if (IS_ERR(dal_test_class)) {
		ret = PTR_ERR(dal_test_class);
		dal_test_class = NULL;
		goto err_unregister_cdev;
	}

	dev->dev = device_create(dal_test_class, NULL, devt, dev, "dal_test0");
	if (IS_ERR(dev->dev)) {
		ret = PTR_ERR(dev->dev);
		goto err_class_destroy;
	}

	cdev_init(&dev->cdev, &dal_test_fops);
	dev->cdev.owner = THIS_MODULE;
	ret = cdev_add(&dev->cdev, devt, 1);
	if (ret)
		goto err_device_destroy;

	return 0;

err_device_destroy:
	device_destroy(dal_test_class, devt);
err_class_destroy:
	class_destroy(dal_test_class);
err_unregister_cdev:
	unregister_chrdev_region(devt, 1);

	return ret;
}

module_init(dal_test_init);
module_exit(dal_test_exit);

MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("Intel(R) DAL test");
MODULE_LICENSE("GPL v2");
