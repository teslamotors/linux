#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/dcache.h>

#include <security/keystore_api_kernel.h>
#include <security/oem_key.h>

#include "keystore_constants.h"
#include "keystore_operations.h"
#include "keystore_context.h"
#include "keystore_debug.h"
#include "keystore_seed.h"

#include "keystore_ecc.h"
#include "keystore_aes.h"
#include "keystore_mac.h"
#include "keystore_rand.h"

#if (RSA_SIGNATURE_SIZE != RSA_SIGNATURE_BYTE_SIZE)
#error RSA_SIGNATURE_BYTE_SIZE set to wrong value in keystore_api_common.h
#endif

static int keysize_is_valid(unsigned int keysize)
{
	return ((keysize == AES128_KEY_SIZE) ||
		(keysize == AES256_KEY_SIZE));
}

static int wrapped_keysize_is_valid(unsigned int wrapped_keysize)
{
	unsigned int keysize = 0;
	int res = 0;

	res = unwrapped_key_size(wrapped_keysize, &keysize);
	if (res)
		return -EINVAL;

	return ((keysize == AES128_KEY_SIZE) ||
		(keysize == AES256_KEY_SIZE));
}

static int keyspec_is_valid(enum keystore_key_spec keyspec)
{
	switch (keyspec) {
	case KEYSPEC_LENGTH_256:
	case KEYSPEC_LENGTH_128:
		return 1;
	default:
		return 0;
	}
}

static int keyspec_to_keysize(enum keystore_key_spec keyspec,
			      unsigned int *keysize)
{
	int res = 0;

	if (!keysize)
		return -EFAULT;

	switch (keyspec) {
	case KEYSPEC_LENGTH_128:
		*keysize = AES128_KEY_SIZE;
		break;
	case KEYSPEC_LENGTH_256:
		*keysize = AES256_KEY_SIZE;
		break;
	default:
		*keysize = 0;
		return -EINVAL;
	}

	return res;
}

int wrapped_key_size(unsigned int unwrapped_size, unsigned int *wrapped_size)
{
	if (!wrapped_size)
		return -EFAULT;

	*wrapped_size =  unwrapped_size + AES_BLOCK_SIZE +
		sizeof(uint8_t);

	return 0;
}

int unwrapped_key_size(unsigned int wrapped_size, unsigned int *unwrapped_size)
{
	if (!unwrapped_size)
		return -EFAULT;

	if (wrapped_size < AES_BLOCK_SIZE - sizeof(uint8_t)) {
		ks_err(KBUILD_MODNAME ": Error wrapped_size (%u) too small (< %u - %lu = %lu)\n",
		       wrapped_size,
		       AES_BLOCK_SIZE,
		       sizeof(uint8_t),
		       AES_BLOCK_SIZE -
		       sizeof(uint8_t));
		return -EINVAL;
	}

	*unwrapped_size = wrapped_size - AES_BLOCK_SIZE -
		sizeof(uint8_t);

	return 0;
}

int keyspec_to_wrapped_keysize(enum keystore_key_spec keyspec,
			       unsigned int *keysize)
{
	int res = 0;
	unsigned int unwrapped_size;

	res = keyspec_to_keysize(keyspec, &unwrapped_size);
	if (res)
		return res;

	res = wrapped_key_size(unwrapped_size, keysize);

	return res;
}

uint8_t *generate_new_key(enum keystore_key_spec keyspec,
			  unsigned int *app_key_size)
{
	int res = 0;
	uint8_t *app_key = NULL;

	if (!app_key_size)
		return NULL;

	/* Generate new random key */
	res = keyspec_to_keysize(keyspec, app_key_size);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Invalid keyspec.\n", __func__);
		return NULL;
	}

	app_key = kmalloc(*app_key_size, GFP_KERNEL);
	if (!app_key)
		return NULL;

	keystore_get_rdrand(app_key, (*app_key_size));

	return app_key;
}

