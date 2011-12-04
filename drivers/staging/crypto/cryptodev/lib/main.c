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
#include <openssl/sha.h>
#include <openssl/aes.h>
#include "hash.h"

void sha_hash(void* text, int size, void* digest)
{
SHA_CTX ctx;

	SHA_Init(&ctx);
	
	SHA_Update(&ctx, text, size);
	
	SHA_Final(digest, &ctx);
}

void aes_sha_combo(void* ctx, void* plaintext, void* ciphertext, int size, void* tag)
{
SHA_CTX sha_ctx;
uint8_t iv[16];
AES_KEY* key = ctx;

	SHA_Init(&sha_ctx);
	
	SHA_Update(&sha_ctx, plaintext, size);
	
	SHA_Final(tag, &sha_ctx);
	
	AES_cbc_encrypt(plaintext, ciphertext, size, key, iv, 1);
}

int main()
{
int ret;
AES_KEY key;
uint8_t ukey[16];

	ret = hash_test(CRYPTO_SHA1, sha_hash);
	if (ret > 0)
		printf("SHA1 in kernel outperforms user-space after %d input bytes\n", ret);
	
	memset(ukey, 0xaf, sizeof(ukey));
	AES_set_encrypt_key(ukey, 16, &key);
	
	ret = aead_test(CRYPTO_AES_CBC, CRYPTO_SHA1, ukey, 16, &key, aes_sha_combo);
	if (ret > 0)
		printf("AES-SHA1 in kernel outperforms user-space after %d input bytes\n", ret);
	
	return 0;
}
