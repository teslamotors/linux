#ifndef KEYSTORE_CLIENT_ID
#define KEYSTORE_CLIENT_ID

#include <linux/types.h>

/**
 * Derive the client ID of a client from the path by
 * calculating SHA-256 on the directory name plus
 * file name.
 *
 * client_id = sha256( path_name + file_name )
 *
 * @param client_id output array containing the client ID
 * @param client_id_size size of the output array
 *
 * @return 0 if OK or negative error code.
 */
int keystore_calc_clientid(u8 *client_id, const unsigned int client_id_size);


/**
 * Device the client key from the client id and seed type
 *
 * client_key = hmac[sha256] ( seed_type, client_id )
 *
 * @param seed_type which SEED to use to derive the key
 * @param client_id input array containing the client ID
 * @param client_id_size size of the input array
 * @param client_key output array to hold the client key
 * @param client_key_size requested size of the client key
 *
 * @return 0 if OK or negative error code.
 */
int keystore_calc_clientkey(const enum keystore_seed_type seed_type,
			    const u8 *client_id,  unsigned int client_id_size,
			    u8 *client_key, const unsigned int client_key_size);




#endif /* KEYSTORE_CLIENT_ID */
