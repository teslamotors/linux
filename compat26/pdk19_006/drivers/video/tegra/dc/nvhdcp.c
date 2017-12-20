/*
 * drivers/video/tegra/dc/nvhdcp.c
 *
 * Copyright (c) 2010-2011, NVIDIA Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <asm/atomic.h>

#include <mach/dc.h>
#include <mach/nvhost.h>
#include <mach/kfuse.h>

#include <video/nvhdcp.h>

#include "dc_reg.h"
#include "dc_priv.h"
#include "hdmi_reg.h"
#include "hdmi.h"

#define BCAPS_REPEATER (1 << 6)
#define BCAPS_READY (1 << 5)
#define BCAPS_11 (1 << 1) /* used for both Bcaps and Ainfo */

// TODO remove this
#define nvhdcp_debug(...) pr_err("nvhdcp: " __VA_ARGS__)
#define nvhdcp_err(...) pr_err("nvhdcp: Error: " __VA_ARGS__)
#define nvhdcp_info(...) pr_info("nvhdcp: " __VA_ARGS__)

/* for nvhdcp.state */
enum {
	STATE_OFF,
	STATE_UNAUTHENTICATED,
	STATE_LINK_VERIFY,
	STATE_RENEGOTIATE,
};

struct tegra_nvhdcp {
	struct work_struct work;
	struct tegra_dc_hdmi_data *hdmi;
	struct workqueue_struct *downstream_wq;
	struct mutex lock;
	struct miscdevice miscdev;
	char name[12];
	unsigned id;
	atomic_t plugged; /* true if hotplug detected */
	atomic_t policy; /* set policy */
	atomic_t state; /* STATE_xxx */

	u64 a_n;
	u64 c_n;
	u64 a_ksv;
	u64 b_ksv;
	u64 c_ksv;
	u64 d_ksv;
	u64 m_prime;
	u32 b_status;
};

#define TEGRA_NVHDCP_NUM_AP 1
static struct tegra_nvhdcp *nvhdcp_dev[TEGRA_NVHDCP_NUM_AP];
static int nvhdcp_ap;

static int nvhdcp_i2c_read(struct tegra_dc_hdmi_data *hdmi, u8 reg, size_t len, u8 *data)
{
	int status;
	struct i2c_msg msg[] = {
		{
			.addr = 0x74 >> 1, /* primary link */
			.flags = 0,
			.len = 1,
			.buf = &reg,
		},
		{
			.addr = 0x74 >> 1, /* primary link */
			.flags = I2C_M_RD,
			.len = len,
			.buf = data,
		},
	};

	status = tegra_hdmi_i2c(hdmi, msg, ARRAY_SIZE(msg));

	if (status < 0) {
		nvhdcp_err("i2c xfer error %d\n", status);
		return status;
	}

	return 0;
}

static int nvhdcp_i2c_write(struct tegra_dc_hdmi_data *hdmi, u8 reg, size_t len, const u8 *data)
{
	int status;
	u8 buf[len + 1];
	struct i2c_msg msg[] = {
		{
			.addr = 0x74 >> 1, /* primary link */
			.flags = 0,
			.len = len + 1,
			.buf = buf,
		},
	};

	buf[0] = reg;
	memcpy(buf + 1, data, len);

	status = tegra_hdmi_i2c(hdmi, msg, ARRAY_SIZE(msg));

	if (status < 0) {
		nvhdcp_err("i2c xfer error %d\n", status);
		return status;
	}

	return 0;
}

static int nvhdcp_i2c_read8(struct tegra_dc_hdmi_data *hdmi, u8 reg, u8 *val)
{
	return nvhdcp_i2c_read(hdmi, reg, 1, val);
}

static int nvhdcp_i2c_write8(struct tegra_dc_hdmi_data *hdmi, u8 reg, u8 val)
{
	return nvhdcp_i2c_write(hdmi, reg, 1, &val);
}

static int nvhdcp_i2c_read16(struct tegra_dc_hdmi_data *hdmi, u8 reg, u16 *val)
{
	u8 buf[2];
	int e;

	e = nvhdcp_i2c_read(hdmi, reg, sizeof buf, buf);
	if (e)
		return e;

	if (val)
		*val = buf[0] | (u16)buf[1] << 8;

	return 0;
}

static int nvhdcp_i2c_read40(struct tegra_dc_hdmi_data *hdmi, u8 reg, u64 *val)
{
	u8 buf[5];
	int e, i;
	u64 n;

	e = nvhdcp_i2c_read(hdmi, reg, sizeof buf, buf);
	if (e)
		return e;

	for(i = 0, n = 0; i < 5; i++ ) {
		n <<= 8;
		n |= buf[4 - i];
	}

	if (val)
		*val = n;

	return 0;
}

