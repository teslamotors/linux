/*
 * pinctrl-apl.h: Apollo Lake GPIO pinctrl header file
 *
 * Copyright (C) 2015 Intel Corporation
 * Author: Chew, Kean Ho <kean.ho.chew@intel.com>
 * Modified: Tan, Jui Nee <jui.nee.tan@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 */

#ifdef CONFIG_PINCTRL_APL_DEVICE
struct apl_pinctrl_port {
	char *unique_id;
};
#endif

