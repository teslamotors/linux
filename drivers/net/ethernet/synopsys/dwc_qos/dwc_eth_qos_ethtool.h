/*
 * DWC Ether QOS controller driver for Samsung TRAV SoCs
 *
 * Copyright (C) Synopsys India Pvt. Ltd.
 * Copyright (C) 2017 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Pankaj Dubey <pankaj.dubey@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DWC_EQOS_ETHTOOL_H__
#define __DWC_EQOS_ETHTOOL_H__

static void dwc_eqos_get_pauseparam(struct net_device *dev,
				    struct ethtool_pauseparam *pause);
static int dwc_eqos_set_pauseparam(struct net_device *dev,
				   struct ethtool_pauseparam *pause);

static void dwc_eqos_get_wol(struct net_device *dev,
			     struct ethtool_wolinfo *wol);
static int dwc_eqos_set_wol(struct net_device *dev,
			    struct ethtool_wolinfo *wol);

static int dwc_eqos_set_coalesce(struct net_device *dev,
				 struct ethtool_coalesce *ec);
static int dwc_eqos_get_coalesce(struct net_device *dev,
				 struct ethtool_coalesce *ec);

static int dwc_eqos_get_sset_count(struct net_device *dev, int sset);

static int dwc_eth_qos_get_ts_info(struct net_device *dev,
		struct ethtool_ts_info *info);

static int dwc_eqos_get_link_ksettings(struct net_device *dev,
				       struct ethtool_link_ksettings *cmd);

static int dwc_eqos_set_link_ksettings(struct net_device *dev,
			const struct ethtool_link_ksettings *cmd);

static void dwc_eqos_get_strings(struct net_device *dev,
				 u32 stringset, u8 *data);

static void dwc_eqos_get_ethtool_stats(struct net_device *dev,
				       struct ethtool_stats *dummy,
				       u64 *data);

#endif