static int nvhdcp_i2c_write40(struct tegra_dc_hdmi_data *hdmi, u8 reg, u64 val)
{
	char buf[5];
	int i;
	for(i = 0; i < 5; i++ ) {
		buf[i] = val;
		val >>= 8;
	}
	return nvhdcp_i2c_write(hdmi, reg, sizeof buf, buf);
}

static int nvhdcp_i2c_write64(struct tegra_dc_hdmi_data *hdmi, u8 reg, u64 val)
{
	char buf[8];
	int i;
	for(i = 0; i < 8; i++ ) {
		buf[i] = val;
		val >>= 8;
	}
	return nvhdcp_i2c_write(hdmi, reg, sizeof buf, buf);
}


/* 64-bit link encryption session random number */
static u64 get_an(struct tegra_dc_hdmi_data *hdmi)
{
	u64 r;
	r = (u64)tegra_hdmi_readl(hdmi, HDMI_NV_PDISP_RG_HDCP_AN_MSB) << 32;
	r |= tegra_hdmi_readl(hdmi, HDMI_NV_PDISP_RG_HDCP_AN_LSB);
	return r;
}

/* 64-bit upstream exchange random number */
static void set_cn(struct tegra_dc_hdmi_data *hdmi, u64 c_n)
{
	tegra_hdmi_writel(hdmi, (u32)c_n, HDMI_NV_PDISP_RG_HDCP_CN_LSB);
	tegra_hdmi_writel(hdmi, c_n >> 32, HDMI_NV_PDISP_RG_HDCP_CN_MSB);
}


/* 40-bit transmitter's key selection vector */
static inline u64 get_aksv(struct tegra_dc_hdmi_data *hdmi)
{
	u64 r;
	r = (u64)tegra_hdmi_readl(hdmi, HDMI_NV_PDISP_RG_HDCP_AKSV_MSB) << 32;
	r |= tegra_hdmi_readl(hdmi, HDMI_NV_PDISP_RG_HDCP_AKSV_LSB);
	return r;
}

/* 40-bit receiver's key selection vector */
static inline u64 get_bksv(struct tegra_dc_hdmi_data *hdmi)
{
	u64 r;
	r = (u64)tegra_hdmi_readl(hdmi, HDMI_NV_PDISP_RG_HDCP_BKSV_MSB) << 32;
	r |= tegra_hdmi_readl(hdmi, HDMI_NV_PDISP_RG_HDCP_BKSV_LSB);
	return r;
}

/* 40-bit software's key selection vector */
static void set_cksv(struct tegra_dc_hdmi_data *hdmi, u64 c_ksv)
{
	tegra_hdmi_writel(hdmi, (u32)c_ksv, HDMI_NV_PDISP_RG_HDCP_CKSV_LSB);
	tegra_hdmi_writel(hdmi, c_ksv >> 32, HDMI_NV_PDISP_RG_HDCP_CKSV_MSB);
}

/* 40-bit connection state */
static inline u64 get_cs(struct tegra_dc_hdmi_data *hdmi)
{
	u64 r;
	r = (u64)tegra_hdmi_readl(hdmi, HDMI_NV_PDISP_RG_HDCP_CS_MSB) << 32;
	r |= tegra_hdmi_readl(hdmi, HDMI_NV_PDISP_RG_HDCP_CS_LSB);
	return r;
}

/* 40-bit upstream key selection vector */
static inline u64 get_dksv(struct tegra_dc_hdmi_data *hdmi)
{
	u64 r;
	r = (u64)tegra_hdmi_readl(hdmi, HDMI_NV_PDISP_RG_HDCP_DKSV_MSB) << 32;
	r |= tegra_hdmi_readl(hdmi, HDMI_NV_PDISP_RG_HDCP_DKSV_LSB);
	return r;
}

/* 64-bit encrypted M0 value */
static inline u64 get_mprime(struct tegra_dc_hdmi_data *hdmi)
{
	u64 r;
	r = (u64)tegra_hdmi_readl(hdmi, HDMI_NV_PDISP_RG_HDCP_MPRIME_MSB) << 32;
	r |= tegra_hdmi_readl(hdmi, HDMI_NV_PDISP_RG_HDCP_MPRIME_LSB);
	return r;
}

/* 56-bit S' value + some status bits in the upper 8-bits
 * STATUS_CMODE_IDX 31:28
 * STATUS_UNPROTECTED 27
 * STATUS_EXTPNL 26
 * STATUS_RPTR 25
 * STATUS_ENCRYPTING 24
 */
static inline u64 get_sprime(struct tegra_dc_hdmi_data *hdmi)
{
	u64 r;
	r = (u64)tegra_hdmi_readl(hdmi, HDMI_NV_PDISP_RG_HDCP_SPRIME_LSB2) << 32;
	r |= tegra_hdmi_readl(hdmi, HDMI_NV_PDISP_RG_HDCP_SPRIME_LSB1);
	return r;
}

