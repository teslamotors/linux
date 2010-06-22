/*  cryptodev_test - simple benchmark tool for cryptodev
 *
 *    Copyright (C) 2010 by Phil Sutter <phil.sutter@viprinet.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <crypto/cryptodev.h>

#if 0
#define TEST_ALGO	CRYPTO_AES_CBC
#define TEST_KEYLEN	32
#else
#define TEST_ALGO	CRYPTO_NULL
#define TEST_KEYLEN	0
#endif

/* transcode a total of 1GB */
#define TOT_LEN (1024 * 1024 * 1024)

static long udifftimeval(struct timeval start, struct timeval end)
{
	return (end.tv_usec - start.tv_usec) +
	       (end.tv_sec - start.tv_sec) * 1000 * 1000 + 0.5;
}

int encrypt_data(struct session_op *sess, int fdc, int len, int chunksize, int flags)
{
	struct crypt_op cop;
	char *buffer, iv[32];
	int i, buffercount;
	static int val = 23;
	struct timeval start, end;

	if (len % chunksize) {
		printf("got milk?!\n");
		return 1;
	}

	buffercount = len / chunksize;

	buffer = malloc(chunksize);
	memset(iv, 0x23, 32);

	printf("Encrypting %d x %d bytes: ", buffercount, chunksize);
	fflush(stdout);

	memset(buffer, val++, chunksize);

	gettimeofday(&start, NULL);
	for (i = 0; i < buffercount; i++) {
		memset(&cop, 0, sizeof(cop));
		cop.ses = sess->ses;
		cop.len = chunksize;
		cop.iv = (unsigned char *)iv;
		cop.op = COP_ENCRYPT;
		cop.flags = flags;
		cop.src = cop.dst = (unsigned char *)buffer;

		if (ioctl(fdc, CIOCCRYPT, &cop)) {
			perror("ioctl(CIOCCRYPT)");
			return 1;
		}
	}
	gettimeofday(&end, NULL);

	printf("done in %.5f seconds - %.5fMB/s\n", (double)udifftimeval(start, end) / 1000000.0,
			(double)len / ((double)udifftimeval(start, end) / 1000000.0) / 1024 / 1024);
	return 0;
}

int main(void)
{
	int fd, i, fdc = -1;
	struct session_op sess;
	char keybuf[32];

	if ((fd = open("/dev/crypto", O_RDWR, 0)) < 0) {
		perror("open()");
		return 1;
	}
	if (ioctl(fd, CRIOGET, &fdc)) {
		perror("ioctl(CRIOGET)");
		return 1;
	}
	memset(&sess, 0, sizeof(sess));
	sess.cipher = TEST_ALGO;
	sess.keylen = TEST_KEYLEN;
	memset(keybuf, 0x42, 32);
	sess.key = (unsigned char *)keybuf;
	if (ioctl(fdc, CIOCGSESSION, &sess)) {
		perror("ioctl(CIOCGSESSION)");
		return 1;
	}

	printf("Standard operation:\n");
	for (i = 16; i <= (64 * 4096); i *= 2) {
		if (encrypt_data(&sess, fdc, TOT_LEN, i, 0))
			break;
	}
	printf("Zero-Copy operation:\n");
	for (i = 16; i <= (64 * 4096); i *= 2) {
		if (encrypt_data(&sess, fdc, TOT_LEN, i, COP_FLAG_ZCOPY))
			break;
	}

	close(fdc);
	close(fd);
	return 0;
}
