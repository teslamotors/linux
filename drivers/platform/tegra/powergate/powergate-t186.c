/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION. All rights reserved
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/tegra-powergate.h>
#include <dt-bindings/soc/tegra186-powergate.h>
#include <soc/tegra/bpmp_abi.h>
#include <soc/tegra/tegra_bpmp.h>

#include "powergate-priv-t18x.h"

#define UPDATE_LOGIC_STATE	0x1
#define LOGIC_STATE_ON		0x1
#define LOGIC_STATE_OFF		0x0

#define UPDATE_SRAM_STATE	0x1
#define SRAM_STATE_SD		(1 << 0)
#define SRAM_STATE_SLP		(1 << 1)
#define SRAM_STATE_DSLP		(1 << 2)
#define SRAM_STATE_ON		0x0

#define UPDATE_CLK_STATE	0x1
#define CLK_STATE_ON		0x1
#define CLK_STATE_OFF		0x0

static const char *partition_names[] = {
	[TEGRA186_POWER_DOMAIN_AUD] = "audio",
	[TEGRA186_POWER_DOMAIN_DFD] = "dfd",
	[TEGRA186_POWER_DOMAIN_DISP] = "disp",
	[TEGRA186_POWER_DOMAIN_DISPB] = "dispb",
	[TEGRA186_POWER_DOMAIN_DISPC] = "dispc",
	[TEGRA186_POWER_DOMAIN_ISPA] = "ispa",
	[TEGRA186_POWER_DOMAIN_NVDEC] = "nvdec",
	[TEGRA186_POWER_DOMAIN_NVJPG] = "nvjpg",
	[TEGRA186_POWER_DOMAIN_MPE] = "nvenc",
	[TEGRA186_POWER_DOMAIN_PCX] = "pcie",
	[TEGRA186_POWER_DOMAIN_SAX] = "sata",
	[TEGRA186_POWER_DOMAIN_VE] = "ve",
	[TEGRA186_POWER_DOMAIN_VIC] = "vic",
	[TEGRA186_POWER_DOMAIN_XUSBA] = "xusba",
	[TEGRA186_POWER_DOMAIN_XUSBB] = "xusbb",
	[TEGRA186_POWER_DOMAIN_XUSBC] = "xusbc",
	[TEGRA186_POWER_DOMAIN_GPU] = "gpu",
};

struct powergate_request {
	uint32_t partition_id;
	uint32_t logic_state;
	uint32_t sram_state;
	uint32_t clk_state;
};

struct powergate_state {
	uint32_t logic_state;
	uint32_t sram_state;
};

static int tegra186_pg_powergate_partition(int id)
{
	int ret;
	struct mrq_pg_update_state_request req;

	req.partition_id = id;
	req.logic_state = UPDATE_LOGIC_STATE | (1 << LOGIC_STATE_OFF);
	req.sram_state = UPDATE_SRAM_STATE | (1 << SRAM_STATE_SD);
	req.clock_state = 0;

	ret = tegra_bpmp_send_receive(MRQ_PG_UPDATE_STATE, &req, sizeof(req), NULL, 0);
	if (ret)
		return ret;

	return 0;
}

static int tegra186_pg_unpowergate_partition(int id)
{
	int ret;
	struct mrq_pg_update_state_request req;

	req.partition_id = id;
	req.logic_state = UPDATE_LOGIC_STATE | (1 << LOGIC_STATE_ON);
	req.sram_state = UPDATE_SRAM_STATE | (1 << SRAM_STATE_ON);
	req.clock_state = 0;

	ret = tegra_bpmp_send_receive(MRQ_PG_UPDATE_STATE, &req, sizeof(req), NULL, 0);
	if (ret)
		return ret;

	return 0;
}

