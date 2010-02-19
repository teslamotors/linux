/*
 * Demo on how to use /dev/crypto device for ciphering.
 *
 * Placed under public domain.
 *
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <crypto/cryptodev.h>

#define	DATA_SIZE	4096
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

	memset(&sess, 0, sizeof(sess));
	memset(&cryp, 0, sizeof(cryp));

	memset(&plaintext, 0x15,  sizeof(plaintext));
	memset(&key, 0x33,  sizeof(key));
	memset(&iv, 0x03,  sizeof(iv));

	/* Get crypto session for AES128 */
	sess.cipher = CRYPTO_AES_CBC;
	sess.keylen = KEY_SIZE;
	sess.key = key;
	if (ioctl(cfd, CIOCGSESSION, &sess)) {
		perror("ioctl(CIOCGSESSION)");
		return 1;
	}

	/* Encrypt data.in to data.encrypted */
	cryp.ses = sess.ses;
	cryp.len = sizeof(plaintext);
	cryp.src = plaintext;
	cryp.dst = ciphertext;
	cryp.iv = iv;
	cryp.op = COP_ENCRYPT;
	if (ioctl(cfd, CIOCCRYPT, &cryp)) {
		perror("ioctl(CIOCCRYPT)");
		return 1;
	}

	if (ioctl(cfd, CIOCGSESSION, &sess)) {
		perror("ioctl(CIOCGSESSION)");
		return 1;
	}
	
	/* Decrypt data.encrypted to data.decrypted */
	cryp.ses = sess.ses;
	cryp.len = sizeof(plaintext);
	cryp.src = ciphertext;
	cryp.dst = ciphertext;
	cryp.iv = iv;
	cryp.op = COP_DECRYPT;
	if (ioctl(cfd, CIOCCRYPT, &cryp)) {
		perror("ioctl(CIOCCRYPT)");
		return 1;
	}

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
