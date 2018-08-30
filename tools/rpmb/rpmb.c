// SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
/*
 * Copyright (C) 2016-2018 Intel Corp. All rights reserved
 */

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

#define RPMB_FRAME_TYPE_JDEC 0
#define RPMB_FRAME_TYPE_NVME 1
#define RPMB_BLOCK_SIZE 256
#define RPMB_SECTOR_SIZE 512

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
		return str(_res);                    \
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

static int open_dev_file(const char *devfile, struct rpmb_ioc_cap_cmd *cap)
{
	struct rpmb_ioc_ver_cmd ver;
	int fd;
	int ret;

	fd = open(devfile, O_RDWR);
	if (fd < 0)
		rpmb_err("Cannot open: %s: %s.\n", devfile, strerror(errno));

	ret = ioctl(fd, RPMB_IOC_VER_CMD, &ver);
	if (ret < 0) {
		rpmb_err("ioctl failure %d: %s.\n", ret, strerror(errno));
		goto err;
	}

	printf("RPMB API Version %X\n", ver.api_version);

	ret = ioctl(fd, RPMB_IOC_CAP_CMD, cap);
	if (ret < 0) {
		rpmb_err("ioctl failure %d: %s.\n", ret, strerror(errno));
		goto err;
	}

	rpmb_dbg("RPMB device_type = %hd\n", cap->device_type);
	rpmb_dbg("RPMB rpmb_target = %hd\n", cap->target);
	rpmb_dbg("RPMB capacity    = %hd\n", cap->capacity);
	rpmb_dbg("RPMB block_size  = %hd\n", cap->block_size);
	rpmb_dbg("RPMB wr_cnt_max  = %hd\n", cap->wr_cnt_max);
	rpmb_dbg("RPMB rd_cnt_max  = %hd\n", cap->rd_cnt_max);
	rpmb_dbg("RPMB auth_method = %hd\n", cap->auth_method);

	return fd;
err:
	close(fd);
	return -1;
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

static void dbg_dump_frame_jdec(const char *title, const void *f, uint32_t cnt)
{
	uint16_t result, req_resp;
	const struct rpmb_frame_jdec *frame = f;

	if (!verbose)
		return;

	if (!f)
		return;

	result = be16toh(frame->result);
	req_resp = be16toh(frame->req_resp);
	if (req_resp & 0xf00)
		req_resp = RPMB_RESP2REQ(req_resp);

	fprintf(stderr, "--------------- %s ---------------\n",
		title ? title : "start");
	fprintf(stderr, "ptr: %p\n", f);
	dump_hex_buffer("key_mac: ", frame->key_mac, 32);
	dump_hex_buffer("data: ", frame->data, 256);
	dump_hex_buffer("nonce: ", frame->nonce, 16);
	fprintf(stderr, "write_counter: %u\n", be32toh(frame->write_counter));
	fprintf(stderr, "address:  %0X\n", be16toh(frame->addr));
	fprintf(stderr, "block_count: %u\n", be16toh(frame->block_count));
	fprintf(stderr, "result %s:%d\n", rpmb_result_str(result), result);
	fprintf(stderr, "req_resp %s\n", rpmb_op_str(req_resp));
	fprintf(stderr, "--------------- End ---------------\n");
}

static void dbg_dump_frame_nvme(const char *title, const void *f, uint32_t cnt)
{
	uint16_t result, req_resp;
	uint32_t keysize = 4;
	uint32_t sector_count;
	const struct rpmb_frame_nvme *frame = f;

	if (!verbose)
		return;

	if (!f)
		return;

	result = le16toh(frame->result);
	req_resp = le16toh(frame->req_resp);
	if (req_resp & 0xf00)
		req_resp = RPMB_RESP2REQ(req_resp);

	sector_count = le32toh(frame->block_count);

	fprintf(stderr, "--------------- %s ---------------\n",
		title ? title : "start");
	fprintf(stderr, "ptr: %p\n", f);
	dump_hex_buffer("key_mac: ", &frame->key_mac[223 - keysize], keysize);
	dump_hex_buffer("nonce: ", frame->nonce, 16);
	fprintf(stderr, "rpmb_target: %u\n", frame->rpmb_target);
	fprintf(stderr, "write_counter: %u\n", le32toh(frame->write_counter));
	fprintf(stderr, "address:  %0X\n", le32toh(frame->addr));
	fprintf(stderr, "block_count: %u\n", sector_count);
	fprintf(stderr, "result %s:%d\n", rpmb_result_str(result), result);
	fprintf(stderr, "req_resp %s\n", rpmb_op_str(req_resp));
	dump_hex_buffer("data: ", frame->data, RPMB_SECTOR_SIZE * cnt);
	fprintf(stderr, "--------------- End --------------\n");
}

static void dbg_dump_frame(uint8_t frame_type, const char *title, const void *f, uint32_t cnt)
{
	if (frame_type == RPMB_FRAME_TYPE_NVME)
		dbg_dump_frame_nvme(title, f, cnt);
	else
		dbg_dump_frame_jdec(title, f, cnt);
}

static int rpmb_frame_set_key_mac_jdec(void *f, uint32_t block_count,
				       uint8_t *key_mac, size_t key_mac_size)
{
	struct rpmb_frame_jdec *frames = f;

	if (block_count == 0)
		block_count = 1;

	memcpy(&frames[block_count - 1].key_mac, key_mac, key_mac_size);

	return 0;
}

static int rpmb_frame_set_key_mac_nvme(void *f, uint32_t block_count,
				       uint8_t *key_mac, size_t key_mac_size)
{
	struct rpmb_frame_nvme *frame = f;

	memcpy(&frame->key_mac[223 - key_mac_size], key_mac, key_mac_size);

	return 0;
}

static int rpmb_frame_set_key_mac(uint8_t frame_type, void *f,
				  uint32_t block_count,
				  uint8_t *key_mac, size_t key_mac_size)
{
	if (frame_type == RPMB_FRAME_TYPE_NVME)
		return rpmb_frame_set_key_mac_nvme(f, block_count,
						   key_mac, key_mac_size);
	else
		return rpmb_frame_set_key_mac_jdec(f, block_count,
						   key_mac, key_mac_size);
}

static uint8_t *rpmb_frame_get_key_mac_ptr_jdec(void *f, uint32_t block_count,
						size_t key_size)
{
	struct rpmb_frame_jdec *frame = f;

	if (block_count == 0)
		block_count = 1;

	return frame[block_count - 1].key_mac;
}

static uint8_t *rpmb_frame_get_key_mac_ptr_nvme(void *f, uint32_t block_count,
						size_t key_size)
{
	struct rpmb_frame_nvme *frame = f;

	return &frame->key_mac[223 - key_size];
}

static uint8_t *rpmb_frame_get_key_mac_ptr(uint8_t frame_type, void *f,
					   uint32_t block_count,
					   size_t key_size)
{
	if (frame_type == RPMB_FRAME_TYPE_NVME)
		return rpmb_frame_get_key_mac_ptr_nvme(f, block_count,
						       key_size);
	else
		return rpmb_frame_get_key_mac_ptr_jdec(f, block_count,
						       key_size);
}

static uint8_t *rpmb_frame_get_nonce_ptr_jdec(void *f)
{
	struct rpmb_frame_jdec *frame = f;

	return frame->nonce;
}

static uint8_t *rpmb_frame_get_nonce_ptr_nvme(void *f)
{
	struct rpmb_frame_nvme *frame = f;

	return frame->nonce;
}

static uint8_t *rpmb_frame_get_nonce_ptr(uint8_t frame_type, void *f)
{
	return frame_type == RPMB_FRAME_TYPE_NVME ?
		rpmb_frame_get_nonce_ptr_nvme(f) :
		rpmb_frame_get_nonce_ptr_jdec(f);
}

static uint32_t rpmb_frame_get_write_counter_jdec(void *f)
{
	struct rpmb_frame_jdec *frame = f;

	return be32toh(frame->write_counter);
}

static uint32_t rpmb_frame_get_write_counter_nvme(void *f)
{
	struct rpmb_frame_nvme *frame = f;

	return le32toh(frame->write_counter);
}

static uint32_t rpmb_frame_get_write_counter(uint8_t frame_type, void *f)
{
	return (frame_type == RPMB_FRAME_TYPE_NVME) ?
		rpmb_frame_get_write_counter_nvme(f) :
		rpmb_frame_get_write_counter_jdec(f);
}

static uint32_t rpmb_frame_get_addr_jdec(void *f)
{
	struct rpmb_frame_jdec *frame = f;

	return be16toh(frame->addr);
}

static uint32_t rpmb_frame_get_addr_nvme(void *f)
{
	struct rpmb_frame_nvme *frame = f;

	return le32toh(frame->addr);
}

static uint32_t rpmb_frame_get_addr(uint8_t frame_type, void *f)
{
	return (frame_type == RPMB_FRAME_TYPE_NVME) ?
		rpmb_frame_get_addr_nvme(f) :
		rpmb_frame_get_addr_jdec(f);
}

static uint16_t rpmb_frame_get_result_jdec(void *f)
{
	struct rpmb_frame_jdec *frames = f;
	uint16_t block_count = be16toh(frames[0].block_count);

	if (block_count == 0)
		block_count = 1;

	return be16toh(frames[block_count - 1].result);
}

static uint16_t rpmb_frame_get_result_nvme(void *f)
{
	struct rpmb_frame_nvme *frame = f;

	return le16toh(frame->result);
}

static uint16_t rpmb_frame_get_result(uint8_t frame_type, void *f)
{
	return (frame_type == RPMB_FRAME_TYPE_NVME) ?
		rpmb_frame_get_result_nvme(f) :
		rpmb_frame_get_result_jdec(f);
}

static uint16_t rpmb_frame_get_req_resp_jdec(void *f)
{
	struct rpmb_frame_jdec *frame = f;

	return be16toh(frame->req_resp);
}

static uint16_t rpmb_frame_get_req_resp_nvme(void *f)
{
	struct rpmb_frame_nvme *frame = f;

	return le16toh(frame->req_resp);
}

static uint16_t rpmb_frame_get_req_resp(uint8_t frame_type, void *f)
{
	return frame_type == RPMB_FRAME_TYPE_NVME ?
		rpmb_frame_get_req_resp_nvme(f) :
		rpmb_frame_get_req_resp_jdec(f);
}

static int rpmb_frame_set_jdec(void *f,
			       uint16_t req_resp, uint32_t block_count,
			       uint32_t addr, uint32_t write_counter)
{
	struct rpmb_frame_jdec *frames = f;
	uint32_t i;
	/* FIMXE validate overflow */
	uint16_t __block_count = (uint16_t)block_count;
	uint16_t __addr  = (uint16_t)addr;

	for (i = 0; i < (block_count ?: 1); i++) {
		frames[i].req_resp      = htobe16(req_resp);
		frames[i].block_count   = htobe16(__block_count);
		frames[i].addr          = htobe16(__addr);
		frames[i].write_counter = htobe32(write_counter);
	}

	return 0;
}

static int rpmb_frame_set_nvme(void *f,
			       uint16_t req_resp, uint32_t block_count,
			       uint32_t addr, uint32_t write_counter)
{
	struct rpmb_frame_nvme *frame = f;

	frame->req_resp      = htole16(req_resp);
	frame->block_count   = htole32(block_count);
	frame->addr          = htole32(addr);
	frame->write_counter = htole32(write_counter);

	return 0;
}

static int rpmb_frame_set(uint8_t frame_type, void *f,
			  uint16_t req_resp, uint32_t block_count,
			  uint32_t addr, uint32_t write_counter)
{
	if (frame_type == RPMB_FRAME_TYPE_NVME) {
		return rpmb_frame_set_nvme(f, req_resp, block_count,
					   addr, write_counter);
	} else {
		return rpmb_frame_set_jdec(f, req_resp, block_count,
					   addr, write_counter);
	}
}

static int rpmb_frame_write_data_jdec(int fd, void *f)
{
	struct rpmb_frame_jdec *frames = f;
	uint16_t i, block_count = be16toh(frames[0].block_count);

	for (i = 0; i < block_count; i++) {
		int ret;

		ret = write_file(fd, frames[i].data, sizeof(frames[i].data));
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int rpmb_frame_write_data_nvme(int fd, void *f)
{
	struct rpmb_frame_nvme *frame = f;
	uint32_t i, block_count = le32toh(frame->block_count);

	for (i = 0; i < block_count; i++) {
		int ret;

		ret = write_file(fd, &frame->data[i], RPMB_SECTOR_SIZE);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int rpmb_frame_write_data(uint8_t frame_type, int fd, void *f)
{
	return frame_type == RPMB_FRAME_TYPE_NVME ?
			rpmb_frame_write_data_nvme(fd, f) :
			rpmb_frame_write_data_jdec(fd, f);
}

static int rpmb_frame_read_data_jdec(int fd, void *f)
{
	struct rpmb_frame_jdec *frames = f;
	uint16_t i, block_count = be16toh(frames[0].block_count);

	for (i = 0; i < block_count; i++) {
		int ret = read_file(fd, frames[i].data,
				sizeof(frames[0].data));
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int rpmb_frame_read_data_nvme(int fd, void *f)
{
	struct rpmb_frame_nvme *frame = f;
	uint32_t i, block_count = le32toh(frame->block_count);

	for (i = 0; i < block_count; i++) {
		int ret;

		ret = read_file(fd, &frame->data[i], RPMB_SECTOR_SIZE);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int rpmb_frame_read_data(uint8_t frame_type, int fd, void *f)
{
	return frame_type == RPMB_FRAME_TYPE_NVME ?
		rpmb_frame_read_data_nvme(fd, f) :
		rpmb_frame_read_data_jdec(fd, f);
}

#if OPENSSL_VERSION_NUMBER < 0x10100000L
static int rpmb_calc_hmac_sha256_jdec(struct rpmb_frame_jdec *frames,
				      size_t blocks_cnt,
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
	for (i = 0; i < block_count; i++)
		HMAC_Update(&ctx, frames[i].data, rpmb_jdec_hmac_data_len);

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

static int rpmb_calc_hmac_sha256_nvme(struct rpmb_frame_nvme *frame,
				      size_t block_count,
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

	HMAC_Update(&ctx, &frame->rpmb_target, hmac_nvme_data_len);
	for (i = 0; i < block_count; i++)
		HMAC_Update(&ctx, frames->data[i], RPMB_SECTOR_SIZE);

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
#else
static int rpmb_calc_hmac_sha256_jdec(struct rpmb_frame_jdec *frames,
				      size_t blocks_cnt,
				      const unsigned char key[],
				      unsigned int key_size,
				      unsigned char mac[],
				      unsigned int mac_size)
{
	HMAC_CTX *ctx;
	int ret;
	unsigned int i;

	 /* SSL returns 1 on success 0 on failure */

	ctx = HMAC_CTX_new();

	ret = HMAC_Init_ex(ctx, key, key_size, EVP_sha256(), NULL);
	if (ret == 0)
		goto out;
	for (i = 0; i < blocks_cnt; i++)
		HMAC_Update(ctx, frames[i].data, rpmb_jdec_hmac_data_len);

	ret = HMAC_Final(ctx, mac, &mac_size);
	if (ret == 0)
		goto out;
	if (mac_size != RPMB_MAC_SIZE)
		ret = 0;

	ret = 1;
out:
	HMAC_CTX_free(ctx);
	return ret == 1 ? 0 : -1;
}

static int rpmb_calc_hmac_sha256_nvme(struct rpmb_frame_nvme *frame,
				      size_t block_count,
				      const unsigned char key[],
				      unsigned int key_size,
				      unsigned char mac[],
				      unsigned int mac_size)
{
	HMAC_CTX *ctx;
	int ret;
	unsigned int i;

	/* SSL returns 1 on success 0 on failure */

	ctx = HMAC_CTX_new();

	ret = HMAC_Init_ex(ctx, key, key_size, EVP_sha256(), NULL);
	if (ret == 0)
		goto out;

	HMAC_Update(ctx, &frame->rpmb_target, rpmb_nvme_hmac_data_len);
	for (i = 0; i < block_count; i++)
		HMAC_Update(ctx, &frame->data[i], RPMB_SECTOR_SIZE);

	ret = HMAC_Final(ctx, mac, &mac_size);
	if (ret == 0)
		goto out;
	if (mac_size != RPMB_MAC_SIZE)
		ret = 0;

	ret = 1;
out:
	HMAC_CTX_free(ctx);
	return ret == 1 ? 0 : -1;
}
#endif

static int rpmb_calc_hmac_sha256(uint8_t frame_type, void *f,
				 size_t block_count,
				 const unsigned char key[],
				 unsigned int key_size,
				 unsigned char mac[],
				 unsigned int mac_size)
{
	if (frame_type == RPMB_FRAME_TYPE_NVME)
		return rpmb_calc_hmac_sha256_nvme(f, block_count,
						  key, key_size,
						  mac, mac_size);
	else
		return rpmb_calc_hmac_sha256_jdec(f, block_count,
						  key, key_size,
						  mac, mac_size);
}

static int rpmb_check_mac(uint8_t frame_type,
			  const unsigned char *key, size_t key_size,
			  void *frames_out, unsigned int block_count)
{
	unsigned char mac[RPMB_MAC_SIZE];
	unsigned char *mac_out;
	int ret;

	if (block_count == 0) {
		rpmb_err("RPMB 0 output frames.\n");
		return -1;
	}

	ret = rpmb_calc_hmac_sha256(frame_type, frames_out, block_count,
				    key, key_size, mac, RPMB_MAC_SIZE);
	if (ret)
		return ret;

	mac_out = rpmb_frame_get_key_mac_ptr(frame_type, frames_out,
					     block_count, RPMB_MAC_SIZE);
	if (memcmp(mac, mac_out, RPMB_MAC_SIZE)) {
		rpmb_err("RPMB hmac mismatch:\n");
		dump_hex_buffer("Result MAC: ", mac_out, RPMB_MAC_SIZE);
		dump_hex_buffer("Expected MAC: ", mac, RPMB_MAC_SIZE);
		return -1;
	}

	return 0;
}

static int rpmb_check_req_resp(uint8_t frame_type,
			       uint16_t req, void *frame_out)
{
	uint16_t req_resp = rpmb_frame_get_req_resp(frame_type, frame_out);

	if (RPMB_REQ2RESP(req) != req_resp) {
		rpmb_err("RPMB response mismatch %04X != %04X\n.",
			 RPMB_REQ2RESP(req), req_resp);
		return -1;
	}

	return 0;
}

static struct rpmb_frame_jdec *rpmb_frame_alloc_jdec(size_t block_count)
{
	return calloc(1, rpmb_ioc_frames_len_jdec(block_count));
}

static struct rpmb_frame_nvme *rpmb_frame_alloc_nvme(size_t sector_count)
{
	return calloc(1, rpmb_ioc_frames_len_nvme(sector_count));
}

static void *rpmb_frame_alloc(uint8_t type, size_t count)
{
	if (type == RPMB_FRAME_TYPE_NVME)
		return rpmb_frame_alloc_nvme(count);
	else
		return rpmb_frame_alloc_jdec(count);
}

static int rpmb_ioctl(uint8_t frame_type, int fd, uint16_t req,
		      const void *frames_in, unsigned int cnt_in,
		      void *frames_out, unsigned int cnt_out)
{
	int ret;
	struct __attribute__((packed)) {
		struct rpmb_ioc_seq_cmd h;
		struct rpmb_ioc_cmd cmd[3];
	} iseq = {};

	void *frame_res = NULL;
	int i;
	uint32_t flags;

	rpmb_dbg("RPMB OP: %s\n", rpmb_op_str(req));
	dbg_dump_frame(frame_type, "In Frame: ", frames_in, cnt_in);

	i = 0;
	flags = RPMB_F_WRITE;
	if (req == RPMB_WRITE_DATA || req == RPMB_PROGRAM_KEY)
		flags |= RPMB_F_REL_WRITE;
	rpmb_ioc_cmd_set(iseq.cmd[i], flags, frames_in, cnt_in);
	i++;

	if (req == RPMB_WRITE_DATA || req == RPMB_PROGRAM_KEY) {
		frame_res = rpmb_frame_alloc(frame_type, 0);
		if (!frame_res)
			return -ENOMEM;
		rpmb_frame_set(frame_type, frame_res,
			       RPMB_RESULT_READ, 0, 0, 0);
		rpmb_ioc_cmd_set(iseq.cmd[i], RPMB_F_WRITE, frame_res, 0);
		i++;
	}

	rpmb_ioc_cmd_set(iseq.cmd[i], 0, frames_out, cnt_out);
	i++;

	iseq.h.num_of_cmds = i;
	ret = ioctl(fd, RPMB_IOC_SEQ_CMD, &iseq);
	if (ret < 0)
		rpmb_err("ioctl failure %d: %s.\n", ret, strerror(errno));

	ret = rpmb_check_req_resp(frame_type, req, frames_out);

	dbg_dump_frame(frame_type, "Res Frame: ", frame_res, 1);
	dbg_dump_frame(frame_type, "Out Frame: ", frames_out, cnt_out);
	free(frame_res);
	return ret;
}

static int op_get_info(int nargs, char *argv[])
{
	int dev_fd;
	struct rpmb_ioc_cap_cmd cap;

	if (nargs != 1)
		return -EINVAL;

	memset(&cap, 0, sizeof(cap));
	dev_fd = open_dev_file(argv[0], &cap);
	if (dev_fd < 0)
		return -errno;
	argv++;

	printf("RPMB device_type = %hd\n", cap.device_type);
	printf("RPMB rpmb_target = %hd\n", cap.target);
	printf("RPMB capacity    = %hd\n", cap.capacity);
	printf("RPMB block_size  = %hd\n", cap.block_size);
	printf("RPMB wr_cnt_max  = %hd\n", cap.wr_cnt_max);
	printf("RPMB rd_cnt_max  = %hd\n", cap.rd_cnt_max);
	printf("RPMB auth_method = %hd\n", cap.auth_method);

	close(dev_fd);

	return 0;
}

static int __rpmb_program_key(uint8_t frame_type, int dev_fd,
			      uint8_t *key, size_t key_size)
{
	void *frame_in, *frame_out;
	uint16_t req = RPMB_PROGRAM_KEY;
	int ret;

	frame_in = rpmb_frame_alloc(frame_type, 0);
	frame_out = rpmb_frame_alloc(frame_type, 0);
	if (!frame_in || !frame_out) {
		ret = -ENOMEM;
		goto out;
	}

	rpmb_frame_set(frame_type, frame_in, req, 0, 0, 0);

	ret = rpmb_frame_set_key_mac(frame_type, frame_in, 0, key, key_size);
	if (ret)
		goto out;

	ret = rpmb_ioctl(frame_type, dev_fd, req, frame_in, 1, frame_out, 1);
	if (ret)
		goto out;

	ret = rpmb_check_req_resp(frame_type, req, frame_out);
	if (ret)
		goto out;

	ret = rpmb_frame_get_result(frame_type, frame_out);
	if (ret)
		rpmb_err("RPMB operation %s failed, %s[0x%04x].\n",
			 rpmb_op_str(req), rpmb_result_str(ret), ret);

out:
	free(frame_in);
	free(frame_out);

	return 0;
}

static uint8_t rpmb_cap_get_frame_type(struct rpmb_ioc_cap_cmd *cap)
{
	if (cap->device_type == RPMB_TYPE_NVME)
		return RPMB_FRAME_TYPE_NVME;
	else
		return RPMB_FRAME_TYPE_JDEC;
}

static int op_rpmb_program_key(int nargs, char *argv[])
{
	int ret;
	int  dev_fd = -1, key_fd = -1;
	uint8_t key[RPMB_KEY_SIZE];
	uint8_t frame_type;
	struct rpmb_ioc_cap_cmd cap;

	ret = -EINVAL;
	if (nargs != 2)
		return ret;

	dev_fd = open_dev_file(argv[0], &cap);
	if (dev_fd < 0)
		goto out;
	argv++;

	key_fd = open_rd_file(argv[0], "key file");
	if (key_fd < 0)
		goto out;
	argv++;

	read_file(key_fd, key, RPMB_KEY_SIZE);

	frame_type = rpmb_cap_get_frame_type(&cap);

	ret = __rpmb_program_key(frame_type, dev_fd, key, RPMB_KEY_SIZE);

out:
	close_fd(dev_fd);
	close_fd(key_fd);

	return ret;
}

static int rpmb_get_write_counter(uint8_t frame_type, int dev_fd,
				  unsigned int *cnt, const unsigned char *key)
{
	int ret;
	uint16_t res = 0x000F;
	uint16_t req = RPMB_GET_WRITE_COUNTER;
	void *frame_in = NULL;
	void *frame_out = NULL;
	uint8_t *nonce_in;
	uint8_t *nonce_out;

	frame_in = rpmb_frame_alloc(frame_type, 0);
	frame_out = rpmb_frame_alloc(frame_type, 0);
	if (!frame_in || !frame_out) {
		ret = -ENOMEM;
		goto out;
	}

	rpmb_frame_set(frame_type, frame_in, req, 0, 0, 0);
	nonce_in = rpmb_frame_get_nonce_ptr(frame_type, frame_in);
	RAND_bytes(nonce_in, RPMB_NONCE_SIZE);

	ret = rpmb_ioctl(frame_type, dev_fd, req, frame_in, 0, frame_out, 0);
	if (ret)
		goto out;

	ret = rpmb_check_req_resp(frame_type, req, frame_out);
	if (ret)
		goto out;

	res = rpmb_frame_get_result(frame_type, frame_out);
	if (res != RPMB_ERR_OK) {
		ret = -1;
		goto out;
	}

	nonce_out = rpmb_frame_get_nonce_ptr(frame_type, frame_out);

	if (memcmp(nonce_in, nonce_out, RPMB_NONCE_SIZE)) {
		rpmb_err("RPMB NONCE mismatch\n");
		dump_hex_buffer("Result NONCE:", nonce_out, RPMB_NONCE_SIZE);
		dump_hex_buffer("Expected NONCE: ", nonce_in, RPMB_NONCE_SIZE);
		ret = -1;
		goto out;
	}

	if (key) {
		ret = rpmb_check_mac(frame_type, key, RPMB_KEY_SIZE,
				     frame_out, 1);
		if (ret)
			goto out;
	}

	*cnt = rpmb_frame_get_write_counter(frame_type, frame_out);

out:
	if (ret)
		rpmb_err("RPMB operation %s failed=%d %s[0x%04x]\n",
			 rpmb_op_str(req), ret, rpmb_result_str(res), res);
	free(frame_in);
	free(frame_out);
	return ret;
}

static int op_rpmb_get_write_counter(int nargs, char **argv)
{
	int ret;
	int dev_fd = -1, key_fd = -1;
	bool has_key;
	struct rpmb_ioc_cap_cmd cap;
	unsigned char key[RPMB_KEY_SIZE];
	unsigned int cnt = 0;
	uint8_t frame_type;

	if (nargs == 2)
		has_key = true;
	else if (nargs == 1)
		has_key = false;
	else
		return -EINVAL;

	ret = -EINVAL;
	dev_fd = open_dev_file(argv[0], &cap);
	if (dev_fd < 0)
		return ret;
	argv++;

	frame_type = rpmb_cap_get_frame_type(&cap);

	if (has_key) {
		key_fd = open_rd_file(argv[0], "key file");
		if (key_fd < 0)
			goto out;
		argv++;

		ret = read_file(key_fd, key, RPMB_KEY_SIZE);
		if (ret < 0)
			goto out;

		ret = rpmb_get_write_counter(frame_type, dev_fd, &cnt, key);
	} else {
		ret = rpmb_get_write_counter(frame_type, dev_fd, &cnt, NULL);
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
	int ret;
	int dev_fd = -1, data_fd = -1, key_fd = -1;
	uint16_t req = RPMB_READ_DATA;
	uint32_t addr, block_count;
	unsigned char key[RPMB_KEY_SIZE];
	uint8_t *nonce_in;
	unsigned long numarg;
	bool has_key;
	struct rpmb_ioc_cap_cmd cap;
	void *frame_in = NULL;
	void *frames_out = NULL;
	uint8_t frame_type;

	ret = -EINVAL;
	if (nargs == 4)
		has_key = false;
	else if (nargs == 5)
		has_key = true;
	else
		return ret;

	dev_fd = open_dev_file(argv[0], &cap);
	if (dev_fd < 0)
		goto out;
	argv++;

	errno = 0;
	numarg = strtoul(argv[0], NULL, 0);
	if (errno || numarg > UINT_MAX) {
		rpmb_err("wrong block address\n");
		goto out;
	}
	addr = (uint32_t)numarg;
	argv++;

	errno = 0;
	numarg = strtoul(argv[0], NULL, 0);
	if (errno || numarg > UINT_MAX) {
		rpmb_err("wrong blocks count\n");
		goto out;
	}
	block_count = (uint32_t)numarg;
	argv++;

	if (block_count == 0) {
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

	frame_type = rpmb_cap_get_frame_type(&cap);

	ret = 0;
	frames_out = rpmb_frame_alloc(frame_type, block_count);
	frame_in = rpmb_frame_alloc(frame_type, 0);
	if (!frames_out || !frame_in) {
		rpmb_err("Cannot allocate %d RPMB frames\n", block_count);
		ret = -ENOMEM;
		goto out;
	}

	/* eMMc spec ask for 0 block_count here
	 * this will be translated by the rpmb layer
	 */
	rpmb_frame_set(frame_type, frame_in, req, block_count, addr, 0);
	nonce_in = rpmb_frame_get_nonce_ptr(frame_type, frame_in);
	RAND_bytes(nonce_in, RPMB_NONCE_SIZE);

	ret = rpmb_ioctl(frame_type, dev_fd, req, frame_in, 0,
			 frames_out, block_count);
	if (ret)
		goto out;

	ret = rpmb_frame_get_result(frame_type, frames_out);
	if (ret) {
		rpmb_err("RPMB operation %s failed, %s[0x%04x]\n",
			 rpmb_op_str(req), rpmb_result_str(ret), ret);
		goto out;
	}

	if (has_key) {
		ret = rpmb_check_mac(frame_type, key, RPMB_KEY_SIZE,
				     frames_out, block_count);
		if (ret)
			goto out;
	}

	ret = rpmb_frame_write_data(frame_type, data_fd, frames_out);

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
	uint16_t req = RPMB_WRITE_DATA;
	unsigned char key[RPMB_KEY_SIZE];
	unsigned char mac[RPMB_MAC_SIZE];
	unsigned long numarg;
	struct rpmb_ioc_cap_cmd cap;
	uint16_t addr, block_count;
	uint32_t write_counter = 0;
	uint32_t write_counter_out = 0;
	void *frames_in = NULL;
	void *frame_out = NULL;
	uint8_t frame_type;

	ret = -EINVAL;
	if (nargs != 5)
		goto out;

	dev_fd = open_dev_file(argv[0], &cap);
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
	block_count = (uint16_t)numarg;
	argv++;

	if (block_count == 0) {
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

	frame_type = rpmb_cap_get_frame_type(&cap);

	frames_in = rpmb_frame_alloc(frame_type, block_count);
	frame_out = rpmb_frame_alloc(frame_type, 0);
	if (!frames_in || !frame_out) {
		rpmb_err("can't allocate memory for RPMB outer frames\n");
		ret = -ENOMEM;
		goto out;
	}

	ret = rpmb_get_write_counter(frame_type, dev_fd, &write_counter, NULL);
	if (ret)
		goto out;

	ret = rpmb_frame_set(frame_type, frames_in,
			     req, block_count, addr, write_counter);
	if (ret)
		goto out;

	ret = rpmb_frame_read_data(frame_type, data_fd, frames_in);
	if (ret)
		goto out;

	rpmb_calc_hmac_sha256(frame_type, frames_in,
			      block_count,
			      key, RPMB_KEY_SIZE,
			      mac, RPMB_MAC_SIZE);

	rpmb_frame_set_key_mac(frame_type, frames_in, block_count,
			       mac, RPMB_MAC_SIZE);

	ret = rpmb_ioctl(frame_type, dev_fd, req,
			 frames_in, block_count,
			 frame_out, 0);
	if (ret != 0)
		goto out;

	ret = rpmb_frame_get_result(frame_type, frame_out);
	if (ret) {
		rpmb_err("RPMB operation %s failed, %s[0x%04x]\n",
			 rpmb_op_str(req), rpmb_result_str(ret), ret);
		ret = -1;
	}

	if (rpmb_frame_get_addr(frame_type, frame_out) != addr) {
		rpmb_err("RPMB addr mismatchs res=%04x req=%04x\n",
			 rpmb_frame_get_addr(frame_type, frame_out), addr);
		ret = -1;
	}

	write_counter_out = rpmb_frame_get_write_counter(frame_type, frame_out);
	if (write_counter_out <= write_counter) {
		rpmb_err("RPMB write counter not incremented res=%x req=%x\n",
			 write_counter_out, write_counter);
		ret = -1;
	}

	//ret = rpmb_check_mac(frame_type, key, RPMB_KEY_SIZE, frame_out, 1);
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
	 "get-info",
	 op_get_info,
	 "<RPMB_DEVICE>",
	 "    Get RPMB device info\n",
	},
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
	printf("Usage: %s [-v] <command> <args>\n\n", prog);
	for (i = 0; cmds[i].op_name; i++)
		printf("       %s %s %s\n",
		       prog, cmds[i].op_name, cmds[i].usage);

	printf("\n");
	printf("      %s -v/--verbose: runs in verbose mode\n", prog);
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
