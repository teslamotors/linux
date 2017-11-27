/*
 *  skl-sdw-pcm.h - Shared header file skylake PCM operations on soundwire
 *
 *  Copyright (C) 2014-2015 Intel Corp
 *  Author:  Hardik Shah  <hardik.t.shah@intel.com>
 *
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */
#ifndef _SKL_SDW_PCM_H
#define  _SKL_SDW_PCM_H
#include <linux/sdw_bus.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#define SDW_BE_DAI_ID_BASE	256
#define SDW_BE_DAI_ID_MSTR0	256
#define SDW_BE_DAI_ID_MSTR1	(SDW_BE_DAI_ID_MSTR0 + 32)
#define SDW_BE_DAI_ID_MSTR2	(SDW_BE_DAI_ID_MSTR1 + 32)
#define SDW_BE_DAI_ID_MSTR3	(SDW_BE_DAI_ID_MSTR2 + 32)


int cnl_sdw_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai);
int cnl_sdw_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai);
int cnl_sdw_hw_free(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai);
int cnl_sdw_pcm_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai);
int cnl_sdw_pcm_trigger(struct snd_pcm_substream *substream,
	int cmd, struct snd_soc_dai *dai);
void cnl_sdw_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai);

#endif /* _SKL_SDW_PCM_H */
