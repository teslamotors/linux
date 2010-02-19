/* cipher stuff */

#ifndef CRYPTODEV_INT_H
# define CRYPTODEV_INT_H

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#define PFX "cryptodev: "
#define dprintk(level,severity,format,a...)			\
	do { 							\
		if (level <= cryptodev_verbosity)				\
			printk(severity PFX "%s[%u]: " format,	\
			       current->comm, current->pid,	\
			       ##a);				\
	} while (0)

extern int cryptodev_verbosity;

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

/* hash stuff */
struct hash_data
{
	int type; /* 1 synchronous, 2 async, 0 uninitialized */
	int digestsize;
	union {
		struct {
			struct crypto_hash* s;
			struct hash_desc desc;
		} sync;
		struct {
			struct crypto_ahash *s;
			struct cryptodev_result *async_result;
			struct ahash_request *async_request;
		} async;
	} u;
	
};

int cryptodev_hash_final( struct hash_data* hdata, void* output);
ssize_t cryptodev_hash_update( struct hash_data* hdata, struct scatterlist *sg, size_t len);
int cryptodev_hash_reset( struct hash_data* hdata);
void cryptodev_hash_deinit(struct hash_data* hdata);
int cryptodev_hash_init( struct hash_data* hdata, const char* alg_name, int hmac_mode, __user void* mackey, size_t mackeylen);

#endif /* CRYPTODEV_INT_H */
