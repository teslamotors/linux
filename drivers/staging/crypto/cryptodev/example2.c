/*
 * Demo on how to use OpenBSD /dev/crypto device.
 *
 * Author: Michal Ludvig <michal@logix.cz>
 *         http://www.logix.cz/michal
 *
 * Note: by default OpenBSD doesn't allow using
 *       /dev/crypto if there is no hardware accelerator
 *       for a given algorithm. To change this you'll have to
 *       set cryptodevallowsoft=1 in 
 *       /usr/src/sys/crypto/cryptodev.c and rebuild your kernel.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/ioctl.h>
//#include <crypto/cryptodev.h>
#include "cryptodev.h"

#define	DATA_SIZE	4096
#define	BLOCK_SIZE	16
#define	KEY_SIZE	16

static int
test_crypto(int cfd)
{
	struct {
		uint8_t	in[DATA_SIZE],
			encrypted[DATA_SIZE],
			decrypted[DATA_SIZE],
			iv[BLOCK_SIZE],
			key[KEY_SIZE];
	} data;
	struct session_op sess;
	struct crypt_op cryp;
	uint8_t mac[HASH_MAX_LEN];
	uint8_t oldmac[HASH_MAX_LEN];
	uint8_t def_out[] = "\x75\x0c\x78\x3e\x6a\xb0\xb5\x03\xea\xa8\x6e\x31\x0a\x5d\xb7\x38";
	uint8_t def_out_hash[] = "\x8f\x82\x03\x94\xf9\x53\x35\x18\x20\x45\xda\x24\xf3\x4d\xe5\x2b\xf8\xbc\x34\x32";
	int i;

	memset(&sess, 0, sizeof(sess));
	memset(&cryp, 0, sizeof(cryp));

	/* Use the garbage that is on the stack :-) */
	/* memset(&data, 0, sizeof(data)); */

	memset(mac, 0, sizeof(mac));

	sess.cipher = 0;
	sess.mac = CRYPTO_SHA1;
	if (ioctl(cfd, CIOCGSESSION, &sess)) {
		perror("ioctl(CIOCGSESSION)");
		return 1;
	}

	cryp.ses = sess.ses;
	cryp.len = sizeof("what do ya want for nothing?")-1;
	cryp.src = "what do ya want for nothing?";
	cryp.mac = mac;
	cryp.op = COP_ENCRYPT;
	if (ioctl(cfd, CIOCCRYPT, &cryp)) {
		perror("ioctl(CIOCCRYPT)");
		return 1;
	}

	if (memcmp(mac, def_out_hash, 20)!=0) {
		printf("mac: ");
		for (i=0;i<SHA1_HASH_LEN;i++) {
			printf("%.2x", (uint8_t)mac[i]);
		}
		puts("\n");
		fprintf(stderr, "HASH test 1: failed\n");
	} else {
		fprintf(stderr, "HASH test 1: passed\n");
	}

	memset(mac, 0, sizeof(mac));

	sess.cipher = 0;
	sess.mackey = (uint8_t*)"Jefe";
	sess.mackeylen = 4;
	sess.mac = CRYPTO_MD5_HMAC;
	if (ioctl(cfd, CIOCGSESSION, &sess)) {
		perror("ioctl(CIOCGSESSION)");
		return 1;
	}

	cryp.ses = sess.ses;
	cryp.len = sizeof("what do ya want for nothing?")-1;
	cryp.src = "what do ya want for nothing?";
	cryp.mac = mac;
	cryp.op = COP_ENCRYPT;
	if (ioctl(cfd, CIOCCRYPT, &cryp)) {
		perror("ioctl(CIOCCRYPT)");
		return 1;
	}

	if (memcmp(mac, def_out, 16)!=0) {
		printf("mac: ");
		for (i=0;i<SHA1_HASH_LEN;i++) {
			printf("%.2x", (uint8_t)mac[i]);
		}
		puts("\n");
		fprintf(stderr, "HMAC test 1: failed\n");
	} else {
		fprintf(stderr, "HMAC test 1: passed\n");
	}
return 0;
	sess.cipher = CRYPTO_AES_CBC;
	sess.mac = CRYPTO_SHA1_HMAC;
	sess.keylen = KEY_SIZE;
	sess.key = data.key;
	sess.mackeylen = 16;
	sess.mackey = (uint8_t*)"\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b";
	if (ioctl(cfd, CIOCGSESSION, &sess)) {
		perror("ioctl(CIOCGSESSION)");
		return 1;
	}

	/* Encrypt data.in to data.encrypted */
	cryp.ses = sess.ses;
	cryp.len = sizeof(data.in);
	cryp.src = data.in;
	cryp.dst = data.encrypted;
	cryp.iv = data.iv;
	cryp.mac = mac;
	cryp.op = COP_ENCRYPT;
	if (ioctl(cfd, CIOCCRYPT, &cryp)) {
		perror("ioctl(CIOCCRYPT)");
		return 1;
	}
	
	memcpy(oldmac, mac, sizeof(mac));
	
	/* Decrypt data.encrypted to data.decrypted */
	cryp.src = data.encrypted;
	cryp.dst = data.decrypted;
	cryp.op = COP_DECRYPT;
	if (ioctl(cfd, CIOCCRYPT, &cryp)) {
		perror("ioctl(CIOCCRYPT)");
		return 1;
	}

	/* Verify the result */
	if (memcmp(data.in, data.decrypted, sizeof(data.in)) != 0) {
		fprintf(stderr,
			"FAIL: Decrypted data are different from the input data.\n");
		return 1;
	} else
		printf("Crypt Test: passed\n");

	if (memcmp(mac, oldmac, 20) != 0) {
		fprintf(stderr,
			"FAIL: Hash in decrypted data different than in encrypted.\n");
		return 1;
	} else
		printf("HMAC Test 2: passed\n");

	/* Finish crypto session */
	if (ioctl(cfd, CIOCFSESSION, &sess.ses)) {
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
