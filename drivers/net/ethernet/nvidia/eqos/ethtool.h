/* =========================================================================
 * The Synopsys DWC ETHER QOS Software Driver and documentation (hereinafter
 * "Software") is an unsupported proprietary work of Synopsys, Inc. unless
 * otherwise expressly agreed to in writing between Synopsys and you.
 *
 * The Software IS NOT an item of Licensed Software or Licensed Product under
 * any End User Software License Agreement or Agreement for Licensed Product
 * with Synopsys or any supplement thereto.  Permission is hereby granted,
 * free of charge, to any person obtaining a copy of this software annotated
 * with this license and the Software, to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * =========================================================================
 */
/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */
#ifndef __EQOS_ETHTOOL_H__

#define __EQOS_ETHTOOL_H__

static void eqos_get_pauseparam(struct net_device *dev,
				       struct ethtool_pauseparam *pause);
static int eqos_set_pauseparam(struct net_device *dev,
				      struct ethtool_pauseparam *pause);

static int eqos_getsettings(struct net_device *dev,
				   struct ethtool_cmd *cmd);
static int eqos_setsettings(struct net_device *dev,
				   struct ethtool_cmd *cmd);
static void eqos_get_wol(struct net_device *dev,
				struct ethtool_wolinfo *wol);
static int eqos_set_wol(struct net_device *dev,
			       struct ethtool_wolinfo *wol);

static int eqos_set_coalesce(struct net_device *dev,
				    struct ethtool_coalesce *ec);
static int eqos_get_coalesce(struct net_device *dev,
				    struct ethtool_coalesce *ec);

static int eqos_get_sset_count(struct net_device *dev, int sset);

static void eqos_get_strings(struct net_device *dev, u32 stringset, u8 *data);

static void eqos_get_ethtool_stats(struct net_device *dev,
	struct ethtool_stats *dummy, u64 *data);
#if 0
static int eqos_set_tso(struct net_device *dev, u32 data);
static u32 eqos_get_tso(struct net_device *dev);
#endif

#endif
