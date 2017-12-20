/******************************************************************************
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * Contact Information:
 *	Intel Corporation.
 *	linux-mei@linux.intel.com
 *	http://www.intel.com
 *
 * BSD LICENSE
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <endian.h>

#include <openssl/engine.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#include "linux/rpmb.h"

#define RPMB_KEY_SIZE 32
#define RPMB_MAC_SIZE 32
#define RPMB_NONCE_SIZE 16

bool verbose;
#define rpmb_dbg(fmt, ARGS...) do {                     \
	if (verbose)                                    \
		fprintf(stderr, "rpmb: " fmt, ##ARGS);  \
} while (0)

#define rpmb_msg(fmt, ARGS...) \
	fprintf(stderr, "rpmb: " fmt, ##ARGS)

#define rpmb_err(fmt, ARGS...) \
	fprintf(stderr, "rpmb: error: " fmt, ##ARGS)

static const char *rpmb_op_str(uint16_t op)
{
#define RPMB_OP(_op) case RPMB_##_op: return #_op

	switch (op) {
	RPMB_OP(PROGRAM_KEY);
	RPMB_OP(GET_WRITE_COUNTER);
	RPMB_OP(WRITE_DATA);
	RPMB_OP(READ_DATA);
	RPMB_OP(RESULT_READ);
	break;
	default:
		return "unknown";
	}
#undef RPMB_OP
}

static const char *rpmb_result_str(enum rpmb_op_result result)
{
#define str(x) #x
#define RPMB_ERR(_res) case RPMB_ERR_##_res:         \
	{ if (result & RPMB_ERR_COUNTER_EXPIRED)     \
		return "COUNTER_EXPIRE:" str(_res);  \
	else                                         \
		return str(x);                       \
	}

	switch (result & 0x000F) {
	RPMB_ERR(OK);
	RPMB_ERR(GENERAL);
	RPMB_ERR(AUTH);
	RPMB_ERR(COUNTER);
	RPMB_ERR(ADDRESS);
	RPMB_ERR(WRITE);
	RPMB_ERR(READ);
	RPMB_ERR(NO_KEY);
	break;
	default:
		return "unknown";
	}
#undef RPMB_ERR
#undef str
};

static inline void __dump_buffer(const char *buf)
{
	fprintf(stderr, "%s\n", buf);
}

static void
dump_hex_buffer(const char *title, const void *buf, size_t len)
{
	const unsigned char *_buf = (const unsigned char *)buf;
	const size_t pbufsz = 16 * 3;
	char pbuf[pbufsz];
	int j = 0;

	if (title)
		fprintf(stderr, "%s\n", title);
	while (len-- > 0) {
		snprintf(&pbuf[j], pbufsz - j, "%02X ", *_buf++);
		j += 3;
		if (j == 16 * 3) {
			__dump_buffer(pbuf);
			j = 0;
		}
	}
	if (j)
		__dump_buffer(pbuf);
}

static void dbg_dump_frame(const char *title, const struct rpmb_frame *f)
{
	uint16_t result, req_resp;

	if (!verbose)
		return;

	if (!f)
		return;

	result = be16toh(f->result);
	req_resp = be16toh(f->req_resp);
	if (req_resp & 0xf00)
		req_resp = RPMB_RESP2REQ(req_resp);

	fprintf(stderr, "--------------- %s ---------------\n",
		title ? title : "start");
	fprintf(stderr, "ptr: %p\n", f);
	dump_hex_buffer("key_mac: ", f->key_mac, 32);
	dump_hex_buffer("data: ", f->data, 256);
	dump_hex_buffer("nonce: ", f->nonce, 16);
	fprintf(stderr, "write_counter: %u\n", be32toh(f->write_counter));
	fprintf(stderr, "address:  %0X\n", be16toh(f->addr));
	fprintf(stderr, "block_count: %u\n", be16toh(f->block_count));
	fprintf(stderr, "result %s:%d\n", rpmb_result_str(result), result);
	fprintf(stderr, "req_resp %s\n", rpmb_op_str(req_resp));
	fprintf(stderr, "--------------- End ---------------\n");
}

static int open_dev_file(const char *devfile)
{
	int fd;

	fd = open(devfile, O_RDWR);
	if (fd < 0)
		rpmb_err("Cannot open: %s: %s.\n", devfile, strerror(errno));
	return fd;
}

static int open_rd_file(const char *datafile, const char *type)
{
	int fd;

	if (!strcmp(datafile, "-"))
		fd = STDIN_FILENO;
	else
		fd = open(datafile, O_RDONLY);

	if (fd < 0)
		rpmb_err("Cannot open %s: %s: %s.\n",
			 type, datafile, strerror(errno));

	return fd;
}

static int open_wr_file(const char *datafile, const char *type)
{
	int fd;

	if (!strcmp(datafile, "-"))
		fd = STDOUT_FILENO;
	else
		fd = open(datafile, O_WRONLY | O_CREAT | O_APPEND, 0600);
	if (fd < 0)
		rpmb_err("Cannot open %s: %s: %s.\n",
			 type, datafile, strerror(errno));
	return fd;
}

static void close_fd(int fd)
{
	if (fd > 0 && fd != STDIN_FILENO && fd != STDOUT_FILENO)
		close(fd);
}

/* need to just cast out 'const' in write(2) */
typedef ssize_t (*rwfunc_t)(int fd, void *buf, size_t count);
/* blocking rw wrapper */
static inline ssize_t rw(rwfunc_t func, int fd, unsigned char *buf, size_t size)
{
	ssize_t ntotal = 0, n;
	char *_buf = (char *)buf;

	do {
		n = func(fd, _buf + ntotal, size - ntotal);
		if (n == -1 && errno != EINTR) {
			ntotal = -1;
			break;
		} else if (n > 0) {
			ntotal += n;
		}
	} while (n != 0 && (size_t)ntotal != size);

	return ntotal;
}

