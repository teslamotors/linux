struct cipher_data
{
	int type; /* 1 synchronous, 2 async, 0 uninitialized */
	int blocksize;
	int ivsize;
	union {
		struct {
			struct crypto_blkcipher* s;
			struct blkcipher_desc desc;
		} blk;
		struct {
			struct crypto_ablkcipher* s;
			struct cryptodev_result *async_result;
			struct ablkcipher_request *async_request;
			uint8_t iv[EALG_MAX_BLOCK_LEN];
		} ablk;
	} u;
};

int cryptodev_cipher_init(struct cipher_data* out, const char* alg_name, __user uint8_t * key, size_t keylen);
void cryptodev_cipher_deinit(struct cipher_data* cdata);
ssize_t cryptodev_cipher_decrypt( struct cipher_data* cdata, struct scatterlist *sg1, struct scatterlist *sg2, size_t len);
ssize_t cryptodev_cipher_encrypt( struct cipher_data* cdata, struct scatterlist *sg1, struct scatterlist *sg2, size_t len);
void cryptodev_cipher_set_iv(struct cipher_data* cdata, void* iv, size_t iv_size);
