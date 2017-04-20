/*
 *
 * Intel DAL Keystore Linux driver
 * Copyright (c) 2013, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/dal.h>
#include <linux/printk.h>
#include "dal_client.h"

#include "keystore_debug.h"

struct dal_hdr_t dal_hdr;

int is_dal_session_created(void)
{
	if (dal_hdr.state == DAL_KSM_SESSION_CREATED)
		return 0;
	else
		return -EFAULT;
}

int create_session(void)
{
	int ret = 0;

	ret = dal_create_session(&(dal_hdr.session_handle),
		CONFIG_DAL_KEYSTORE_APPLET_ID, dal_hdr.acp_pkg,
		dal_hdr.acp_pkg_len, NULL, 0);

	if (ret != DAL_KDI_SUCCESS) {
		ks_err(KBUILD_MODNAME ": %s dal_create_session failed\n",
					__func__);
		return ret;
	} else {
		ks_debug(KBUILD_MODNAME ": %s dal_create_session succeeded\n",
					__func__);
	}

	return 0;
}

int install_applet(void)
{
	int ret = 0;
	u8 *acp_pkg = NULL;
	size_t acp_pkg_len = 0;

	dal_hdr.state = DAL_KSM_INIT;
	ret = read_keystore_applet(CONFIG_DAL_KEYSTORE_APPLET_PATH, &acp_pkg,
		&acp_pkg_len);
	if (ret != 0)
		return ret;

	if (acp_pkg == NULL) {
		ks_err(KBUILD_MODNAME ": %s acp_pkg is NULL\n", __func__);
		return -EFAULT;
	}

	ks_debug(KBUILD_MODNAME ": %s acp_pkg_len = %zu\n", __func__,
				acp_pkg_len);
	dal_hdr.acp_pkg = acp_pkg;
	dal_hdr.acp_pkg_len = acp_pkg_len;

	ret = create_session();
	if (ret != 0)
		return ret;

	dal_hdr.state = DAL_KSM_SESSION_CREATED;
	return 0;
}

int send_and_receive(size_t command_id, const u8 *input, size_t input_len,
			u8 **output, size_t *output_len, size_t *response_code)
{
	int ret = 0;

	/* output_len can not be 0 */
	ret = dal_send_and_receive(dal_hdr.session_handle,
	command_id, input, input_len, output, output_len, (int *)response_code);

	if (ret != DAL_KDI_SUCCESS) {
		ks_err(KBUILD_MODNAME ": %s dal_send_and_receive failed\n",
					__func__);
	} else {
		ks_debug(KBUILD_MODNAME ": %s dal_send_and_receive succeeded\n",
					__func__);
	}
	ks_debug(KBUILD_MODNAME
		": %s command id = %zu, response code = %zu output_len = %zu\n",
		__func__, command_id, *response_code, *output_len);

	return ret;
}

int close_session(void)
{
	int ret = 0;

	ret = dal_close_session(dal_hdr.session_handle);
	if (ret != DAL_KDI_SUCCESS)
		ks_err(KBUILD_MODNAME ": %s dal_close_session failed\n",
					__func__);

	dal_hdr.state = DAL_KSM_SESSION_CLOSED;

	return ret;
}