static ssize_t read_file(int fd, unsigned char *data, size_t size)
{
	ssize_t ret;

	ret = rw(read, fd, data, size);
	if (ret < 0) {
		rpmb_err("cannot read file: %s\n.", strerror(errno));
	} else if ((size_t)ret != size) {
		rpmb_err("read %zd but must be %zu bytes length.\n", ret, size);
		ret = -EINVAL;
	}

	return ret;
}

static ssize_t write_file(int fd, unsigned char *data, size_t size)
{
	ssize_t ret;

	ret = rw((rwfunc_t)write, fd, data, size);
	if (ret < 0) {
		rpmb_err("cannot read file: %s.\n", strerror(errno));
	} else if ((size_t)ret != size) {
		rpmb_err("data is %zd but must be %zu bytes length.\n",
			 ret, size);
		ret = -EINVAL;
	}
	return ret;
}

static struct rpmb_frame *rpmb_alloc_frames(unsigned int cnt)
{
	return calloc(cnt, sizeof(struct rpmb_frame));
}

static int rpmb_calc_hmac_sha256(struct rpmb_frame *frames, size_t blocks_cnt,
				 const unsigned char key[],
				 unsigned int key_size,
				 unsigned char mac[],
				 unsigned int mac_size)
{
	HMAC_CTX ctx;
	int ret;
	unsigned int i;

	 /* SSL returns 1 on success 0 on failure */

	HMAC_CTX_init(&ctx);
	ret = HMAC_Init_ex(&ctx, key, key_size, EVP_sha256(), NULL);
	if (ret == 0)
		goto out;
	for (i = 0; i < blocks_cnt; i++)
		HMAC_Update(&ctx, frames[i].data, hmac_data_len);

	ret = HMAC_Final(&ctx, mac, &mac_size);
	if (ret == 0)
		goto out;
	if (mac_size != RPMB_MAC_SIZE)
		ret = 0;

	ret = 1;
out:
	HMAC_CTX_cleanup(&ctx);
	return ret == 1 ? 0 : -1;
}

static int rpmb_check_req_resp(uint16_t req, struct rpmb_frame *frame_out)
{
	if (RPMB_REQ2RESP(req) != be16toh(frame_out->req_resp)) {
		rpmb_err("RPMB response mismatch %04X != %04X\n.",
			 RPMB_REQ2RESP(req), be16toh(frame_out->req_resp));
		return -1;
	}
	return 0;
}

