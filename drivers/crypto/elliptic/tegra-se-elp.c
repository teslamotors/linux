/*
 * Cryptographic API.
 * drivers/crypto/tegra-se-elp.c
 *
 * Support for Tegra Security Engine hardware crypto algorithms.
 *
 * Copyright (c) 2015-2017, NVIDIA Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/clk/tegra.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/tegra-soc.h>
#include <crypto/scatterwalk.h>
#include <crypto/algapi.h>
#include <crypto/aes.h>
#include <crypto/internal/rng.h>
#include <crypto/internal/hash.h>
#include <crypto/sha.h>
#include <linux/tegra_pm_domains.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#include "tegra-se-elp.h"

#define DRIVER_NAME	"tegra-se-elp"

#define PKA1	0
#define RNG1	1

#define TEGRA_SE_MUTEX_WDT_UNITS	0x600000
#define RNG1_TIMEOUT			2000	/*micro seconds*/
#define RAND_128			16	/*bytes*/
#define RAND_256			32	/*bytes*/
#define ADV_STATE_FREQ			3

enum tegra_se_pka_rsa_type {
	RSA_EXP_MOD,
	RSA_CRT_KEY_SETUP,
	RSA_CRT,
};

enum tegra_se_pka_ecc_type {
	ECC_POINT_MUL,
	ECC_POINT_ADD,
	ECC_POINT_DOUBLE,
	ECC_POINT_VER,
	ECC_SHAMIR_TRICK,
};

enum tegra_se_elp_precomp_vals {
	PRECOMP_RINV,
	PRECOMP_M,
	PRECOMP_R2,
};

/* Security Engine operation modes */
enum tegra_se_elp_op_mode {
	SE_ELP_OP_MODE_RSA512,
	SE_ELP_OP_MODE_RSA768,
	SE_ELP_OP_MODE_RSA1024,
	SE_ELP_OP_MODE_RSA1536,
	SE_ELP_OP_MODE_RSA2048,
	SE_ELP_OP_MODE_RSA3072,
	SE_ELP_OP_MODE_RSA4096,
	SE_ELP_OP_MODE_ECC160,
	SE_ELP_OP_MODE_ECC192,
	SE_ELP_OP_MODE_ECC224,
	SE_ELP_OP_MODE_ECC256,
	SE_ELP_OP_MODE_ECC384,
	SE_ELP_OP_MODE_ECC512,
	SE_ELP_OP_MODE_ECC521,
};

enum tegra_se_elp_rng1_cmd {
	RNG1_CMD_NOP,
	RNG1_CMD_GEN_NOISE,
	RNG1_CMD_GEN_NONCE,
	RNG1_CMD_CREATE_STATE,
	RNG1_CMD_RENEW_STATE,
	RNG1_CMD_REFRESH_ADDIN,
	RNG1_CMD_GEN_RANDOM,
	RNG1_CMD_ADVANCE_STATE,
	RNG1_CMD_KAT,
	RNG1_CMD_ZEROIZE = 15,
};

static char *rng1_cmd[10] = {"RNG1_CMD_NOP", "RNG1_CMD_GEN_NOISE",
	"RNG1_CMD_GEN_NONCE", "RNG1_CMD_CREATE_STATE", "RNG1_CMD_RENEW_STATE",
	"RNG1_CMD_REFRESH_ADDIN", "RNG1_CMD_GEN_RANDOM",
	"RNG1_CMD_ADVANCE_STATE", "RNG1_CMD_KAT", "RNG1_CMD_ZEROIZE"};

struct tegra_se_chipdata {
	bool use_key_slot;
};

struct tegra_se_elp_dev {
	struct platform_device *pdev;
	struct device *dev;
	void __iomem *io_reg[2];
	struct clk *c;
	struct tegra_se_slot *slot_list;
	struct tegra_se_chipdata *chipdata;
	struct tegra_se_elp_pka_request *pka_req;
};

static struct tegra_se_elp_dev *elp_dev;

struct tegra_se_elp_rng_request {
	int size;
	u32 *rdata;
	u32 *rdata1;
	u32 *rdata2;
	u32 *rdata3;
	bool test_full_cmd_flow;
	bool adv_state_on;
};

struct tegra_se_elp_pka_request {
	struct tegra_se_elp_dev *se_dev;
	u8 slot_num;	/* Key slot number */
	u32 *message;
	u32 *result;
	u32 *exponent;
	u32 *modulus;
	u32 *m;
	u32 *r2;
	u32 *rinv;
	unsigned int op_mode;
	u32 size;
	int ecc_type;
	int rsa_type;
	u32 *curve_param_a;
	u32 *curve_param_b;
	u32 *order;
	u32 *base_pt_x;
	u32 *base_pt_y;
	u32 *res_pt_x;
	u32 *res_pt_y;
	u32 *key;
	bool pv_ok;
};

/* Security Engine key slot */
struct tegra_se_slot {
	struct list_head node;
	u8 slot_num;	/* Key slot number */
	bool available; /* Tells whether key slot is free to use */
};

static LIST_HEAD(key_slot);
static DEFINE_SPINLOCK(key_slot_lock);

static u32 pka_op_size[16] = {512, 768, 1024, 1536, 2048, 3072, 4096, 160, 192,
				224, 256, 384, 512, 640};
static struct timeval tv;
static long usec;

static inline u32 num_words(int mode)
{
	u32 words = 0;
	switch (mode) {
	case SE_ELP_OP_MODE_ECC160:
	case SE_ELP_OP_MODE_ECC192:
	case SE_ELP_OP_MODE_ECC224:
	case SE_ELP_OP_MODE_ECC256:
		words = pka_op_size[SE_ELP_OP_MODE_ECC256] / 32;
		break;
	case SE_ELP_OP_MODE_RSA512:
	case SE_ELP_OP_MODE_ECC384:
	case SE_ELP_OP_MODE_ECC512:
		words = pka_op_size[SE_ELP_OP_MODE_RSA512] / 32;
		break;
	case SE_ELP_OP_MODE_RSA768:
	case SE_ELP_OP_MODE_RSA1024:
	case SE_ELP_OP_MODE_ECC521:
		words = pka_op_size[SE_ELP_OP_MODE_RSA1024] / 32;
		break;
	case SE_ELP_OP_MODE_RSA1536:
	case SE_ELP_OP_MODE_RSA2048:
		words = pka_op_size[SE_ELP_OP_MODE_RSA2048] / 32;
		break;
	case SE_ELP_OP_MODE_RSA3072:
	case SE_ELP_OP_MODE_RSA4096:
		words = pka_op_size[SE_ELP_OP_MODE_RSA4096] / 32;
		break;
	default:
		dev_warn(elp_dev->dev, "%s: Invalid operation mode\n",
					__func__);
		break;
	}
	return words;
}

static inline void se_elp_writel(struct tegra_se_elp_dev *se_dev, int elp_type,
	unsigned int val, unsigned int reg_offset)
{
	writel(val, se_dev->io_reg[elp_type] + reg_offset);
}

static inline unsigned int se_elp_readl(struct tegra_se_elp_dev *se_dev,
	int elp_type, unsigned int reg_offset)
{
	unsigned int val;

	val = readl(se_dev->io_reg[elp_type] + reg_offset);

	return val;
}

static void tegra_se_pka_free_key_slot(struct tegra_se_slot *slot)
{
	if (slot) {
		spin_lock(&key_slot_lock);
		slot->available = true;
		spin_unlock(&key_slot_lock);
	}
}

static struct tegra_se_slot *tegra_se_pka_alloc_key_slot(void)
{
	struct tegra_se_slot *slot = NULL;
	bool found = false;

	spin_lock(&key_slot_lock);
	list_for_each_entry(slot, &key_slot, node) {
		if (slot->available) {
			slot->available = false;
			found = true;
			break;
		}
	}
	spin_unlock(&key_slot_lock);
	return found ? slot : NULL;
}

static int tegra_se_pka_init_key_slot(void)
{
	struct tegra_se_elp_dev *se_dev = elp_dev;
	int i;

	se_dev->slot_list = devm_kzalloc(se_dev->dev,
		sizeof(struct tegra_se_slot) * TEGRA_SE_PKA_KEYSLOT_COUNT,
			GFP_KERNEL);
	if (se_dev->slot_list == NULL) {
		dev_err(se_dev->dev, "slot list memory allocation failed\n");
		return -ENOMEM;
	}

	spin_lock_init(&key_slot_lock);
	spin_lock(&key_slot_lock);

	for (i = 0; i < TEGRA_SE_PKA_KEYSLOT_COUNT; i++) {
		se_dev->slot_list[i].available = true;
		se_dev->slot_list[i].slot_num = i;
		INIT_LIST_HEAD(&se_dev->slot_list[i].node);
		list_add_tail(&se_dev->slot_list[i].node, &key_slot);
	}

	spin_unlock(&key_slot_lock);
	return 0;
}

static u32 tegra_se_check_trng_op(struct tegra_se_elp_dev *se_dev)
{
	u32 val = 0;

	val = se_elp_readl(se_dev, PKA1, TEGRA_SE_ELP_PKA_TRNG_STATUS_OFFSET);

	if ((val & TEGRA_SE_ELP_PKA_TRNG_STATUS_SECURE(TRUE)) &&
		(val & TEGRA_SE_ELP_PKA_TRNG_STATUS_NONCE(FALSE)) &&
		(val & TEGRA_SE_ELP_PKA_TRNG_STATUS_SEEDED(TRUE)) &&
		((val &
		TEGRA_SE_ELP_PKA_TRNG_STATUS_LAST_RESEED(TRNG_LAST_RESEED_HOST))
		|| (val &
	TEGRA_SE_ELP_PKA_TRNG_STATUS_LAST_RESEED(TRNG_LAST_RESEED_RESEED))))
		return 0;
	else
		return -EINVAL;
}

