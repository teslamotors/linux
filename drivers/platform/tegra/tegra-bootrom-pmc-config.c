/*
 * tegra-bootrom-pmc-config: Commands for bootrom to configure PMIC trhough PMC
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
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

#include <linux/err.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/tegra-pmc.h>
#include <linux/power/reset/system-pmic.h>

#define PMC_REG_8bit_MASK			0xFF
#define PMC_REG_16bit_MASK			0xFFFF
#define PMC_BR_COMMAND_I2C_ADD_MASK		0x7F
#define PMC_BR_COMMAND_WR_COMMANDS_MASK		0x3F
#define PMC_BR_COMMAND_WR_COMMANDS_SHIFT	8
#define PMC_BR_COMMAND_OPERAND_SHIFT		15
#define PMC_BR_COMMAND_CSUM_MASK		0xFF
#define PMC_BR_COMMAND_CSUM_SHIFT		16
#define PMC_BR_COMMAND_PMUX_MASK		0x7
#define PMC_BR_COMMAND_PMUX_SHIFT		24
#define PMC_BR_COMMAND_CTRL_ID_MASK		0x7
#define PMC_BR_COMMAND_CTRL_ID_SHIFT		27
#define PMC_BR_COMMAND_CTRL_TYPE_SHIFT		30
#define PMC_BR_COMMAND_RST_EN_SHIFT		31

struct tegra_bootrom_block {
	const char *name;
	int address;
	bool reg_8bits;
	bool data_8bits;
	bool i2c_controller;
	int controller_id;
	bool enable_reset;
	int ncommands;
	u32 *commands;
};

struct tegra_bootrom_commands {
	u32 command_retry_count;
	u32 delay_between_commands;
	u32 wait_before_bus_clear;
	struct tegra_bootrom_block *blocks;
	int nblocks;
};

static struct tegra_bootrom_commands *br_rst_commands;
static struct tegra_bootrom_commands *br_off_commands;

static int tegra_bootrom_get_commands_from_node(struct device *dev,
	struct device_node *np, struct tegra_bootrom_commands **br_commands)
{
	struct device_node *child;
	struct tegra_bootrom_commands *bcommands;
	int *command_ptr;
	struct tegra_bootrom_block *block;
	int nblocks;
	u32 reg, data, pval;
	u32 *wr_commands;
	int count, nblock, ncommands, i, reg_shift;
	int ret;
	int sz_bcommand, sz_blocks;

	nblocks = of_get_child_count(np);
	if (!nblocks) {
		dev_err(dev, "Bootrom I2C Command block not found\n");
		return -ENOENT;
	}

	count = 0;
	for_each_child_of_node(np, child) {
		if (!of_device_is_available(child))
			continue;

		ret = of_property_count_u32(child, "nvidia,write-commands");
		if (ret < 0) {
			dev_err(dev, "Node %s does not have write-commnds\n",
					child->full_name);
			return -EINVAL;
		}
		count += ret / 2;
	}

	sz_bcommand = (sizeof(*bcommands) + 0x3) & ~0x3;
	sz_blocks = (sizeof(*block) + 0x3) & ~0x3;
	bcommands = devm_kzalloc(dev,  sz_bcommand + nblocks * sz_blocks +
			count * sizeof(u32), GFP_KERNEL);
	if (!bcommands)
		return -ENOMEM;

	bcommands->nblocks = nblocks;
	bcommands->blocks = (void *)bcommands + sz_bcommand;
	command_ptr = (void *)bcommands->blocks + nblocks * sz_blocks;

	of_property_read_u32(np, "nvidia,command-retries-count",
			&bcommands->command_retry_count);
	of_property_read_u32(np, "nvidia,delay-between-commands-us",
			&bcommands->delay_between_commands);
	of_property_read_u32(np, "nvidia,wait-before-start-bus-clear-us",
			&bcommands->wait_before_bus_clear);

	nblock = 0;
	for_each_child_of_node(np, child) {
		if (!of_device_is_available(child))
			continue;

		block = &bcommands->blocks[nblock];

		ret = of_property_read_u32(child, "reg", &pval);
		if (ret) {
			dev_err(dev, "Reg property missing on block %s\n",
					child->name);
			return ret;
		}
		block->address = pval;
		of_property_read_string(child, "nvidia,command-names",
				&block->name);
		block->reg_8bits = !of_property_read_bool(child,
					"nvidia,enable-16bit-register");
		block->data_8bits = !of_property_read_bool(child,
					"nvidia,enable-16bit-data");
		block->i2c_controller = of_property_read_bool(child,
					"nvidia,controller-type-i2c");
		block->enable_reset = of_property_read_bool(child,
					"nvidia,enable-controller-reset");
		count = of_property_count_u32(child, "nvidia,write-commands");
		ncommands = count / 2;

		block->commands = command_ptr;
		command_ptr += ncommands;
		wr_commands = block->commands;
		reg_shift = (block->data_8bits) ? 8 : 16;
		for (i = 0; i < ncommands; ++i) {
			of_property_read_u32_index(child,
				"nvidia,write-commands", i * 2, &reg);
			of_property_read_u32_index(child,
				"nvidia,write-commands", i * 2 + 1, &data);

			wr_commands[i] = (data << reg_shift) | reg;
		}
		block->ncommands = ncommands;
		nblock++;
	}
	*br_commands = bcommands;
	return 0;
}

static int tegra_bootrom_get_commands_from_dt(struct device *dev,
		struct tegra_bootrom_commands **br_rst_commands,
		struct tegra_bootrom_commands **br_off_commands)
{
	struct device_node *np = dev->of_node;
	struct device_node *br_np, *rst_np, *off_np;
	int ret;

	*br_rst_commands = NULL;
	*br_off_commands = NULL;

	if (!np) {
		dev_err(dev, "No device node pointer\n");
		return -EINVAL;
	}

	br_np = of_find_node_by_name(np, "bootrom-commands");
	if (!br_np) {
		dev_info(dev, "Bootrom commmands not found\n");
		return -ENOENT;
	}

	rst_np = of_find_node_by_name(br_np, "reset-commands");
	if (!rst_np) {
		dev_info(dev, "bootrom-commands used for reset commands\n");
		rst_np = br_np;
	}

	ret = tegra_bootrom_get_commands_from_node(dev, rst_np,
				br_rst_commands);
	if (ret < 0) {
		dev_err(dev, "Parsing in bootrom-reset-command failed: %d\n",
			ret);
		return ret;
	}

	if (rst_np == br_np)
		return 0;

	off_np = of_find_node_by_name(br_np, "power-off-commands");
	if (!off_np) {
		dev_info(dev, "bootrom-commands used for poweroff commands\n");
		return 0;
	}

	ret = tegra_bootrom_get_commands_from_node(dev, off_np,
				br_off_commands);
	if (ret < 0) {
		dev_err(dev, "Parsing in bootrom-power-off-command failed %d\n",
			ret);
		return ret;
	}
	return 0;
}

static int tegra210_configure_pmc_scratch(struct device *dev,
		struct tegra_bootrom_commands *br_commands)
{
	struct tegra_bootrom_block *block;
	int i, j, k;
	u32 command;
	int reg_offset = 1;
	u32 reg_data_mask;
	int command_pw;
	u32 block_add, block_val, csum;

	for (i = 0; i < br_commands->nblocks; ++i) {
		block = &br_commands->blocks[i];

		command = block->address & PMC_BR_COMMAND_I2C_ADD_MASK;
		command |= (block->ncommands <<
					PMC_BR_COMMAND_WR_COMMANDS_SHIFT);
		if (!block->reg_8bits || !block->data_8bits)
			command |= BIT(PMC_BR_COMMAND_OPERAND_SHIFT);

		if (block->enable_reset)
			command |= BIT(PMC_BR_COMMAND_RST_EN_SHIFT);

		command |= (block->controller_id &
				PMC_BR_COMMAND_CTRL_ID_MASK) <<
					PMC_BR_COMMAND_CTRL_ID_SHIFT;

		/* Checksum will be added after parsing from reg/data */
		tegra_pmc_write_bootrom_command(reg_offset * 4, command);
		block_add = reg_offset * 4;
		block_val = command;
		reg_offset++;

		command_pw = (block->reg_8bits && block->data_8bits) ? 2 : 1;
		reg_data_mask = (command_pw == 1) ? 0xFFFF : 0xFFFFFFFFUL;
		csum = 0;

		for (j = 0; j < block->ncommands; j++) {
			command = block->commands[j] & reg_data_mask;
			if (command_pw == 2) {
				j++;
				if (j == block->ncommands)
					goto reg_update;
				command |= (block->commands[j] &
							reg_data_mask) << 16;
			}
reg_update:
			tegra_pmc_write_bootrom_command(reg_offset * 4,
						command);
			for (k = 0; k < 4; ++k)
				csum += (command >> (k * 8)) & 0xFF;
			reg_offset++;
		}
		for (k = 0; k < 4; ++k)
			csum += (block_val >> (k * 8)) & 0xFF;
		csum = 0x100 - csum;
		block_val = (block_val & 0xFF00FFFF) | ((csum & 0xFF) << 16);
		tegra_pmc_write_bootrom_command(block_add, block_val);
	}

	command = br_commands->command_retry_count & 0x7;
	command |= (br_commands->delay_between_commands & 0x1F) << 3;
	command |= (br_commands->nblocks & 0x7) << 8;
	command |= (br_commands->wait_before_bus_clear & 0x1F) << 11;
	tegra_pmc_write_bootrom_command(0, command);
	return 0;
}