static inline u16 get_transmitter_ri(struct tegra_dc_hdmi_data *hdmi)
{
	return tegra_hdmi_readl(hdmi, HDMI_NV_PDISP_RG_HDCP_RI);
}

static inline int get_receiver_ri(struct tegra_dc_hdmi_data *hdmi, u16 *r)
{
	// TODO: implement short Ri read format
	return nvhdcp_i2c_read16(hdmi, 0x8, r); /* long read */
}

static int get_bcaps(struct tegra_dc_hdmi_data *hdmi, u8 *b_caps)
{
	int e, retries = 3;
	do {
		e = nvhdcp_i2c_read8(hdmi, 0x40, b_caps);
		if (!e)
			return 0;
		msleep(100);
	} while (--retries);

	return -EIO;
}

/* set or clear RUN_YES */
static void hdcp_ctrl_run(struct tegra_dc_hdmi_data *hdmi, bool v)
{
	u32 ctrl;
	ctrl = tegra_hdmi_readl(hdmi, HDMI_NV_PDISP_RG_HDCP_CTRL);

	if (v)
		ctrl |= HDCP_RUN_YES;
	else
		ctrl = 0;

	tegra_hdmi_writel(hdmi, ctrl, HDMI_NV_PDISP_RG_HDCP_CTRL);
}

/* wait for any bits in mask to be set in HDMI_NV_PDISP_RG_HDCP_CTRL
 * sleeps up to 120mS */
static int wait_hdcp_ctrl(struct tegra_dc_hdmi_data *hdmi, u32 mask, u32 *v)
{
	int retries = 12;
	u32 ctrl;

	do {
		ctrl = tegra_hdmi_readl(hdmi, HDMI_NV_PDISP_RG_HDCP_CTRL);
		if ((ctrl | (mask))) {
			if (v)
				*v = ctrl;
			break;
		}
		msleep(10);
	} while (--retries);
	if (!retries) {
		nvhdcp_err("ctrl read timeout (mask=0x%x)\n", mask);
		return -EIO;
	}
	return 0;
}

/* wait for any bits in mask to be set in HDMI_NV_PDISP_KEY_CTRL
 * waits up to 100mS */
static int wait_key_ctrl(struct tegra_dc_hdmi_data *hdmi, u32 mask)
{
	int retries = 100;
	u32 ctrl;

	do {
		ctrl = tegra_hdmi_readl(hdmi, HDMI_NV_PDISP_KEY_CTRL);
		if ((ctrl | (mask)))
			break;
		msleep(1);
	} while (--retries);
	if (!retries) {
		nvhdcp_err("key ctrl read timeout (mask=0x%x)\n", mask);
		return -EIO;
	}
	return 0;
}

/* check that key selection vector is well formed.
 * NOTE: this function assumes KSV has already been checked against
 * revocation list.
 */
static int verify_ksv(u64 k)
{
	unsigned i;

	/* count set bits, must be exactly 20 set to be valid */
	for(i = 0; k; i++)
		k ^= k & -k;

	return  (i != 20) ? -EINVAL : 0;
}

