// SPDX-License-Identifier: GPL-2.0-only
/*
 * line_buffer.c: Convenience functionality for buffered reading of lines from
 * a text file from the kernel.
 */

#define pr_fmt(fmt) KBUILD_MODNAME "-linebuffer: " fmt

#include "xpin_internal.h"
#include <linux/bug.h>
#include <linux/printk.h>
#include <linux/string_helpers.h>
#include <linux/string.h>


static int xpin_read_more(struct xpin_line_buffer *lb)
{
	ssize_t bytes_read;

	if (PARANOID(lb->read_pos > lb->write_pos))
		return -EINVAL;

	lb->write_pos -= lb->read_pos;

	/* Move unread data from end of buffer to beginning */
	memmove(lb->buffer, lb->buffer + lb->read_pos, lb->write_pos);
	lb->read_pos = 0;

	/*
	 * Read in more data after the unread bytes, trying to fill the buffer
	 * (with room for NUL).
	 */
	bytes_read = kernel_read(
		lb->file,
		lb->buffer + lb->write_pos,
		lb->buffer_size - lb->write_pos - 1,
		&lb->file_off
	);
	if (bytes_read < 0)
		return bytes_read;

	lb->write_pos += (unsigned int)bytes_read;
	lb->buffer[lb->write_pos] = '\0';

	if (strlen(lb->buffer) != lb->write_pos) {
		char *file_str = kstrdup_quotable_file(lb->file, GFP_KERNEL);

		pr_warn(
			"Text file contains a NUL: length=%zu write_pos=%u file=\"%s\"\n",
			strlen(lb->buffer),
			lb->write_pos,
			file_str ?: "<no_memory>"
		);
		kfree(file_str);
		return -EINVAL;
	}

	return 0;
}


char *xpin_read_line(struct xpin_line_buffer *lb)
{
	int err;
	char *line;
	char *eol = NULL;

	/* If buffer is empty, ask for more bytes */
	if (lb->read_pos < lb->write_pos) {
		/* Check for a newline */
		line = lb->buffer + lb->read_pos;
		eol = strnchr(line, lb->write_pos - lb->read_pos, '\n');
	}

	if (!eol) {
		/* If no newline (or empty buffer), attempt to read more */
		err = xpin_read_more(lb);
		if (err < 0)
			return ERR_PTR(err);

		/* EOF */
		if (!lb->write_pos)
			return NULL;

		/*
		 * If this doesn't find anything, we're on the last line of a
		 * file without a trailing newline.
		 */
		line = lb->buffer + lb->read_pos;
		eol = strnchr(line, lb->write_pos - lb->read_pos, '\n');
	}

	/* Replace newline (if found), and update buffer read position */
	if (eol) {
		*eol = '\0';

		/* Move ahead by length of current line plus 1 for NUL */
		lb->read_pos += eol - line + 1;
	} else {
		/*
		 * Line is entire buffer contents, as xpin_read_more()
		 * guarantees no embedded NULs.
		 */
		lb->read_pos = lb->write_pos;
	}

	return line;
}