static int rpmb_check_mac(const unsigned char *key,
			  struct rpmb_frame *frames_out,
			  unsigned int cnt_out)
{
	unsigned char mac[RPMB_MAC_SIZE];

	if (cnt_out == 0) {
		rpmb_err("RPMB 0 output frames.\n");
		return -1;
	}

	rpmb_calc_hmac_sha256(frames_out, cnt_out,
			      key, RPMB_KEY_SIZE,
			      mac, RPMB_MAC_SIZE);

	if (memcmp(mac, frames_out[cnt_out - 1].key_mac, RPMB_MAC_SIZE)) {
		rpmb_err("RPMB hmac mismatch:\n");
		dump_hex_buffer("Result MAC: ",
				frames_out[cnt_out - 1].key_mac, RPMB_MAC_SIZE);
		dump_hex_buffer("Expected MAC: ", mac, RPMB_MAC_SIZE);
		return -1;
	}

	return 0;
}

static int (*rpmb_ioctl)(int fd, uint16_t req,
			 const struct rpmb_frame *frames_in,
			 unsigned int cnt_in,
			 struct rpmb_frame *frames_out,
			 unsigned int cnt_out);

static int rpmb_ioctl_seq(int fd, uint16_t req,
			  const struct rpmb_frame *frames_in,
			  unsigned int cnt_in,
			  struct rpmb_frame *frames_out,
			  unsigned int cnt_out)
{
	int ret;
	struct {
		struct rpmb_ioc_seq_cmd h;
		struct rpmb_ioc_cmd cmd[3];
	} iseq = {};
	struct rpmb_frame *frame_res = NULL;
	int i;
	uint32_t flags;

	rpmb_dbg("RPMB OP: %s\n", rpmb_op_str(req));
	dbg_dump_frame("In Frame: ", frames_in);

	i = 0;
	flags = RPMB_F_WRITE;
	if (req == RPMB_WRITE_DATA || req == RPMB_PROGRAM_KEY)
		flags |= RPMB_F_REL_WRITE;
	rpmb_ioc_cmd_set(iseq.cmd[i], flags, frames_in, cnt_in);
	i++;

	if (req == RPMB_WRITE_DATA || req == RPMB_PROGRAM_KEY) {
		frame_res = rpmb_alloc_frames(1);
		if (!frame_res)
			return -ENOMEM;
		frame_res->req_resp =  htobe16(RPMB_RESULT_READ);
		rpmb_ioc_cmd_set(iseq.cmd[i], RPMB_F_WRITE, frame_res, 1);
		i++;
	}

	rpmb_ioc_cmd_set(iseq.cmd[i], 0, frames_out, cnt_out);
	i++;

	iseq.h.num_of_cmds = i;
	ret = ioctl(fd, RPMB_IOC_SEQ_CMD, &iseq);
	if (ret < 0)
		rpmb_err("ioctl failure %d: %s.\n", ret, strerror(errno));

	ret = rpmb_check_req_resp(req, frames_out);

	dbg_dump_frame("Res Frame: ", frame_res);
	dbg_dump_frame("Out Frame: ", frames_out);
	free(frame_res);
	return ret;
}

static int rpmb_ioctl_req(int fd, uint16_t req,
			  const struct rpmb_frame *frames_in,
			  unsigned int cnt_in,
			  struct rpmb_frame *frames_out,
			  unsigned int cnt_out)
{
	struct rpmb_ioc_req_cmd ireq;
	int ret;

	ireq.req_type = req;
	rpmb_ioc_cmd_set(ireq.icmd, RPMB_F_WRITE, frames_in, cnt_in);
	rpmb_ioc_cmd_set(ireq.ocmd, 0, frames_out, cnt_out);

	rpmb_dbg("RPMB OP: %s\n", rpmb_op_str(req));
	dbg_dump_frame("In Frame: ", frames_in);
	ret = ioctl(fd, RPMB_IOC_REQ_CMD, &ireq);
	if (ret < 0)
		rpmb_err("ioctl failure %d: %s.\n", ret, strerror(errno));

	ret = rpmb_check_req_resp(req, frames_out);
	dbg_dump_frame("Out Frame: ", frames_out);

	return ret;
}

