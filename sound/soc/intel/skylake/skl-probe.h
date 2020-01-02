
/*
 *  skl-probe.h - Skylake probe header file
 *
 *  Copyright (C) 2015-16 Intel Corp
 *  Author: Divya Prakash <divya1.prakash@intel.com>
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

int skl_probe_compr_open(struct snd_compr_stream *substream,
					struct snd_soc_dai *dai);

int skl_probe_compr_set_params(struct snd_compr_stream *substream,
			struct snd_compr_params *params, struct snd_soc_dai *dai);

int skl_probe_compr_tstamp(struct snd_compr_stream *stream,
			struct snd_compr_tstamp *tstamp, struct snd_soc_dai *dai);
int skl_probe_compr_close(struct snd_compr_stream *substream,
					struct snd_soc_dai *dai);
int skl_probe_compr_ack(struct snd_compr_stream *substream, size_t bytes,
					struct snd_soc_dai *dai);
int skl_probe_compr_copy(struct snd_compr_stream *stream, char __user *buf,
					size_t count, struct snd_soc_dai *dai);
int skl_probe_compr_trigger(struct snd_compr_stream *substream, int cmd,
					struct snd_soc_dai *dai);
