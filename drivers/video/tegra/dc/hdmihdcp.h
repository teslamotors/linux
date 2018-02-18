/*
 * drivers/video/tegra/dc/hdmihdcp.h
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION, All rights reserved.
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

#ifndef __DRIVERS_VIDEO_TEGRA_DC_HDMIHDCP_H
#define __DRIVERS_VIDEO_TEGRA_DC_HDMIHDCP_H
#include <video/nvhdcp.h>

#define HDCP_HDCP2_VERSION		0x50
#define HDCP_HDCP2_VERSION_HDCP22_YES	0x04
#define HDCP_HDCP2_VERSION_HDCP22_NO	0x00

#define HDCP_WRITE_MSG			0x60

#define HDCP_RX_STATUS			0x70
#define HDCP_RX_STATUS_MSG_SIZE_MASK	0x03ff
#define HDCP_RX_STATUS_MSG_READY_YES	0x0400
#define HDCP_RX_STATUS_MSG_READY_NO	0x0000
#define HDCP_RX_STATUS_MSG_REAUTH_REQ	0x0800

#define HDCP_READ_MSG			0x80

/* for nvhdcp.state */
enum tegra_nvhdcp_state {
	STATE_OFF,
	STATE_UNAUTHENTICATED,
	STATE_LINK_VERIFY,
	STATE_RENEGOTIATE,
};

struct tegra_nvhdcp {
	struct delayed_work		work;
	struct tegra_hdmi		*hdmi;
	struct workqueue_struct		*downstream_wq;
	struct mutex			lock;
	struct miscdevice		miscdev;
	char				name[12];
	unsigned			id;
	bool				plugged; /* true if hotplug detected */
	atomic_t			policy; /* set policy */
	enum tegra_nvhdcp_state		state; /* STATE_xxx */
	struct i2c_client		*client;
	struct i2c_board_info		info;
	int				bus;
	u32				b_status;
	u64				a_n;
	u64				c_n;
	u64				a_ksv;
	u64				b_ksv;
	u64				c_ksv;
	u64				d_ksv;
	u8				v_prime[20];
	u64				m_prime;
	u32				num_bksv_list;
	u64				bksv_list[TEGRA_NVHDCP_MAX_DEVS];
	int				fail_count;
	char				hdcp22;
	u8				max_retries;
	u8				repeater;
	struct workqueue_struct		*fallback_wq;
	struct delayed_work		fallback_work;
};

#ifdef CONFIG_HDCP
void tegra_nvhdcp_set_plug(struct tegra_nvhdcp *nvhdcp, bool hpd);
int tegra_nvhdcp_set_policy(struct tegra_nvhdcp *nvhdcp, int pol);
void tegra_nvhdcp_suspend(struct tegra_nvhdcp *nvhdcp);
void tegra_nvhdcp_resume(struct tegra_nvhdcp *nvhdcp);
void tegra_nvhdcp_shutdown(struct tegra_nvhdcp *nvhdcp);
struct tegra_nvhdcp *tegra_nvhdcp_create(struct tegra_hdmi *hdmi,
					int id, int bus);
void tegra_nvhdcp_destroy(struct tegra_nvhdcp *nvhdcp);
void tegra_nvhdcp_debugfs_init(struct tegra_nvhdcp *nvhdcp);
#if (defined(CONFIG_ARCH_TEGRA_18x_SOC) \
&& defined(CONFIG_TRUSTED_LITTLE_KERNEL))
extern int te_launch_trusted_oper(u64 *buf_ptr,
					u64 buflen, u32 session_id,
				u32 *ta_uuid, u32 cmd, u32 size);
extern int te_open_trusted_session(u32 *ta_uuid, u32 size, u32 *session_id);
extern void te_close_trusted_session(u32 session_id, u32 *ta_uuid,
							u32 size);
#endif
#else
static inline void tegra_nvhdcp_set_plug(struct tegra_nvhdcp *nvhdcp, bool hpd)
{
}
static inline int tegra_nvhdcp_set_policy(struct tegra_nvhdcp *nvhdcp, int pol)
{
	return 0;
}
static inline void tegra_nvhdcp_suspend(struct tegra_nvhdcp *nvhdcp) { }
static inline void tegra_nvhdcp_resume(struct tegra_nvhdcp *nvhdcp) { }
static inline void tegra_nvhdcp_shutdown(struct tegra_nvhdcp *nvhdcp) { }
static inline struct tegra_nvhdcp *tegra_nvhdcp_create(
	struct tegra_hdmi *hdmi, int id, int bus)
{
	return NULL;
}
static inline void tegra_nvhdcp_destroy(struct tegra_nvhdcp *nvhdcp) { }
#endif
#endif
