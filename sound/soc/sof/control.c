// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

/* Mixer Controls */

#include <linux/slab.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <sound/soc-topology.h>
#include <sound/soc.h>
#include <sound/control.h>
#include <uapi/sound/sof-ipc.h>
#include "sof-priv.h"

/* simple volume table TODO: to be replaced by coefficients from topology */
/* -52 dB to +12 dB in 2 dB steps, 33 values */
static const u32 volume_map[] = {
	165,
	207,
	261,
	328,
	414,
	521,
	655,
	825,
	1039,
	1308,
	1646,
	2072,
	2609,
	3285,
	4135,
	5206,
	6554,
	8250,
	10387,
	13076,
	16462,
	20724,
	26090,
	32846,
	41350,
	52057,
	65536, /* 0 dB for Qx.16 gain value */
	82505,
	103868,
	130762,
	164619,
	207243,
	260904
};

static inline u32 mixer_to_ipc(unsigned int value)
{
	if (value >= ARRAY_SIZE(volume_map))
		return volume_map[0];
	else
		return volume_map[value];
}

static inline u32 ipc_to_mixer(u32 value)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(volume_map); i++) {
		if (volume_map[i] >= value)
			return i;
	}

	return i - 1;
}

int snd_sof_volume_get(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *sm =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_sof_control *scontrol = sm->dobj.private;
	struct snd_sof_dev *sdev = scontrol->sdev;
	struct sof_ipc_ctrl_data *cdata = scontrol->control_data;
	unsigned int i, channels = scontrol->num_channels;

	pm_runtime_get_sync(sdev->dev);

	/* get all the mixer data from DSP */
	snd_sof_ipc_get_comp_data(sdev->ipc, scontrol, SOF_IPC_COMP_GET_VALUE,
				  SOF_CTRL_TYPE_VALUE_CHAN_GET,
				  SOF_CTRL_CMD_VOLUME);

	/* read back each channel */
	for (i = 0; i < channels; i++)
		ucontrol->value.integer.value[i] =
			ipc_to_mixer(cdata->chanv[i].value);

	pm_runtime_mark_last_busy(sdev->dev);
	pm_runtime_put_autosuspend(sdev->dev);
	return 0;
}

int snd_sof_volume_put(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *sm =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_sof_control *scontrol = sm->dobj.private;
	struct snd_sof_dev *sdev = scontrol->sdev;
	struct sof_ipc_ctrl_data *cdata = scontrol->control_data;
	unsigned int i, channels = scontrol->num_channels;

	pm_runtime_get_sync(sdev->dev);

	/* update each channel */
	for (i = 0; i < channels; i++) {
		cdata->chanv[i].value =
			mixer_to_ipc(ucontrol->value.integer.value[i]);
		cdata->chanv[i].channel = i;
	}

	/* notify DSP of mixer updates */
	snd_sof_ipc_set_comp_data(sdev->ipc, scontrol, SOF_IPC_COMP_SET_VALUE,
				  SOF_CTRL_TYPE_VALUE_CHAN_GET,
				  SOF_CTRL_CMD_VOLUME);

	pm_runtime_mark_last_busy(sdev->dev);
	pm_runtime_put_autosuspend(sdev->dev);
	return 0;
}

int snd_sof_enum_get(struct snd_kcontrol *kcontrol,
		     struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *se =
		(struct soc_enum *)kcontrol->private_value;
	struct snd_sof_control *scontrol = se->dobj.private;
	struct snd_sof_dev *sdev = scontrol->sdev;
	struct sof_ipc_ctrl_data *cdata = scontrol->control_data;
	unsigned int i, channels = scontrol->num_channels;

	pm_runtime_get_sync(sdev->dev);

	/* get all the mixer data from DSP */
	snd_sof_ipc_get_comp_data(sdev->ipc, scontrol, SOF_IPC_COMP_GET_VALUE,
				  SOF_CTRL_TYPE_VALUE_CHAN_GET,
				  SOF_CTRL_CMD_ENUM);

	/* read back each channel */
	for (i = 0; i < channels; i++)
		ucontrol->value.integer.value[i] = cdata->chanv[i].value;

	pm_runtime_mark_last_busy(sdev->dev);
	pm_runtime_put_autosuspend(sdev->dev);
	return 0;
}

int snd_sof_enum_put(struct snd_kcontrol *kcontrol,
		     struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *se =
		(struct soc_enum *)kcontrol->private_value;
	struct snd_sof_control *scontrol = se->dobj.private;
	struct snd_sof_dev *sdev = scontrol->sdev;
	struct sof_ipc_ctrl_data *cdata = scontrol->control_data;
	unsigned int i, channels = scontrol->num_channels;

	pm_runtime_get_sync(sdev->dev);

	/* update each channel */
	for (i = 0; i < channels; i++)
		cdata->chanv[i].value = ucontrol->value.integer.value[i];

	/* notify DSP of mixer updates */
	snd_sof_ipc_set_comp_data(sdev->ipc, scontrol, SOF_IPC_COMP_SET_VALUE,
				  SOF_CTRL_TYPE_VALUE_CHAN_SET,
				  SOF_CTRL_CMD_ENUM);

	pm_runtime_mark_last_busy(sdev->dev);
	pm_runtime_put_autosuspend(sdev->dev);
	return 0;
}

int snd_sof_bytes_get(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_bytes_ext *be =
		(struct soc_bytes_ext *)kcontrol->private_value;
	struct snd_sof_control *scontrol = be->dobj.private;
	struct snd_sof_dev *sdev = scontrol->sdev;
	//struct sof_ipc_ctrl_data *cdata = scontrol->control_data;
	//unsigned int i, channels = scontrol->num_channels;

	pm_runtime_get_sync(sdev->dev);

	/* get all the mixer data from DSP */
	snd_sof_ipc_get_comp_data(sdev->ipc, scontrol, SOF_IPC_COMP_GET_DATA,
				  SOF_CTRL_TYPE_DATA_GET, scontrol->cmd);

	/* TODO: copy back to userspace */

	pm_runtime_mark_last_busy(sdev->dev);
	pm_runtime_put_autosuspend(sdev->dev);
	return 0;
}

int snd_sof_bytes_put(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_bytes_ext *be =
		(struct soc_bytes_ext *)kcontrol->private_value;
	struct snd_sof_control *scontrol = be->dobj.private;
	struct snd_sof_dev *sdev = scontrol->sdev;
	//struct sof_ipc_ctrl_data *cdata = scontrol->control_data;
	//unsigned int i, channels = scontrol->num_channels;

	pm_runtime_get_sync(sdev->dev);

	/* TODO: copy from userspace */

	/* notify DSP of mixer updates */
	snd_sof_ipc_set_comp_data(sdev->ipc, scontrol, SOF_IPC_COMP_SET_DATA,
				  SOF_CTRL_TYPE_DATA_SET, scontrol->cmd);

	pm_runtime_mark_last_busy(sdev->dev);
	pm_runtime_put_autosuspend(sdev->dev);
	return 0;
}
