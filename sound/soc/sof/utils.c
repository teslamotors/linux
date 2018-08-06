// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Keyon Jie <yang.jie@linux.intel.com>
 */

#include <linux/device.h>
#include <sound/soc.h>
#include <sound/sof.h>
#include "sof-priv.h"

int sof_bes_setup(struct device *dev, struct snd_sof_dsp_ops *ops,
		  struct snd_soc_dai_link *links, int link_num,
		  struct snd_soc_card *card)
{
	char name[32];
	int i;

	if (!ops || !links || !card)
		return -EINVAL;

	/* set up BE dai_links */
	for (i = 0; i < link_num; i++) {
		snprintf(name, 32, "NoCodec-%d", i);
		links[i].name = kmemdup(name, sizeof(name), GFP_KERNEL);
		if (!links[i].name)
			goto no_mem;

		links[i].id = i;
		links[i].no_pcm = 1;
		links[i].cpu_dai_name = ops->dai_drv->drv[i].name;
		links[i].platform_name = "sof-audio";
		links[i].codec_dai_name = "snd-soc-dummy-dai";
		links[i].codec_name = "snd-soc-dummy";
		links[i].dpcm_playback = 1;
		links[i].dpcm_capture = 1;
	}

	card->dai_link = links;
	card->num_links = link_num;

	return 0;
no_mem:
	/* free allocated memories and return error */
	for (; i > 0; i--)
		kfree(links[i - 1].name);

	return -ENOMEM;
}
EXPORT_SYMBOL(sof_bes_setup);