int wrap_key(const uint8_t *client_key,
	     const uint8_t *app_key,
	     unsigned int app_key_size,
	     const enum keystore_key_spec keyspec,
	     uint8_t *wrapped_key)
{
	int res = 0;
	uint8_t *key;
	unsigned int key_size;
	unsigned int wrapped_key_size;

	/* Prepare app key */
	if (!client_key || !app_key || !wrapped_key)
		return -EFAULT;

	if (!keysize_is_valid(app_key_size) || !keyspec_is_valid(keyspec))
		return -EINVAL;

	res = keyspec_to_wrapped_keysize(keyspec, &wrapped_key_size);
	if (res)
		return res;

	key_size = sizeof(uint8_t) + app_key_size;
	key = kmalloc(key_size, GFP_KERNEL);
	if (!key)
		return -ENOMEM;

	/* prepare key for wrapping */
	key[0] = (uint8_t)keyspec;
	memcpy(key + sizeof(uint8_t), app_key, app_key_size);

	/* wrappedKey = KENC(key:ClientKey, data:KeySpec||NewKey) */
	res = keystore_aes_siv_crypt(1, client_key, KEYSTORE_CLIENT_KEY_SIZE,
				     key, key_size,
				     NULL, 0,
				     wrapped_key, wrapped_key_size);
	memset(key, 0, key_size);
	kfree(key);
	return res;
}

int unwrap_key(const uint8_t *client_key,
	       const uint8_t *wrapped_key,
	       unsigned int wrapped_key_size,
	       enum keystore_key_spec *keyspec,
	       uint8_t *app_key)
{
	int res = 0;
	uint8_t *key = NULL;
	unsigned int key_size;
	unsigned int app_key_size = 0;

	if (!client_key || !wrapped_key ||
	    !keyspec || !app_key)
		return -EFAULT;

	if (!wrapped_keysize_is_valid(wrapped_key_size)) {
		ks_err(KBUILD_MODNAME ": %s: Invalid wrapped key size\n",
		       __func__);
		return -EINVAL;
	}

	res = unwrapped_key_size(wrapped_key_size, &app_key_size);
	if (res)
		return -EINVAL;

	key_size = app_key_size + sizeof(uint8_t);
	key = kmalloc(key_size, GFP_KERNEL);
	if (!key)
		return -ENOMEM;

	/* decrypt AppKey||KeySpec = KDEC(key:ClientKey, data:wrappedKey) */
	res = keystore_aes_siv_crypt(0, client_key,
				     KEYSTORE_CLIENT_KEY_SIZE,
				     wrapped_key, wrapped_key_size,
				     NULL, 0,
				     key, key_size);

	if (res)
		goto free_key;

	/* Copy to output */
	*keyspec = key[0];
	memcpy(app_key, key + sizeof(uint8_t), app_key_size);

	if (!keyspec_is_valid(*keyspec))
		res = -EINVAL;

	memset(key, 0, key_size);
free_key:
	kfree(key);
	return res;
}

int encrypt_output_size(enum keystore_algo_spec algo_spec,
			unsigned int input_size,
			unsigned int *output_size)
{
	if (!output_size)
		return -EFAULT;

	*output_size = 0;

	switch (algo_spec) {
	case ALGOSPEC_AES:
		*output_size = input_size + KEYSTORE_CCM_AUTH_SIZE;
		break;
	default:
		ks_err(KBUILD_MODNAME "Unknown algo_spec: %u\n", algo_spec);
		return -EINVAL;
	}

	return 0;
}

