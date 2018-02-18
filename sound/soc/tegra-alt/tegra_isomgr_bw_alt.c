/*
 * tegra_isomgr_bw_alt.c - ADMA bandwidth calculation
 *
 * Copyright (c) 2016 NVIDIA CORPORATION.  All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/module.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/platform/tegra/isomgr.h>
#include <linux/platform/tegra/latency_allowance.h>
#include "tegra_isomgr_bw_alt.h"

#if defined(CONFIG_ARCH_TEGRA_18x_SOC) && defined(CONFIG_TEGRA_ISOMGR)

#define MAX_BW	393216 /*Maximum KiloByte*/
#define MAX_DEV_NUM 256

static long tegra_adma_calc_min_bandwidth(void);

static struct adma_isomgr {
	int current_bandwidth;
	bool device_number[MAX_DEV_NUM];
	int bw_per_device[MAX_DEV_NUM];
	struct mutex mutex;
	/* iso manager handle */
	tegra_isomgr_handle isomgr_handle;
} *adma;

static long tegra_adma_calc_min_bandwidth(void)
{
	int max_srate = 192; /*Khz*/
	int max_bpp = 4; /* Bytes per sample*/
	int slot = 8; /* Max Channel per stream*/
	int num_streams = 4; /* Min simultaneous usecase consideration*/
	long min_bw;

	min_bw = max_srate * max_bpp * slot * num_streams;

	return min_bw;
}

static int adma_isomgr_request(uint adma_bw, uint lt)
{
	int ret;

	if (!adma->isomgr_handle) {
		pr_err("%s: adma iso handle not initialized\n", __func__);
		return -1;
	}

	/* return value of tegra_isomgr_reserve is dvfs latency in usec */
	ret = tegra_isomgr_reserve(adma->isomgr_handle,
				adma_bw,	/* KB/sec */
				lt);	/* usec */
	if (!ret) {
		pr_err("%s: failed to reserve %u KBps\n", __func__, adma_bw);
		return -1;
	}

	/* return value of tegra_isomgr_realize is dvfs latency in usec */
	ret = tegra_isomgr_realize(adma->isomgr_handle);
	if (!ret) {
		pr_err("%s: failed to realize %u KBps\n", __func__, adma_bw);
		return -1;
	}

	return 0;
}

void tegra_isomgr_adma_setbw(struct snd_pcm_substream *substream,
				bool is_running)
{
	int bandwidth, sample_bytes;
	int ret;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_pcm *pcm = substream->pcm;

	if (!adma || !runtime)
		return;

	if (pcm->device >= MAX_DEV_NUM) {
		pr_err("%s: PCM device number is greater than %d\n", __func__,
			MAX_DEV_NUM);
		return;
	}

	if (((adma->device_number[pcm->device] == true) && is_running) ||
		((adma->device_number[pcm->device] == false) && !is_running)
		)
		return;

	mutex_lock(&adma->mutex);

	if (is_running) {
		sample_bytes = snd_pcm_format_width(runtime->format)/8;
		if (sample_bytes < 0)
			sample_bytes = 0;

		/* KB/s kilo bytes per sec */
		bandwidth = runtime->channels * (runtime->rate/1000) *
				sample_bytes;

		adma->device_number[pcm->device] = true;
		adma->current_bandwidth += bandwidth;
		adma->bw_per_device[pcm->device] = bandwidth;
	} else {
		adma->device_number[pcm->device] = false;
		adma->current_bandwidth -= adma->bw_per_device[pcm->device];
		adma->bw_per_device[pcm->device] = 0;
	}

	mutex_unlock(&adma->mutex);

	if (adma->current_bandwidth < 0) {
		pr_err("%s: ADMA ISO BW can't be less than zero\n", __func__);
		adma->current_bandwidth = 0;
	} else if (adma->current_bandwidth > MAX_BW) {
		pr_err("%s: ADMA ISO BW can't be more than %d\n", __func__,
			MAX_BW);
		adma->current_bandwidth = MAX_BW;
	}

	ret = adma_isomgr_request(adma->current_bandwidth, 1000);
	if (!ret) {
		/* Call LA/PTSA driver which will configure the Memory
		controller to support APEDMA's new BW requirement in MBps*/
		ret = tegra_set_latency_allowance(TEGRA_LA_APEDMAW,
				((adma->current_bandwidth + 999)/1000));
		if (ret)
			pr_err("%s: LA/PTSA setting Failed\n", __func__);
	}
}
EXPORT_SYMBOL(tegra_isomgr_adma_setbw);

void tegra_isomgr_adma_renegotiate(void *p, u32 avail_bw)
{
	/* For Audio usecase there is no possibility of renegotiation
	as it may lead to glitches. So currently dummy renegotiate call
	is added to support bandwidth request more than registered bw which
	got initialized during register call */
}
EXPORT_SYMBOL(tegra_isomgr_adma_renegotiate);

void tegra_isomgr_adma_register(void)
{
	adma = kzalloc(sizeof(struct adma_isomgr), GFP_KERNEL);
	if (!adma) {
		pr_err("%s: Failed to allocate adma isomgr struct\n", __func__);
		return;
	}

	adma->current_bandwidth = 0;
	memset(&adma->device_number, 0, sizeof(bool) * MAX_DEV_NUM);
	memset(&adma->bw_per_device, 0, sizeof(int) * MAX_DEV_NUM);

	mutex_init(&adma->mutex);

	/* Register the required BW for adma usecases.*/
	adma->isomgr_handle = tegra_isomgr_register(TEGRA_ISO_CLIENT_APE_ADMA,
					tegra_adma_calc_min_bandwidth(),
					tegra_isomgr_adma_renegotiate,
					adma);
	if (IS_ERR(adma->isomgr_handle)) {
		pr_err("%s: Failed to register adma isomgr client. err=%ld\n",
			__func__, PTR_ERR(adma->isomgr_handle));
		adma->isomgr_handle = NULL;
		mutex_destroy(&adma->mutex);
		kfree(adma);
		adma = NULL;
	}
}
EXPORT_SYMBOL(tegra_isomgr_adma_register);
void tegra_isomgr_adma_unregister(void)
{
	if (!adma)
		return;

	mutex_destroy(&adma->mutex);

	if (adma->isomgr_handle) {
		tegra_isomgr_unregister(adma->isomgr_handle);
		adma->isomgr_handle = NULL;
	}

	kfree(adma);
	adma = NULL;
}
EXPORT_SYMBOL(tegra_isomgr_adma_unregister);
#endif

MODULE_LICENSE("GPL");
