/*
 * mods_adsp.c - This file is part of NVIDIA MODS kernel driver.
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA MODS kernel driver is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * NVIDIA MODS kernel driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NVIDIA MODS kernel driver.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/uaccess.h>
#include "mods_internal.h"
#include <linux/tegra_nvadsp.h>

int esc_mods_adsp_load(struct file *pfile)
{
	return nvadsp_os_load();
}

int esc_mods_adsp_start(struct file *pfile)
{
	return nvadsp_os_start();
}

int esc_mods_adsp_stop(struct file *pfile)
{
	nvadsp_os_stop();
	return OK;
}

int esc_mods_adsp_run_app(struct file *pfile, struct MODS_ADSP_RUN_APP_INFO *p)
{
	int rc = -1;
	int max_retry = 3;
	int rcount = 0;
	nvadsp_app_handle_t handle;
	nvadsp_app_info_t *p_app_info;
	nvadsp_app_args_t app_args;

	handle = nvadsp_app_load(p->app_name,  p->app_file_name);
	if (!handle) {
		mods_error_printk("load adsp app fail");
		return -1;
	}

	if (p->argc > 0 && p->argc <= MODS_ADSP_APP_MAX_PARAM) {
		app_args.argc = p->argc;
		memcpy(app_args.argv, p->argv, p->argc * sizeof(__u32));
		p_app_info = nvadsp_app_init(handle, &app_args);
	} else
		p_app_info = nvadsp_app_init(handle, NULL);

	if (!p_app_info) {
		mods_error_printk("init adsp app fail");
		nvadsp_app_unload(handle);
		return -1;
	}

	rc = nvadsp_app_start(p_app_info);
	if (rc) {
		mods_error_printk("start adsp app fail");
		goto failed;
	}

	while (rcount++ < max_retry) {
		rc = wait_for_nvadsp_app_complete_timeout(p_app_info,
						msecs_to_jiffies(p->timeout));
		if (rc == -ERESTARTSYS)
			continue;
		else if (rc == 0) {
			mods_error_printk("app timeout(%d)", p->timeout);
			rc = -1;
		} else if (rc < 0) {
			mods_error_printk("run app failed, err=%d\n", rc);
			rc = -1;
		} else
			rc = 0;
		break;
	}

failed:
	nvadsp_app_deinit(p_app_info);
	nvadsp_app_unload(handle);

	return rc;
}
