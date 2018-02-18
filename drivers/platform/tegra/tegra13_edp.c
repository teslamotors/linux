/*
 * drivers/platform/tegra/tegra13_edp.c
 *
 * Copyright (c) 2014, NVIDIA CORPORATION. All rights reserved.
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

#ifdef CONFIG_ARCH_TEGRA_13x_SOC

#include <mach/edp.h>

#ifdef CONFIG_SYSEDP_FRAMEWORK
static struct tegra_sysedp_corecap t132_sysedp_corecap[] = {
	/*mW	 CPU intensive load	   GPU intensive load	 */
	/*mW	 budget	 gpu(khz) mem(khz)  budget  gpu(khz) mem(khz) pthrot(mW)*/
	{5000,  {3000,  180000, 933000}, {3000,  180000, 933000}, 2604 },
	{6000,  {4000,  180000, 933000}, {3500,  252000, 933000}, 3374 },
	{7000,  {5000,  180000, 933000}, {3500,  324000, 933000}, 3840 },
	{8000,  {5000,  252000, 933000}, {3500,  396000, 933000}, 4265 },
	{9000,  {5000,  396000, 933000}, {3500,  468000, 933000}, 4849 },
	{10000, {6000,  324000, 933000}, {3500,  540000, 933000}, 5710 },
	{11000, {7000,  252000, 933000}, {4500,  540000, 933000}, 6159 },
	{12000, {7000,  396000, 933000}, {5500,  540000, 933000}, 7009 },
	{13000, {8000,  252000, 933000}, {6000,  540000, 933000}, 6765 },
	{14000, {10000, 252000, 933000}, {6000,  612000, 933000}, 8491 },
	{15000, {10000, 324000, 933000}, {6000,  684000, 933000}, 9185 },
	{16000, {11000, 252000, 933000}, {6000,  708000, 933000}, 9640 },
	{17000, {11000, 396000, 933000}, {6000,  756000, 933000}, 10508 },
	{18000, {12000, 324000, 933000}, {7000,  756000, 933000}, 11149 },
	{19000, {13000, 252000, 933000}, {7000,  804000, 933000}, 11745 },
	{20000, {13000, 324000, 933000}, {7500,  804000, 933000}, 12170 },
	{21000, {14000, 252000, 933000}, {6500,  853000, 933000}, 12570 },
	{22000, {14000, 396000, 933000}, {9000,  853000, 933000}, 13421 },
	{23000, {15000, 252000, 933000}, {9000,  853000, 933000}, 13453 },
	{24000, {15000, 396000, 933000}, {9500,  853000, 933000}, 14304 },
	{25000, {16000, 324000, 933000}, {10000, 853000, 933000}, 15147 },
	{26000, {16800, 324000, 933000}, {11000, 853000, 933000}, 16530 },
	{27000, {16800, 468000, 933000}, {11000, 853000, 933000}, 17282 },
	{28000, {16800, 540000, 933000}, {11500, 853000, 933000}, 17282 },
	{29000, {16800, 540000, 933000}, {13000, 853000, 933000}, 18400 },
	{30000, {16800, 648000, 933000}, {13000, 853000, 933000}, 19238 },
	{31000, {16800, 708000, 933000}, {13500, 853000, 933000}, 20064 },
	{32000, {16800, 708000, 933000}, {14000, 853000, 933000}, 20064 },
	{33000, {16800, 756000, 933000}, {14500, 853000, 933000}, 20947 },
};

struct tegra_sysedp_corecap *tegra_get_sysedp_corecap(unsigned int *sz)
{
	BUG_ON(sz == NULL);
	*sz = ARRAY_SIZE(t132_sysedp_corecap);
	return t132_sysedp_corecap;
}
#endif

#endif /* CONFIG_ARCH_TEGRA_13x_SOC */