static u32 tegra_se_set_trng_op(struct tegra_se_elp_dev *se_dev)
{
	u32 val = 0;
	u32 err = 0;

	se_elp_writel(se_dev, PKA1,
		TEGRA_SE_ELP_PKA_TRNG_SMODE_SECURE(ENABLE) |
		TEGRA_SE_ELP_PKA_TRNG_SMODE_NONCE(DISABLE),
		TEGRA_SE_ELP_PKA_TRNG_SMODE_OFFSET);
	se_elp_writel(se_dev, PKA1,
		TEGRA_SE_ELP_PKA_CTRL_CONTROL_AUTO_RESEED(ENABLE),
		TEGRA_SE_ELP_PKA_CTRL_CONTROL_OFFSET);

	/* Poll seeded status */
	val = se_elp_readl(se_dev, PKA1, TEGRA_SE_ELP_PKA_TRNG_STATUS_OFFSET);
	while (val & TEGRA_SE_ELP_PKA_TRNG_STATUS_SEEDED(FALSE))
		val = se_elp_readl(se_dev, PKA1,
			TEGRA_SE_ELP_PKA_TRNG_STATUS_OFFSET);

	if (val & TEGRA_SE_ELP_PKA_TRNG_STATUS_SEEDED(FALSE)) {
		dev_err(se_dev->dev, "\nAbrupt end of Seeding operation\n");
		err = -EINVAL;
	}

	return err;
}

static void tegra_se_restart_pka_mutex_wdt(struct tegra_se_elp_dev *se_dev)
{
	se_elp_writel(se_dev, PKA1, TEGRA_SE_MUTEX_WDT_UNITS,
			TEGRA_SE_ELP_PKA_MUTEX_WATCHDOG_OFFSET);
}

static u32 tegra_se_acquire_pka_mutex(struct tegra_se_elp_dev *se_dev)
{
	u32 val = 0;
	u32 err = 0;

	/* TODO: Add timeout for while loop */
	/* Acquire pka mutex */
	val = se_elp_readl(se_dev, PKA1, TEGRA_SE_ELP_PKA_MUTEX_OFFSET);
	while (val != 0x01)
		val = se_elp_readl(se_dev, PKA1, TEGRA_SE_ELP_PKA_MUTEX_OFFSET);

	/* One unit is 256 SE Cycles */
	tegra_se_restart_pka_mutex_wdt(se_dev);
	se_elp_writel(se_dev, PKA1, TEGRA_SE_ELP_PKA_MUTEX_TIMEOUT_ACTION,
			TEGRA_SE_ELP_PKA_MUTEX_TIMEOUT_ACTION_OFFSET);

	return err;
}

static void tegra_se_release_pka_mutex(struct tegra_se_elp_dev *se_dev)
{
	se_elp_writel(se_dev, PKA1, 0x01,
			TEGRA_SE_ELP_PKA_MUTEX_RELEASE_OFFSET);
}

static inline u32 pka_bank_start(u32 bank)
{
	return PKA_BANK_START_A + (bank * 0x400);
}

static inline u32 reg_bank_offset(u32 bank, u32 idx, u32 mode)
{
	return pka_bank_start(bank) + ((idx * 4) * num_words(mode));
}

static int tegra_se_fill_pka_opmem_addr(struct tegra_se_elp_dev *se_dev,
		struct tegra_se_elp_pka_request *req)
{
	u32 i = 0;
	u32 len = req->size;
	u32 *MOD, *EXP, *MSG;
	u32 *A, *B, *PX, *PY, *K, *QX, *QY;
	u32 t_a[64];
	u32 tmp1[64], tmp2[64];
	int ret;

	if (req->op_mode > SE_ELP_OP_MODE_ECC521)
		return -EINVAL;

	if (req->size > sizeof(tmp1))
		return -EINVAL;

	switch (req->op_mode) {

	case SE_ELP_OP_MODE_RSA512:
	case SE_ELP_OP_MODE_RSA768:
	case SE_ELP_OP_MODE_RSA1024:
	case SE_ELP_OP_MODE_RSA1536:
	case SE_ELP_OP_MODE_RSA2048:
	case SE_ELP_OP_MODE_RSA3072:
	case SE_ELP_OP_MODE_RSA4096:
		ret = copy_from_user((void *)tmp1,
				(void __user *)req->exponent, req->size);
		if (ret)
			return -EFAULT;
		EXP = tmp1;

		ret = copy_from_user((void *)tmp2,
				(void __user *)req->message, req->size);
		if (ret)
			return -EFAULT;
		MSG = tmp2;

		for (i = 0; i < req->size/4; i++) {
			se_elp_writel(se_dev, PKA1, *EXP++,
				reg_bank_offset(TEGRA_SE_ELP_PKA_RSA_EXP_BANK,
					TEGRA_SE_ELP_PKA_RSA_EXP_ID,
					req->op_mode) + (i*4));
			se_elp_writel(se_dev, PKA1, *MSG++,
				reg_bank_offset(TEGRA_SE_ELP_PKA_RSA_MSG_BANK,
					TEGRA_SE_ELP_PKA_RSA_MSG_ID,
					req->op_mode) + (i*4));
		}
		break;

	case SE_ELP_OP_MODE_ECC160:
	case SE_ELP_OP_MODE_ECC192:
	case SE_ELP_OP_MODE_ECC224:
	case SE_ELP_OP_MODE_ECC256:
	case SE_ELP_OP_MODE_ECC384:
	case SE_ELP_OP_MODE_ECC512:
	case SE_ELP_OP_MODE_ECC521:
		ret = copy_from_user((void *)t_a,
				(void __user *)req->curve_param_a, req->size);
		if (ret)
			return -EFAULT;
		A = t_a;

		if (req->op_mode == SE_ELP_OP_MODE_ECC521) {
			ret = copy_from_user((void *)tmp1,
					(void __user *)req->modulus, req->size);
			if (ret)
				return -EFAULT;
			MOD = tmp1;
			for (i = 0; i < req->size/4; i++)
				se_elp_writel(se_dev, PKA1, *MOD++,
				reg_bank_offset(TEGRA_SE_ELP_PKA_MOD_BANK,
				TEGRA_SE_ELP_PKA_MOD_ID, req->op_mode) + (i*4));
		}

		for (i = 0; i < req->size/4; i++)
			se_elp_writel(se_dev, PKA1, *A++,
				reg_bank_offset(TEGRA_SE_ELP_PKA_ECC_A_BANK,
					TEGRA_SE_ELP_PKA_ECC_A_ID,
					req->op_mode) + (i*4));

		if (req->ecc_type != ECC_POINT_DOUBLE) {
			ret = copy_from_user((void *)tmp1,
					(void __user *)req->base_pt_x,
					req->size);
			if (ret)
				return -EFAULT;
			PX = tmp1;

			ret = copy_from_user((void *)tmp2,
					(void __user *)req->base_pt_y,
					req->size);
			if (ret)
				return -EFAULT;
			PY = tmp2;

			for (i = 0; i < req->size/4; i++) {
				se_elp_writel(se_dev, PKA1, *PX++,
				reg_bank_offset(TEGRA_SE_ELP_PKA_ECC_XP_BANK,
					TEGRA_SE_ELP_PKA_ECC_XP_ID,
					req->op_mode) + (i*4));

				se_elp_writel(se_dev, PKA1, *PY++,
				reg_bank_offset(TEGRA_SE_ELP_PKA_ECC_YP_BANK,
					TEGRA_SE_ELP_PKA_ECC_YP_ID,
					req->op_mode) + (i*4));
			}
		}

		if (req->ecc_type == ECC_POINT_VER ||
				req->ecc_type == ECC_SHAMIR_TRICK) {
			/* For shamir trick, curve_param_b is paramater k
			 * and k should be of size CTRL_BASE_RADIX
			 */
			if (req->ecc_type == ECC_SHAMIR_TRICK)
				len = (num_words(req->op_mode)) * 4;

			ret = copy_from_user((void *)tmp1,
					(void __user *)req->curve_param_b, len);
			if (ret)
				return -EFAULT;
			B = tmp1;

			for (i = 0; i < len / 4; i++)
				se_elp_writel(se_dev, PKA1, *B++,
				reg_bank_offset(TEGRA_SE_ELP_PKA_ECC_B_BANK,
					TEGRA_SE_ELP_PKA_ECC_B_ID,
					req->op_mode) + (i*4));
		}

		if (req->ecc_type == ECC_POINT_ADD ||
				req->ecc_type == ECC_SHAMIR_TRICK ||
				req->ecc_type == ECC_POINT_DOUBLE) {
			ret = copy_from_user((void *)tmp1,
					(void __user *)req->res_pt_x,
					req->size);
			if (ret)
				return -EFAULT;
			QX = tmp1;

			ret = copy_from_user((void *)tmp2,
					(void __user *)req->res_pt_y,
					req->size);
			if (ret)
				return -EFAULT;
			QY = tmp2;
			for (i = 0; i < req->size/4; i++) {
				se_elp_writel(se_dev, PKA1, *QX++,
				reg_bank_offset(TEGRA_SE_ELP_PKA_ECC_XQ_BANK,
					TEGRA_SE_ELP_PKA_ECC_XQ_ID,
					req->op_mode) + (i*4));

				se_elp_writel(se_dev, PKA1, *QY++,
				reg_bank_offset(TEGRA_SE_ELP_PKA_ECC_YQ_BANK,
					TEGRA_SE_ELP_PKA_ECC_YQ_ID,
					req->op_mode) + (i*4));
			}
		}

		if (req->ecc_type == ECC_POINT_MUL ||
				req->ecc_type == ECC_SHAMIR_TRICK) {
			/* For shamir trick, key is paramater l
			 * and k for ECC_POINT_MUL and l for ECC_SHAMIR_TRICK
			* should be of size CTRL_BASE_RADIX
			*/
			len = (num_words(req->op_mode)) * 4;
			ret = copy_from_user((void *)tmp1,
					(void __user *)req->key, len);
			if (ret)
				return -EFAULT;

			K = tmp1;
			for (i = 0; i < len / 4; i++)
				se_elp_writel(se_dev, PKA1, *K++,
				reg_bank_offset(TEGRA_SE_ELP_PKA_ECC_K_BANK,
					TEGRA_SE_ELP_PKA_ECC_K_ID,
					req->op_mode) + (i*4));
		}
		break;
	}

	return 0;
}

static u32 pka_ctrl_base(u32 mode)
{
	struct tegra_se_elp_dev *se_dev = elp_dev;
	u32 val = 0, base_radix = 0;

	val = num_words(mode) * 32;
	switch (val) {
	case PKA_OP_SIZE_256:
		base_radix = TEGRA_SE_ELP_PKA_CTRL_BASE_256;
		break;
	case PKA_OP_SIZE_512:
		base_radix = TEGRA_SE_ELP_PKA_CTRL_BASE_512;
		break;
	case PKA_OP_SIZE_1024:
		base_radix = TEGRA_SE_ELP_PKA_CTRL_BASE_1024;
		break;
	case PKA_OP_SIZE_2048:
		base_radix = TEGRA_SE_ELP_PKA_CTRL_BASE_2048;
		break;
	case PKA_OP_SIZE_4096:
		base_radix = TEGRA_SE_ELP_PKA_CTRL_BASE_4096;
		break;
	default:
		dev_warn(se_dev->dev, "%s: Invalid base radix size\n",
						__func__);
		break;
	}
	return base_radix;
}