/* get Status and Kprime signature - READ_S on TMDS0_LINK0 only */
static int get_s_prime(struct tegra_nvhdcp *nvhdcp, struct tegra_nvhdcp_packet *pkt)
{
	struct tegra_dc_hdmi_data *hdmi = nvhdcp->hdmi;
	u32 sp_msb, sp_lsb1, sp_lsb2;
	int e;

	pkt->packet_results = TEGRA_NVHDCP_RESULT_UNSUCCESSFUL;

	/* we will be taking c_n, c_ksv as input */
	if (!(pkt->value_flags & TEGRA_NVHDCP_FLAG_CN)
			|| !(pkt->value_flags & TEGRA_NVHDCP_FLAG_CKSV)) {
		nvhdcp_err("missing value_flags (0x%x)\n", pkt->value_flags);
		return -EINVAL;
	}

	pkt->value_flags = 0;

	pkt->a_ksv = nvhdcp->a_ksv;
	pkt->a_n = nvhdcp->a_n;
	pkt->value_flags = TEGRA_NVHDCP_FLAG_AKSV | TEGRA_NVHDCP_FLAG_AN;

	nvhdcp_debug("%s():cn %llx cksv %llx\n", __func__, pkt->c_n, pkt->c_ksv);

	set_cn(hdmi, pkt->c_n);

	tegra_hdmi_writel(hdmi, TMDS0_LINK0 | READ_S,
					HDMI_NV_PDISP_RG_HDCP_CMODE);

	set_cksv(hdmi, pkt->c_ksv);

	e = wait_hdcp_ctrl(hdmi, SPRIME_VALID, NULL);
	if (e) {
		nvhdcp_err("Sprime read timeout\n");
		pkt->packet_results = TEGRA_NVHDCP_RESULT_UNSUCCESSFUL;
		return -EIO;
	}

	msleep(100);

	/* read 56-bit Sprime plus 16 status bits */
	sp_msb = tegra_hdmi_readl(hdmi, HDMI_NV_PDISP_RG_HDCP_SPRIME_MSB);
	sp_lsb1 = tegra_hdmi_readl(hdmi, HDMI_NV_PDISP_RG_HDCP_SPRIME_LSB1);
	sp_lsb2 = tegra_hdmi_readl(hdmi, HDMI_NV_PDISP_RG_HDCP_SPRIME_LSB2);

	/* top 8 bits of LSB2 and bottom 8 bits of MSB hold status bits. */
	pkt->hdcp_status = ( sp_msb << 8 ) | ( sp_lsb2 >> 24);
	pkt->value_flags |= TEGRA_NVHDCP_FLAG_S;

	/* 56-bit Kprime */
	pkt->k_prime = ((u64)(sp_lsb2 & 0xffffff) << 32) | sp_lsb1;
	pkt->value_flags |= TEGRA_NVHDCP_FLAG_KP;

	/* is connection state supported? */
	if (sp_msb & STATUS_CS) {
		pkt->cs = get_cs(hdmi);
		pkt->value_flags |= TEGRA_NVHDCP_FLAG_CS;
	}

	/* load Dksv */
	pkt->d_ksv = get_dksv(hdmi);
	if (verify_ksv(pkt->d_ksv)) {
		nvhdcp_err("Dksv invalid!\n");
		pkt->packet_results = TEGRA_NVHDCP_RESULT_UNSUCCESSFUL;
		return -EIO; /* treat bad Dksv as I/O error */
	}
	pkt->value_flags |= TEGRA_NVHDCP_FLAG_DKSV;

	/* copy current Bksv */
	pkt->b_ksv = nvhdcp->b_ksv;
	pkt->value_flags |= TEGRA_NVHDCP_FLAG_BKSV;

	pkt->packet_results = TEGRA_NVHDCP_RESULT_SUCCESS;
	return 0;
}

/* get M prime - READ_M on TMDS0_LINK0 only */
static inline int get_m_prime(struct tegra_nvhdcp *nvhdcp, struct tegra_nvhdcp_packet *pkt)
{
	struct tegra_dc_hdmi_data *hdmi = nvhdcp->hdmi;
	int e;

	pkt->packet_results = TEGRA_NVHDCP_RESULT_UNSUCCESSFUL;

	/* if connection isn't authenticated ... */
	if (atomic_read(&nvhdcp->state) == STATE_UNAUTHENTICATED) {
		memset(pkt, 0, sizeof *pkt);
		pkt->packet_results = TEGRA_NVHDCP_RESULT_LINK_FAILED;
		return 0;
	}

	pkt->a_ksv = nvhdcp->a_ksv;
	pkt->a_n = nvhdcp->a_n;
	pkt->value_flags = TEGRA_NVHDCP_FLAG_AKSV | TEGRA_NVHDCP_FLAG_AN;

	set_cn(hdmi, pkt->c_n);

	// TODO: original code used TMDS0_LINK1, weird
	tegra_hdmi_writel(hdmi, TMDS0_LINK0 | READ_M,
					HDMI_NV_PDISP_RG_HDCP_CMODE);

	/* Cksv write triggers Mprime update */
	set_cksv(hdmi, pkt->c_ksv);

	e = wait_hdcp_ctrl(hdmi, MPRIME_VALID, NULL);
	if (e) {
		nvhdcp_err("Mprime read timeout\n");
		return -EIO;
	}

	/* load Mprime */
	pkt->m_prime = (u64)tegra_hdmi_readl(hdmi,
				HDMI_NV_PDISP_RG_HDCP_MPRIME_MSB) << 32;
	pkt->m_prime |= tegra_hdmi_readl(hdmi,
				HDMI_NV_PDISP_RG_HDCP_MPRIME_LSB);
	pkt->value_flags |= TEGRA_NVHDCP_FLAG_MP;

	/* load Dksv */
	pkt->d_ksv = get_dksv(hdmi);
	if (verify_ksv(pkt->d_ksv)) {
		nvhdcp_err("Dksv invalid!\n");
		return -EIO; /* treat bad Dksv as I/O error */
	}
	pkt->value_flags |= TEGRA_NVHDCP_FLAG_DKSV;

	pkt->packet_results = TEGRA_NVHDCP_RESULT_SUCCESS;
	return 0;
}

