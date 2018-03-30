/*
 * Copyright (c) 2013
 * Phillip Lougher <phillip@squashfs.org.uk>
 *
 * This work is licensed under the terms of the GNU GPL, version 2. See
 * the COPYING file in the top-level directory.
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/percpu.h>
#include <linux/buffer_head.h>

#include "squashfs_fs.h"
#include "squashfs_fs_sb.h"
#include "decompressor.h"
#include "squashfs.h"

/*
 * This file implements multi-threaded decompression using percpu
 * variables, one thread per cpu core.
 */

struct squashfs_stream {
	struct mutex  mutex;
	void		*stream;
};

void *squashfs_decompressor_create(struct squashfs_sb_info *msblk,
						void *comp_opts)
{
	struct squashfs_stream *stream;
	struct squashfs_stream __percpu *percpu;
	int err, cpu;

	percpu = alloc_percpu(struct squashfs_stream);
	if (percpu == NULL)
		return ERR_PTR(-ENOMEM);

	for_each_possible_cpu(cpu) {
		stream = per_cpu_ptr(percpu, cpu);
		mutex_init(&stream->mutex);
		stream->stream = msblk->decompressor->init(msblk, comp_opts);
		if (IS_ERR(stream->stream)) {
			err = PTR_ERR(stream->stream);
			goto out;
		}
	}

	kfree(comp_opts);
	return (__force void *) percpu;

out:
	for_each_possible_cpu(cpu) {
		stream = per_cpu_ptr(percpu, cpu);
		if (!IS_ERR_OR_NULL(stream->stream))
			msblk->decompressor->free(stream->stream);
	}
	free_percpu(percpu);
	return ERR_PTR(err);
}

void squashfs_decompressor_destroy(struct squashfs_sb_info *msblk)
{
	struct squashfs_stream __percpu *percpu =
			(struct squashfs_stream __percpu *) msblk->stream;
	struct squashfs_stream *stream;
	int cpu;

	if (msblk->stream) {
		for_each_possible_cpu(cpu) {
			stream = per_cpu_ptr(percpu, cpu);
			msblk->decompressor->free(stream->stream);
		}
		free_percpu(percpu);
	}
}

int squashfs_decompress(struct squashfs_sb_info *msblk, struct buffer_head **bh,
	int b, int offset, int length, struct squashfs_page_actor *output)
{
	struct squashfs_stream __percpu *percpu =
			(struct squashfs_stream __percpu *) msblk->stream;
	struct squashfs_stream *stream = per_cpu_ptr(percpu, smp_processor_id());
	int res;

	/*
	 * Even if we get migrated to a different cpu, mutex
	 * will prevent two tasks running on two different cpu's
	 * stepping on to the same buffer.
	 */
	mutex_lock(&stream->mutex);
	res = msblk->decompressor->decompress(msblk, stream->stream, bh, b,
		offset, length, output);
	mutex_unlock(&stream->mutex);

	if (res < 0)
		ERROR("%s decompression failed, data probably corrupt\n",
			msblk->decompressor->name);

	return res;
}

int squashfs_max_decompressors(void)
{
	return num_possible_cpus();
}