static bool is_a_m3(u32 *param_a)
{
	return false;
}

static void tegra_se_program_pka_regs(struct tegra_se_elp_dev *se_dev,
					struct tegra_se_elp_pka_request *req)
{
	u32 val = 0;

	se_elp_writel(se_dev, PKA1, 0, TEGRA_SE_ELP_PKA_FLAGS_OFFSET);
	se_elp_writel(se_dev, PKA1, 0, TEGRA_SE_ELP_PKA_FSTACK_PTR_OFFSET);

	switch (req->op_mode) {

	case SE_ELP_OP_MODE_RSA512:
	case SE_ELP_OP_MODE_RSA768:
	case SE_ELP_OP_MODE_RSA1024:
	case SE_ELP_OP_MODE_RSA1536:
	case SE_ELP_OP_MODE_RSA2048:
	case SE_ELP_OP_MODE_RSA3072:
	case SE_ELP_OP_MODE_RSA4096:
		se_elp_writel(se_dev, PKA1,
			TEGRA_SE_ELP_PKA_RSA_MOD_EXP_PRG_ENTRY_VAL,
			TEGRA_SE_ELP_PKA_PRG_ENTRY_OFFSET);
		se_elp_writel(se_dev, PKA1,
			TEGRA_SE_ELP_PKA_INT_ENABLE_IE_IRQ_EN(ENABLE),
			TEGRA_SE_ELP_PKA_INT_ENABLE_OFFSET);

		val =
		TEGRA_SE_ELP_PKA_CTRL_BASE_RADIX(pka_ctrl_base(req->op_mode))
			| TEGRA_SE_ELP_PKA_CTRL_PARTIAL_RADIX(req->size/4);
		val |= TEGRA_SE_ELP_PKA_CTRL_GO(TEGRA_SE_ELP_PKA_CTRL_GO_START);
		se_elp_writel(se_dev, PKA1, val, TEGRA_SE_ELP_PKA_CTRL_OFFSET);
		break;

	case SE_ELP_OP_MODE_ECC160:
	case SE_ELP_OP_MODE_ECC192:
	case SE_ELP_OP_MODE_ECC224:
	case SE_ELP_OP_MODE_ECC256:
	case SE_ELP_OP_MODE_ECC384:
	case SE_ELP_OP_MODE_ECC512:
	case SE_ELP_OP_MODE_ECC521:
		if (req->ecc_type == ECC_POINT_MUL) {
			se_elp_writel(se_dev, PKA1,
				TEGRA_SE_ELP_PKA_ECC_POINT_MUL_PRG_ENTRY_VAL,
				TEGRA_SE_ELP_PKA_PRG_ENTRY_OFFSET);
			if (req->op_mode != SE_ELP_OP_MODE_ECC521) {
				if (is_a_m3(req->curve_param_a))
					se_elp_writel(se_dev, PKA1,
					TEGRA_SE_ELP_PKA_FLAGS_FLAG_F3(ENABLE),
						TEGRA_SE_ELP_PKA_FLAGS_OFFSET);
			}
			/*clear F0 for binding val*/
			se_elp_writel(se_dev, PKA1,
				TEGRA_SE_ELP_PKA_FLAGS_FLAG_F0(DISABLE),
				TEGRA_SE_ELP_PKA_FLAGS_OFFSET);
		} else if (req->ecc_type == ECC_POINT_ADD) {
			se_elp_writel(se_dev, PKA1,
				TEGRA_SE_ELP_PKA_ECC_POINT_ADD_PRG_ENTRY_VAL,
				TEGRA_SE_ELP_PKA_PRG_ENTRY_OFFSET);
		} else if (req->ecc_type == ECC_POINT_DOUBLE) {
			se_elp_writel(se_dev, PKA1,
				TEGRA_SE_ELP_PKA_ECC_POINT_DOUBLE_PRG_ENTRY_VAL,
				TEGRA_SE_ELP_PKA_PRG_ENTRY_OFFSET);
		} else if (req->ecc_type == ECC_POINT_VER) {
			se_elp_writel(se_dev, PKA1,
				TEGRA_SE_ELP_PKA_ECC_ECPV_PRG_ENTRY_VAL,
				TEGRA_SE_ELP_PKA_PRG_ENTRY_OFFSET);
		} else {
			se_elp_writel(se_dev, PKA1,
				TEGRA_SE_ELP_PKA_ECC_SHAMIR_TRICK_PRG_ENTRY_VAL,
				TEGRA_SE_ELP_PKA_PRG_ENTRY_OFFSET);

			if (req->op_mode != SE_ELP_OP_MODE_ECC521) {
				if (is_a_m3(req->curve_param_a))
					se_elp_writel(se_dev, PKA1,
					TEGRA_SE_ELP_PKA_FLAGS_FLAG_F3(ENABLE),
						TEGRA_SE_ELP_PKA_FLAGS_OFFSET);
			}
		}

		se_elp_writel(se_dev, PKA1,
			TEGRA_SE_ELP_PKA_INT_ENABLE_IE_IRQ_EN(ENABLE),
			TEGRA_SE_ELP_PKA_INT_ENABLE_OFFSET);

		if (req->op_mode == SE_ELP_OP_MODE_ECC521) {
			se_elp_writel(se_dev, PKA1,
				TEGRA_SE_ELP_PKA_FLAGS_FLAG_F1(ENABLE),
				TEGRA_SE_ELP_PKA_FLAGS_OFFSET);
		}

		se_elp_writel(se_dev, PKA1,
		TEGRA_SE_ELP_PKA_CTRL_BASE_RADIX(pka_ctrl_base(req->op_mode)) |
			TEGRA_SE_ELP_PKA_CTRL_PARTIAL_RADIX(req->size/4) |
		TEGRA_SE_ELP_PKA_CTRL_GO(TEGRA_SE_ELP_PKA_CTRL_GO_START),
			TEGRA_SE_ELP_PKA_CTRL_OFFSET);
		break;
	default:
		dev_warn(se_dev->dev, "Invalid operation mode\n");
		break;
	}
}

static int tegra_se_check_pka_op_done(struct tegra_se_elp_dev *se_dev)
{
	u32 val = 0;

	val = se_elp_readl(se_dev, PKA1, TEGRA_SE_ELP_PKA_STATUS_OFFSET);
	while (!(val & TEGRA_SE_ELP_PKA_STATUS_IRQ_STAT(ENABLE)))
		val = se_elp_readl(se_dev, PKA1,
				TEGRA_SE_ELP_PKA_STATUS_OFFSET);

	if (!(val & TEGRA_SE_ELP_PKA_STATUS_IRQ_STAT(ENABLE))) {
		dev_err(se_dev->dev, "\nAbrupt end of operation\n");
		return -EINVAL;
	}

	val = se_elp_readl(se_dev, PKA1, TEGRA_SE_ELP_PKA_RETURN_CODE_OFFSET);
	if ((val | 0xFF00FFFF) != 0xFF00FFFF) {
		dev_err(se_dev->dev, "\nPKA Operation ended Abnormally\n");
		return -EINVAL;
	}
	/* Write Status Register to acknowledge interrupt */
	val = se_elp_readl(se_dev, PKA1, TEGRA_SE_ELP_PKA_STATUS_OFFSET);
	se_elp_writel(se_dev, PKA1, val, TEGRA_SE_ELP_PKA_STATUS_OFFSET);
	return 0;
}

static int tegra_se_read_pka_result(struct tegra_se_elp_dev *se_dev,
					struct tegra_se_elp_pka_request *req)
{
	u32 val, i, ret = 0, bytes_to_copy;
	unsigned char temp1[TEGRA_SE_ELP_MAX_DATA_SIZE];
	unsigned char temp2[TEGRA_SE_ELP_MAX_DATA_SIZE];
	unsigned char temp3[TEGRA_SE_ELP_MAX_DATA_SIZE];
	u32 *RES, *QX, *QY;

	if (req->size > TEGRA_SE_ELP_MAX_DATA_SIZE)
		return -EINVAL;

	bytes_to_copy = ((req->size / 4) * 4);
	RES = (u32 *)temp1;
	QX = (u32 *)temp2;
	QY = (u32 *)temp3;

