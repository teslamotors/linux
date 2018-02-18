/*
 * arch/arm64/kernel/cputable.c
 *
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/init.h>

#include <asm/cputable.h>

extern unsigned long __cortexa57_cpu_setup(void);
extern unsigned long __cortexa53_cpu_setup(void);
extern unsigned long __cpu_setup(void);

struct cpu_info cpu_table[] = {
	{
		.cpu_id_val	= 0x4e0f0000,
		.cpu_id_mask	= 0xffff000f,
		.cpu_name	= "NVIDIA Denver 1.0",
		.cpu_setup	= __cpu_setup,
	},
	{
		.cpu_id_val	= 0x410fd070,
		.cpu_id_mask	= 0xff0ffff0,
		.cpu_name	= "Cortex A57 Processor",
		.cpu_setup	= __cortexa57_cpu_setup,
	},
	{
		.cpu_id_val	= 0x410fd030,
		.cpu_id_mask	= 0xff0ffff0,
		.cpu_name	= "Cortex A53 Processor",
		.cpu_setup	= __cortexa53_cpu_setup,
	},
	{
		.cpu_id_val	= 0x000f0000,
		.cpu_id_mask	= 0x000f0000,
		.cpu_name	= "AArch64 Processor",
		.cpu_setup	= __cpu_setup,
	},
	{ /* Empty */ },
};