static int tegra186_pg_powergate_clk_off(int id)
{
	int ret;
	struct mrq_pg_update_state_request req;

	req.partition_id = id;
	req.logic_state = UPDATE_LOGIC_STATE | (1 << LOGIC_STATE_OFF);
	req.sram_state = UPDATE_SRAM_STATE | (1 << SRAM_STATE_SD);
	req.clock_state = UPDATE_CLK_STATE | (1 << CLK_STATE_OFF);

	ret = tegra_bpmp_send_receive(MRQ_PG_UPDATE_STATE, &req, sizeof(req), NULL, 0);
	if (ret)
		return ret;

	return 0;
}

static int tegra186_pg_unpowergate_clk_on(int id)
{
	int ret;
	struct mrq_pg_update_state_request req;

	req.partition_id = id;
	req.logic_state = UPDATE_LOGIC_STATE | (1 << LOGIC_STATE_ON);
	req.sram_state = UPDATE_SRAM_STATE | (1 << SRAM_STATE_ON);
	req.clock_state = UPDATE_CLK_STATE | (1 << CLK_STATE_ON);

	ret = tegra_bpmp_send_receive(MRQ_PG_UPDATE_STATE, &req, sizeof(req), NULL, 0);
	if (ret)
		return ret;

	return 0;
}

static const char *tegra186_pg_get_name(int id)
{
	return partition_names[id];
}

static bool tegra186_pg_is_powered(int id)
{
	int ret;
	struct mrq_pg_read_state_response msg;
	struct mrq_pg_read_state_request req;

	req.partition_id = id;
	ret = tegra_bpmp_send_receive(MRQ_PG_READ_STATE, &req, sizeof(req), &msg, sizeof(msg));
	if (ret)
		return 0;

	return !!msg.logic_state;
}

static int tegra186_init_refcount(void)
{
	/*
	 * Ensure reference count is correct before turning off partitions
	 */
	if (!tegra_powergate_is_powered(TEGRA186_POWER_DOMAIN_XUSBA))
		tegra_unpowergate_partition_with_clk_on(TEGRA186_POWER_DOMAIN_XUSBA);
	if (!tegra_powergate_is_powered(TEGRA186_POWER_DOMAIN_XUSBB))
		tegra_unpowergate_partition_with_clk_on(TEGRA186_POWER_DOMAIN_XUSBB);
	if (!tegra_powergate_is_powered(TEGRA186_POWER_DOMAIN_XUSBC))
		tegra_unpowergate_partition_with_clk_on(TEGRA186_POWER_DOMAIN_XUSBC);
	if (!tegra_powergate_is_powered(TEGRA186_POWER_DOMAIN_SAX))
		tegra_unpowergate_partition_with_clk_on(TEGRA186_POWER_DOMAIN_SAX);
	if (!tegra_powergate_is_powered(TEGRA186_POWER_DOMAIN_PCX))
		tegra_unpowergate_partition_with_clk_on(TEGRA186_POWER_DOMAIN_PCX);


	tegra_powergate_partition_with_clk_off(TEGRA186_POWER_DOMAIN_XUSBA);
	tegra_powergate_partition_with_clk_off(TEGRA186_POWER_DOMAIN_XUSBB);
	tegra_powergate_partition_with_clk_off(TEGRA186_POWER_DOMAIN_XUSBC);
	tegra_powergate_partition_with_clk_off(TEGRA186_POWER_DOMAIN_SAX);
	tegra_powergate_partition_with_clk_off(TEGRA186_POWER_DOMAIN_PCX);

	return 0;
}

static struct powergate_ops tegra186_pg_ops = {
	.soc_name = "tegra186",

	.num_powerdomains = TEGRA_NUM_POWERGATE,

	.get_powergate_domain_name = tegra186_pg_get_name,

	.powergate_partition = tegra186_pg_powergate_partition,
	.unpowergate_partition = tegra186_pg_unpowergate_partition,

	.powergate_partition_with_clk_off = tegra186_pg_powergate_clk_off,
	.unpowergate_partition_with_clk_on = tegra186_pg_unpowergate_clk_on,

	.powergate_is_powered = tegra186_pg_is_powered,
	.powergate_init_refcount = tegra186_init_refcount,
};

struct powergate_ops *tegra186_powergate_init_chip_support(void)
{
	return &tegra186_pg_ops;
}