static int op_rpmb_program_key(int nargs, char *argv[])
{
	int ret;
	int  dev_fd = -1, key_fd = -1;
	uint16_t req = RPMB_PROGRAM_KEY;
	struct rpmb_frame *frame_in = NULL, *frame_out = NULL;

	ret = -EINVAL;
	if (nargs != 2)
		return ret;

	dev_fd = open_dev_file(argv[0]);
	if (dev_fd < 0)
		goto out;
	argv++;

	key_fd = open_rd_file(argv[0], "key file");
	if (key_fd < 0)
		goto out;
	argv++;

	frame_in = rpmb_alloc_frames(1);
	frame_out = rpmb_alloc_frames(1);
	if (!frame_in || !frame_out) {
		ret = -ENOMEM;
		goto out;
	}

	frame_in->req_resp = htobe16(req);

	read_file(key_fd, frame_in->key_mac, RPMB_KEY_SIZE);

	ret = rpmb_ioctl(dev_fd, req, frame_in, 1, frame_out, 1);
	if (ret)
		goto out;

	if (RPMB_REQ2RESP(req) != be16toh(frame_out->req_resp)) {
		rpmb_err("RPMB response mismatch.\n");
		ret = -1;
		goto out;
	}

	ret = be16toh(frame_out->result);
	if (ret)
		rpmb_err("RPMB operation %s failed, %s[0x%04x].\n",
			 rpmb_op_str(req), rpmb_result_str(ret), ret);

out:
	free(frame_in);
	free(frame_out);
	close_fd(dev_fd);
	close_fd(key_fd);

	return ret;
}

static int rpmb_get_write_counter(int dev_fd, unsigned int *cnt,
				  const unsigned char *key)
{
	int ret;
	uint16_t res = 0x000F;
	uint16_t req = RPMB_GET_WRITE_COUNTER;
	struct rpmb_frame *frame_in = NULL;
	struct rpmb_frame *frame_out = NULL;

	frame_in = rpmb_alloc_frames(1);
	frame_out = rpmb_alloc_frames(1);
	if (!frame_in || !frame_out) {
		ret = -ENOMEM;
		goto out;
	}

	frame_in->req_resp = htobe16(req);
	RAND_bytes(frame_in->nonce, RPMB_NONCE_SIZE);

	ret = rpmb_ioctl(dev_fd, req, frame_in, 1, frame_out, 1);
	if (ret)
		goto out;

	res = be16toh(frame_out->result);
	if (res != RPMB_ERR_OK) {
		ret = -1;
		goto out;
	}

	if (memcmp(&frame_in->nonce, &frame_out->nonce, RPMB_NONCE_SIZE)) {
		rpmb_err("RPMB NONCE mismatch\n");
		dump_hex_buffer("Result NONCE:",
				&frame_out->nonce, RPMB_NONCE_SIZE);
		dump_hex_buffer("Expected NONCE: ",
				&frame_in->nonce, RPMB_NONCE_SIZE);
		ret = -1;
		goto out;
	}

	if (key) {
		ret = rpmb_check_mac(key, frame_out, 1);
		if (ret)
			goto out;
	}

	*cnt = be32toh(frame_out->write_counter);

out:
	if (ret)
		rpmb_err("RPMB operation %s failed, %s[0x%04x]\n",
			 rpmb_op_str(req), rpmb_result_str(res), res);
	free(frame_in);
	free(frame_out);
	return ret;
}

static int op_rpmb_get_write_counter(int nargs, char **argv)
{
	int ret;
	int dev_fd = -1, key_fd = -1;
	bool has_key;
	unsigned char key[RPMB_KEY_SIZE];
	unsigned int cnt;

	if (nargs == 2)
		has_key = true;
	else if (nargs == 1)
		has_key = false;
	else
		return -EINVAL;

	ret = -EINVAL;
	dev_fd = open_dev_file(argv[0]);
	if (dev_fd < 0)
		return ret;
	argv++;

	if (has_key) {
		key_fd = open_rd_file(argv[0], "key file");
		if (key_fd < 0)
			goto out;
		argv++;

		ret = read_file(key_fd, key, RPMB_KEY_SIZE);
		if (ret < 0)
			goto out;

		ret = rpmb_get_write_counter(dev_fd, &cnt, key);
	} else {
		ret = rpmb_get_write_counter(dev_fd, &cnt, NULL);
	}

	if (!ret)
		printf("Counter value: 0x%08x\n", cnt);

out:
	close_fd(dev_fd);
	close_fd(key_fd);
	return ret;
}

