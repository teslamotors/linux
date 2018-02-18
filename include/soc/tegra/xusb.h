/*
 * Copyright (C) 2014-2016, NVIDIA Corporation.  All rights reserved.
 * Copyright (C) 2014 Google, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#ifndef __SOC_TEGRA_XUSB_H__
#define __SOC_TEGRA_XUSB_H__

#include <linux/usb/ch9.h>

/* Command requests from the firmware */
enum tegra_xusb_mbox_cmd {
	MBOX_CMD_MSG_ENABLED = 1,
	MBOX_CMD_INC_FALC_CLOCK,
	MBOX_CMD_DEC_FALC_CLOCK,
	MBOX_CMD_INC_SSPI_CLOCK,
	MBOX_CMD_DEC_SSPI_CLOCK,
	MBOX_CMD_SET_BW, /* no ACK/NAK required */
	MBOX_CMD_SET_SS_PWR_GATING,
	MBOX_CMD_SET_SS_PWR_UNGATING,
	MBOX_CMD_SAVE_DFE_CTLE_CTX,
	MBOX_CMD_AIRPLANE_MODE_ENABLED, /* unused */
	MBOX_CMD_AIRPLANE_MODE_DISABLED, /* unused */
	MBOX_CMD_START_HSIC_IDLE,
	MBOX_CMD_STOP_HSIC_IDLE,
	MBOX_CMD_DBC_WAKE_STACK, /* unused */
	MBOX_CMD_HSIC_PRETEND_CONNECT,
	MBOX_CMD_RESET_SSPI,
	MBOX_CMD_DISABLE_SS_LFPS_DETECTION,
	MBOX_CMD_ENABLE_SS_LFPS_DETECTION,
	MBOX_CMD_SS_PORT_IN_POLLING,
	MBOX_CMD_MAX,

	/* Response message to above commands */
	MBOX_CMD_ACK = 128,
	MBOX_CMD_NAK,
	MBOX_CMD_COMPL /* mailbox completed without sending ACK/NAK */
};

struct tegra_xusb_mbox_msg {
	u32 cmd;
	u32 data;
};

struct phy;

int tegra_phy_xusb_set_vbus_override(struct phy *phy);
int tegra_phy_xusb_clear_vbus_override(struct phy *phy);

int tegra_phy_xusb_set_id_override(struct phy *phy);
int tegra_phy_xusb_clear_id_override(struct phy *phy);
bool tegra_phy_xusb_has_otg_cap(struct phy *phy);

int tegra_phy_xusb_enable_sleepwalk(struct phy *phy,
				    enum usb_device_speed speed);
int tegra_phy_xusb_disable_sleepwalk(struct phy *phy);

int tegra_phy_xusb_enable_wake(struct phy *phy);
int tegra_phy_xusb_disable_wake(struct phy *phy);

int tegra_phy_xusb_pretend_connected(struct phy *phy);

/* tegra_phy_xusb_remote_wake_detected()
 *   check if a XUSB phy has detected remote wake.
 *   return values:
 *         0: no
 *       > 0: yes
 *       < 0: error
 */
int tegra_phy_xusb_remote_wake_detected(struct phy *phy);
void tegra_phy_xusb_utmi_pad_power_on(struct phy *phy);
void tegra_phy_xusb_utmi_pad_power_down(struct phy *phy);
#endif /* __SOC_TEGRA_XUSB_H__ */
