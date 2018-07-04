#ifndef KEYSTORE_CLIENT_ID
#define KEYSTORE_CLIENT_ID

#include <linux/types.h>

/**
 * DOC: Introduction
 *
 * The client operations include anything to do with user-space
 * client identification and (in the future) authentication.
 *
 */

/**
 * keystore_calc_clientid() - Derive the client ID
 *
 * @client_id: output array containing the client ID
 * @client_id_size: size of the output array
 * @timeout: timeout parameter for signed manifest caching
 * @caps: capabilities parameter for signed manifest verification
 *
 * Derive the client ID of a client from the path by
 * calculating SHA-256 on the directory name plus
 * file name.
 *
 * client_id = sha256( path_name + file_name )
 *
 * Returns: 0 if OK or negative error code.
 */
#ifdef CONFIG_APPLICATION_AUTH
int keystore_calc_clientid(u8 *client_id, const unsigned int client_id_size,
		int timeout, u16 caps);
#else
int keystore_calc_clientid(u8 *client_id, const unsigned int client_id_size);
#endif

/**
 * keystore_calc_clientkey() - Derive the client key
 *
 * @seed_type: which SEED to use to derive the key
 * @client_id: input array containing the client ID
 * @client_id_size: size of the input array
 * @client_key: output array to hold the client key
 * @client_key_size: requested size of the client key
 * @seed_index: which seed value to use from keystore_get_seed_list_entry()
 *
 * Derive the client key from the
 * client id and seed type:
 * client_key = hmac[sha256] ( seed_type, client_id )
 *
 * The SEED used will depend on the seed_index argument.
 *
 * Returns: 0 if OK or negative error code.
 *
 */
int keystore_calc_clientkey_prev(const enum keystore_seed_type seed_type,
				 const u8 *client_id,
				 unsigned int client_id_size,
				 u8 *client_key,
				 const unsigned int client_key_size,
				 unsigned int seed_index);

/**
 * keystore_calc_clientkey_prev() - Derive the client key with previous SVN
 *
 * @seed_type: which SEED to use to derive the key
 * @client_id: input array containing the client ID
 * @client_id_size: size of the input array
 * @client_key: output array to hold the client key
 * @client_key_size: requested size of the client key
 *
 * Derive the client key from the
 * client id and seed type:
 * client_key = hmac[sha256] ( seed, client_id )
 *
 * Returns: 0 if OK or negative error code.
 *
 */
int keystore_calc_clientkey(const enum keystore_seed_type seed_type,
			    const u8 *client_id,  unsigned int client_id_size,
			    u8 *client_key, const unsigned int client_key_size);




#endif /* KEYSTORE_CLIENT_ID */