static int tegra210_boorom_pmc_power_off_commands_init(struct device *dev)
{
	int ret;

	if (!br_off_commands) {
		pr_info("T210 pmc config for power off not available\n");
		return 0;
	}

	ret = tegra210_configure_pmc_scratch(NULL, br_off_commands);
	if (ret < 0) {
		pr_err("T210 pmc config for power off failed, %d\n", ret);
		return ret;
	}
	pr_info("T210 pmc config for power off passed\n");
	return 0;
}

static void tegra210_soc_power_off(void)
{
	tegra210_boorom_pmc_power_off_commands_init(NULL);
	tegra_pmc_reset_system();
}

int tegra210_boorom_pmc_init(struct device *dev)
{
	int ret;

	ret = tegra_bootrom_get_commands_from_dt(dev, &br_rst_commands,
				&br_off_commands);
	if (ret < 0) {
		if (ret == -ENOENT)
			ret = 0;
		else
			pr_info("T210 pmc config for bootrom command failed\n");
		return ret;
	}

	if (br_off_commands)
		soc_specific_power_off = tegra210_soc_power_off;

	ret = tegra210_configure_pmc_scratch(dev, br_rst_commands);
	if (ret < 0) {
		pr_info("T210 pmc config for bootrom command failed\n");
		return ret;
	}
	pr_info("T210 pmc config for bootrom command passed\n");
	return 0;
}
EXPORT_SYMBOL(tegra210_boorom_pmc_init);