static int op_rpmb_read_blocks(int nargs, char **argv)
{
	int i, ret;
	int dev_fd = -1, data_fd = -1, key_fd = -1;
	uint16_t req = RPMB_READ_DATA;
	uint16_t addr, blocks_cnt;
	unsigned char key[RPMB_KEY_SIZE];
	unsigned long numarg;
	bool has_key;
	struct rpmb_frame *frame_in = NULL;
	struct rpmb_frame *frames_out = NULL;
	struct rpmb_frame *frame_out;

	ret = -EINVAL;
	if (nargs == 4)
		has_key = false;
	else if (nargs == 5)
		has_key = true;
	else
		return ret;

	dev_fd = open_dev_file(argv[0]);
	if (dev_fd < 0)
		goto out;
	argv++;

	errno = 0;
	numarg = strtoul(argv[0], NULL, 0);
	if (errno || numarg > USHRT_MAX) {
		rpmb_err("wrong block address\n");
		goto out;
	}
	addr = (uint16_t)numarg;
	argv++;

	errno = 0;
	numarg = strtoul(argv[0], NULL, 0);
	if (errno || numarg > USHRT_MAX) {
		rpmb_err("wrong blocks count\n");
		goto out;
	}
	blocks_cnt = (uint16_t)numarg;
	argv++;

	if (blocks_cnt == 0) {
		rpmb_err("wrong blocks count\n");
		goto out;
	}

	data_fd = open_wr_file(argv[0], "output data");
	if (data_fd < 0)
		goto out;
	argv++;

	if (has_key) {
		key_fd = open_rd_file(argv[0], "key file");
		if (key_fd < 0)
			goto out;
		argv++;

		ret = read_file(key_fd, key, RPMB_KEY_SIZE);
		if (ret < 0)
			goto out;
	}

	ret = 0;
	frames_out = rpmb_alloc_frames(blocks_cnt);
	frame_in = rpmb_alloc_frames(1);
	if (!frames_out || !frame_in) {
		rpmb_err("Cannot allocate %d RPMB frames\n", blocks_cnt);
		ret = -ENOMEM;
		goto out;
	}

	frame_in->req_resp = htobe16(req);
	frame_in->addr = htobe16(addr);
	/* eMMc spec ask for 0 here this will be translated by the rpmb layer */
	frame_in->block_count = htobe16(blocks_cnt);
	if (has_key)
		RAND_bytes(frame_in->nonce, RPMB_NONCE_SIZE);

	ret = rpmb_ioctl(dev_fd, req, frame_in, 1, frames_out, blocks_cnt);
	if (ret)
		goto out;

	frame_out = &frames_out[blocks_cnt - 1];
	ret = be16toh(frame_out->result);
	if (ret) {
		rpmb_err("RPMB operation %s failed, %s[0x%04x]\n",
			 rpmb_op_str(req), rpmb_result_str(ret), ret);
		goto out;
	}

	if (has_key) {
		ret = rpmb_check_mac(key, frames_out, blocks_cnt);
		if (ret)
			goto out;
	}

	for (i = 0; i < blocks_cnt; i++) {
		ret = write_file(data_fd, frames_out[i].data,
				 sizeof(frames_out[i].data));
		if (ret < 0)
			goto out;
	}

out:
	free(frame_in);
	free(frames_out);
	close_fd(dev_fd);
	close_fd(data_fd);
	close_fd(key_fd);

	return ret;
}

