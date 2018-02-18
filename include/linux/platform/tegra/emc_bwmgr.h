/**
 * Copyright (c) 2015-2016, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef __EMC_BWMGR_H
#define __EMC_BWMGR_H

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/of_address.h>

/* keep in sync with tegra_bwmgr_client_names */
enum tegra_bwmgr_client_id {
	TEGRA_BWMGR_CLIENT_CPU_0,
	TEGRA_BWMGR_CLIENT_CPU_1,
	TEGRA_BWMGR_CLIENT_DISP0,
	TEGRA_BWMGR_CLIENT_DISP1,
	TEGRA_BWMGR_CLIENT_DISP2,
	TEGRA_BWMGR_CLIENT_USBD,
	TEGRA_BWMGR_CLIENT_XHCI,
	TEGRA_BWMGR_CLIENT_SDMMC1,
	TEGRA_BWMGR_CLIENT_SDMMC2,
	TEGRA_BWMGR_CLIENT_SDMMC3,
	TEGRA_BWMGR_CLIENT_SDMMC4,
	TEGRA_BWMGR_CLIENT_MON,
	TEGRA_BWMGR_CLIENT_GPU,
	TEGRA_BWMGR_CLIENT_MSENC,
	TEGRA_BWMGR_CLIENT_NVJPG,
	TEGRA_BWMGR_CLIENT_NVDEC,
	TEGRA_BWMGR_CLIENT_TSEC,
	TEGRA_BWMGR_CLIENT_TSECB,
	TEGRA_BWMGR_CLIENT_VI,
	TEGRA_BWMGR_CLIENT_ISPA,
	TEGRA_BWMGR_CLIENT_ISPB,
	TEGRA_BWMGR_CLIENT_CAMERA,
	TEGRA_BWMGR_CLIENT_ISOMGR,
	TEGRA_BWMGR_CLIENT_THERMAL_CAP,
	TEGRA_BWMGR_CLIENT_VIC,
	TEGRA_BWMGR_CLIENT_APE_ADSP,
	TEGRA_BWMGR_CLIENT_APE_ADMA,
	TEGRA_BWMGR_CLIENT_PCIE,
	TEGRA_BWMGR_CLIENT_BBC_0,
	TEGRA_BWMGR_CLIENT_EQOS,
	TEGRA_BWMGR_CLIENT_SE0,
	TEGRA_BWMGR_CLIENT_SE1,
	TEGRA_BWMGR_CLIENT_SE2,
	TEGRA_BWMGR_CLIENT_SE3,
	TEGRA_BWMGR_CLIENT_SE4,
	TEGRA_BWMGR_CLIENT_DEBUG,
	TEGRA_BWMGR_CLIENT_COUNT /* Should always be last */
};

enum tegra_bwmgr_request_type {
	TEGRA_BWMGR_SET_EMC_FLOOR, /* lower bound */
	TEGRA_BWMGR_SET_EMC_CAP, /* upper bound */
	TEGRA_BWMGR_SET_EMC_ISO_CAP, /* upper bound that affects ISO Bw */
	TEGRA_BWMGR_SET_EMC_SHARED_BW, /* shared bw request */
	TEGRA_BWMGR_SET_EMC_SHARED_BW_ISO, /* for use by ISO Mgr only */
	TEGRA_BWMGR_SET_EMC_REQ_COUNT /* Should always be last */
};

struct tegra_bwmgr_client;

#if defined(CONFIG_TEGRA_BWMGR)
/**
 * tegra_bwmgr_register - register an EMC Bandwidth Manager client.
 *			  Also see tegra_bwmgr_unregister().
 * @client      client id from tegra_bwmgr_client_id
 *
 * Returns a valid handle on successful registration, NULL on error.
 */
struct tegra_bwmgr_client *tegra_bwmgr_register(
		enum tegra_bwmgr_client_id client);

/**
 * tegra_bwmgr_unregister - unregister an EMC Bandwidth Manager client.
 *			    Callers should match register/unregister calls.
 *			    Persistence of old requests across
 *			    register/unregister calls is undefined.
 *			    Also see tegra_bwmgr_set_emc()
 *
 * @handle      handle acquired during tegra_bwmgr_register
 */
void tegra_bwmgr_unregister(struct tegra_bwmgr_client *handle);

/**
 * tegra_bwmgr_get_emc_rate - get the current EMC rate.
 *
 * Returns current memory clock rate in Hz.
 */
unsigned long tegra_bwmgr_get_emc_rate(void);

/**
 * tegra_bwmgr_get_max_emc_rate - get the max EMC rate.
 *
 * Returns the max memory clock rate in Hz.
 */
unsigned long tegra_bwmgr_get_max_emc_rate(void);

/**
 * tegra_bwmgr_round_rate - round up to next EMC rate which can be provided
 *
 * @bw		Input rate
 *
 * Returns the next higher rate from the Input rate that EMC can run at.
 */
unsigned long tegra_bwmgr_round_rate(unsigned long bw);

