/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDE_CAMRTC_CTRL_COMMANDS_H
#define INCLUDE_CAMRTC_CTRL_COMMANDS_H

#include "camrtc-common.h"

enum camrtc_pm_ctrl_status {
	CAMRTC_PM_CTRL_STATUS_OK = 0,
	CAMRTC_PM_CTRL_STATUS_ERROR = 1,	/* Generic error */
	CAMRTC_PM_CTRL_STATUS_CMD_UNKNOWN = 2,	/* Unknown command */
	CAMRTC_PM_CTRL_STATUS_NOT_IMPLEMENTED = 3,/* Command not implemented */
	CAMRTC_PM_CTRL_STATUS_INVALID_PARAM = 4,/* Invalid parameter */
};

enum camrtc_pm_ctrl_state {
	CAMRTC_PM_CTRL_STATE_ACTIVE = 0,
	CAMRTC_PM_CTRL_STATE_SUSPEND,
	CAMRTC_PM_CTRL_STATE_IDLE,
	CAMRTC_PM_CTRL_STATE_TYPE_MAX,
};

#endif /* INCLUDE_CAMRTC_PM_CTRL_COMMANDS_H */