int decrypt_output_size(enum keystore_algo_spec algo_spec,
			unsigned int input_size,
			unsigned int *output_size)
{
	if (!output_size)
		return -EFAULT;

	*output_size = 0;

	switch (algo_spec) {
	case ALGOSPEC_AES:
		if (input_size < KEYSTORE_CCM_AUTH_SIZE)
			return -EINVAL;
		*output_size = input_size - KEYSTORE_CCM_AUTH_SIZE;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int do_encrypt(enum keystore_algo_spec algo_spec,
	       const void *app_key, unsigned int app_key_size,
	       const void *iv, unsigned int iv_size,
	       const void *input, unsigned int input_size,
	       void *output)
{
	int res = 0;
	unsigned int output_size;

	/* Check inputs */
	if (!app_key || !iv || !input || !output)
		return -EFAULT;

	if (!input_size || (algo_spec != ALGOSPEC_AES) ||
	    (iv_size != KEYSTORE_AES_IV_SIZE)) {
		ks_err(KBUILD_MODNAME ": Incorrect input values to %s\n",
		       __func__);
		return -EINVAL;
	}

	res = encrypt_output_size(algo_spec, input_size, &output_size);
	if (res)
		return res;

	/* Cipher = AENC(AppKey, AlgoSpec, IV, Plaintext) */
	res = keystore_aes_ccm_crypt(1, app_key, app_key_size, iv, iv_size,
				     input, input_size, NULL, 0,
				     output, output_size);
	return res;
}

int do_decrypt(enum keystore_algo_spec algo_spec,
	       const void *app_key, unsigned int app_key_size,
	       const void *iv, unsigned int iv_size,
	       const void *input, unsigned int input_size,
	       void *output)
{
	int res = 0;
	unsigned int output_size;

	/* Check inputs */
	if (!app_key || !input || !output || !iv)
		return -EFAULT;

	if (!input_size || (algo_spec != ALGOSPEC_AES) ||
	    (iv_size != KEYSTORE_AES_IV_SIZE)) {
		ks_err(KBUILD_MODNAME ": Incorrect input values to %s\n",
		       __func__);
		return -EINVAL;
	}

	res = decrypt_output_size(algo_spec, input_size, &output_size);
	if (res)
		return res;

	/* decrypt Cipher using AppKey and AlgoSpec/IV */
	/* Plaintext = ADEC(AppKey, AlgoSpec, IV, Cipher) */
	res = keystore_aes_ccm_crypt(0, app_key, app_key_size, iv, iv_size,
				     input, input_size,
				     NULL, 0, output, output_size);

	return res;
}

int verify_oem_signature(const void *data, unsigned int data_size,
			 const void *sig, unsigned int sig_size)
{
	int res = 0;
	unsigned char digest[SHA256_HMAC_SIZE];

	if (!data || !sig)
		return -EFAULT;

	if (sig_size != RSA_SIGNATURE_BYTE_SIZE)
		return -EINVAL;

	/* calculate ECC key digest */
	res = keystore_sha256_block(data, data_size, digest, sizeof(digest));
	if (res < 0) {
		ks_err(KBUILD_MODNAME ": keystore_sha256_block() error %d in %s\n",
		       res, __func__);
		return res;
	}

	keystore_hexdump("Digest", digest, sizeof(digest));
	keystore_hexdump("Signature", sig, sig_size);

	/* verify public key signature */
	res = oem_key_verify_digest(digest, sizeof(digest),
				    sig, sig_size);

	return res;
}

/* TODO: remove need to specify sizes here */
int encrypt_for_host(const struct keystore_ecc_public_key *oem_pub,
		     const void *in, unsigned int in_size,
		     void *out, unsigned int out_size)
{
	int res = 0;

	FUNC_BEGIN;

	if (!in || !out || !oem_pub)
		return -EFAULT;

	/* Encrypt the backup with the backup ECC key */
	res = keystore_ecc_encrypt(oem_pub,
				   in, in_size,
				   out, out_size);
	FUNC_RES(res);
	return res;
}

/* TODO: Remove need to specify size here */
int decrypt_from_target(const uint32_t *oem_priv,
			const void *in, unsigned int in_size,
			void *out, unsigned int out_size)
{
	int res = 0;

	FUNC_BEGIN;

	if (!oem_priv || !in || !out)
		return -EFAULT;

	res = keystore_ecc_decrypt(oem_priv,
				   in, in_size,
				   out, out_size);
	FUNC_RES(res);
	return res;
}

int keystore_get_ksm_keypair(struct ias_keystore_ecc_keypair *key_pair)
{
	int res = 0;

	FUNC_BEGIN;

	if (!key_pair)
		return -EFAULT;

	res = keystore_ecc_gen_keys(sec_seed[SEED_TYPE_DEVICE], SEC_SEED_SIZE,
				    key_pair);

	keystore_hexdump("KSM public", &key_pair->public_key,
			 sizeof(struct keystore_ecc_public_key));
	keystore_hexdump("KSM private", key_pair->private_key,
			 KEYSTORE_ECC_PRIV_KEY_SIZE);

	FUNC_RES(res);

	return res;
}

int sign_for_host(const void *data, unsigned int data_size,
		  struct keystore_ecc_signature *signature)
{
	int res = 0;
	struct ias_keystore_ecc_keypair key_pair;

	if (!data || !signature)
		return -EFAULT;

	/* Calculate the Keystore internal ECC keys */
	res = keystore_get_ksm_keypair(&key_pair);

	if (res) {
		ks_err(KBUILD_MODNAME ": Error retrieving KSM ECC keys.\n");
		return res;
	}

	/* Sign the encrypted backup blob with the Keystore ECC key */
	res = keystore_ecc_sign(key_pair.private_key,
				data, data_size, signature);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Error %d in keystore_ecc_sign\n",
		       __func__, res);
		goto zero_buf;
	}

	/* Self-check of the signature */
	res = keystore_ecc_verify(&key_pair.public_key,
				  data, data_size, signature);


zero_buf:
	memset(&key_pair, 0, sizeof(key_pair)); /* empty */
	return res;
}