	switch (req->op_mode) {

	case SE_ELP_OP_MODE_RSA512:
	case SE_ELP_OP_MODE_RSA768:
	case SE_ELP_OP_MODE_RSA1024:
	case SE_ELP_OP_MODE_RSA1536:
	case SE_ELP_OP_MODE_RSA2048:
	case SE_ELP_OP_MODE_RSA3072:
	case SE_ELP_OP_MODE_RSA4096:
		for (i = 0; i < req->size/4; i++) {
			val = se_elp_readl(se_dev, PKA1,
			reg_bank_offset(TEGRA_SE_ELP_PKA_RSA_RESULT_BANK,
				TEGRA_SE_ELP_PKA_RSA_RESULT_ID,
				req->op_mode) + (i*4));
			*RES = be32_to_cpu(val);
			RES++;
		}
		ret = copy_to_user((void __user *)req->result,
				(void *)temp1,
				bytes_to_copy);
		if (ret)
			return -EINVAL;

		break;

	case SE_ELP_OP_MODE_ECC160:
	case SE_ELP_OP_MODE_ECC192:
	case SE_ELP_OP_MODE_ECC224:
	case SE_ELP_OP_MODE_ECC256:
	case SE_ELP_OP_MODE_ECC384:
	case SE_ELP_OP_MODE_ECC512:
	case SE_ELP_OP_MODE_ECC521:
		if (req->ecc_type == ECC_POINT_VER) {
			val = se_elp_readl(se_dev, PKA1,
				TEGRA_SE_ELP_PKA_FLAGS_OFFSET);
			if (val & TEGRA_SE_ELP_PKA_FLAGS_FLAG_ZERO(ENABLE))
				req->pv_ok = true;
			else
				req->pv_ok = false;
		} else if (req->ecc_type == ECC_POINT_DOUBLE) {
			for (i = 0; i < req->size/4; i++) {
				val = se_elp_readl(se_dev, PKA1,
				reg_bank_offset(TEGRA_SE_ELP_PKA_ECC_XP_BANK,
					TEGRA_SE_ELP_PKA_ECC_XP_ID,
					req->op_mode) + (i*4));
				*QX = be32_to_cpu(val);
				QX++;
			}
			for (i = 0; i < req->size/4; i++) {
				val = se_elp_readl(se_dev, PKA1,
				reg_bank_offset(TEGRA_SE_ELP_PKA_ECC_YP_BANK,
					TEGRA_SE_ELP_PKA_ECC_YP_ID,
					req->op_mode) + (i*4));
				*QY = be32_to_cpu(val);
				QY++;
			}
			ret = copy_to_user((void __user *)req->res_pt_x,
					(void *)temp2,
					bytes_to_copy);
			if (ret)
				return -EINVAL;
			ret = copy_to_user((void __user *)req->res_pt_y,
					(void *)temp3,
					bytes_to_copy);
			if (ret)
				return -EINVAL;
		} else {
			for (i = 0; i < req->size/4; i++) {
				val = se_elp_readl(se_dev, PKA1,
				reg_bank_offset(TEGRA_SE_ELP_PKA_ECC_XQ_BANK,
					TEGRA_SE_ELP_PKA_ECC_XQ_ID,
					req->op_mode) + (i*4));
				*QX = be32_to_cpu(val);
				QX++;
			}
			for (i = 0; i < req->size/4; i++) {
				val = se_elp_readl(se_dev, PKA1,
				reg_bank_offset(TEGRA_SE_ELP_PKA_ECC_YQ_BANK,
					TEGRA_SE_ELP_PKA_ECC_YQ_ID,
					req->op_mode) + (i*4));
				*QY = be32_to_cpu(val);
				QY++;
			}
			ret = copy_to_user((void __user *)req->res_pt_x,
					(void *)temp2,
					bytes_to_copy);
			if (ret)
				return -EINVAL;
			ret = copy_to_user((void __user *)req->res_pt_y,
					(void *)temp3,
					bytes_to_copy);
			if (ret)
				return -EINVAL;
		}
		break;
	}
	return 0;
}

enum tegra_se_elp_pka_keyslot_field {
	EXPONENT,
	MOD_RSA,
	M_RSA,
	R2_RSA,
	PARAM_A,
	PARAM_B,
	MOD_ECC,
	XP,
	YP,
	XQ,
	YQ,
	KEY,
	M_ECC,
	R2_ECC,
};

static int tegra_se_set_pka_key(struct tegra_se_elp_dev *se_dev,
	enum tegra_se_elp_op_mode mode, struct tegra_se_elp_pka_request *req)
{
	u32 i = 0, ret = 0;
	unsigned char temp1[TEGRA_SE_ELP_MAX_DATA_SIZE];
	unsigned char temp2[TEGRA_SE_ELP_MAX_DATA_SIZE];
	unsigned char temp3[TEGRA_SE_ELP_MAX_DATA_SIZE];
	unsigned char temp4[TEGRA_SE_ELP_MAX_DATA_SIZE];
	unsigned char temp5[TEGRA_SE_ELP_MAX_DATA_SIZE];
	unsigned char temp6[TEGRA_SE_ELP_MAX_DATA_SIZE];
	unsigned char temp7[TEGRA_SE_ELP_MAX_DATA_SIZE];
	unsigned char temp8[TEGRA_SE_ELP_MAX_DATA_SIZE];
	unsigned char temp9[TEGRA_SE_ELP_MAX_DATA_SIZE];
	unsigned char temp10[TEGRA_SE_ELP_MAX_DATA_SIZE];
	u8 slot_num = req->slot_num;
	u32 *MOD, *M, *R2, *EXP, *MSG;
	u32 *A, *B, *PX, *PY, *K;

	if (req->size > TEGRA_SE_ELP_MAX_DATA_SIZE)
		return -EINVAL;

	ret = copy_from_user((void *)temp1,
	(void __user *)req->modulus, req->size);
	if (ret)
		return -EINVAL;
	MOD = (u32 *)temp1;

	ret = copy_from_user((void *)temp2,
	(void __user *)req->m, req->size);
	if (ret)
		return -EINVAL;
	M = (u32 *)temp2;

	ret = copy_from_user((void *)temp3,
	(void __user *)req->r2, req->size);
	if (ret)
		return -EINVAL;
	R2 = (u32 *)temp3;

	switch (mode) {

	case SE_ELP_OP_MODE_RSA512:
	case SE_ELP_OP_MODE_RSA768:
	case SE_ELP_OP_MODE_RSA1024:
	case SE_ELP_OP_MODE_RSA1536:
	case SE_ELP_OP_MODE_RSA2048:
	case SE_ELP_OP_MODE_RSA3072:
	case SE_ELP_OP_MODE_RSA4096:
		ret = copy_from_user((void *)temp4,
		(void __user *)req->exponent, req->size);
		if (ret)
			return -EINVAL;
		EXP = (u32 *)temp4;

		ret = copy_from_user((void *)temp5,
		(void __user *)req->message, req->size);
		if (ret)
			return -EINVAL;
		MSG = (u32 *)temp5;

		for (i = 0; i < req->size/4; i++) {
			se_elp_writel(se_dev, PKA1,
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_FIELD(EXPONENT) |
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_WORD(i),
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_OFFSET(slot_num));
			se_elp_writel(se_dev, PKA1, *EXP++,
				TEGRA_SE_ELP_PKA_KEYSLOT_DATA_OFFSET(slot_num));

			se_elp_writel(se_dev, PKA1,
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_FIELD(MOD_RSA) |
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_WORD(i),
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_OFFSET(slot_num));
			se_elp_writel(se_dev, PKA1, *MOD++,
				TEGRA_SE_ELP_PKA_KEYSLOT_DATA_OFFSET(slot_num));

			se_elp_writel(se_dev, PKA1,
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_FIELD(M_RSA) |
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_WORD(i),
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_OFFSET(slot_num));
			se_elp_writel(se_dev, PKA1, *M++,
				TEGRA_SE_ELP_PKA_KEYSLOT_DATA_OFFSET(slot_num));

			se_elp_writel(se_dev, PKA1,
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_FIELD(R2_RSA) |
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_WORD(i),
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_OFFSET(slot_num));
			se_elp_writel(se_dev, PKA1, *R2++,
				TEGRA_SE_ELP_PKA_KEYSLOT_DATA_OFFSET(slot_num));
		}
		break;

	case SE_ELP_OP_MODE_ECC160:
	case SE_ELP_OP_MODE_ECC192:
	case SE_ELP_OP_MODE_ECC224:
	case SE_ELP_OP_MODE_ECC256:
	case SE_ELP_OP_MODE_ECC384:
	case SE_ELP_OP_MODE_ECC512:
	case SE_ELP_OP_MODE_ECC521:
		ret = copy_from_user((void *)temp6,
		(void __user *)req->curve_param_a, req->size);
		if (ret)
			return -EINVAL;
		A = (u32 *)temp6;

		ret = copy_from_user((void *)temp7,
		(void __user *)req->curve_param_b, req->size);
		if (ret)
			return -EINVAL;
		B = (u32 *)temp7;

		ret = copy_from_user((void *)temp8,
		(void __user *)req->base_pt_x, req->size);
		if (ret)
			return -EINVAL;
		PX = (u32 *)temp8;

		ret = copy_from_user((void *)temp9,
		(void __user *)req->base_pt_y, req->size);
		if (ret)
			return -EINVAL;
		PY = (u32 *)temp9;

		ret = copy_from_user((void *)temp10,
		(void __user *)req->key, req->size);
		if (ret)
			return -EINVAL;
		K = (u32 *)temp10;

		for (i = 0; i < req->size/4; i++) {
			se_elp_writel(se_dev, PKA1,
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_FIELD(PARAM_A) |
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_WORD(i),
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_OFFSET(slot_num));
			se_elp_writel(se_dev, PKA1, *A++,
				TEGRA_SE_ELP_PKA_KEYSLOT_DATA_OFFSET(slot_num));

			se_elp_writel(se_dev, PKA1,
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_FIELD(PARAM_B) |
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_WORD(i),
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_OFFSET(slot_num));
			se_elp_writel(se_dev, PKA1, *B++,
				TEGRA_SE_ELP_PKA_KEYSLOT_DATA_OFFSET(slot_num));

			se_elp_writel(se_dev, PKA1,
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_FIELD(MOD_ECC) |
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_WORD(i),
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_OFFSET(slot_num));
			se_elp_writel(se_dev, PKA1, *MOD++,
				TEGRA_SE_ELP_PKA_KEYSLOT_DATA_OFFSET(slot_num));

			se_elp_writel(se_dev, PKA1,
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_FIELD(XP) |
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_WORD(i),
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_OFFSET(slot_num));
			se_elp_writel(se_dev, PKA1, *PX++,
				TEGRA_SE_ELP_PKA_KEYSLOT_DATA_OFFSET(slot_num));

			se_elp_writel(se_dev, PKA1,
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_FIELD(YP) |
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_WORD(i),
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_OFFSET(slot_num));
			se_elp_writel(se_dev, PKA1, *PY++,
				TEGRA_SE_ELP_PKA_KEYSLOT_DATA_OFFSET(slot_num));

			se_elp_writel(se_dev, PKA1,
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_FIELD(KEY) |
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_WORD(i),
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_OFFSET(slot_num));
			se_elp_writel(se_dev, PKA1, *K++,
				TEGRA_SE_ELP_PKA_KEYSLOT_DATA_OFFSET(slot_num));

			se_elp_writel(se_dev, PKA1,
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_FIELD(M_ECC) |
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_WORD(i),
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_OFFSET(slot_num));
			se_elp_writel(se_dev, PKA1, *M++,
				TEGRA_SE_ELP_PKA_KEYSLOT_DATA_OFFSET(slot_num));

			se_elp_writel(se_dev, PKA1,
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_FIELD(R2_ECC) |
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_WORD(i),
				TEGRA_SE_ELP_PKA_KEYSLOT_ADDR_OFFSET(slot_num));
			se_elp_writel(se_dev, PKA1, *R2++,
				TEGRA_SE_ELP_PKA_KEYSLOT_DATA_OFFSET(slot_num));
		}
		break;
	}
	return 0;
}