static int op_rpmb_write_blocks(int nargs, char **argv)
{
	int ret;
	int dev_fd = -1, key_fd = -1, data_fd = -1;
	int i;
	uint16_t req = RPMB_WRITE_DATA;
	unsigned char key[RPMB_KEY_SIZE];
	unsigned char mac[RPMB_MAC_SIZE];
	unsigned long numarg;
	uint16_t addr, blocks_cnt;
	uint32_t write_counter;
	struct rpmb_frame *frames_in = NULL;
	struct rpmb_frame *frame_out = NULL;

	ret = -EINVAL;
	if (nargs != 5)
		goto out;

	dev_fd = open_dev_file(argv[0]);
	if (dev_fd < 0)
		goto out;
	argv++;

	errno = 0;
	numarg = strtoul(argv[0], NULL, 0);
	if (errno || numarg > USHRT_MAX) {
		rpmb_err("wrong block address %s\n", argv[0]);
		goto out;
	}
	addr = (uint16_t)numarg;
	argv++;

	errno = 0;
	numarg = strtoul(argv[0], NULL, 0);
	if (errno || numarg > USHRT_MAX) {
		rpmb_err("wrong blocks count\n");
		goto out;
	}
	blocks_cnt = (uint16_t)numarg;
	argv++;

	if (blocks_cnt == 0) {
		rpmb_err("wrong blocks count\n");
		goto out;
	}

	data_fd = open_rd_file(argv[0], "data file");
	if (data_fd < 0)
		goto out;
	argv++;

	key_fd = open_rd_file(argv[0], "key file");
	if (key_fd < 0)
		goto out;
	argv++;

	ret = read_file(key_fd, key, RPMB_KEY_SIZE);
	if (ret < 0)
		goto out;

	frames_in = rpmb_alloc_frames(blocks_cnt);
	frame_out = rpmb_alloc_frames(1);
	if (!frames_in || !frame_out) {
		rpmb_err("can't allocate memory for RPMB outer frames\n");
		ret = -ENOMEM;
		goto out;
	}

	ret = rpmb_get_write_counter(dev_fd, &write_counter, key);
	if (ret)
		goto out;

	for (i = 0; i < blocks_cnt; i++) {
		frames_in[i].req_resp      = htobe16(req);
		frames_in[i].block_count   = htobe16(blocks_cnt);
		frames_in[i].addr          = htobe16(addr);
		frames_in[i].write_counter = htobe32(write_counter);
	}

	for (i = 0; i < blocks_cnt; i++) {
		ret = read_file(data_fd, frames_in[i].data,
				sizeof(frames_in[0].data));
		if (ret < 0)
			goto out;
	}

	rpmb_calc_hmac_sha256(frames_in, blocks_cnt,
			      key, RPMB_KEY_SIZE,
			      mac, RPMB_MAC_SIZE);
	memcpy(frames_in[blocks_cnt - 1].key_mac, mac, RPMB_MAC_SIZE);
	ret = rpmb_ioctl(dev_fd, req, frames_in, blocks_cnt, frame_out, 1);
	if (ret != 0)
		goto out;

	ret = be16toh(frame_out->result);
	if (ret) {
		rpmb_err("RPMB operation %s failed, %s[0x%04x]\n",
			 rpmb_op_str(req), rpmb_result_str(ret), ret);
		ret = -1;
	}

	if (be16toh(frame_out->addr) != addr) {
		rpmb_err("RPMB addr mismatchs res=%04x req=%04x\n",
			 be16toh(frame_out->addr), addr);
		ret = -1;
	}

	if (be32toh(frame_out->write_counter) <= write_counter) {
		rpmb_err("RPMB write counter not incremeted res=%x req=%x\n",
			 be32toh(frame_out->write_counter), write_counter);
		ret = -1;
	}

	ret = rpmb_check_mac(key, frame_out, 1);
out:
	free(frames_in);
	free(frame_out);
	close_fd(dev_fd);
	close_fd(data_fd);
	close_fd(key_fd);
	return ret;
}

typedef int (*rpmb_op)(int argc, char *argv[]);

struct rpmb_cmd {
	const char *op_name;
	rpmb_op     op;
	const char  *usage; /* usage title */
	const char  *help;  /* help */
};

static const struct rpmb_cmd cmds[] = {
	{
	 "program-key",
	 op_rpmb_program_key,
	 "<RPMB_DEVICE> <KEY_FILE>",
	 "    Program authentication key of 32 bytes length from the KEY_FILE\n"
	 "    when KEY_FILE is -, read standard input.\n"
	 "    NOTE: This is a one-time programmable irreversible change.\n",
	},
	{
	 "write-counter",
	 op_rpmb_get_write_counter,
	 "<RPMB_DEVICE> [KEY_FILE]",
	 "    Rertrive write counter value from the <RPMB_DEVICE> to stdout.\n"
	 "    When KEY_FILE is present data is verified via HMAC\n"
	 "    when KEY_FILE is -, read standard input.\n"
	},
	{
	  "write-blocks",
	  op_rpmb_write_blocks,
	  "<RPMB_DEVICE> <address> <block_count> <DATA_FILE> <KEY_FILE>",
	  "    <block count> of 256 bytes will be written from the DATA_FILE\n"
	  "    to the <RPMB_DEVICE> at block offset <address>.\n"
	  "    When DATA_FILE is -, read from standard input.\n",
	},
	{
	  "read-blocks",
	  op_rpmb_read_blocks,
	  "<RPMB_DEVICE> <address> <blocks count> <OUTPUT_FILE> [KEY_FILE]",
	  "    <block count> of 256 bytes will be read from <RPMB_DEVICE>\n"
	  "    to the OUTPUT_FILE\n"
	  "    When KEY_FILE is present data is verified via HMAC\n"
	  "    When OUTPUT/KEY_FILE is -, read from standard input.\n"
	  "    When OUTPUT_FILE is -, write to standard output\n",
	},

	{ NULL, NULL, NULL, NULL }
};