static int load_kfuse(struct tegra_dc_hdmi_data *hdmi, bool repeater)
{
	unsigned buf[KFUSE_DATA_SZ / 4];
	int e, i;
	u32 ctrl;
	u32 tmp;
	int retries;

	/* copy load kfuse into buffer - only needed for early Tegra parts */
	e = tegra_kfuse_read(buf, sizeof buf);
	if (e) {
		nvhdcp_err("Kfuse read failure\n");
		return e;
	}

	/* write the kfuse to HDMI SRAM */
	if (repeater) {
		tegra_hdmi_writel(hdmi, 0, HDMI_NV_PDISP_RG_HDCP_BKSV_LSB);
		tegra_hdmi_writel(hdmi, REPEATER,
					HDMI_NV_PDISP_RG_HDCP_BKSV_MSB);
	}

	tegra_hdmi_writel(hdmi, 1, HDMI_NV_PDISP_KEY_CTRL); /* LOAD_KEYS */

	/* issue a reload */
	ctrl = tegra_hdmi_readl(hdmi, HDMI_NV_PDISP_KEY_CTRL);
	tegra_hdmi_writel(hdmi, ctrl | PKEY_REQUEST_RELOAD_TRIGGER
					| LOCAL_KEYS , HDMI_NV_PDISP_KEY_CTRL);

	e = wait_key_ctrl(hdmi, PKEY_LOADED);
	if (e) {
		nvhdcp_err("key reload timeout\n");
		return -EIO;
	}

	tegra_hdmi_writel(hdmi, 0, HDMI_NV_PDISP_KEY_SKEY_INDEX);

	/* wait for SRAM to be cleared */
	retries = 5;
	do {
		tmp = tegra_hdmi_readl(hdmi, HDMI_NV_PDISP_KEY_DEBUG0);
		if ((tmp & 1) == 0) break;
		mdelay(1);
	} while (--retries);
	if (!retries) {
		nvhdcp_err("key SRAM clear timeout\n");
		return -EIO;
	}

	for (i = 0; i < KFUSE_DATA_SZ / 4; i += 4) {

		/* load 128-bits*/
		tegra_hdmi_writel(hdmi, buf[i], HDMI_NV_PDISP_KEY_HDCP_KEY_0);
		tegra_hdmi_writel(hdmi, buf[i+1], HDMI_NV_PDISP_KEY_HDCP_KEY_1);
		tegra_hdmi_writel(hdmi, buf[i+2], HDMI_NV_PDISP_KEY_HDCP_KEY_2);
		tegra_hdmi_writel(hdmi, buf[i+3], HDMI_NV_PDISP_KEY_HDCP_KEY_3);

		/* trigger LOAD_HDCP_KEY */
		tegra_hdmi_writel(hdmi, 0x100, HDMI_NV_PDISP_KEY_HDCP_KEY_TRIG);

		tmp = 0x11; /* LOCAL_KEYS | WRITE16 */
		if (i)
			tmp |= 0x2; /* AUTOINC */
		tegra_hdmi_writel(hdmi, tmp, HDMI_NV_PDISP_KEY_CTRL);

		/* wait for WRITE16 to complete */
		e = wait_key_ctrl(hdmi, 0x10); /* WRITE16 */
		if (e) {
			nvhdcp_err("key write timeout\n");
			return -EIO;
		}
	}

	return 0;
}

static int verify_link(struct tegra_nvhdcp *nvhdcp, bool wait_ri)
{
	struct tegra_dc_hdmi_data *hdmi = nvhdcp->hdmi;
	int retries = 3;
	u16 old, rx, tx;
	int e;

	old = 0;
	rx = 0;
	tx = 0;
	/* retry 3 times to deal with I2C link issues */
	do {
		if (wait_ri)
			old = get_transmitter_ri(hdmi);

		e = get_receiver_ri(hdmi, &rx);
		if (e)
			continue;

		if (!rx) {
			nvhdcp_err("Ri is 0!\n");
			return -EINVAL;
		}

		tx = get_transmitter_ri(hdmi);

	} while (wait_ri && --retries && old != tx);

	nvhdcp_debug("%s():rx(0x%04x) == tx(0x%04x)\n", __func__, rx, tx);

	if (!atomic_read(&nvhdcp->plugged)) {
		nvhdcp_err("aborting verify links - lost hdmi connection\n");
		return -EIO;
	}

	if (rx != tx)
		return -EINVAL;

	return 0;
}

