/*
 * Demo on how to use /dev/crypto device for ciphering.
 *
 * Placed under public domain.
 *
 */
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <crypto/cryptodev.h>

#include "testhelper.h"

#define	DATA_SIZE	8*1024
#define	BLOCK_SIZE	16
#define	KEY_SIZE	16

static int
test_crypto(int cfd)
{
	char plaintext[DATA_SIZE];
	char ciphertext[DATA_SIZE];
	char iv[BLOCK_SIZE];
	char key[KEY_SIZE];

	struct session_op sess;
	struct crypt_op cryp;

	printf("running %s\n", __func__);

	memset(&sess, 0, sizeof(sess));
	memset(&cryp, 0, sizeof(cryp));

	memset(plaintext, 0x15,  sizeof(plaintext));
	memset(key, 0x33,  sizeof(key));
	memset(iv, 0x03,  sizeof(iv));

	/* Get crypto session for AES128 */
	sess.cipher = CRYPTO_AES_CBC;
	sess.keylen = KEY_SIZE;
	sess.key = key;
	if (ioctl(cfd, CIOCGSESSION, &sess)) {
		perror("ioctl(CIOCGSESSION)");
		return 1;
	}

	printf("%s: got the session\n", __func__);


	/* Encrypt data.in to data.encrypted */
	cryp.ses = sess.ses;
	cryp.len = sizeof(plaintext);
	cryp.src = plaintext;
	cryp.dst = ciphertext;
	cryp.iv = iv;
	cryp.op = COP_ENCRYPT;

	DO_OR_DIE(do_async_crypt(cfd, &cryp), 0);
	DO_OR_DIE(do_async_fetch(cfd, &cryp), 0);

	printf("%s: data encrypted\n", __func__);

	if (ioctl(cfd, CIOCFSESSION, &sess.ses)) {
		perror("ioctl(CIOCFSESSION)");
		return 1;
	}
	printf("%s: session finished\n", __func__);

	if (ioctl(cfd, CIOCGSESSION, &sess)) {
		perror("ioctl(CIOCGSESSION)");
		return 1;
	}
	printf("%s: got new session\n", __func__);

	/* Decrypt data.encrypted to data.decrypted */
	cryp.ses = sess.ses;
	cryp.len = sizeof(plaintext);
	cryp.src = ciphertext;
	cryp.dst = ciphertext;
	cryp.iv = iv;
	cryp.op = COP_DECRYPT;

	DO_OR_DIE(do_async_crypt(cfd, &cryp), 0);
	DO_OR_DIE(do_async_fetch(cfd, &cryp), 0);

	printf("%s: data encrypted\n", __func__);

	/* Verify the result */
	if (memcmp(plaintext, ciphertext, sizeof(plaintext)) != 0) {
		fprintf(stderr,
			"FAIL: Decrypted data are different from the input data.\n");
		return 1;
	} else
		printf("Test passed\n");

	/* Finish crypto session */
	if (ioctl(cfd, CIOCFSESSION, &sess.ses)) {
		perror("ioctl(CIOCFSESSION)");
		return 1;
	}

	return 0;
}