static int tegra_se_elp_pka_precomp(struct tegra_se_elp_dev *se_dev,
			struct tegra_se_elp_pka_request *req, u32 op)
{
	int ret = 0, i, bytes_to_copy;
	unsigned char temp[TEGRA_SE_ELP_MAX_DATA_SIZE];
	unsigned char temp1[TEGRA_SE_ELP_MAX_DATA_SIZE];
	unsigned char temp2[TEGRA_SE_ELP_MAX_DATA_SIZE];
	unsigned char temp3[TEGRA_SE_ELP_MAX_DATA_SIZE];
	u32 *MOD, *RINV, *M, *R2;

	if (req->size > TEGRA_SE_ELP_MAX_DATA_SIZE)
		return -EINVAL;

	RINV = (u32 *)temp1;
	M = (u32 *)temp2;
	R2 = (u32 *)temp3;
	bytes_to_copy = ((req->size / 4) * 4);

	if (req->op_mode == SE_ELP_OP_MODE_ECC521)
		return 0;

	se_elp_writel(se_dev, PKA1, 0, TEGRA_SE_ELP_PKA_FLAGS_OFFSET);
	se_elp_writel(se_dev, PKA1, 0, TEGRA_SE_ELP_PKA_FSTACK_PTR_OFFSET);

	if (op == PRECOMP_RINV) {
		ret = copy_from_user((void *)temp,
					(void __user *)req->modulus,
					req->size);
		if (ret)
			return -EINVAL;

		MOD = (u32 *)temp;
		for (i = 0; i < req->size/4; i++) {
			se_elp_writel(se_dev, PKA1, *MOD++,
				reg_bank_offset(TEGRA_SE_ELP_PKA_MOD_BANK,
				TEGRA_SE_ELP_PKA_MOD_ID, req->op_mode) + (i*4));
		}

		se_elp_writel(se_dev, PKA1,
			TEGRA_SE_ELP_PKA_RSA_RINV_PRG_ENTRY_VAL,
			TEGRA_SE_ELP_PKA_PRG_ENTRY_OFFSET);
	} else if (op == PRECOMP_M) {
		se_elp_writel(se_dev, PKA1,
			TEGRA_SE_ELP_PKA_RSA_M_PRG_ENTRY_VAL,
			TEGRA_SE_ELP_PKA_PRG_ENTRY_OFFSET);
	} else {
		se_elp_writel(se_dev, PKA1,
			TEGRA_SE_ELP_PKA_RSA_R2_PRG_ENTRY_VAL,
			TEGRA_SE_ELP_PKA_PRG_ENTRY_OFFSET);
	}

	se_elp_writel(se_dev, PKA1,
			TEGRA_SE_ELP_PKA_INT_ENABLE_IE_IRQ_EN(ENABLE),
			TEGRA_SE_ELP_PKA_INT_ENABLE_OFFSET);
	se_elp_writel(se_dev, PKA1,
		TEGRA_SE_ELP_PKA_CTRL_BASE_RADIX(pka_ctrl_base(req->op_mode)) |
		TEGRA_SE_ELP_PKA_CTRL_PARTIAL_RADIX(req->size/4) |
		TEGRA_SE_ELP_PKA_CTRL_GO(TEGRA_SE_ELP_PKA_CTRL_GO_START),
		TEGRA_SE_ELP_PKA_CTRL_OFFSET);

	ret = tegra_se_check_pka_op_done(se_dev);
	if (ret)
		return -EINVAL;

	if (op == PRECOMP_RINV) {
		for (i = 0; i < req->size/4; i++) {
			*RINV = se_elp_readl(se_dev, PKA1,
				reg_bank_offset(TEGRA_SE_ELP_PKA_RINV_BANK,
				TEGRA_SE_ELP_PKA_RINV_ID,
					req->op_mode) + (i*4));
			RINV++;
		}
		ret = copy_to_user((void __user *)req->rinv,
					(void *)temp1,
					bytes_to_copy);
		if (ret)
			return -EINVAL;
	} else if (op == PRECOMP_M) {
		for (i = 0; i < req->size/4; i++) {
			*M = se_elp_readl(se_dev, PKA1,
				reg_bank_offset(TEGRA_SE_ELP_PKA_M_BANK,
				TEGRA_SE_ELP_PKA_M_ID, req->op_mode) + (i*4));
			M++;
		}
		ret = copy_to_user((void __user *)req->m,
					(void *)temp2,
					bytes_to_copy);
		if (ret)
			return -EINVAL;
	} else {
		for (i = 0; i < req->size/4; i++) {
			*R2 = se_elp_readl(se_dev, PKA1,
				reg_bank_offset(TEGRA_SE_ELP_PKA_R2_BANK,
				TEGRA_SE_ELP_PKA_R2_ID, req->op_mode) + (i*4));
			R2++;
		}
		ret = copy_to_user((void __user *)req->r2,
					(void *)temp3,
					bytes_to_copy);
		if (ret)
			return -EINVAL;
	}
	return ret;
}

static int tegra_se_elp_pka_do(struct tegra_se_elp_dev *se_dev,
				struct tegra_se_elp_pka_request *req)
{
	int ret = 0;
	u32 val;
	struct tegra_se_slot *pslot = NULL;

	if (se_dev->chipdata->use_key_slot) {
		pslot = tegra_se_pka_alloc_key_slot();
		if (!pslot) {
			dev_err(se_dev->dev, "no free key slot\n");
			return -ENOMEM;
		}

		if (pslot->slot_num > TEGRA_SE_PKA_KEYSLOT_COUNT)
			return -EINVAL;
		req->slot_num = pslot->slot_num;

		if (tegra_se_set_pka_key(se_dev, req->op_mode, req)) {
			dev_err(se_dev->dev, "%s: Failed in tegra_se_set_pka_key\n",
				__func__);
			ret = -ENOMEM;
			goto end;
		}
		/* Set LOAD_KEY */
		val = se_elp_readl(se_dev, PKA1,
				TEGRA_SE_ELP_PKA_CTRL_CONTROL_OFFSET);
		val |= TEGRA_SE_ELP_PKA_CTRL_CONTROL_LOAD_KEY(ENABLE);
		se_elp_writel(se_dev, PKA1, val,
				TEGRA_SE_ELP_PKA_CTRL_CONTROL_OFFSET);

		/*Write KEYSLOT Number */
		val = se_elp_readl(se_dev, PKA1,
				TEGRA_SE_ELP_PKA_CTRL_CONTROL_OFFSET);
		val |=
		TEGRA_SE_ELP_PKA_CTRL_CONTROL_KEYSLOT(req->slot_num);
		se_elp_writel(se_dev, PKA1, val,
				TEGRA_SE_ELP_PKA_CTRL_CONTROL_OFFSET);
	} else {
		ret = tegra_se_fill_pka_opmem_addr(se_dev, req);
		if (ret)
			goto end;
	}

	tegra_se_program_pka_regs(se_dev, req);

	tegra_se_check_pka_op_done(se_dev);

	if (tegra_se_read_pka_result(se_dev, req)) {
		dev_err(se_dev->dev, "\n%s Failed in tegra_se_read_pka_result\n",
			__func__);
		ret = -EINVAL;
		goto end;
	}
end:
	if (se_dev->chipdata->use_key_slot)
		tegra_se_pka_free_key_slot(pslot);

	return ret;
}

static int tegra_se_elp_pka_init(struct tegra_se_elp_pka_request *req)
{
	struct tegra_se_elp_dev *se_dev = elp_dev;

	if (req->op_mode == SE_ELP_OP_MODE_ECC521)
		return 0;

	req->rinv = devm_kzalloc(se_dev->dev, req->size, GFP_KERNEL);
	if (!req->rinv) {
		dev_err(se_dev->dev, "\n%s: Memory alloc fail for req->rinv\n",
					__func__);
		return -ENOMEM;
	}

	req->m = devm_kzalloc(se_dev->dev, req->size, GFP_KERNEL);
	if (!req->m) {
		dev_err(se_dev->dev, "\n%s: Memory alloc fail for req->m\n",
					__func__);
		devm_kfree(se_dev->dev, req->rinv);
		return -ENOMEM;
	}

	req->r2 = devm_kzalloc(se_dev->dev, req->size, GFP_KERNEL);
	if (!req->r2) {
		dev_err(se_dev->dev, "\n%s: Memory alloc fail for req->r2\n",
					__func__);
		devm_kfree(se_dev->dev, req->m);
		devm_kfree(se_dev->dev, req->rinv);
		return -ENOMEM;
	}

	return 0;
}

static void tegra_se_elp_pka_exit(struct tegra_se_elp_pka_request *req)
{
	struct tegra_se_elp_dev *se_dev = elp_dev;

	if (req->op_mode == SE_ELP_OP_MODE_ECC521)
		return;

	devm_kfree(se_dev->dev, req->r2);
	devm_kfree(se_dev->dev, req->m);
	devm_kfree(se_dev->dev, req->rinv);
}

static void tegra_se_release_rng_mutex(struct tegra_se_elp_dev *se_dev)
{
	se_elp_writel(se_dev, RNG1, 0x01, TEGRA_SE_ELP_RNG_MUTEX_OFFSET);
}

static u32 tegra_se_acquire_rng_mutex(struct tegra_se_elp_dev *se_dev)
{
	u32 val = 0;

	do_gettimeofday(&tv);
	usec = tv.tv_usec;

	while (val != 0x01) {
		val = se_elp_readl(se_dev, RNG1, TEGRA_SE_ELP_RNG_MUTEX_OFFSET);
		do_gettimeofday(&tv);
		if (tv.tv_usec - usec > RNG1_TIMEOUT) {
			dev_err(se_dev->dev, "\nAcquire RNG1 Mutex timed out\n");
			return -EINVAL;
		}
	}

	/* One unit is 256 SE Cycles */
	se_elp_writel(se_dev, RNG1, TEGRA_SE_MUTEX_WDT_UNITS,
				TEGRA_SE_ELP_RNG_MUTEX_WATCHDOG_OFFSET);
	se_elp_writel(se_dev, RNG1,
		TEGRA_SE_ELP_RNG_MUTEX_TIMEOUT_ACTION,
		TEGRA_SE_ELP_RNG_MUTEX_TIMEOUT_ACTION_OFFSET);

	return 0;
}

