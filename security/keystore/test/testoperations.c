#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/dcache.h>

#include <security/keystore_api_common.h>

#include "keystore_operations.h"
#include "keystore_tests.h"
#include "keystore_rand.h"
#include "keystore_context.h"
#include "keystore_context_safe.h"
#include "keystore_debug.h"
#include "keystore_ecc.h"
#include "keystore_aes.h"

int keystore_test_genkey(void)
{
	int res = 0;
	void *app_key = NULL;
	unsigned int app_key_size;
	enum keystore_key_spec spec = KEYSPEC_LENGTH_256;

	app_key = generate_new_key(spec, &app_key_size);

	if (keystore_assert(app_key != NULL))
		goto exit;
	if (keystore_assert(app_key_size == 32))
		goto free_key;

free_key:
	kfree(app_key);
exit:
	return res;
}

int keystore_test_wrap_unwrap(void)
{
	int res = 0;
	uint8_t client_key[KEYSTORE_CLIENT_KEY_SIZE];
	void *app_key = NULL;
	unsigned int app_key_size;

	uint8_t *wrapped_key = NULL;
	unsigned int wrapped_size;

	uint8_t *unwrapped_key = NULL;
	unsigned int unwrapped_size;

	enum keystore_key_spec spec = KEYSPEC_LENGTH_256;
	enum keystore_key_spec unwrap_spec = 0;

	/* Generate a fake client key */
	keystore_get_rdrand(client_key, sizeof(client_key));

	app_key = generate_new_key(spec, &app_key_size);
	if (keystore_assert(app_key != NULL))
		goto exit;

	res = wrapped_key_size(app_key_size, &wrapped_size);
	if (keystore_assert(res == 0))
		goto free_key;

	wrapped_key = kmalloc(wrapped_size, GFP_KERNEL);
	if (keystore_assert(wrapped_key != NULL))
		goto free_key;

	res = wrap_key(client_key, app_key, app_key_size, spec, wrapped_key);
	if (keystore_assert(res == 0))
		goto free_key;

	res = unwrapped_key_size(wrapped_size, &unwrapped_size);
	if (keystore_assert(res == 0))
		goto free_key;
	if (keystore_assert(app_key_size == unwrapped_size))
		goto free_key;

	unwrapped_key = kmalloc(unwrapped_size, GFP_KERNEL);
	if (keystore_assert(unwrapped_key != NULL))
		goto free_key;

	res = unwrap_key(client_key, wrapped_key, wrapped_size,
			 &unwrap_spec, unwrapped_key);
	if (keystore_assert(res == 0))
		goto free_key;
	if (keystore_assert(
		    memcmp(app_key, unwrapped_key, app_key_size) == 0)) {
		res = -1;
		goto free_key;
	}

free_key:
	kzfree(app_key);
	kzfree(wrapped_key);
	kzfree(unwrapped_key);
exit:
	return res;
}

int keystore_test_encrypt_decrypt(void)
{
	int res = 0;
	enum keystore_algo_spec algo_spec = ALGOSPEC_AES;
	size_t m_len = 32;
	uint8_t m[m_len];
	uint8_t key[AES128_KEY_SIZE];
	uint8_t iv[KEYSTORE_AES_IV_SIZE];
	uint8_t *c = NULL;
	unsigned int c_len = 0;
	uint8_t *m2 = NULL;
	unsigned int m2_len = 0;

	res = keystore_get_rdrand(iv, sizeof(iv));
	keystore_hexdump("IV", iv, sizeof(iv));
	if (keystore_assert(res == 0))
		return res;
	/* In CCM mode, the first byte of the IV has special meaning         */
	/* In rfc3610 the IV is called the nonce                             */
	/* Flags = 64 * Adata + 8 * M' + L'                                  */
	/* Adata = 0 if l(a) = 0, 1 otherwise (length of authentication data)*/
	/* M' = (M-2)/2 (M is authentication length in bytes)                */
	/* L' = L - 1   (nonce length in bytes)                              */
	/* No additional data, 2 byte authentication field, 2 byte nonce     */
	iv[0] = 1;

	res = keystore_get_rdrand(m, sizeof(m));
	keystore_hexdump("Plain data", m, m_len);
	if (keystore_assert(res == 0))
		return res;

	res = keystore_get_rdrand(key, sizeof(key));
	keystore_hexdump("Key", key, sizeof(key));
	if (keystore_assert(res == 0))
		return res;

	res = encrypt_output_size(algo_spec, m_len, &c_len);
	if (keystore_assert(res == 0))
		return res;

	c = kmalloc(c_len, GFP_KERNEL);
	if (keystore_assert(c != NULL))
		return res;
	memset(c, 0, c_len);

	res = do_encrypt(algo_spec, key, sizeof(key),
			 iv, sizeof(iv), m, m_len, c);

	keystore_hexdump("Encrypted data", c, c_len);

	if (keystore_assert(res == 0))
		goto free_mem;

	res = decrypt_output_size(algo_spec, c_len, &m2_len);
	if (keystore_assert(res == 0) || keystore_assert(m2_len == m_len)) {
		res = -1;
		goto free_mem;
	}

	m2 = kmalloc(m2_len, GFP_KERNEL);
	if (keystore_assert(c != NULL))
		goto free_mem;

	res = do_decrypt(algo_spec, key, sizeof(key),
			 iv, sizeof(iv), c, c_len, m2);
	if (keystore_assert(res == 0))
		goto free_mem;

	res = keystore_assert(memcmp(m, m2, m_len) == 0);

free_mem:
	kzfree(c);
	kzfree(m2);
	return res;
}

int keystore_test_encrypt_for_host(void)
{
	int res = 0;
	struct ias_keystore_ecc_keypair key_pair;
	uint8_t random_seed[64];
	struct keystore_backup_data backup;
	uint8_t backup_enc[KEYSTORE_BACKUP_SIZE];
	struct keystore_backup_data backup_dec;

	res = keystore_get_rdrand(random_seed, sizeof(random_seed));
	if (keystore_assert(res == 0))
		return res;
	res = keystore_ecc_gen_keys(random_seed, sizeof(random_seed),
				    &key_pair);
	if (keystore_assert(res == 0))
		return res;

	res = encrypt_for_host(&key_pair.public_key, &backup, sizeof(backup),
			       backup_enc, KEYSTORE_BACKUP_SIZE);
	if (keystore_assert(res == 0))
		return res;

	res = decrypt_from_target(key_pair.private_key,
				  backup_enc, KEYSTORE_BACKUP_SIZE,
				  &backup_dec, sizeof(backup_dec));
	if (keystore_assert(res == 0))
		return res;

	res = keystore_assert(memcmp(&backup, &backup_dec,
				     sizeof(backup)) == 0);

	return res;
}