static int test_aes(int cfd)
{
	char plaintext1[BLOCK_SIZE];
	char ciphertext1[BLOCK_SIZE] = { 0xdf, 0x55, 0x6a, 0x33, 0x43, 0x8d, 0xb8, 0x7b, 0xc4, 0x1b, 0x17, 0x52, 0xc5, 0x5e, 0x5e, 0x49 };
	char iv1[BLOCK_SIZE];
	char key1[KEY_SIZE] = { 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	char plaintext2[BLOCK_SIZE] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x00 };
	char ciphertext2[BLOCK_SIZE] = { 0xb7, 0x97, 0x2b, 0x39, 0x41, 0xc4, 0x4b, 0x90, 0xaf, 0xa7, 0xb2, 0x64, 0xbf, 0xba, 0x73, 0x87 };
	char iv2[BLOCK_SIZE];
	char key2[KEY_SIZE];

	struct session_op sess1, sess2;
	struct crypt_op cryp1, cryp2;

	printf("running %s\n", __func__);

	memset(&sess1, 0, sizeof(sess1));
	memset(&sess2, 0, sizeof(sess2));
	memset(&cryp1, 0, sizeof(cryp1));
	memset(&cryp2, 0, sizeof(cryp2));

	memset(plaintext1, 0x0, sizeof(plaintext1));
	memset(iv1, 0x0, sizeof(iv1));

	/* Get crypto session for AES128 */
	sess1.cipher = CRYPTO_AES_CBC;
	sess1.keylen = KEY_SIZE;
	sess1.key = key1;
	if (ioctl(cfd, CIOCGSESSION, &sess1)) {
		perror("ioctl(CIOCGSESSION)");
		return 1;
	}

	memset(key2, 0x0, sizeof(key2));

	/* Get second crypto session for AES128 */
	sess2.cipher = CRYPTO_AES_CBC;
	sess2.keylen = KEY_SIZE;
	sess2.key = key2;
	if (ioctl(cfd, CIOCGSESSION, &sess2)) {
		perror("ioctl(CIOCGSESSION)");
		return 1;
	}

	/* Encrypt data.in to data.encrypted */
	cryp1.ses = sess1.ses;
	cryp1.len = sizeof(plaintext1);
	cryp1.src = plaintext1;
	cryp1.dst = plaintext1;
	cryp1.iv = iv1;
	cryp1.op = COP_ENCRYPT;

	DO_OR_DIE(do_async_crypt(cfd, &cryp1), 0);
	printf("cryp1 written out\n");

	memset(iv2, 0x0, sizeof(iv2));

	/* Encrypt data.in to data.encrypted */
	cryp2.ses = sess2.ses;
	cryp2.len = sizeof(plaintext2);
	cryp2.src = plaintext2;
	cryp2.dst = plaintext2;
	cryp2.iv = iv2;
	cryp2.op = COP_ENCRYPT;

	DO_OR_DIE(do_async_crypt(cfd, &cryp2), 0);
	printf("cryp2 written out\n");

	DO_OR_DIE(do_async_fetch(cfd, &cryp1), 0);
	DO_OR_DIE(do_async_fetch(cfd, &cryp2), 0);
	printf("cryp1 + cryp2 successfully read\n");

	/* Verify the result */
	if (memcmp(plaintext1, ciphertext1, sizeof(plaintext1)) != 0) {
		int i;
		fprintf(stderr,
			"FAIL: Decrypted data are different from the input data.\n");
		printf("cipher text should be:\t");
		for (i = 0; i < BLOCK_SIZE; i++) {
			printf("%02x ", (unsigned char)ciphertext1[i]);
		}
		printf("\nbut really is:\t\t");
		for (i = 0; i < BLOCK_SIZE; i++) {
			printf("%02x ", (unsigned char)plaintext1[i]);
		}
		printf("\n");
		return 1;
	} else {
		printf("result 1 passed\n");
	}

	/* Test 2 */

	/* Verify the result */
	if (memcmp(plaintext2, ciphertext2, sizeof(plaintext2)) != 0) {
		int i;
		fprintf(stderr,
			"FAIL: Decrypted data are different from the input data.\n");
		printf("cipher text should be:\t");
		for (i = 0; i < BLOCK_SIZE; i++) {
			printf("%02x ", (unsigned char)ciphertext2[i]);
		}
		printf("\nbut really is:\t\t");
		for (i = 0; i < BLOCK_SIZE; i++) {
			printf("%02x ", (unsigned char)plaintext2[i]);
		}
		printf("\n");
		return 1;
	} else {
		printf("result 2 passed\n");
	}

	printf("AES Test passed\n");

	/* Finish crypto session */
	if (ioctl(cfd, CIOCFSESSION, &sess1.ses)) {
		perror("ioctl(CIOCFSESSION)");
		return 1;
	}
	if (ioctl(cfd, CIOCFSESSION, &sess2.ses)) {
		perror("ioctl(CIOCFSESSION)");
		return 1;
	}

	return 0;
}

int
main()
{
	int fd = -1, cfd = -1;

	/* Open the crypto device */
	fd = open("/dev/crypto", O_RDWR, 0);
	if (fd < 0) {
		perror("open(/dev/crypto)");
		return 1;
	}

	/* Clone file descriptor */
	if (ioctl(fd, CRIOGET, &cfd)) {
		perror("ioctl(CRIOGET)");
		return 1;
	}

	/* Set close-on-exec (not really neede here) */
	if (fcntl(cfd, F_SETFD, 1) == -1) {
		perror("fcntl(F_SETFD)");
		return 1;
	}

	/* Run the test itself */
	if (test_aes(cfd))
		return 1;

	if (test_crypto(cfd))
		return 1;

	/* Close cloned descriptor */
	if (close(cfd)) {
		perror("close(cfd)");
		return 1;
	}

	/* Close the original descriptor */
	if (close(fd)) {
		perror("close(fd)");
		return 1;
	}

	return 0;
}