static u32 tegra_se_check_rng_status(struct tegra_se_elp_dev *se_dev)
{
	static bool rng1_first = true;
	bool secure_mode;
	u32 ret = 0;
	u32 val = TEGRA_SE_ELP_RNG_STATUS_BUSY(TRUE);

	do_gettimeofday(&tv);
	usec = tv.tv_usec;

	/*Wait until RNG is Idle */
	while (val & TEGRA_SE_ELP_RNG_STATUS_BUSY(TRUE)) {
		val = se_elp_readl(se_dev, RNG1,
				TEGRA_SE_ELP_RNG_STATUS_OFFSET);
		do_gettimeofday(&tv);
		if (tv.tv_usec - usec > RNG1_TIMEOUT) {
			dev_err(se_dev->dev, "\nWait for RNG1 Idle timed out\n");
			return -EINVAL;
		}
	}

	if (rng1_first) {
		val = se_elp_readl(se_dev, RNG1,
				TEGRA_SE_ELP_RNG_STATUS_OFFSET);
		if (val & TEGRA_SE_ELP_RNG_STATUS_SECURE(STATUS_SECURE))
			secure_mode = true;
		else
			secure_mode = false;

		/*Check health test is ok*/
		val = se_elp_readl(se_dev, RNG1,
					TEGRA_SE_ELP_RNG_ISTATUS_OFFSET);
		if (secure_mode)
			val &= TEGRA_SE_ELP_RNG_ISTATUS_DONE(ISTATUS_ACTIVE);
		else
			val &= TEGRA_SE_ELP_RNG_ISTATUS_DONE(ISTATUS_ACTIVE) |
			TEGRA_SE_ELP_RNG_ISTATUS_NOISE_RDY(ISTATUS_ACTIVE);
		if (!val) {
			dev_err(se_dev->dev,
				"\nWrong Startup value in RNG_ISTATUS Reg\n");
			return -EINVAL;
		}
		rng1_first = false;
	}

	val = se_elp_readl(se_dev, RNG1, TEGRA_SE_ELP_RNG_ISTATUS_OFFSET);
	se_elp_writel(se_dev, RNG1, val, TEGRA_SE_ELP_RNG_ISTATUS_OFFSET);

	val = se_elp_readl(se_dev, RNG1, TEGRA_SE_ELP_RNG_ISTATUS_OFFSET);
	if (val) {
		dev_err(se_dev->dev, "\nRNG_ISTATUS Reg is not cleared\n");
		return -EINVAL;
	}

	return ret;
}

static void tegra_se_set_rng1_mode(unsigned int mode)
{
	struct tegra_se_elp_dev *se_dev = elp_dev;
	/*no additional input mode*/
	se_elp_writel(se_dev, RNG1, mode, TEGRA_SE_ELP_RNG_SE_MODE_OFFSET);
}

static void tegra_se_set_rng1_smode(bool secure, bool nonce)
{
	u32 val = 0;
	struct tegra_se_elp_dev *se_dev = elp_dev;

	if (secure)
		val |= TEGRA_SE_ELP_RNG_SE_SMODE_SECURE(SMODE_SECURE);
	if (nonce)
		val |= TEGRA_SE_ELP_RNG_SE_SMODE_NONCE(ENABLE);

	/* need to write twice, switch secure/promiscuous
	 * mode would reset other bits
	 */
	se_elp_writel(se_dev, RNG1, val, TEGRA_SE_ELP_RNG_SE_SMODE_OFFSET);
	se_elp_writel(se_dev, RNG1, val, TEGRA_SE_ELP_RNG_SE_SMODE_OFFSET);
}

static int tegra_se_execute_rng1_ctrl_cmd(unsigned int cmd)
{
	u32 val, stat;
	bool secure_mode;
	struct tegra_se_elp_dev *se_dev = elp_dev;
	int ret = 0;

	se_elp_writel(se_dev, RNG1, 0xFFFFFFFF, TEGRA_SE_ELP_RNG_INT_EN_OFFSET);
	se_elp_writel(se_dev, RNG1, 0xFFFFFFFF, TEGRA_SE_ELP_RNG_IE_OFFSET);

	val = se_elp_readl(se_dev, RNG1, TEGRA_SE_ELP_RNG_STATUS_OFFSET);
	if (val & TEGRA_SE_ELP_RNG_STATUS_SECURE(STATUS_SECURE))
		secure_mode = true;
	else
		secure_mode = false;

	switch (cmd) {
	case RNG1_CMD_GEN_NONCE:
	case RNG1_CMD_CREATE_STATE:
	case RNG1_CMD_RENEW_STATE:
	case RNG1_CMD_REFRESH_ADDIN:
	case RNG1_CMD_GEN_RANDOM:
	case RNG1_CMD_ADVANCE_STATE:
		stat = TEGRA_SE_ELP_RNG_ISTATUS_DONE(ISTATUS_ACTIVE);
		break;
	case RNG1_CMD_GEN_NOISE:
		if (secure_mode)
			stat = TEGRA_SE_ELP_RNG_ISTATUS_DONE(ISTATUS_ACTIVE);
		else
			stat = TEGRA_SE_ELP_RNG_ISTATUS_DONE(ISTATUS_ACTIVE) |
			TEGRA_SE_ELP_RNG_ISTATUS_NOISE_RDY(ISTATUS_ACTIVE);
		break;
	case RNG1_CMD_KAT:
		stat = TEGRA_SE_ELP_RNG_ISTATUS_KAT_COMPLETED(ISTATUS_ACTIVE);
		break;
	case RNG1_CMD_ZEROIZE:
		stat = TEGRA_SE_ELP_RNG_ISTATUS_ZEROIZED(ISTATUS_ACTIVE);
		break;
	case RNG1_CMD_NOP:
	default:
		dev_err(se_dev->dev,
			"\nCmd %d has nothing to do (or) invalid\n", cmd);
		dev_err(se_dev->dev,
			"\nRNG1 Command Failure: %s\n", rng1_cmd[cmd]);
		return -EINVAL;
		break;
	}
	se_elp_writel(se_dev, RNG1, cmd, TEGRA_SE_ELP_RNG_CTRL_OFFSET);

	val = se_elp_readl(se_dev, RNG1, TEGRA_SE_ELP_RNG_ISTATUS_OFFSET);
	while ((val | ~stat) != 0xffffffff)
		val = se_elp_readl(se_dev, RNG1,
				TEGRA_SE_ELP_RNG_ISTATUS_OFFSET);

	if (val != stat) {
		dev_err(se_dev->dev,
		"\nInvalid ISTAT 0x%x after cmd %d execution\n", stat, cmd);
		dev_err(se_dev->dev, "\nRNG1 Command Failure: %s\n",
		((cmd == RNG1_CMD_ZEROIZE) ? rng1_cmd[9] : rng1_cmd[cmd]));
		return -EINVAL;
	}

	val = se_elp_readl(se_dev, RNG1, TEGRA_SE_ELP_RNG_IE_OFFSET);
	val = se_elp_readl(se_dev, RNG1, TEGRA_SE_ELP_RNG_INT_EN_OFFSET);

	val = se_elp_readl(se_dev, RNG1, TEGRA_SE_ELP_RNG_INT_STATUS_OFFSET);
	while (!(val & TEGRA_SE_ELP_RNG_INT_STATUS_EIP0(STATUS_ACTIVE)))
		se_elp_readl(se_dev, RNG1, TEGRA_SE_ELP_RNG_INT_STATUS_OFFSET);

	if (!(val & TEGRA_SE_ELP_RNG_INT_STATUS_EIP0(STATUS_ACTIVE))) {
		dev_err(se_dev->dev,
		"\nRNG1 interrupt is not set (0x%x) after cmd %d execution\n",
			val, cmd);
		dev_err(se_dev->dev, "\nRNG1 Command Failure: %s\n",
		((cmd == RNG1_CMD_ZEROIZE) ? rng1_cmd[9] : rng1_cmd[cmd]));
		return -EINVAL;
	}

	se_elp_writel(se_dev, RNG1, stat, TEGRA_SE_ELP_RNG_ISTATUS_OFFSET);
	val = se_elp_readl(se_dev, RNG1, TEGRA_SE_ELP_RNG_INT_STATUS_OFFSET);
	if (val & TEGRA_SE_ELP_RNG_INT_STATUS_EIP0(STATUS_ACTIVE)) {
		dev_err(se_dev->dev,
		"\nRNG1 interrupt could not be cleared (0x%x) after cmd %d execution\n",
			val, cmd);
		dev_err(se_dev->dev, "\nRNG1 Command Failure: %s\n",
		((cmd == RNG1_CMD_ZEROIZE) ? rng1_cmd[9] : rng1_cmd[cmd]));
		return -EINVAL;
	}
	return ret;
}

static u32 *tegra_se_check_rng1_result(struct tegra_se_elp_rng_request *req)
{
	u32 i, val;
	u32 *rand;
	struct tegra_se_elp_dev *se_dev = elp_dev;

	rand = devm_kzalloc(se_dev->dev, sizeof(*rand) * 4, GFP_KERNEL);

	for (i = 0; i < 4; i++) {
		val = se_elp_readl(se_dev, RNG1,
			TEGRA_SE_ELP_RNG_RAND0_OFFSET + i*4);
		if (!val) {
			devm_kfree(se_dev->dev, rand);
			dev_err(se_dev->dev, "\nNo random data from RAND\n");
			return NULL;
		} else {
			rand[i] = val;
		}
	}

	return rand;
}

static int tegra_se_check_rng1_alarms(void)
{
	u32 val;
	struct tegra_se_elp_dev *se_dev = elp_dev;

	val = se_elp_readl(se_dev, RNG1, TEGRA_SE_ELP_RNG_ALARMS_OFFSET);
	if (val) {
		dev_err(se_dev->dev,
			"\nRNG Alarms are not clear (0x%x)\n", val);
		return -EINVAL;
	}
	return 0;
}

static void tegra_se_rng1_feed_npa_data(void)
{
	int i = 0;
	u32 data, r;
	struct tegra_se_elp_dev *se_dev = elp_dev;

	for (i = 0; i < 16; i++) {
		get_random_bytes(&r, sizeof(int));
		data = r & 0xffffffff;
		se_elp_writel(se_dev, RNG1, data,
			TEGRA_SE_ELP_RNG_NPA_DATA0_OFFSET + i*4);
	}
}

static int tegra_se_elp_rng_do(struct tegra_se_elp_dev *se_dev,
			struct tegra_se_elp_rng_request *req)
{
	u32 *rdata;
	u32 *rand_num;
	int i, j, k, err, ret = 0;
	bool adv_state = false;
	if (req->size < RAND_128 || (req->size % RAND_128 != 0))
		return -EINVAL;