static void nvhdcp_downstream_worker(struct work_struct *work)
{
	struct tegra_nvhdcp *nvhdcp =
	        container_of(work, struct tegra_nvhdcp, work);
	struct tegra_dc_hdmi_data *hdmi = nvhdcp->hdmi;
	int e;
	u8 b_caps;
	u32 tmp;
	u32 res;
	int st;

	nvhdcp_debug("%s():started thread %s\n", __func__, nvhdcp->name);

	st = atomic_read(&nvhdcp->state);
	if (st == STATE_OFF) {
		nvhdcp_err("nvhdcp failure - giving up\n");
		return;
	}

	atomic_set(&nvhdcp->state, STATE_UNAUTHENTICATED);

	/* check plug state to terminate early in case flush_workqueue() */
	if (!atomic_read(&nvhdcp->plugged)) {
		nvhdcp_err("worker started while unplugged!\n");
		goto lost_hdmi;
	}

	nvhdcp->a_ksv = 0;
	nvhdcp->b_ksv = 0;
	nvhdcp->a_n = 0;

	nvhdcp_debug("%s():hpd=%d\n", __func__, atomic_read(&nvhdcp->plugged));

	e = get_bcaps(hdmi, &b_caps);
	if (e) {
		nvhdcp_err("Bcaps read failure\n");
		goto failure;
	}

	nvhdcp_debug("read Bcaps = 0x%02x\n", b_caps);

	nvhdcp_debug("kfuse loading ...\n");

	e = load_kfuse(hdmi, (b_caps & BCAPS_REPEATER) != 0);
	if (e) {
		nvhdcp_err("kfuse could not be loaded\n");
		goto failure;
	}

	hdcp_ctrl_run(hdmi, 1);

	nvhdcp_debug("wait AN_VALID ...\n");

	/* wait for hardware to generate HDCP values */
	e = wait_hdcp_ctrl(hdmi, AN_VALID | SROM_ERR, &res);
	if (e) {
		nvhdcp_err("An key generation timeout\n");
		goto failure;
	}
	if (res & SROM_ERR) {
		nvhdcp_err("SROM error\n");
		goto failure;
	}

	nvhdcp->a_ksv = get_aksv(hdmi);
	nvhdcp->a_n = get_an(hdmi);
	nvhdcp_debug("Aksv is 0x%016llx\n", nvhdcp->a_ksv);
	nvhdcp_debug("An is 0x%016llx\n", nvhdcp->a_n);
	if (verify_ksv(nvhdcp->a_ksv)) {
		nvhdcp_err("Aksv verify failure!\n");
		goto failure;
	}

	/* write Ainfo to receiver - set 1.1 only if b_caps supports it */
	e = nvhdcp_i2c_write8(hdmi, 0x15, b_caps & BCAPS_11);
	if (e) {
		nvhdcp_err("Ainfo write failure\n");
		goto failure;
	}

	/* write An to receiver */
	e = nvhdcp_i2c_write64(hdmi, 0x18, nvhdcp->a_n);
	if (e) {
		nvhdcp_err("An write failure\n");
		goto failure;
	}

	nvhdcp_debug("wrote An = 0x%016llx\n", nvhdcp->a_n);

	/* write Aksv to receiver - triggers auth sequence */
	e = nvhdcp_i2c_write40(hdmi, 0x10, nvhdcp->a_ksv);
	if (e) {
		nvhdcp_err("Aksv write failure\n");
		goto failure;
	}

	nvhdcp_debug("wrote Aksv = 0x%010llx\n", nvhdcp->a_ksv);

	/* bail out if unplugged in the middle of negotiation */
	if (!atomic_read(&nvhdcp->plugged))
		goto lost_hdmi;

	/* get Bksv from reciever */
	e = nvhdcp_i2c_read40(hdmi, 0x00, &nvhdcp->b_ksv);
	if (e) {
		nvhdcp_err("Bksv read failure\n");
		goto failure;
	}
	nvhdcp_debug("Bksv is 0x%016llx\n", nvhdcp->b_ksv);
	if (verify_ksv(nvhdcp->b_ksv)) {
		nvhdcp_err("Bksv verify failure!\n");
		goto failure;
	}

	nvhdcp_debug("read Bksv = 0x%010llx from device\n", nvhdcp->b_ksv);

	/* LSB must be written first */
	tmp = (u32)nvhdcp->b_ksv;
	tegra_hdmi_writel(hdmi, tmp, HDMI_NV_PDISP_RG_HDCP_BKSV_LSB);
	// TODO: mix in repeater flags
	tmp = (u32)(nvhdcp->b_ksv >> 32);
	tegra_hdmi_writel(hdmi, tmp, HDMI_NV_PDISP_RG_HDCP_BKSV_MSB);

	nvhdcp_debug("load Bksv into controller\n");

	e = wait_hdcp_ctrl(hdmi, R0_VALID, NULL);
	if (e) {
		nvhdcp_err("R0 read failure!\n");
		goto failure;
	}

	nvhdcp_debug("R0 valid\n");

	/* can't read R0' within 100ms of writing Aksv */
	msleep(100);

	nvhdcp_debug("verifying links ...\n");

	e = verify_link(nvhdcp, false);
	if (e) {
		nvhdcp_err("link verification failed err %d\n", e);
		goto failure;
	}

	tmp = tegra_hdmi_readl(hdmi, HDMI_NV_PDISP_RG_HDCP_CTRL);
	tmp |= CRYPT_ENABLED;
	if (b_caps & BCAPS_11) /* HDCP 1.1 ? */
		tmp |= ONEONE_ENABLED;
	tegra_hdmi_writel(hdmi, tmp, HDMI_NV_PDISP_RG_HDCP_CTRL);

	nvhdcp_debug("CRYPT enabled\n");

	/* if repeater then get repeater info */
	if (b_caps & BCAPS_REPEATER) {
		u64 v_prime;
		u16 b_status;

		nvhdcp_debug("repeater stuff ... \n"); // TODO: remove comments

		/* Vprime */
		e = nvhdcp_i2c_read40(hdmi, 0x20, &v_prime);
		if (e) {
			nvhdcp_err("V' read failure!\n");
			goto failure;
		}
		// TODO: do somthing with Vprime

		// TODO: get Bstatus
		e = nvhdcp_i2c_read16(hdmi, 0x41, &b_status);
		if (e) {
			nvhdcp_err("Bstatus read failure!\n");
			goto failure;
		}

		nvhdcp_debug("Bstatus: device_count=%d\n", b_status & 0x7f);

		// TODO: read KSV fifo(0x43) for DEVICE_COUNT devices

		nvhdcp_err("not implemented - repeater info\n");
		goto failure;
	}

	atomic_set(&nvhdcp->state, STATE_LINK_VERIFY);
	nvhdcp_info("link verified!\n");

	while (atomic_read(&nvhdcp->state) == STATE_LINK_VERIFY) {
		if (!atomic_read(&nvhdcp->plugged))
			goto lost_hdmi;
		e = verify_link(nvhdcp, true);
		if (e) {
			nvhdcp_err("link verification failed err %d\n", e);
			goto failure;
		}
		msleep(1500);
	}

	return;
lost_hdmi:
	nvhdcp_err("lost hdmi connection\n");
failure:
	nvhdcp_err("nvhdcp failure - giving up\n");
	atomic_set(&nvhdcp->state, STATE_UNAUTHENTICATED);
	/* TODO: disable hdcp
	hdcp_ctrl_run(nvhdcp->hdmi, 0);
	*/
	return;
}

