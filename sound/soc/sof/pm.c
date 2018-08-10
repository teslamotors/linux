// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <linux/mm.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <sound/core.h>
#include <sound/soc.h>
#include "sof-priv.h"

int snd_sof_runtime_suspend(struct device *dev)
{
	return 0;
}
EXPORT_SYMBOL(snd_sof_runtime_suspend);

int snd_sof_runtime_resume(struct device *dev)
{
	return 0;
}
EXPORT_SYMBOL(snd_sof_runtime_resume);

int snd_sof_resume(struct device *dev)
{
	return 0;
}
EXPORT_SYMBOL(snd_sof_resume);

int snd_sof_suspend(struct device *dev)
{
	return 0;
}
EXPORT_SYMBOL(snd_sof_suspend);

int snd_sof_suspend_late(struct device *dev)
{
	return 0;
}
EXPORT_SYMBOL(snd_sof_suspend_late);
