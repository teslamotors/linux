/*
 * Copyright (c) 2013-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _DRIVERS_BPMP_H
#define _DRIVERS_BPMP_H

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <soc/tegra/tegra_bpmp.h>

#define MSG_SZ			128
#define MSG_DATA_SZ		120

#ifdef CONFIG_ARCH_TEGRA_18x_SOC
#define NR_CHANNELS		14
#define NR_THREAD_CH		7
#else
#define NR_CHANNELS		12
#define NR_THREAD_CH		4
#endif

#define NR_MRQS			32
#define __MRQ_ATTRS		0xff000000
#define __MRQ_INDEX(id)		((id) & ~__MRQ_ATTRS)

#define DO_ACK			(1 << 0)
#define RING_DOORBELL		(1 << 1)

struct fops_entry {
	char *name;
	const struct file_operations *fops;
	mode_t mode;
};

struct bpmp_cpuidle_state {
	int id;
	const char *name;
};

struct mb_data {
	int32_t code;
	int32_t flags;
	u8 data[MSG_DATA_SZ];
} __packed;

struct channel_data {
	struct mb_data *ib;
	struct mb_data *ob;
};

extern struct channel_data channel_area[NR_CHANNELS];

#ifdef CONFIG_DEBUG_FS
int bpmp_fwdebug_init(struct dentry *root);
#else
static inline int bpmp_fwdebug_init(struct dentry *root) { return -ENODEV; }
#endif

#ifdef CONFIG_ARCH_TEGRA_21x_SOC
int bpmp_linear_map_init(struct device *device);
int bpmp_init_cpuidle_debug(struct dentry *root);
int bpmp_platdbg_init(struct dentry *root, struct platform_device *pdev);
int bpmp_clk_init(struct platform_device *pdev);
#else
void bpmp_setup_allocator(struct device *dev);
void *bpmp_get_virt_for_alloc(void *virt, dma_addr_t phys);
void *bpmp_get_virt_for_free(void *virt, dma_addr_t phys);
static inline int bpmp_linear_map_init(struct device *device) { return 0; }
static inline int bpmp_init_cpuidle_debug(struct dentry *root) { return 0; }
static inline int bpmp_platdbg_init(struct dentry *root,
		struct platform_device *pdev) { return 0; }
static inline int bpmp_clk_init(struct platform_device *pdev) { return 0; }
#endif

extern struct mutex bpmp_lock;
extern int connected;

int bpmp_mail_init_prepare(void);
int bpmp_mail_init(void);
int bpmp_get_fwtag(void);
int __bpmp_do_ping(void);
int bpmp_init_modules(struct platform_device *pdev);
void bpmp_cleanup_modules(void);
int bpmp_create_attrs(const struct fops_entry *fent, struct dentry *parent,
		void *data);
int bpmp_attach(void);
void bpmp_detach(void);
int bpmp_mailman_init(void);
void bpmp_handle_mail(int mrq, int ch);

void bpmp_ring_doorbell(int ch);
uint32_t bpmp_mail_token(void);
void bpmp_mail_token_set(uint32_t val);
void bpmp_mail_token_clr(uint32_t val);
int bpmp_thread_ch_index(int ch);
int bpmp_ob_channel(void);
int bpmp_thread_ch(int idx);
int bpmp_init_irq(void);
int bpmp_connect(void);
void bpmp_handle_irq(int ch);

bool bpmp_master_free(int ch);
bool bpmp_slave_signalled(int ch);
bool bpmp_master_acked(int ch);
void bpmp_signal_slave(int ch);
void bpmp_free_master(int ch);

#endif