void tegra_nvhdcp_set_plug(struct tegra_nvhdcp *nvhdcp, bool hpd)
{
	nvhdcp_info("hdmi hotplug detected (hpd = %d)\n", hpd);

	atomic_set(&nvhdcp->plugged, hpd);

	if (hpd) {
		queue_work(nvhdcp->downstream_wq, &nvhdcp->work);
	} else {
		flush_workqueue(nvhdcp->downstream_wq);
	}
}

static int tegra_nvhdcp_on(struct tegra_nvhdcp *nvhdcp)
{
	atomic_set(&nvhdcp->state, STATE_UNAUTHENTICATED);
	if (atomic_read(&nvhdcp->plugged))
		queue_work(nvhdcp->downstream_wq, &nvhdcp->work);
	return 0;
}

static int tegra_nvhdcp_off(struct tegra_nvhdcp *nvhdcp)
{
	atomic_set(&nvhdcp->state, STATE_OFF);
	atomic_set(&nvhdcp->plugged, 0); /* force early termination */
	flush_workqueue(nvhdcp->downstream_wq);
	return 0;
}

int tegra_nvhdcp_set_policy(struct tegra_nvhdcp *nvhdcp, int pol)
{
	pol = !!pol;
	nvhdcp_info("using \"%s\" policy.\n", pol ? "always on" : "on demand");
	if (atomic_xchg(&nvhdcp->policy, pol) == TEGRA_NVHDCP_POLICY_ON_DEMAND
				&& pol == TEGRA_NVHDCP_POLICY_ALWAYS_ON) {
		/* policy was off but now it is on, start working */
		tegra_nvhdcp_on(nvhdcp);
	}
	return 0;
}

static int tegra_nvhdcp_renegotiate(struct tegra_nvhdcp *nvhdcp)
{
	atomic_set(&nvhdcp->state, STATE_RENEGOTIATE);
	tegra_nvhdcp_on(nvhdcp);
	return 0;
}

void tegra_nvhdcp_suspend(struct tegra_nvhdcp *nvhdcp)
{
	if (!nvhdcp) return;
	tegra_nvhdcp_off(nvhdcp);
}


static ssize_t
nvhdcp_dev_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
	struct tegra_nvhdcp *nvhdcp = filp->private_data;
	u16 data[2];
	int e;
	size_t len;

	if (!count)
		return 0;

	/* read Ri pairs as fast as possible into buffer */
	for (len = 0; (len + sizeof(data)) < count; len += sizeof(data)) {
		data[0] = get_transmitter_ri(nvhdcp->hdmi);
		data[1] = 0;
		e = get_receiver_ri(nvhdcp->hdmi, &data[1]);
		if (e)
			return e;

		memcpy(buf + len, data, 2);
	}
	return len;
}