static void help(const char *prog, const struct rpmb_cmd *cmd)
{
	printf("%s %s %s\n", prog, cmd->op_name, cmd->usage);
	printf("%s\n", cmd->help);
}

static void usage(const char *prog)
{
	int i;

	printf("\n");
	printf("Usage: %s [-v] [-r|-s] <command> <args>\n\n", prog);
	for (i = 0; cmds[i].op_name; i++)
		printf("       %s %s %s\n",
		       prog, cmds[i].op_name, cmds[i].usage);

	printf("\n");
	printf("      %s -v/--verbose: runs in verbose mode\n", prog);
	printf("      %s -s/ --sequence: use RPMB_IOC_SEQ_CMD\n", prog);
	printf("      %s -r/--request: use RPMB_IOC_REQ_CMD\n", prog);
	printf("      %s help : shows this help\n", prog);
	printf("      %s help <command>: shows detailed help\n", prog);
}

static bool call_for_help(const char *arg)
{
	return !strcmp(arg, "help") ||
	       !strcmp(arg, "-h")   ||
	       !strcmp(arg, "--help");
}

static bool parse_verbose(const char *arg)
{
	return !strcmp(arg, "-v") ||
	       !strcmp(arg, "--verbose");
}

static bool parse_req(const char *arg)
{
	return !strcmp(arg, "-r") ||
	       !strcmp(arg, "--request");
}

static bool parse_seq(const char *arg)
{
	return !strcmp(arg, "-s") ||
	       !strcmp(arg, "--sequence");
}

static const
struct rpmb_cmd *parse_args(const char *prog, int *_argc, char **_argv[])
{
	int i;
	int argc = *_argc;
	char **argv =  *_argv;
	const struct rpmb_cmd *cmd = NULL;
	bool need_help = false;

	argc--; argv++;

	if (argc == 0)
		goto out;

	if (call_for_help(argv[0])) {
		argc--; argv++;
		if (argc == 0)
			goto out;

		need_help = true;
	}

	if (parse_verbose(argv[0])) {
		argc--; argv++;
		if (argc == 0)
			goto out;

		verbose = true;
	}

	if (parse_req(argv[0])) {
		argc--; argv++;
		if (argc == 0)
			goto out;

		rpmb_ioctl = rpmb_ioctl_req;
	}

	if (parse_seq(argv[0])) {
		argc--; argv++;
		if (argc == 0)
			goto out;

		rpmb_ioctl = rpmb_ioctl_seq;
	}

	if (!rpmb_ioctl)
		rpmb_ioctl = rpmb_ioctl_req;

	for (i = 0; cmds[i].op_name; i++) {
		if (!strncmp(argv[0], cmds[i].op_name,
			     strlen(cmds[i].op_name))) {
			cmd = &cmds[i];
			argc--; argv++;
			break;
		}
	}

	if (!cmd)
		goto out;

	if (need_help || (argc > 0 && call_for_help(argv[0]))) {
		help(prog, cmd);
		argc--; argv++;
		return NULL;
	}

out:
	*_argc = argc;
	*_argv = argv;

	if (!cmd)
		usage(prog);

	return cmd;
}

int main(int argc, char *argv[])
{
	const char *prog = basename(argv[0]);
	const struct rpmb_cmd *cmd;
	int ret;

	cmd = parse_args(prog, &argc, &argv);
	if (!cmd)
		exit(EXIT_SUCCESS);

	ret = cmd->op(argc, argv);
	if (ret == -EINVAL)
		help(prog, cmd);

	if (ret)
		exit(EXIT_FAILURE);

	exit(EXIT_SUCCESS);
}