int generate_migration_key(const void *mkey_nonce, uint8_t *mkey)
{
	int res = 0;

	if (!mkey || !mkey_nonce)
		return -EFAULT;

	memset(mkey, 0, KEYSTORE_MKEY_SIZE);
	res = keystore_calc_mac("hmac(sha256)",
				(const char *)&sec_seed[0][0],
				SEC_SEED_SIZE,
				mkey_nonce,
				KEYSTORE_MKEY_SIZE,
				mkey,
				KEYSTORE_MKEY_SIZE);

	keystore_hexdump("migration - MKEY", mkey, KEYSTORE_MKEY_SIZE);

	return res;
}

int wrap_backup(const uint8_t *mkey,
		const void *input, unsigned int input_size,
		void *output, unsigned int output_size)
{
	int res = 0;
	uint8_t *p_reencrypted;
	uint8_t iv[KEYSTORE_MAX_IV_SIZE];

	FUNC_BEGIN;

	if (!mkey || !input || !output || !output_size)
		return -EFAULT;

	if (output_size <
	    1 + KEYSTORE_MAX_IV_SIZE + input_size + KEYSTORE_GCM_AUTH_SIZE) {
		ks_err(KBUILD_MODNAME ": %s: Error %u, %u fix output size\n",
		       __func__, input_size + 1 + KEYSTORE_MAX_IV_SIZE +
		       KEYSTORE_GCM_AUTH_SIZE, output_size);
		FUNC_RES(res);
		return -EINVAL;
	}

	/* Generate a random IV */
	res = keystore_get_rdrand(iv, KEYSTORE_MAX_IV_SIZE);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Error generating random number\n",
		       __func__);
		FUNC_RES(res);
		return res;
	}

	/* Encrypt the backup data with the Migration Key */
	p_reencrypted = output;
	/* prepare block to be returned */
	/* AlgoSpec || IV || Cipher */
	/* and algo spec at 0 possition */
	*p_reencrypted++ = (uint8_t)ALGOSPEC_AES;
	/* copy iv to 1...iv_size */
	memcpy(p_reencrypted, iv, sizeof(iv));
	p_reencrypted += sizeof(iv);

	/* Cipher = AENC(AppKey, AlgoSpec, IV, Plaintext) */
	res = keystore_aes_gcm_crypt(1, mkey, KEYSTORE_MKEY_SIZE,
				     iv, sizeof(iv),
				     input, input_size,
				     NULL, 0,
				     p_reencrypted,
				     input_size +
				     KEYSTORE_GCM_AUTH_SIZE);

	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Error %d in keystore_aes_gcm_crypt\n",
		       __func__, res);
	}

	FUNC_RES(res);
	return res;
}

int unwrap_backup(const uint8_t *mkey,
		  const void *input, unsigned int input_size,
		  void *output, unsigned int output_size)
{
	int res = 0;
	uint8_t *p_backup = NULL;
	uint8_t *p_iv_dec = NULL;
	unsigned int backup_data_size;

	FUNC_BEGIN;

	if (!mkey || !input || !output)
		return -EFAULT;

	if (output_size <
	    input_size - 1 - KEYSTORE_MAX_IV_SIZE - KEYSTORE_GCM_AUTH_SIZE) {
		ks_err(KBUILD_MODNAME ": %s: Error %u, %u fix output size\n",
		       __func__, input_size + 1 + KEYSTORE_MAX_IV_SIZE +
		       KEYSTORE_GCM_AUTH_SIZE, output_size);
		return -EINVAL;
	}

	/* decrypt Cipher using AppKey and AlgoSpec/IV */
	/* Plaintext = ADEC(AppKey, AlgoSpec, IV, Cipher) */
	p_backup = (uint8_t *)input;
	p_iv_dec = ++p_backup;/*skip algo spec */
	p_backup += KEYSTORE_MAX_IV_SIZE;/*skip iv */
	backup_data_size =  input_size - 1 -
		KEYSTORE_MAX_IV_SIZE;/*correct size */

	res = keystore_aes_gcm_crypt(0, mkey, KEYSTORE_MKEY_SIZE,
				     p_iv_dec, KEYSTORE_MAX_IV_SIZE,
				     p_backup, backup_data_size,
				     NULL, 0,
				     output,
				     backup_data_size -
				     KEYSTORE_GCM_AUTH_SIZE);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Error %d in keystore_aes_gcm_crypt\n",
		       __func__, res);
		return -EINVAL;
	}

	FUNC_RES(res);
	return res;
}