/**
 * tegra_bwmgr_set_emc - request to bwmgr to set an EMC rate parameter.
 *			 Actual clock rate depends on aggregation of
 *			 requests by all clients. If needed, use
 *			 tegra_bwmgr_get_emc_rate() to get the rate after
 *			 a tegra_bwmgr_set_emc() call.
 *
 *			 Call tegra_bwmgr_set_emc() with same request type and
 *			 val = 0 to clear request.
 *
 * @handle      handle acquired during tegra_bwmgr_register
 * @val         value to be set in Hz, 0 to clear old request of the same type
 * @req         chosen type from tegra_bwmgr_request_type
 *
 * Returns success (0) or negative errno.
 */
int tegra_bwmgr_set_emc(struct tegra_bwmgr_client *handle, unsigned long val,
		enum tegra_bwmgr_request_type req);

/**
 * tegra_bwmgr_notifier_register - register a notifier callback when
 *		emc rate changes. Must be called from non-atomic
 *		context. The callback must not call any bwmgr API.
 * @nb		linux notifier block
 *
 * Returns success (0) or negative errno.
 */
int tegra_bwmgr_notifier_register(struct notifier_block *nb);

/**
 * tegra_bwmgr_notifier_unregister - unregister a notifier callback.
 * @nb		linux notifier block
 *
 * Returns success (0) or negative errno.
 */
int tegra_bwmgr_notifier_unregister(struct notifier_block *nb);

/*
 * Initialize bwmgr.
 * This api would be called by .init_machine during boot.
 * bwmgr clients, don't call this api.
 */
int __init bwmgr_init(void);

void __exit bwmgr_exit(void);

#else /* CONFIG_TEGRA_BWMGR */

static inline struct tegra_bwmgr_client *tegra_bwmgr_register(
		enum tegra_bwmgr_client_id client_id)
{
	static int i;
	/* return a dummy handle to allow client to function
	 * as if bwmgr were enabled.
	 */
	return (struct tegra_bwmgr_client *) &i;
}

static inline void tegra_bwmgr_unregister(struct tegra_bwmgr_client *handle) {}

static inline int bwmgr_init(void)
{
	return 0;
}

static inline void bwmgr_exit(void) {}

static inline unsigned long tegra_bwmgr_get_emc_rate(void)
{
	static struct clk *bwmgr_emc_clk;
	struct device_node *dn;

	if (!bwmgr_emc_clk) {
		dn = of_find_compatible_node(NULL, NULL, "nvidia,bwmgr");
		if (dn == NULL) {
			pr_err("bwmgr: dt node not found.\n");
			return 0;
		}

		bwmgr_emc_clk = of_clk_get(dn, 0);
		if (IS_ERR_OR_NULL(bwmgr_emc_clk)) {
			pr_err("bwmgr: couldn't find emc clock.\n");
			bwmgr_emc_clk = NULL;
			WARN_ON(true);
			return 0;
		}
	}

	return clk_get_rate(bwmgr_emc_clk);
}

static inline unsigned long tegra_bwmgr_get_max_emc_rate(void)
{
	static struct clk *bwmgr_emc_clk;
	struct device_node *dn;

	if (!bwmgr_emc_clk) {
		dn = of_find_compatible_node(NULL, NULL, "nvidia,bwmgr");
		if (dn == NULL) {
			pr_err("bwmgr: dt node not found.\n");
			return 0;
		}

		bwmgr_emc_clk = of_clk_get(dn, 0);
		if (IS_ERR_OR_NULL(bwmgr_emc_clk)) {
			pr_err("bwmgr: couldn't find emc clock.\n");
			bwmgr_emc_clk = NULL;
			WARN_ON(true);
			return 0;
		}
	}

	/* Use LONG_MAX as clk_round_rate treats rate argument as signed */
	return clk_round_rate(bwmgr_emc_clk, LONG_MAX);
}

static inline unsigned long tegra_bwmgr_round_rate(unsigned long bw)
{
	static struct clk *bwmgr_emc_clk;
	struct device_node *dn;

	if (!bwmgr_emc_clk) {
		dn = of_find_compatible_node(NULL, NULL, "nvidia,bwmgr");
		if (dn == NULL) {
			pr_err("bwmgr: dt node not found.\n");
			return 0;
		}

		bwmgr_emc_clk = of_clk_get(dn, 0);
		if (IS_ERR_OR_NULL(bwmgr_emc_clk)) {
			pr_err("bwmgr: couldn't find emc clock.\n");
			bwmgr_emc_clk = NULL;
			WARN_ON(true);
			return 0;
		}
	}

	return clk_round_rate(bwmgr_emc_clk, bw);
}

static inline int tegra_bwmgr_set_emc(struct tegra_bwmgr_client *handle,
		unsigned long val, enum tegra_bwmgr_request_type req)
{
	return 0;
}

static inline int tegra_bwmgr_notifier_register(struct notifier_block *nb)
{
	return 0;
}

static inline int tegra_bwmgr_notifier_unregister(struct notifier_block *nb)
{
	return 0;
}

#endif /* CONFIG_TEGRA_BWMGR */
#endif /* __EMC_BWMGR_H */

