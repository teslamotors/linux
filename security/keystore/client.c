#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <linux/fdtable.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/fs_struct.h>
#include <linux/path.h>
#include <asm-generic/current.h>
#include <linux/dcache.h>

#include <security/keystore_api_kernel.h>
#include "keystore_client.h"
#include "keystore_mac.h"
#include "keystore_seed.h"
#include "keystore_debug.h"

#define KERNEL_CLIENTS_ID			"+(!$(%@#%$$)*"

/**
 * Get the absolute path of current process in the filesystem. This is used
 * for client authentication purpose.
 *
 * Always use the return variable from this function, input variable 'buf'
 * may not contain the start of the path. In case if kernel was executing
 * a kernel thread then this function return NULL.
 *
 * @param input buf place holder for updating the path
 * @param input buflen avaialbe space
 *
 * @return path of current process, or NULL if it was kernel thread.
 */

static char *get_current_process_path(char *buf, int buflen)
{
	struct file *exe_file = NULL;
	char *result = NULL;
	struct mm_struct *mm = NULL;

	mm = get_task_mm(current);
	if (!mm) {
		ks_info(KBUILD_MODNAME ": %s error get_task_mm\n", __func__);
		goto out;
	}

	down_read(&mm->mmap_sem);
	exe_file = mm->exe_file;

	if (exe_file)
		path_get(&exe_file->f_path);

	up_read(&mm->mmap_sem);
	mmput(mm);
	if (exe_file) {
		result = d_path(&exe_file->f_path, buf, buflen);
		path_put(&exe_file->f_path);
	}
out:
	return result;
}

int keystore_calc_clientid(u8 *client_id, const unsigned int client_id_size)
{
	char *buf = NULL, *f_path = NULL;
	int res = 0;

	if (!client_id)
		return -EINVAL;

	/* alloc mem for pwd##app, use linux defined limits */
	buf = kmalloc(PATH_MAX + NAME_MAX, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	/* clear the buf */
	memset(buf, 0, PATH_MAX + NAME_MAX);

	f_path = get_current_process_path(buf, (PATH_MAX + NAME_MAX));

	ks_debug(KBUILD_MODNAME ": %s KSM-Client ABS path: %s\n",
		 __func__, f_path);

	if (f_path && IS_ERR(f_path)) {
		/* error case, do not register */
		ks_err(KBUILD_MODNAME ": Cannot register with keystore - failed client auth\n");
		res = -EFAULT;
		goto out_buf;
	}

	/* f_path is NULL for PF_KTHREAD type */
	if (!f_path) {
		ks_info(KBUILD_MODNAME ": %s Kernel client - use default.\n",
			__func__);
		f_path = KERNEL_CLIENTS_ID;
	}

	/* Clear the output buffer */
	memset(client_id, 0, sizeof(u8) * client_id_size);

	/* calculate sha2 on new cwd - this is new clientid! */
	keystore_sha256_block(f_path, strlen(f_path),
			      client_id,
			      client_id_size);

out_buf:
	kfree(buf);
	return res;
}

int keystore_calc_clientkey(const enum keystore_seed_type seed_type,
			    const u8 *client_id, unsigned int client_id_size,
			    u8 *client_key, const unsigned int client_key_size)
{
	int res = 0;
	const uint8_t *seed = NULL;

	if (!client_key || !client_id)
		return -EFAULT;

	seed = keystore_get_seed(seed_type);
	if (!seed) {
		ks_err(KBUILD_MODNAME ": %s: Seed type %d invalid or not available.\n",
			       __func__, seed_type);
		return -EINVAL;
	}

	/* calculate KDF(key = SEED, data = ClientID) */
	res = keystore_calc_mac("hmac(sha256)",
				seed, SEC_SEED_SIZE,
				client_id,  client_id_size,
				client_key, client_key_size);

	return res;
}