static long nvhdcp_dev_ioctl(struct file *filp,
                unsigned int cmd, unsigned long arg)
{
	struct tegra_nvhdcp *nvhdcp = filp->private_data;
	struct tegra_nvhdcp_packet *pkt;
	int e = -ENOTTY;

	switch (cmd) {
	case TEGRAIO_NVHDCP_ON:
		return tegra_nvhdcp_on(nvhdcp);

	case TEGRAIO_NVHDCP_OFF:
		return tegra_nvhdcp_off(nvhdcp);

	case TEGRAIO_NVHDCP_SET_POLICY:
		return tegra_nvhdcp_set_policy(nvhdcp, arg);

	case TEGRAIO_NVHDCP_READ_M:
		pkt = kmalloc(sizeof(*pkt), GFP_KERNEL);
		if (!pkt)
			return -ENOMEM;
		if (copy_from_user(pkt, (void __user *)arg, sizeof(*pkt))) {
			e = -EFAULT;
			goto kfree_pkt;
		}
		e = get_m_prime(nvhdcp, pkt);
		if (copy_to_user((void __user *)arg, pkt, sizeof(*pkt))) {
			e = -EFAULT;
			goto kfree_pkt;
		}
		kfree(pkt);
		return e;

	case TEGRAIO_NVHDCP_READ_S:
		pkt = kmalloc(sizeof(*pkt), GFP_KERNEL);
		if (!pkt)
			return -ENOMEM;
		if (copy_from_user(pkt, (void __user *)arg, sizeof(*pkt))) {
			e = -EFAULT;
			goto kfree_pkt;
		}
		e = get_s_prime(nvhdcp, pkt);
		if (copy_to_user((void __user *)arg, pkt, sizeof(*pkt))) {
			e = -EFAULT;
			goto kfree_pkt;
		}
		kfree(pkt);
		return e;

	case TEGRAIO_NVHDCP_RENEGOTIATE:
		e = tegra_nvhdcp_renegotiate(nvhdcp);
		break;
	}

	return e;
kfree_pkt:
	kfree(pkt);
	return e;
}

/* every open indexes a new AP link */
static int nvhdcp_dev_open(struct inode *inode, struct file *filp)
{
	if (nvhdcp_ap >= TEGRA_NVHDCP_NUM_AP)
		return -EMFILE;

	if (!nvhdcp_dev[nvhdcp_ap])
		return -ENODEV;

	filp->private_data = nvhdcp_dev[nvhdcp_ap++];

	return 0;
}

static int nvhdcp_dev_release(struct inode *inode, struct file *filp)
{
	filp->private_data = NULL;

	--nvhdcp_ap;

	// TODO: deactivate something maybe?
	return 0;
}

static const struct file_operations nvhdcp_fops = {
	.owner          = THIS_MODULE,
	.llseek         = no_llseek,
	.read           = nvhdcp_dev_read,
	.unlocked_ioctl = nvhdcp_dev_ioctl,
	.open           = nvhdcp_dev_open,
	.release        = nvhdcp_dev_release,
};

struct tegra_nvhdcp *tegra_nvhdcp_create(struct tegra_dc_hdmi_data *hdmi, int id)
{
	struct tegra_nvhdcp *nvhdcp;
	int e;

	nvhdcp = kzalloc(sizeof(*nvhdcp), GFP_KERNEL);
	if (!nvhdcp)
		return ERR_PTR(-ENOMEM);

	nvhdcp->id = id;
	snprintf(nvhdcp->name, sizeof(nvhdcp->name), "nvhdcp%u", id);
	nvhdcp->hdmi = hdmi;
	mutex_init(&nvhdcp->lock);

	atomic_set(&nvhdcp->state, STATE_UNAUTHENTICATED);

	nvhdcp->downstream_wq = create_singlethread_workqueue(nvhdcp->name);
	INIT_WORK(&nvhdcp->work, nvhdcp_downstream_worker);

	nvhdcp->miscdev.minor = MISC_DYNAMIC_MINOR;
	nvhdcp->miscdev.name = nvhdcp->name;
	nvhdcp->miscdev.fops = &nvhdcp_fops;

	e = misc_register(&nvhdcp->miscdev);
	if (e)
		goto out1;

	nvhdcp_debug("%s(): created misc device %s\n", __func__, nvhdcp->name);

	nvhdcp_dev[0] = nvhdcp; /* we only support on AP right now */

	return nvhdcp;
out1:
	destroy_workqueue(nvhdcp->downstream_wq);
	kfree(nvhdcp);
	nvhdcp_err("unable to create device.\n");
	return ERR_PTR(e);
}

void tegra_nvhdcp_destroy(struct tegra_nvhdcp *nvhdcp)
{
	misc_deregister(&nvhdcp->miscdev);
	atomic_set(&nvhdcp->plugged, 0); /* force early termination */
	flush_workqueue(nvhdcp->downstream_wq);
	destroy_workqueue(nvhdcp->downstream_wq);
	kfree(nvhdcp);
}
