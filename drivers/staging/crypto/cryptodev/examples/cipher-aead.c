/*
 * Demo on how to use /dev/crypto device for ciphering.
 *
 * Placed under public domain.
 *
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <crypto/cryptodev.h>

#define	DATA_SIZE	(8*1024+11)
#define	BLOCK_SIZE	16
#define	KEY_SIZE	16

#define MAC_SIZE 20 /* SHA1 */

static int
get_sha1_hmac(int cfd, void* key, int key_size, void* data, int data_size, void* mac)
{
	struct session_op sess;
	struct crypt_op cryp;
	int i;

	memset(&sess, 0, sizeof(sess));
	memset(&cryp, 0, sizeof(cryp));

	sess.cipher = 0;
	sess.mac = CRYPTO_SHA1_HMAC;
	sess.mackeylen = key_size;
	sess.mackey = key;
	if (ioctl(cfd, CIOCGSESSION, &sess)) {
		perror("ioctl(CIOCGSESSION)");
		return 1;
	}

	/* Encrypt data.in to data.encrypted */
	cryp.ses = sess.ses;
	cryp.len = data_size;
	cryp.src = data;
	cryp.dst = NULL;
	cryp.iv = NULL;
	cryp.mac = mac;
	cryp.op = COP_ENCRYPT;
	if (ioctl(cfd, CIOCCRYPT, &cryp)) {
		perror("ioctl(CIOCCRYPT)");
		return 1;
	}

	/* Finish crypto session */
	if (ioctl(cfd, CIOCFSESSION, &sess.ses)) {
		perror("ioctl(CIOCFSESSION)");
		return 1;
	}

	return 0;
}

static void print_buf(char* desc, unsigned char* buf, int size)
{
int i;
	fputs(desc, stdout);
	for (i=0;i<size;i++) {
		printf("%.2x", (uint8_t)buf[i]);
	}
	fputs("\n", stdout);
}

static int
test_crypto(int cfd)
{
	char plaintext_raw[DATA_SIZE + 63], *plaintext;
	char ciphertext_raw[DATA_SIZE + 63], *ciphertext;
	char iv[BLOCK_SIZE];
	char key[KEY_SIZE];
	unsigned char sha1mac[20];
	int pad, i;

	struct session_op sess;
	struct crypt_op co;
	struct crypt_auth_op cao;
#ifdef CIOCGSESSINFO
	struct session_info_op siop;
#endif

	memset(&sess, 0, sizeof(sess));
	memset(&cao, 0, sizeof(cao));
	memset(&co, 0, sizeof(co));

	memset(key,0x33,  sizeof(key));
	memset(iv, 0x03,  sizeof(iv));

	/* Get crypto session for AES128 */
	sess.cipher = CRYPTO_AES_CBC;
	sess.keylen = KEY_SIZE;
	sess.key = key;

	sess.mac = CRYPTO_SHA1_HMAC;
	sess.mackeylen = 16;
	sess.mackey = (uint8_t*)"\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b";

	if (ioctl(cfd, CIOCGSESSION, &sess)) {
		perror("ioctl(CIOCGSESSION)");
		return 1;
	}

#ifdef CIOCGSESSINFO
	siop.ses = sess.ses;
	if (ioctl(cfd, CIOCGSESSINFO, &siop)) {
		perror("ioctl(CIOCGSESSINFO)");
		return 1;
	}
	printf("requested cipher CRYPTO_AES_CBC/HMAC-SHA1, got %s with driver %s\n",
			siop.cipher_info.cra_name, siop.cipher_info.cra_driver_name);

	plaintext = (char *)(((unsigned long)plaintext_raw + siop.alignmask) & ~siop.alignmask);
	ciphertext = (char *)(((unsigned long)ciphertext_raw + siop.alignmask) & ~siop.alignmask);
#else
	plaintext = plaintext_raw;
	ciphertext = ciphertext_raw;
#endif
	memset(plaintext, 0x15, DATA_SIZE);

	if (get_sha1_hmac(cfd, sess.mackey, sess.mackeylen, plaintext, DATA_SIZE, sha1mac) != 0) {
		fprintf(stderr, "SHA1 MAC failed\n");
		return 1;
	}

	memcpy(ciphertext, plaintext, DATA_SIZE);

	/* Encrypt data.in to data.encrypted */
	cao.ses = sess.ses;
	cao.len = DATA_SIZE;
	cao.src = ciphertext;
	cao.dst = ciphertext;
	cao.iv = iv;
	cao.op = COP_ENCRYPT;
	cao.flags = COP_FLAG_AEAD_TLS_TYPE;

	if (ioctl(cfd, CIOCAUTHCRYPT, &cao)) {
		perror("ioctl(CIOCAUTHCRYPT)");
		return 1;
	}

	printf("Original plaintext size: %d, ciphertext: %d\n", DATA_SIZE, cao.len);

	if (ioctl(cfd, CIOCFSESSION, &sess.ses)) {
		perror("ioctl(CIOCFSESSION)");
		return 1;
	}

	/* Get crypto session for AES128 */
	memset(&sess, 0, sizeof(sess));
	sess.cipher = CRYPTO_AES_CBC;
	sess.keylen = KEY_SIZE;
	sess.key = key;

	if (ioctl(cfd, CIOCGSESSION, &sess)) {
		perror("ioctl(CIOCGSESSION)");
		return 1;
	}

	/* Decrypt data.encrypted to data.decrypted */
	co.ses = sess.ses;
	co.len = cao.len;
	co.src = ciphertext;
	co.dst = ciphertext;
	co.iv = iv;
	co.op = COP_DECRYPT;
	if (ioctl(cfd, CIOCCRYPT, &co)) {
		perror("ioctl(CIOCCRYPT)");
		return 1;
	}

	/* Verify the result */
	if (memcmp(plaintext, ciphertext, DATA_SIZE) != 0) {
		int i;
		fprintf(stderr,
			"FAIL: Decrypted data are different from the input data.\n");
		printf("plaintext:");
		for (i = 0; i < DATA_SIZE; i++) {
			if ((i % 30) == 0)
				printf("\n");
			printf("%02x ", plaintext[i]);
		}
		printf("ciphertext:");
		for (i = 0; i < DATA_SIZE; i++) {
			if ((i % 30) == 0)
				printf("\n");
			printf("%02x ", ciphertext[i]);
		}
		printf("\n");
		return 1;
	}

	if (memcmp(&ciphertext[cao.len-MAC_SIZE-1], sha1mac, 20) != 0) {
		fprintf(stderr, "AEAD SHA1 MAC does not match plain MAC\n");
		print_buf("SHA1: ", sha1mac, 20);
		print_buf("SHA1-TLS: ", &ciphertext[cao.len-MAC_SIZE-1], 20);
		return 1;
	}

	pad = ciphertext[cao.len-1];

	for (i=0;i<pad;i++)
		if (ciphertext[cao.len-MAC_SIZE-1-i] != pad) {
			fprintf(stderr, "Pad does not match (expected %d)\n", pad);
			print_buf("PAD", &ciphertext[cao.len-MAC_SIZE-1-pad], pad);
			return 1;
		}

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