	rand_num = devm_kzalloc(se_dev->dev, req->size, GFP_KERNEL);
	if (!rand_num)
		return -ENOMEM;

	tegra_se_set_rng1_smode(true, false);
	tegra_se_set_rng1_mode(RNG1_MODE_SEC_ALG);
	/* Generate Noise */
	ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_GEN_NOISE);
	if (ret)
		goto ret;
	ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_CREATE_STATE);
	if (ret)
		goto ret;

	for (i = 0; i < req->size/RAND_128; i++) {
		if (i && req->adv_state_on && (i % ADV_STATE_FREQ == 0)) {
			ret =
			tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_ADVANCE_STATE);
			if (ret)
				goto ret;
			adv_state = true;
		}
		ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_GEN_RANDOM);
		if (ret)
			goto ret;

		rdata = tegra_se_check_rng1_result(req);
		if (!rdata) {
			dev_err(se_dev->dev, "\nRNG1 Failed for Sub-Step 1\n");
			goto ret;
		}

		for (k = (4*i), j = 0; k < 4*(i+1); k++, j++)
			rand_num[k] = rdata[j];

		if (adv_state) {
			ret =
			tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_ADVANCE_STATE);
			if (ret)
				goto ret;
			if ((i+1) < req->size/RAND_128) {
				ret =
				tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_GEN_NOISE);
				if (ret)
					goto ret;
				ret =
				tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_RENEW_STATE);
				if (ret)
					goto ret;
			}
		}
	}

	err = copy_to_user((void __user *)req->rdata, rand_num, req->size);
	if (err) {
		ret = -EFAULT;
		goto ret;
	}

	tegra_se_check_rng1_alarms();

	if (!req->test_full_cmd_flow)
		goto ret;

	ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_ZEROIZE);
	if (ret)
		goto ret;

	tegra_se_set_rng1_smode(true, false);
	tegra_se_set_rng1_mode(RNG1_MODE_ADDIN_PRESENT);

	ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_GEN_NOISE);
	if (ret)
		goto ret;
	tegra_se_rng1_feed_npa_data();
	ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_CREATE_STATE);
	if (ret)
		goto ret;
	tegra_se_rng1_feed_npa_data();
	ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_REFRESH_ADDIN);
	if (ret)
		goto ret;

	for (i = 0; i < req->size/RAND_128; i++) {
		if (i && req->adv_state_on && (i % ADV_STATE_FREQ == 0)) {
			ret =
			tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_ADVANCE_STATE);
			if (ret)
				goto ret;
			tegra_se_rng1_feed_npa_data();
			ret =
			tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_REFRESH_ADDIN);
			if (ret)
				goto ret;
			adv_state = true;
		}
		ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_GEN_RANDOM);
		if (ret)
			goto ret;

		rdata = tegra_se_check_rng1_result(req);
		if (!rdata) {
			dev_err(se_dev->dev, "\nRNG1 Failed for Sub-Step 2\n");
			goto ret;
		}

		for (k = (4*i), j = 0; k < 4*(i+1); k++, j++)
			rand_num[k] = rdata[j];

		if (adv_state) {
			ret =
			tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_ADVANCE_STATE);
			if (ret)
				goto ret;
			if ((i+1) < req->size/RAND_128) {
				ret =
				tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_GEN_NOISE);
				if (ret)
					goto ret;
				tegra_se_rng1_feed_npa_data();
				ret =
				tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_RENEW_STATE);
				if (ret)
					goto ret;
				tegra_se_rng1_feed_npa_data();
				ret =
				tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_REFRESH_ADDIN);
				if (ret)
					goto ret;
			}
		}
	}

	err = copy_to_user((void __user *)req->rdata1, rand_num, req->size);
	if (err) {
		ret = -EFAULT;
		goto ret;
	}

	tegra_se_check_rng1_alarms();

	ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_ZEROIZE);
	if (ret)
		goto ret;

	tegra_se_set_rng1_smode(true, true);
	tegra_se_set_rng1_mode(RNG1_MODE_ADDIN_PRESENT);

	tegra_se_rng1_feed_npa_data();
	ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_GEN_NONCE);
	if (ret)
		goto ret;
	tegra_se_rng1_feed_npa_data();
	ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_GEN_NONCE);
	if (ret)
		goto ret;
	tegra_se_rng1_feed_npa_data();
	ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_CREATE_STATE);
	if (ret)
		goto ret;
	tegra_se_rng1_feed_npa_data();
	ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_REFRESH_ADDIN);
	if (ret)
		goto ret;

	for (i = 0; i < req->size/RAND_128; i++) {
		if (i && req->adv_state_on && (i % ADV_STATE_FREQ == 0)) {
			ret =
			tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_ADVANCE_STATE);
			if (ret)
				goto ret;
			tegra_se_rng1_feed_npa_data();
			ret =
			tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_REFRESH_ADDIN);
			if (ret)
				goto ret;
			adv_state = true;
		}
		ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_GEN_RANDOM);
		if (ret)
			goto ret;

		rdata = tegra_se_check_rng1_result(req);
		if (!rdata) {
			dev_err(se_dev->dev, "\nRNG1 Failed for Sub-Step 3\n");
			goto ret;
		}

		for (k = (4*i), j = 0; k < 4*(i+1); k++, j++)
			rand_num[k] = rdata[j];

		if (adv_state) {
			ret =
			tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_ADVANCE_STATE);
			if (ret)
				goto ret;
			if ((i+1) < req->size/RAND_128) {
				ret =
				tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_GEN_NONCE);
				if (ret)
					goto ret;
				tegra_se_rng1_feed_npa_data();
				ret =
				tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_RENEW_STATE);
				if (ret)
					goto ret;
				tegra_se_rng1_feed_npa_data();
				ret =
				tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_REFRESH_ADDIN);
				if (ret)
					goto ret;
			}
		}
	}

	err = copy_to_user((void __user *)req->rdata2, rand_num, req->size);
	if (err) {
		ret = -EFAULT;
		goto ret;
	}

	tegra_se_check_rng1_alarms();

	ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_ZEROIZE);
ret:
	devm_kfree(se_dev->dev, rand_num);
	return ret;
}

int tegra_se_elp_rng_op(struct tegra_se_elp_rng_request *req)
{
	struct tegra_se_elp_dev *se_dev = elp_dev;
	int ret = 0;

	ret = tegra_se_acquire_rng_mutex(se_dev);
	if (ret) {
		dev_err(se_dev->dev, "\n RNG Mutex acquire failed\n");
		return ret;
	}

	ret = tegra_se_check_rng_status(se_dev);
	if (ret) {
		dev_err(se_dev->dev, "\n RNG1 initial state is wrong\n");
		goto rel_mutex;
	}

	ret = tegra_se_elp_rng_do(se_dev, req);
rel_mutex:
	tegra_se_release_rng_mutex(se_dev);
	return ret;
}
EXPORT_SYMBOL(tegra_se_elp_rng_op);

int tegra_se_elp_pka_op(struct tegra_se_elp_pka_request *req)
{
	struct tegra_se_elp_dev *se_dev = elp_dev;
	int ret = 0;

	ret = tegra_se_acquire_pka_mutex(se_dev);
	if (ret) {
		dev_err(se_dev->dev, "\nPKA Mutex acquire failed\n");
		return ret;
	}

	ret = tegra_se_elp_pka_init(req);
	if (ret)
		goto mutex_rel;

	ret = tegra_se_check_trng_op(se_dev);
	if (ret)
		ret = tegra_se_set_trng_op(se_dev);
	if (ret) {
		dev_err(se_dev->dev, "\nset_trng_op Failed\n");
		goto exit;
	}
	ret = tegra_se_elp_pka_precomp(se_dev, req, PRECOMP_RINV);
	if (ret) {
		dev_err(se_dev->dev,
		"\ntegra_se_elp_pka_precomp Failed(%d) for RINV\n", ret);
		goto exit;
	}
	ret = tegra_se_elp_pka_precomp(se_dev, req, PRECOMP_M);
	if (ret) {
		dev_err(se_dev->dev,
			"\ntegra_se_elp_pka_precomp Failed(%d) for M\n", ret);
		goto exit;
	}
	ret = tegra_se_elp_pka_precomp(se_dev, req, PRECOMP_R2);
	if (ret) {
		dev_err(se_dev->dev,
			"\ntegra_se_elp_pka_precomp Failed(%d) for R2\n", ret);
		goto exit;
	}
	tegra_se_elp_pka_do(se_dev, req);
exit:
	tegra_se_elp_pka_exit(req);
mutex_rel:
	tegra_se_release_pka_mutex(se_dev);
	return ret;
}
EXPORT_SYMBOL(tegra_se_elp_pka_op);

struct tegra_se_elp_rng_context {
	struct tegra_se_elp_dev *se_dev;
};

static int tegra_se_elp_get_rng1_result(struct crypto_rng *tfm, u32 *rdata)
{
	struct tegra_se_elp_rng_context *rng_ctx = crypto_rng_ctx(tfm);
	struct tegra_se_elp_dev *se_dev = rng_ctx->se_dev;
	u32 i, val;

	for (i = 0; i < 4; i++) {
		val = se_elp_readl(se_dev, RNG1,
				TEGRA_SE_ELP_RNG_RAND0_OFFSET + i*4);
		if (!val) {
			dev_err(se_dev->dev, "\nNo random data from RAND\n");
			return -1;
		}
		rdata[i] = val;
	}

	return 0;
}

