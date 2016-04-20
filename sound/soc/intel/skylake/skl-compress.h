
/*
 *  skl-compress.h - Skylake compress header file
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

inline
struct hdac_ext_stream *get_hdac_ext_compr_stream(struct snd_compr_stream *stream);
struct hdac_ext_bus *get_bus_compr_ctx(struct snd_compr_stream *substream);
void skl_set_compr_runtime_buffer(struct snd_compr_stream *substream,
				struct snd_dma_buffer *bufp, size_t size);
int skl_compr_malloc_pages(struct snd_compr_stream *substream,
					struct hdac_ext_bus *ebus, size_t size);
int skl_substream_alloc_compr_pages(struct hdac_ext_bus *ebus,
				 struct snd_compr_stream *substream,
				 size_t size);
int skl_compr_free_pages(struct snd_compr_stream *substream);
int skl_substream_free_compr_pages(struct hdac_bus *bus,
				struct snd_compr_stream *substream);

