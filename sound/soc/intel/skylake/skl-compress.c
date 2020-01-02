
/*
 *  skl-compress.c -ASoC HDA Platform driver file implementing compress functionality
 *
 *  Copyright (C) 2015-2016 Intel Corp
 *  Author:  Divya Prakash <divya1.prakash@intel.com>
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

#include <linux/pci.h>
#include <linux/pm_runtime.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include "skl.h"
#include "skl-compress.h"

struct hdac_ext_bus *get_bus_compr_ctx(struct snd_compr_stream *substream)
{
	struct hdac_ext_stream *stream = get_hdac_ext_compr_stream(substream);
	struct hdac_stream *hstream = hdac_stream(stream);
	struct hdac_bus *bus = hstream->bus;

	return hbus_to_ebus(bus);
}

void skl_set_compr_runtime_buffer(struct snd_compr_stream *substream,
				struct snd_dma_buffer *bufp, size_t size)
{
	struct snd_compr_runtime *runtime = substream->runtime;

	if (bufp) {
		runtime->dma_buffer_p = bufp;
		runtime->dma_area = bufp->area;
		runtime->dma_addr = bufp->addr;
		runtime->dma_bytes = size;
	} else {
		runtime->dma_buffer_p = NULL;
		runtime->dma_area = NULL;
		runtime->dma_addr = 0;
		runtime->dma_bytes = 0;
	}
}

int skl_compr_malloc_pages(struct snd_compr_stream *substream,
					struct hdac_ext_bus *ebus, size_t size)
{
	struct snd_compr_runtime *runtime;
	struct snd_dma_buffer *dmab = NULL;
	struct skl *skl = ebus_to_skl(ebus);

	runtime = substream->runtime;

	dmab = kzalloc(sizeof(*dmab), GFP_KERNEL);
	if (!dmab)
		return -ENOMEM;
	substream->dma_buffer.dev.type = SNDRV_DMA_TYPE_DEV_SG;
	substream->dma_buffer.dev.dev = snd_dma_pci_data(skl->pci);
	dmab->dev = substream->dma_buffer.dev;
	if (snd_dma_alloc_pages(substream->dma_buffer.dev.type,
				substream->dma_buffer.dev.dev,
				size, dmab) < 0) {
		dev_err(ebus_to_hbus(ebus)->dev,
			"Error in snd_dma_alloc_pages\n");
		kfree(dmab);
		return -ENOMEM;
	}
	skl_set_compr_runtime_buffer(substream, dmab, size);

	return 1;
}

int skl_substream_alloc_compr_pages(struct hdac_ext_bus *ebus,
				 struct snd_compr_stream *substream,
				 size_t size)
{
	struct hdac_ext_stream *stream = get_hdac_ext_compr_stream(substream);
	int ret;

	hdac_stream(stream)->bufsize = 0;
	hdac_stream(stream)->period_bytes = 0;
	hdac_stream(stream)->format_val = 0;

	ret = skl_compr_malloc_pages(substream, ebus, size);
	if (ret < 0)
		return ret;
	ebus->bus.io_ops->mark_pages_uc(snd_pcm_get_dma_buf(substream), true);

	return ret;
}

int skl_compr_free_pages(struct snd_compr_stream *substream)
{
	struct snd_compr_runtime *runtime;

	runtime = substream->runtime;
	if (runtime->dma_area == NULL)
		return 0;

	if (runtime->dma_buffer_p != &substream->dma_buffer) {
		/* it's a newly allocated buffer.  release it now. */
		snd_dma_free_pages(runtime->dma_buffer_p);
		kfree(runtime->dma_buffer_p);
	}

	skl_set_compr_runtime_buffer(substream, NULL, 0);
	return 0;
}

int skl_substream_free_compr_pages(struct hdac_bus *bus,
				struct snd_compr_stream *substream)
{
	bus->io_ops->mark_pages_uc((substream)->runtime->dma_buffer_p, false);

	return skl_compr_free_pages(substream);
}