static int tegra_se_elp_rng_get(struct crypto_rng *tfm,
		u8 *rdata, unsigned int dlen)
{
	struct tegra_se_elp_rng_context *rng_ctx = crypto_rng_ctx(tfm);
	struct tegra_se_elp_dev *se_dev = rng_ctx->se_dev;
	int i, ret = 0;
	u32 *rand_num, *temp_rand;

	if (dlen < RAND_128 || (dlen % RAND_128 != 0))
		return -EINVAL;

	rand_num = devm_kzalloc(se_dev->dev, dlen, GFP_KERNEL);
	if (!rand_num)
		return -EINVAL;

	temp_rand = rand_num;
	tegra_se_set_rng1_smode(true, false);
	tegra_se_set_rng1_mode(RNG1_MODE_SEC_ALG);

	/* Generate Noise */
	ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_GEN_NOISE);
	if (ret)
		goto ret;
	ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_CREATE_STATE);
	if (ret)
		goto ret;

	for (i = 0; i < dlen/RAND_128; i++) {
		ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_GEN_RANDOM);
		if (ret)
			goto ret;

		ret = tegra_se_elp_get_rng1_result(tfm, rand_num);
		if (ret < -1) {
			dev_err(se_dev->dev, "\nRNG1 Failed for Sub-Step 1\n");
			goto ret;
		}

		ret = tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_ADVANCE_STATE);
		if (ret)
			goto ret;
		if ((i+1) < dlen/RAND_128) {
			rand_num += 4;
			ret = tegra_se_execute_rng1_ctrl_cmd(
				RNG1_CMD_GEN_NOISE);
			if (ret)
				goto ret;
			ret =
				tegra_se_execute_rng1_ctrl_cmd(
					RNG1_CMD_RENEW_STATE);
			if (ret)
				goto ret;
		}
	}

	memcpy(rdata, temp_rand, dlen);

	tegra_se_check_rng1_alarms();
	tegra_se_execute_rng1_ctrl_cmd(RNG1_CMD_ZEROIZE);
	ret = dlen;
ret:
	devm_kfree(se_dev->dev, temp_rand);

	return ret;
}

#if KERNEL_VERSION(4, 3, 0) < LINUX_VERSION_CODE
static int tegra_se_elp_rng_get_random(struct crypto_rng *tfm,
		const u8 *src, unsigned int slen,
		u8 *rdata, unsigned int dlen)
#else
static int tegra_se_elp_rng_get_random(struct crypto_rng *tfm,
		u8 *rdata, u32 dlen)
#endif
{
	struct tegra_se_elp_rng_context *rng_ctx = crypto_rng_ctx(tfm);
	struct tegra_se_elp_dev *se_dev = rng_ctx->se_dev;
	int ret = 0;

	clk_prepare_enable(se_dev->c);
	ret = tegra_se_acquire_rng_mutex(se_dev);
	if (ret) {
		dev_err(se_dev->dev, "\n RNG Mutex acquire failed\n");
		clk_disable_unprepare(se_dev->c);
		return ret;
	}

	ret = tegra_se_check_rng_status(se_dev);
	if (ret) {
		dev_err(se_dev->dev, "\n RNG1 initial state is wrong\n");
		goto rel_mutex;
	}

	ret = tegra_se_elp_rng_get(tfm, rdata, dlen);

rel_mutex:
	tegra_se_release_rng_mutex(se_dev);
	clk_disable_unprepare(se_dev->c);
	return ret;

}


#if KERNEL_VERSION(4, 3, 0) < LINUX_VERSION_CODE
static int tegra_se_elp_rng_reset(struct crypto_rng *tfm, const u8 *seed,
		unsigned int slen)
#else
static int tegra_se_elp_rng_reset(struct crypto_rng *tfm, u8 *seed, u32 slen)
#endif
{
	return 0;
}

static int tegra_se_elp_rng_init(struct crypto_tfm *tfm)
{
	struct tegra_se_elp_rng_context *rng_ctx = crypto_tfm_ctx(tfm);

	rng_ctx->se_dev = elp_dev;
	return 0;
}

static void tegra_se_elp_rng_exit(struct crypto_tfm *tfm)
{
	struct tegra_se_elp_rng_context *rng_ctx = crypto_tfm_ctx(tfm);

	rng_ctx->se_dev = NULL;
}

#if KERNEL_VERSION(4, 3, 0) < LINUX_VERSION_CODE
static struct rng_alg rng_algs[] = { {
	.generate       = tegra_se_elp_rng_get_random,
	.seed           = tegra_se_elp_rng_reset,
	.base           = {
			.cra_name = "rng1_elp",
			.cra_driver_name = "rng1_elp-tegra",
			.cra_priority = 300,
			.cra_flags = CRYPTO_ALG_TYPE_RNG,
			.cra_ctxsize = sizeof(struct tegra_se_elp_rng_context),
			.cra_module = THIS_MODULE,
			.cra_init = tegra_se_elp_rng_init,
			.cra_exit = tegra_se_elp_rng_exit,
	}
} };
#elif KERNEL_VERSION(3, 19, 0) > LINUX_VERSION_CODE
static struct crypto_alg rng_algs[] = {
	{
		.cra_name = "rng1_elp",
		.cra_driver_name = "rng1_elp-tegra",
		.cra_priority = 300,
		.cra_flags = CRYPTO_ALG_TYPE_RNG,
		.cra_ctxsize = sizeof(struct tegra_se_elp_rng_context),
		.cra_type = &crypto_rng_type,
		.cra_module = THIS_MODULE,
		.cra_init = tegra_se_elp_rng_init,
		.cra_exit = tegra_se_elp_rng_exit,
		.cra_u = {
			.rng = {
				.rng_make_random = tegra_se_elp_rng_get_random,
				.rng_reset = tegra_se_elp_rng_reset,
			}
		}
	}
};
#endif

static struct tegra_se_chipdata tegra18_se_chipdata = {
	.use_key_slot = false,
};

static struct of_device_id tegra_se_elp_of_match[] = {
	{
		.compatible = "nvidia,tegra186-se-elp",
		.data = &tegra18_se_chipdata,
	},
};
MODULE_DEVICE_TABLE(of, tegra_se_elp_of_match);

static int tegra_se_elp_probe(struct platform_device *pdev)
{
	struct tegra_se_elp_dev *se_dev = NULL;
	struct resource *res = NULL;
	const struct of_device_id *match;
	int err = 0;

	se_dev = devm_kzalloc(&pdev->dev, sizeof(struct tegra_se_elp_dev),
					GFP_KERNEL);
	if (!se_dev) {
		dev_err(&pdev->dev, "se_dev memory allocation failed\n");
		return -ENOMEM;
	}

	if (pdev->dev.of_node) {
		match = of_match_device(of_match_ptr(tegra_se_elp_of_match),
				&pdev->dev);
		if (!match) {
			dev_err(&pdev->dev, "Error: No device match found\n");
			devm_kfree(&pdev->dev, se_dev);
			return -ENODEV;
		}
		se_dev->chipdata = (struct tegra_se_chipdata *)match->data;
	} else {
		se_dev->chipdata =
			(struct tegra_se_chipdata *)pdev->id_entry->driver_data;
	}

	platform_set_drvdata(pdev, se_dev);
	se_dev->dev = &pdev->dev;
	se_dev->pdev = pdev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		err = -ENXIO;
		dev_err(se_dev->dev, "platform_get_resource failed for pka1\n");
		goto fail;
	}

	se_dev->io_reg[0] = ioremap(res->start, resource_size(res));
	if (!se_dev->io_reg[0]) {
		err = -ENOMEM;
		dev_err(se_dev->dev, "ioremap failed for pka1\n");
		goto fail;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		err = -ENXIO;
		dev_err(se_dev->dev, "platform_get_resource failed for rng1\n");
		goto res_fail;
	}

	se_dev->io_reg[1] = ioremap(res->start, resource_size(res));
	if (!se_dev->io_reg[1]) {
		err = -ENOMEM;
		dev_err(se_dev->dev, "ioremap failed for rng1\n");
		goto res_fail;
	}

	se_dev->c = devm_clk_get(se_dev->dev, "se");
	if (IS_ERR(se_dev->c)) {
		err = PTR_ERR(se_dev->c);
		dev_err(se_dev->dev, "clk_get_sys failed for se: %d\n", err);
		goto clk_get_fail;
	}

	err = clk_prepare_enable(se_dev->c);
	if (err) {
		dev_err(se_dev->dev, "clk enable failed for se\n");
		goto clk_en_fail;
	}
	elp_dev = se_dev;

	err = tegra_se_pka_init_key_slot();
	if (err) {
		dev_err(se_dev->dev, "tegra_se_pka_init_key_slot failed\n");
		goto kslt_fail;
	}

#if KERNEL_VERSION(3, 19, 0) > LINUX_VERSION_CODE
	err = crypto_register_alg(&rng_algs[0]);
#else
	err = crypto_register_rng(&rng_algs[0]);
#endif
	if (err) {
		dev_err(se_dev->dev,
			"crypto_register_rng failed\n");
		goto kslt_fail;
	}

	dev_info(se_dev->dev, "%s: complete", __func__);
	return 0;

kslt_fail:
	clk_disable_unprepare(se_dev->c);
clk_en_fail:
	devm_clk_put(se_dev->dev, se_dev->c);
clk_get_fail:
	iounmap(se_dev->io_reg[1]);
res_fail:
	iounmap(se_dev->io_reg[0]);
fail:
	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, se_dev);
	elp_dev = NULL;

	return err;
}

static int tegra_se_elp_remove(struct platform_device *pdev)
{
	struct tegra_se_elp_dev *se_dev = platform_get_drvdata(pdev);
	int i;

	if (!se_dev)
		return -ENODEV;

#if KERNEL_VERSION(3, 19, 0) > LINUX_VERSION_CODE
	crypto_unregister_alg(&rng_algs[0]);
#else
	crypto_unregister_rng(&rng_algs[0]);
#endif

	for (i = 0; i < 2; i++)
		iounmap(se_dev->io_reg[i]);
	clk_disable_unprepare(se_dev->c);
	devm_clk_put(se_dev->dev, se_dev->c);
	devm_kfree(&pdev->dev, se_dev);
	elp_dev = NULL;

	return 0;
}


static struct platform_device_id tegra_dev_se_elp_devtype[] = {
	{
		.name = "tegra-se-elp",
		.driver_data = (unsigned long)&tegra18_se_chipdata,
	}
};

static struct platform_driver tegra_se_elp_driver = {
	.probe  = tegra_se_elp_probe,
	.remove = tegra_se_elp_remove,
	.id_table = tegra_dev_se_elp_devtype,
	.driver = {
		.name   = "tegra-se-elp",
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(tegra_se_elp_of_match),
	},
};

static int __init tegra_se_elp_module_init(void)
{
	return  platform_driver_register(&tegra_se_elp_driver);
}

static void __exit tegra_se_elp_module_exit(void)
{
	platform_driver_unregister(&tegra_se_elp_driver);
}

module_init(tegra_se_elp_module_init);
module_exit(tegra_se_elp_module_exit);

MODULE_DESCRIPTION("Tegra Elliptic Crypto algorithm support");
MODULE_AUTHOR("NVIDIA Corporation");
MODULE_LICENSE("GPL");
MODULE_ALIAS("tegra-se-elp");

