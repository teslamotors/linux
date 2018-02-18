/*
 * drivers/video/tegra/dc/nvdisplay/nvdisp.c
 *
 * Copyright (c) 2014-2016, NVIDIA CORPORATION, All rights reserved.
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


#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/dma-mapping.h>
#include <linux/tegra_pm_domains.h>
#include <linux/tegra-pm.h>
#include <linux/platform/tegra/emc_bwmgr.h>
#include <linux/uaccess.h>
#include <linux/platform/tegra/isomgr.h>
#include <linux/debugfs.h>
#include <linux/ktime.h>

#include <mach/dc.h>
#include <mach/fb.h>

#include "nvdisp.h"
#include "nvdisp_priv.h"
#include "dc_config.h"
#include "dc_priv.h"
#include "dp.h"
#include "hw_nvdisp_nvdisp.h"
#include "hw_win_nvdisp.h"

DEFINE_MUTEX(tegra_nvdisp_lock);

static DECLARE_WAIT_QUEUE_HEAD(nvdisp_common_channel_reservation_wq);
static DECLARE_WAIT_QUEUE_HEAD(nvdisp_common_channel_promotion_wq);

#define NVDISP_INPUT_LUT_SIZE   257
#define NVDISP_OUTPUT_LUT_SIZE  1025
#define NSEC_PER_MICROSEC 1000
#define MIN_FRAME_INTERVAL 1000

/* Global variables provided for clocks
 * common to all heads
 */
struct clk *hubclk;
static struct clk *compclk;

static struct reset_control *nvdisp_common_rst[DC_N_WINDOWS+1];

static int tegra_nvdisp_set_color_control(struct tegra_dc *dc);

static struct of_device_id nvdisp_disa_pd[] = {
	{ .compatible = "nvidia,tegra186-disa-pd", },
	{},
};

static struct of_device_id nvdisp_disb_pd[] = {
	{ .compatible = "nvidia,tegra186-disb-pd", },
	{},
};

static struct of_device_id nvdisp_disc_pd[] = {
	{ .compatible = "nvidia,tegra186-disc-pd", },
	{},
};

static struct nvdisp_pd_info nvdisp_pg[NVDISP_PD_COUNT];
static struct nvdisp_compclk_client clk_client[TEGRA_MAX_DC];
static bool compclk_already_on, hubclk_already_on;
static int cur_clk_client_index;

static unsigned int default_srgb_regamma_lut[] = {
		0x6000, 0x60CE, 0x619D, 0x626C, 0x632D, 0x63D4,
		0x6469, 0x64F0, 0x656B, 0x65DF, 0x664A, 0x66B0,
		0x6711, 0x676D, 0x67C4, 0x6819, 0x686A, 0x68B8,
		0x6904, 0x694D, 0x6994, 0x69D8, 0x6A1B, 0x6A5D,
		0x6A9C, 0x6ADA, 0x6B17, 0x6B52, 0x6B8C, 0x6BC5,
		0x6BFD, 0x6C33, 0x6C69, 0x6C9E, 0x6CD1, 0x6D04,
		0x6D36, 0x6D67, 0x6D98, 0x6DC7, 0x6DF6, 0x6E25,
		0x6E52, 0x6E7F, 0x6EAC, 0x6ED7, 0x6F03, 0x6F2D,
		0x6F58, 0x6F81, 0x6FAA, 0x6FD3, 0x6FFB, 0x7023,
		0x704B, 0x7071, 0x7098, 0x70BE, 0x70E4, 0x7109,
		0x712E, 0x7153, 0x7177, 0x719B, 0x71BF, 0x71E2,
		0x7205, 0x7227, 0x724A, 0x726C, 0x728E, 0x72AF,
		0x72D0, 0x72F1, 0x7312, 0x7333, 0x7353, 0x7373,
		0x7392, 0x73B2, 0x73D1, 0x73F0, 0x740F, 0x742D,
		0x744C, 0x746A, 0x7488, 0x74A6, 0x74C3, 0x74E0,
		0x74FE, 0x751B, 0x7537, 0x7554, 0x7570, 0x758D,
		0x75A9, 0x75C4, 0x75E0, 0x75FC, 0x7617, 0x7632,
		0x764D, 0x7668, 0x7683, 0x769E, 0x76B8, 0x76D3,
		0x76ED, 0x7707, 0x7721, 0x773B, 0x7754, 0x776E,
		0x7787, 0x77A0, 0x77B9, 0x77D2, 0x77EB, 0x7804,
		0x781D, 0x7835, 0x784E, 0x7866, 0x787E, 0x7896,
		0x78AE, 0x78C6, 0x78DD, 0x78F5, 0x790D, 0x7924,
		0x793B, 0x7952, 0x796A, 0x7981, 0x7997, 0x79AE,
		0x79C5, 0x79DB, 0x79F2, 0x7A08, 0x7A1F, 0x7A35,
		0x7A4B, 0x7A61, 0x7A77, 0x7A8D, 0x7AA3, 0x7AB8,
		0x7ACE, 0x7AE3, 0x7AF9, 0x7B0E, 0x7B24, 0x7B39,
		0x7B4E, 0x7B63, 0x7B78, 0x7B8D, 0x7BA2, 0x7BB6,
		0x7BCB, 0x7BE0, 0x7BF4, 0x7C08, 0x7C1D, 0x7C31,
		0x7C45, 0x7C59, 0x7C6E, 0x7C82, 0x7C96, 0x7CA9,
		0x7CBD, 0x7CD1, 0x7CE5, 0x7CF8, 0x7D0C, 0x7D1F,
		0x7D33, 0x7D46, 0x7D59, 0x7D6D, 0x7D80, 0x7D93,
		0x7DA6, 0x7DB9, 0x7DCC, 0x7DDF, 0x7DF2, 0x7E04,
		0x7E17, 0x7E2A, 0x7E3C, 0x7E4F, 0x7E61, 0x7E74,
		0x7E86, 0x7E98, 0x7EAB, 0x7EBD, 0x7ECF, 0x7EE1,
		0x7EF3, 0x7F05, 0x7F17, 0x7F29, 0x7F3B, 0x7F4D,
		0x7F5E, 0x7F70, 0x7F82, 0x7F93, 0x7FA5, 0x7FB6,
		0x7FC8, 0x7FD9, 0x7FEB, 0x7FFC, 0x800D, 0x801E,
		0x8030, 0x8041, 0x8052, 0x8063, 0x8074, 0x8085,
		0x8096, 0x80A7, 0x80B7, 0x80C8, 0x80D9, 0x80EA,
		0x80FA, 0x810B, 0x811C, 0x812C, 0x813D, 0x814D,
		0x815D, 0x816E, 0x817E, 0x818E, 0x819F, 0x81AF,
		0x81BF, 0x81CF, 0x81DF, 0x81EF, 0x81FF, 0x820F,
		0x821F, 0x822F, 0x823F, 0x824F, 0x825F, 0x826F,
		0x827E, 0x828E, 0x829E, 0x82AD, 0x82BD, 0x82CC,
		0x82DC, 0x82EB, 0x82FB, 0x830A, 0x831A, 0x8329,
		0x8338, 0x8348, 0x8357, 0x8366, 0x8375, 0x8385,
		0x8394, 0x83A3, 0x83B2, 0x83C1, 0x83D0, 0x83DF,
		0x83EE, 0x83FD, 0x840C, 0x841A, 0x8429, 0x8438,
		0x8447, 0x8455, 0x8464, 0x8473, 0x8481, 0x8490,
		0x849F, 0x84AD, 0x84BC, 0x84CA, 0x84D9, 0x84E7,
		0x84F5, 0x8504, 0x8512, 0x8521, 0x852F, 0x853D,
		0x854B, 0x855A, 0x8568, 0x8576, 0x8584, 0x8592,
		0x85A0, 0x85AE, 0x85BC, 0x85CA, 0x85D8, 0x85E6,
		0x85F4, 0x8602, 0x8610, 0x861E, 0x862C, 0x8639,
		0x8647, 0x8655, 0x8663, 0x8670, 0x867E, 0x868C,
		0x8699, 0x86A7, 0x86B5, 0x86C2, 0x86D0, 0x86DD,
		0x86EB, 0x86F8, 0x8705, 0x8713, 0x8720, 0x872E,
		0x873B, 0x8748, 0x8756, 0x8763, 0x8770, 0x877D,
		0x878B, 0x8798, 0x87A5, 0x87B2, 0x87BF, 0x87CC,
		0x87D9, 0x87E6, 0x87F3, 0x8801, 0x880E, 0x881A,
		0x8827, 0x8834, 0x8841, 0x884E, 0x885B, 0x8868,
		0x8875, 0x8882, 0x888E, 0x889B, 0x88A8, 0x88B5,
		0x88C1, 0x88CE, 0x88DB, 0x88E7, 0x88F4, 0x8900,
		0x890D, 0x891A, 0x8926, 0x8933, 0x893F, 0x894C,
		0x8958, 0x8965, 0x8971, 0x897D, 0x898A, 0x8996,
		0x89A3, 0x89AF, 0x89BB, 0x89C8, 0x89D4, 0x89E0,
		0x89EC, 0x89F9, 0x8A05, 0x8A11, 0x8A1D, 0x8A29,
		0x8A36, 0x8A42, 0x8A4E, 0x8A5A, 0x8A66, 0x8A72,
		0x8A7E, 0x8A8A, 0x8A96, 0x8AA2, 0x8AAE, 0x8ABA,
		0x8AC6, 0x8AD2, 0x8ADE, 0x8AEA, 0x8AF5, 0x8B01,
		0x8B0D, 0x8B19, 0x8B25, 0x8B31, 0x8B3C, 0x8B48,
		0x8B54, 0x8B60, 0x8B6B, 0x8B77, 0x8B83, 0x8B8E,
		0x8B9A, 0x8BA6, 0x8BB1, 0x8BBD, 0x8BC8, 0x8BD4,
		0x8BDF, 0x8BEB, 0x8BF6, 0x8C02, 0x8C0D, 0x8C19,
		0x8C24, 0x8C30, 0x8C3B, 0x8C47, 0x8C52, 0x8C5D,
		0x8C69, 0x8C74, 0x8C80, 0x8C8B, 0x8C96, 0x8CA1,
		0x8CAD, 0x8CB8, 0x8CC3, 0x8CCF, 0x8CDA, 0x8CE5,
		0x8CF0, 0x8CFB, 0x8D06, 0x8D12, 0x8D1D, 0x8D28,
		0x8D33, 0x8D3E, 0x8D49, 0x8D54, 0x8D5F, 0x8D6A,
		0x8D75, 0x8D80, 0x8D8B, 0x8D96, 0x8DA1, 0x8DAC,
		0x8DB7, 0x8DC2, 0x8DCD, 0x8DD8, 0x8DE3, 0x8DEE,
		0x8DF9, 0x8E04, 0x8E0E, 0x8E19, 0x8E24, 0x8E2F,
		0x8E3A, 0x8E44, 0x8E4F, 0x8E5A, 0x8E65, 0x8E6F,
		0x8E7A, 0x8E85, 0x8E90, 0x8E9A, 0x8EA5, 0x8EB0,
		0x8EBA, 0x8EC5, 0x8ECF, 0x8EDA, 0x8EE5, 0x8EEF,
		0x8EFA, 0x8F04, 0x8F0F, 0x8F19, 0x8F24, 0x8F2E,
		0x8F39, 0x8F43, 0x8F4E, 0x8F58, 0x8F63, 0x8F6D,
		0x8F78, 0x8F82, 0x8F8C, 0x8F97, 0x8FA1, 0x8FAC,
		0x8FB6, 0x8FC0, 0x8FCB, 0x8FD5, 0x8FDF, 0x8FEA,
		0x8FF4, 0x8FFE, 0x9008, 0x9013, 0x901D, 0x9027,
		0x9031, 0x903C, 0x9046, 0x9050, 0x905A, 0x9064,
		0x906E, 0x9079, 0x9083, 0x908D, 0x9097, 0x90A1,
		0x90AB, 0x90B5, 0x90BF, 0x90C9, 0x90D3, 0x90DD,
		0x90E7, 0x90F1, 0x90FB, 0x9105, 0x910F, 0x9119,
		0x9123, 0x912D, 0x9137, 0x9141, 0x914B, 0x9155,
		0x915F, 0x9169, 0x9173, 0x917D, 0x9186, 0x9190,
		0x919A, 0x91A4, 0x91AE, 0x91B8, 0x91C1, 0x91CB,
		0x91D5, 0x91DF, 0x91E9, 0x91F2, 0x91FC, 0x9206,
		0x9210, 0x9219, 0x9223, 0x922D, 0x9236, 0x9240,
		0x924A, 0x9253, 0x925D, 0x9267, 0x9270, 0x927A,
		0x9283, 0x928D, 0x9297, 0x92A0, 0x92AA, 0x92B3,
		0x92BD, 0x92C6, 0x92D0, 0x92DA, 0x92E3, 0x92ED,
		0x92F6, 0x9300, 0x9309, 0x9313, 0x931C, 0x9325,
		0x932F, 0x9338, 0x9342, 0x934B, 0x9355, 0x935E,
		0x9367, 0x9371, 0x937A, 0x9384, 0x938D, 0x9396,
		0x93A0, 0x93A9, 0x93B2, 0x93BC, 0x93C5, 0x93CE,
		0x93D7, 0x93E1, 0x93EA, 0x93F3, 0x93FC, 0x9406,
		0x940F, 0x9418, 0x9421, 0x942B, 0x9434, 0x943D,
		0x9446, 0x944F, 0x9459, 0x9462, 0x946B, 0x9474,
		0x947D, 0x9486, 0x948F, 0x9499, 0x94A2, 0x94AB,
		0x94B4, 0x94BD, 0x94C6, 0x94CF, 0x94D8, 0x94E1,
		0x94EA, 0x94F3, 0x94FC, 0x9505, 0x950E, 0x9517,
		0x9520, 0x9529, 0x9532, 0x953B, 0x9544, 0x954D,
		0x9556, 0x955F, 0x9568, 0x9571, 0x957A, 0x9583,
		0x958C, 0x9595, 0x959D, 0x95A6, 0x95AF, 0x95B8,
		0x95C1, 0x95CA, 0x95D3, 0x95DB, 0x95E4, 0x95ED,
		0x95F6, 0x95FF, 0x9608, 0x9610, 0x9619, 0x9622,
		0x962B, 0x9633, 0x963C, 0x9645, 0x964E, 0x9656,
		0x965F, 0x9668, 0x9671, 0x9679, 0x9682, 0x968B,
		0x9693, 0x969C, 0x96A5, 0x96AD, 0x96B6, 0x96BF,
		0x96C7, 0x96D0, 0x96D9, 0x96E1, 0x96EA, 0x96F2,
		0x96FB, 0x9704, 0x970C, 0x9715, 0x971D, 0x9726,
		0x972E, 0x9737, 0x9740, 0x9748, 0x9751, 0x9759,
		0x9762, 0x976A, 0x9773, 0x977B, 0x9784, 0x978C,
		0x9795, 0x979D, 0x97A6, 0x97AE, 0x97B6, 0x97BF,
		0x97C7, 0x97D0, 0x97D8, 0x97E1, 0x97E9, 0x97F1,
		0x97FA, 0x9802, 0x980B, 0x9813, 0x981B, 0x9824,
		0x982C, 0x9834, 0x983D, 0x9845, 0x984D, 0x9856,
		0x985E, 0x9866, 0x986F, 0x9877, 0x987F, 0x9888,
		0x9890, 0x9898, 0x98A0, 0x98A9, 0x98B1, 0x98B9,
		0x98C1, 0x98CA, 0x98D2, 0x98DA, 0x98E2, 0x98EB,
		0x98F3, 0x98FB, 0x9903, 0x990B, 0x9914, 0x991C,
		0x9924, 0x992C, 0x9934, 0x993C, 0x9945, 0x994D,
		0x9955, 0x995D, 0x9965, 0x996D, 0x9975, 0x997D,
		0x9986, 0x998E, 0x9996, 0x999E, 0x99A6, 0x99AE,
		0x99B6, 0x99BE, 0x99C6, 0x99CE, 0x99D6, 0x99DE,
		0x99E6, 0x99EE, 0x99F6, 0x99FE, 0x9A06, 0x9A0E,
		0x9A16, 0x9A1E, 0x9A26, 0x9A2E, 0x9A36, 0x9A3E,
		0x9A46, 0x9A4E, 0x9A56, 0x9A5E, 0x9A66, 0x9A6E,
		0x9A76, 0x9A7E, 0x9A86, 0x9A8E, 0x9A96, 0x9A9D,
		0x9AA5, 0x9AAD, 0x9AB5, 0x9ABD, 0x9AC5, 0x9ACD,
		0x9AD5, 0x9ADC, 0x9AE4, 0x9AEC, 0x9AF4, 0x9AFC,
		0x9B04, 0x9B0C, 0x9B13, 0x9B1B, 0x9B23, 0x9B2B,
		0x9B33, 0x9B3A, 0x9B42, 0x9B4A, 0x9B52, 0x9B59,
		0x9B61, 0x9B69, 0x9B71, 0x9B79, 0x9B80, 0x9B88,
		0x9B90, 0x9B97, 0x9B9F, 0x9BA7, 0x9BAF, 0x9BB6,
		0x9BBE, 0x9BC6, 0x9BCD, 0x9BD5, 0x9BDD, 0x9BE5,
		0x9BEC, 0x9BF4, 0x9BFC, 0x9C03, 0x9C0B, 0x9C12,
		0x9C1A, 0x9C22, 0x9C29, 0x9C31, 0x9C39, 0x9C40,
		0x9C48, 0x9C50, 0x9C57, 0x9C5F, 0x9C66, 0x9C6E,
		0x9C75, 0x9C7D, 0x9C85, 0x9C8C, 0x9C94, 0x9C9B,
		0x9CA3, 0x9CAA, 0x9CB2, 0x9CBA, 0x9CC1, 0x9CC9,
		0x9CD0, 0x9CD8, 0x9CDF, 0x9CE7, 0x9CEE, 0x9CF6,
		0x9CFD, 0x9D05, 0x9D0C, 0x9D14, 0x9D1B, 0x9D23,
		0x9D2A, 0x9D32, 0x9D39, 0x9D40, 0x9D48, 0x9D4F,
		0x9D57, 0x9D5E, 0x9D66, 0x9D6D, 0x9D75, 0x9D7C,
		0x9D83, 0x9D8B, 0x9D92, 0x9D9A, 0x9DA1, 0x9DA8,
		0x9DB0, 0x9DB7, 0x9DBE, 0x9DC6, 0x9DCD, 0x9DD5,
		0x9DDC, 0x9DE3, 0x9DEB, 0x9DF2, 0x9DF9, 0x9E01,
		0x9E08, 0x9E0F, 0x9E17, 0x9E1E, 0x9E25, 0x9E2D,
		0x9E34, 0x9E3B, 0x9E43, 0x9E4A, 0x9E51, 0x9E58,
		0x9E60, 0x9E67, 0x9E6E, 0x9E75, 0x9E7D, 0x9E84,
		0x9E8B, 0x9E92, 0x9E9A, 0x9EA1, 0x9EA8, 0x9EAF,
		0x9EB7, 0x9EBE, 0x9EC5, 0x9ECC, 0x9ED4, 0x9EDB,
		0x9EE2, 0x9EE9, 0x9EF0, 0x9EF7, 0x9EFF, 0x9F06,
		0x9F0D, 0x9F14, 0x9F1B, 0x9F23, 0x9F2A, 0x9F31,
		0x9F38, 0x9F3F, 0x9F46, 0x9F4D, 0x9F55, 0x9F5C,
		0x9F63, 0x9F6A, 0x9F71, 0x9F78, 0x9F7F, 0x9F86,
		0x9F8D, 0x9F95, 0x9F9C, 0x9FA3, 0x9FAA, 0x9FB1,
		0x9FB8, 0x9FBF, 0x9FC6, 0x9FCD, 0x9FD4, 0x9FDB,
		0x9FE2, 0x9FE9, 0x9FF0, 0x9FF7, 0x9FFF,
};

static unsigned int yuv8_10bpc_regamma_lut[] = {
		0x6000, 0x6047, 0x608F, 0x60D7, 0x611F, 0x6167,
		0x61AF, 0x61F7, 0x623F, 0x6287, 0x62CF, 0x6317,
		0x635F, 0x63A7, 0x63EF, 0x6437, 0x647F, 0x64C7,
		0x650F, 0x655B, 0x65A1, 0x65E5, 0x6627, 0x6668,
		0x66A7, 0x66E5, 0x6721, 0x675C, 0x6796, 0x67CF,
		0x6806, 0x683D, 0x6873, 0x68A7, 0x68DB, 0x690E,
		0x6941, 0x6972, 0x69A3, 0x69D3, 0x6A02, 0x6A31,
		0x6A5F, 0x6A8D, 0x6ABA, 0x6AE6, 0x6B12, 0x6B3D,
		0x6B68, 0x6B93, 0x6BBD, 0x6BE6, 0x6C0F, 0x6C37,
		0x6C60, 0x6C87, 0x6CAF, 0x6CD6, 0x6CFC, 0x6D22,
		0x6D48, 0x6D6E, 0x6D93, 0x6DB8, 0x6DDC, 0x6E00,
		0x6E24, 0x6E48, 0x6E6B, 0x6E8E, 0x6EB1, 0x6ED3,
		0x6EF6, 0x6F18, 0x6F39, 0x6F5B, 0x6F7C, 0x6F9D,
		0x6FBE, 0x6FDE, 0x6FFE, 0x701E, 0x703E, 0x705E,
		0x707D, 0x709C, 0x70BB, 0x70DA, 0x70F9, 0x7117,
		0x7135, 0x7154, 0x7171, 0x718F, 0x71AD, 0x71CA,
		0x71E7, 0x7204, 0x7221, 0x723E, 0x725A, 0x7277,
		0x7293, 0x72AF, 0x72CB, 0x72E6, 0x7302, 0x731E,
		0x7339, 0x7354, 0x736F, 0x738A, 0x73A5, 0x73C0,
		0x73DA, 0x73F4, 0x740F, 0x7429, 0x7443, 0x745D,
		0x7477, 0x7490, 0x74AA, 0x74C3, 0x74DD, 0x74F6,
		0x750F, 0x7528, 0x7541, 0x755A, 0x7572, 0x758B,
		0x75A3, 0x75BC, 0x75D4, 0x75EC, 0x7604, 0x761C,
		0x7634, 0x764C, 0x7663, 0x767B, 0x7693, 0x76AA,
		0x76C1, 0x76D8, 0x76F0, 0x7707, 0x771E, 0x7735,
		0x774B, 0x7762, 0x7779, 0x778F, 0x77A6, 0x77BC,
		0x77D2, 0x77E9, 0x77FF, 0x7815, 0x782B, 0x7841,
		0x7857, 0x786C, 0x7882, 0x7898, 0x78AD, 0x78C3,
		0x78D8, 0x78EE, 0x7903, 0x7918, 0x792D, 0x7942,
		0x7957, 0x796C, 0x7981, 0x7996, 0x79AB, 0x79BF,
		0x79D4, 0x79E8, 0x79FD, 0x7A11, 0x7A26, 0x7A3A,
		0x7A4E, 0x7A63, 0x7A77, 0x7A8B, 0x7A9F, 0x7AB3,
		0x7AC7, 0x7ADA, 0x7AEE, 0x7B02, 0x7B16, 0x7B29,
		0x7B3D, 0x7B50, 0x7B64, 0x7B77, 0x7B8A, 0x7B9E,
		0x7BB1, 0x7BC4, 0x7BD7, 0x7BEA, 0x7BFD, 0x7C10,
		0x7C23, 0x7C36, 0x7C49, 0x7C5C, 0x7C6F, 0x7C81,
		0x7C94, 0x7CA7, 0x7CB9, 0x7CCC, 0x7CDE, 0x7CF1,
		0x7D03, 0x7D15, 0x7D27, 0x7D3A, 0x7D4C, 0x7D5E,
		0x7D70, 0x7D82, 0x7D94, 0x7DA6, 0x7DB8, 0x7DCA,
		0x7DDC, 0x7DEE, 0x7DFF, 0x7E11, 0x7E23, 0x7E34,
		0x7E46, 0x7E58, 0x7E69, 0x7E7B, 0x7E8C, 0x7E9D,
		0x7EAF, 0x7EC0, 0x7ED1, 0x7EE3, 0x7EF4, 0x7F05,
		0x7F16, 0x7F27, 0x7F38, 0x7F49, 0x7F5A, 0x7F6B,
		0x7F7C, 0x7F8D, 0x7F9E, 0x7FAF, 0x7FBF, 0x7FD0,
		0x7FE1, 0x7FF1, 0x8002, 0x8013, 0x8023, 0x8034,
		0x8044, 0x8055, 0x8065, 0x8075, 0x8086, 0x8096,
		0x80A6, 0x80B7, 0x80C7, 0x80D7, 0x80E7, 0x80F7,
		0x8107, 0x8118, 0x8128, 0x8138, 0x8148, 0x8157,
		0x8167, 0x8177, 0x8187, 0x8197, 0x81A7, 0x81B6,
		0x81C6, 0x81D6, 0x81E6, 0x81F5, 0x8205, 0x8214,
		0x8224, 0x8234, 0x8243, 0x8252, 0x8262, 0x8271,
		0x8281, 0x8290, 0x829F, 0x82AF, 0x82BE, 0x82CD,
		0x82DD, 0x82EC, 0x82FB, 0x830A, 0x8319, 0x8328,
		0x8337, 0x8346, 0x8355, 0x8364, 0x8373, 0x8382,
		0x8391, 0x83A0, 0x83AF, 0x83BE, 0x83CD, 0x83DB,
		0x83EA, 0x83F9, 0x8408, 0x8416, 0x8425, 0x8434,
		0x8442, 0x8451, 0x845F, 0x846E, 0x847C, 0x848B,
		0x8499, 0x84A8, 0x84B6, 0x84C5, 0x84D3, 0x84E1,
		0x84F0, 0x84FE, 0x850C, 0x851B, 0x8529, 0x8537,
		0x8545, 0x8553, 0x8562, 0x8570, 0x857E, 0x858C,
		0x859A, 0x85A8, 0x85B6, 0x85C4, 0x85D2, 0x85E0,
		0x85EE, 0x85FC, 0x860A, 0x8618, 0x8626, 0x8633,
		0x8641, 0x864F, 0x865D, 0x866B, 0x8678, 0x8686,
		0x8694, 0x86A1, 0x86AF, 0x86BD, 0x86CA, 0x86D8,
		0x86E5, 0x86F3, 0x8701, 0x870E, 0x871C, 0x8729,
		0x8737, 0x8744, 0x8751, 0x875F, 0x876C, 0x877A,
		0x8787, 0x8794, 0x87A2, 0x87AF, 0x87BC, 0x87C9,
		0x87D7, 0x87E4, 0x87F1, 0x87FE, 0x880B, 0x8819,
		0x8826, 0x8833, 0x8840, 0x884D, 0x885A, 0x8867,
		0x8874, 0x8881, 0x888E, 0x889B, 0x88A8, 0x88B5,
		0x88C2, 0x88CF, 0x88DC, 0x88E9, 0x88F6, 0x8902,
		0x890F, 0x891C, 0x8929, 0x8936, 0x8942, 0x894F,
		0x895C, 0x8969, 0x8975, 0x8982, 0x898F, 0x899B,
		0x89A8, 0x89B4, 0x89C1, 0x89CE, 0x89DA, 0x89E7,
		0x89F3, 0x8A00, 0x8A0C, 0x8A19, 0x8A25, 0x8A32,
		0x8A3E, 0x8A4B, 0x8A57, 0x8A63, 0x8A70, 0x8A7C,
		0x8A89, 0x8A95, 0x8AA1, 0x8AAE, 0x8ABA, 0x8AC6,
		0x8AD2, 0x8ADF, 0x8AEB, 0x8AF7, 0x8B03, 0x8B0F,
		0x8B1C, 0x8B28, 0x8B34, 0x8B40, 0x8B4C, 0x8B58,
		0x8B64, 0x8B70, 0x8B7D, 0x8B89, 0x8B95, 0x8BA1,
		0x8BAD, 0x8BB9, 0x8BC5, 0x8BD1, 0x8BDD, 0x8BE9,
		0x8BF4, 0x8C00, 0x8C0C, 0x8C18, 0x8C24, 0x8C30,
		0x8C3C, 0x8C48, 0x8C53, 0x8C5F, 0x8C6B, 0x8C77,
		0x8C83, 0x8C8E, 0x8C9A, 0x8CA6, 0x8CB1, 0x8CBD,
		0x8CC9, 0x8CD5, 0x8CE0, 0x8CEC, 0x8CF8, 0x8D03,
		0x8D0F, 0x8D1A, 0x8D26, 0x8D32, 0x8D3D, 0x8D49,
		0x8D54, 0x8D60, 0x8D6B, 0x8D77, 0x8D82, 0x8D8E,
		0x8D99, 0x8DA5, 0x8DB0, 0x8DBC, 0x8DC7, 0x8DD2,
		0x8DDE, 0x8DE9, 0x8DF4, 0x8E00, 0x8E0B, 0x8E17,
		0x8E22, 0x8E2D, 0x8E38, 0x8E44, 0x8E4F, 0x8E5A,
		0x8E66, 0x8E71, 0x8E7C, 0x8E87, 0x8E92, 0x8E9E,
		0x8EA9, 0x8EB4, 0x8EBF, 0x8ECA, 0x8ED6, 0x8EE1,
		0x8EEC, 0x8EF7, 0x8F02, 0x8F0D, 0x8F18, 0x8F23,
		0x8F2E, 0x8F39, 0x8F44, 0x8F4F, 0x8F5A, 0x8F65,
		0x8F70, 0x8F7B, 0x8F86, 0x8F91, 0x8F9C, 0x8FA7,
		0x8FB2, 0x8FBD, 0x8FC8, 0x8FD3, 0x8FDE, 0x8FE9,
		0x8FF3, 0x8FFE, 0x9009, 0x9014, 0x901F, 0x902A,
		0x9034, 0x903F, 0x904A, 0x9055, 0x905F, 0x906A,
		0x9075, 0x9080, 0x908A, 0x9095, 0x90A0, 0x90AA,
		0x90B5, 0x90C0, 0x90CB, 0x90D5, 0x90E0, 0x90EA,
		0x90F5, 0x9100, 0x910A, 0x9115, 0x911F, 0x912A,
		0x9135, 0x913F, 0x914A, 0x9154, 0x915F, 0x9169,
		0x9174, 0x917E, 0x9189, 0x9193, 0x919E, 0x91A8,
		0x91B3, 0x91BD, 0x91C7, 0x91D2, 0x91DC, 0x91E7,
		0x91F1, 0x91FB, 0x9206, 0x9210, 0x921B, 0x9225,
		0x922F, 0x923A, 0x9244, 0x924E, 0x9259, 0x9263,
		0x926D, 0x9277, 0x9282, 0x928C, 0x9296, 0x92A0,
		0x92AB, 0x92B5, 0x92BF, 0x92C9, 0x92D4, 0x92DE,
		0x92E8, 0x92F2, 0x92FC, 0x9306, 0x9311, 0x931B,
		0x9325, 0x932F, 0x9339, 0x9343, 0x934D, 0x9357,
		0x9361, 0x936B, 0x9376, 0x9380, 0x938A, 0x9394,
		0x939E, 0x93A8, 0x93B2, 0x93BC, 0x93C6, 0x93D0,
		0x93DA, 0x93E4, 0x93EE, 0x93F8, 0x9402, 0x940B,
		0x9415, 0x941F, 0x9429, 0x9433, 0x943D, 0x9447,
		0x9451, 0x945B, 0x9465, 0x946E, 0x9478, 0x9482,
		0x948C, 0x9496, 0x94A0, 0x94A9, 0x94B3, 0x94BD,
		0x94C7, 0x94D1, 0x94DA, 0x94E4, 0x94EE, 0x94F8,
		0x9501, 0x950B, 0x9515, 0x951F, 0x9528, 0x9532,
		0x953C, 0x9545, 0x954F, 0x9559, 0x9562, 0x956C,
		0x9576, 0x957F, 0x9589, 0x9593, 0x959C, 0x95A6,
		0x95AF, 0x95B9, 0x95C3, 0x95CC, 0x95D6, 0x95DF,
		0x95E9, 0x95F2, 0x95FC, 0x9606, 0x960F, 0x9619,
		0x9622, 0x962C, 0x9635, 0x963F, 0x9648, 0x9652,
		0x965B, 0x9665, 0x966E, 0x9677, 0x9681, 0x968A,
		0x9694, 0x969D, 0x96A7, 0x96B0, 0x96B9, 0x96C3,
		0x96CC, 0x96D6, 0x96DF, 0x96E8, 0x96F2, 0x96FB,
		0x9705, 0x970E, 0x9717, 0x9721, 0x972A, 0x9733,
		0x973C, 0x9746, 0x974F, 0x9758, 0x9762, 0x976B,
		0x9774, 0x977D, 0x9787, 0x9790, 0x9799, 0x97A2,
		0x97AC, 0x97B5, 0x97BE, 0x97C7, 0x97D1, 0x97DA,
		0x97E3, 0x97EC, 0x97F5, 0x97FE, 0x9808, 0x9811,
		0x981A, 0x9823, 0x982C, 0x9835, 0x983F, 0x9848,
		0x9851, 0x985A, 0x9863, 0x986C, 0x9875, 0x987E,
		0x9887, 0x9890, 0x9899, 0x98A3, 0x98AC, 0x98B5,
		0x98BE, 0x98C7, 0x98D0, 0x98D9, 0x98E2, 0x98EB,
		0x98F4, 0x98FD, 0x9906, 0x990F, 0x9918, 0x9921,
		0x992A, 0x9933, 0x993C, 0x9945, 0x994E, 0x9956,
		0x995F, 0x9968, 0x9971, 0x997A, 0x9983, 0x998C,
		0x9995, 0x999E, 0x99A7, 0x99AF, 0x99B8, 0x99C1,
		0x99CA, 0x99D3, 0x99DC, 0x99E5, 0x99ED, 0x99F6,
		0x99FF, 0x9A08, 0x9A11, 0x9A1A, 0x9A22, 0x9A2B,
		0x9A34, 0x9A3D, 0x9A46, 0x9A4E, 0x9A57, 0x9A60,
		0x9A69, 0x9A71, 0x9A7A, 0x9A83, 0x9A8C, 0x9A94,
		0x9A9D, 0x9AA6, 0x9AAE, 0x9AB7, 0x9AC0, 0x9AC9,
		0x9AD1, 0x9ADA, 0x9AE3, 0x9AEB, 0x9AF4, 0x9AFD,
		0x9B05, 0x9B0E, 0x9B17, 0x9B1F, 0x9B28, 0x9B30,
		0x9B39, 0x9B42, 0x9B4A, 0x9B53, 0x9B5C, 0x9B64,
		0x9B6D, 0x9B75, 0x9B7E, 0x9B86, 0x9B8F, 0x9B98,
		0x9BA0, 0x9BA9, 0x9BB1, 0x9BBA, 0x9BC2, 0x9BCB,
		0x9BD3, 0x9BDC, 0x9BE4, 0x9BED, 0x9BF5, 0x9BFE,
		0x9C06, 0x9C0F, 0x9C17, 0x9C20, 0x9C28, 0x9C31,
		0x9C39, 0x9C42, 0x9C4A, 0x9C53, 0x9C5B, 0x9C64,
		0x9C6C, 0x9C74, 0x9C7D, 0x9C85, 0x9C8E, 0x9C96,
		0x9C9F, 0x9CA7, 0x9CAF, 0x9CB8, 0x9CC0, 0x9CC9,
		0x9CD1, 0x9CD9, 0x9CE2, 0x9CEA, 0x9CF2, 0x9CFB,
		0x9D03, 0x9D0B, 0x9D14, 0x9D1C, 0x9D24, 0x9D2D,
		0x9D35, 0x9D3D, 0x9D46, 0x9D4E, 0x9D56, 0x9D5E,
		0x9D67, 0x9D6F, 0x9D77, 0x9D80, 0x9D88, 0x9D90,
		0x9D98, 0x9DA1, 0x9DA9, 0x9DB1, 0x9DB9, 0x9DC2,
		0x9DCA, 0x9DD2, 0x9DDA, 0x9DE2, 0x9DEB, 0x9DF3,
		0x9DFB, 0x9E03, 0x9E0C, 0x9E14, 0x9E1C, 0x9E24,
		0x9E2C, 0x9E34, 0x9E3D, 0x9E45, 0x9E4D, 0x9E55,
		0x9E5D, 0x9E65, 0x9E6D, 0x9E76, 0x9E7E, 0x9E86,
		0x9E8E, 0x9E96, 0x9E9E, 0x9EA6, 0x9EAE, 0x9EB6,
		0x9EBF, 0x9EC7, 0x9ECF, 0x9ED7, 0x9EDF, 0x9EE7,
		0x9EEF, 0x9EF7, 0x9EFF, 0x9F07, 0x9F0F, 0x9F17,
		0x9F1F, 0x9F27, 0x9F2F, 0x9F37, 0x9F3F, 0x9F47,
		0x9F4F, 0x9F57, 0x9F5F, 0x9F67, 0x9F6F, 0x9F77,
		0x9F7F, 0x9F87, 0x9F8F, 0x9F97, 0x9F9F, 0x9FA7,
		0x9FAF, 0x9FB7, 0x9FBF, 0x9FC7, 0x9FCF, 0x9FD7,
		0x9FDF, 0x9FE7, 0x9FEF, 0x9FF7, 0x9FFF,
};

static unsigned int yuv12bpc_regamma_lut[] = {
		0x6000, 0x6047, 0x608F, 0x60D7, 0x611F, 0x6167,
		0x61AF, 0x61F7, 0x623F, 0x6287, 0x62CF, 0x6317,
		0x635F, 0x63A7, 0x63EF, 0x6437, 0x647F, 0x64C7,
		0x650F, 0x6557, 0x659D, 0x65E1, 0x6623, 0x6664,
		0x66A3, 0x66E1, 0x671D, 0x6758, 0x6792, 0x67CB,
		0x6802, 0x6839, 0x686F, 0x68A4, 0x68D7, 0x690B,
		0x693D, 0x696E, 0x699F, 0x69CF, 0x69FF, 0x6A2D,
		0x6A5C, 0x6A89, 0x6AB6, 0x6AE3, 0x6B0E, 0x6B3A,
		0x6B65, 0x6B8F, 0x6BB9, 0x6BE2, 0x6C0B, 0x6C34,
		0x6C5C, 0x6C84, 0x6CAB, 0x6CD2, 0x6CF9, 0x6D1F,
		0x6D45, 0x6D6A, 0x6D8F, 0x6DB4, 0x6DD9, 0x6DFD,
		0x6E21, 0x6E44, 0x6E68, 0x6E8B, 0x6EAD, 0x6ED0,
		0x6EF2, 0x6F14, 0x6F36, 0x6F57, 0x6F78, 0x6F99,
		0x6FBA, 0x6FDB, 0x6FFB, 0x701B, 0x703B, 0x705A,
		0x707A, 0x7099, 0x70B8, 0x70D7, 0x70F6, 0x7114,
		0x7132, 0x7150, 0x716E, 0x718C, 0x71A9, 0x71C7,
		0x71E4, 0x7201, 0x721E, 0x723A, 0x7257, 0x7273,
		0x7290, 0x72AC, 0x72C8, 0x72E3, 0x72FF, 0x731A,
		0x7336, 0x7351, 0x736C, 0x7387, 0x73A2, 0x73BC,
		0x73D7, 0x73F1, 0x740C, 0x7426, 0x7440, 0x745A,
		0x7474, 0x748D, 0x74A7, 0x74C0, 0x74DA, 0x74F3,
		0x750C, 0x7525, 0x753E, 0x7557, 0x756F, 0x7588,
		0x75A0, 0x75B9, 0x75D1, 0x75E9, 0x7601, 0x7619,
		0x7631, 0x7649, 0x7661, 0x7678, 0x7690, 0x76A7,
		0x76BE, 0x76D6, 0x76ED, 0x7704, 0x771B, 0x7732,
		0x7748, 0x775F, 0x7776, 0x778C, 0x77A3, 0x77B9,
		0x77D0, 0x77E6, 0x77FC, 0x7812, 0x7828, 0x783E,
		0x7854, 0x786A, 0x787F, 0x7895, 0x78AB, 0x78C0,
		0x78D5, 0x78EB, 0x7900, 0x7915, 0x792A, 0x7940,
		0x7955, 0x796A, 0x797E, 0x7993, 0x79A8, 0x79BD,
		0x79D1, 0x79E6, 0x79FA, 0x7A0F, 0x7A23, 0x7A37,
		0x7A4C, 0x7A60, 0x7A74, 0x7A88, 0x7A9C, 0x7AB0,
		0x7AC4, 0x7AD8, 0x7AEC, 0x7AFF, 0x7B13, 0x7B27,
		0x7B3A, 0x7B4E, 0x7B61, 0x7B75, 0x7B88, 0x7B9B,
		0x7BAE, 0x7BC2, 0x7BD5, 0x7BE8, 0x7BFB, 0x7C0E,
		0x7C21, 0x7C34, 0x7C47, 0x7C59, 0x7C6C, 0x7C7F,
		0x7C92, 0x7CA4, 0x7CB7, 0x7CC9, 0x7CDC, 0x7CEE,
		0x7D00, 0x7D13, 0x7D25, 0x7D37, 0x7D49, 0x7D5C,
		0x7D6E, 0x7D80, 0x7D92, 0x7DA4, 0x7DB6, 0x7DC8,
		0x7DD9, 0x7DEB, 0x7DFD, 0x7E0F, 0x7E20, 0x7E32,
		0x7E44, 0x7E55, 0x7E67, 0x7E78, 0x7E8A, 0x7E9B,
		0x7EAC, 0x7EBE, 0x7ECF, 0x7EE0, 0x7EF1, 0x7F03,
		0x7F14, 0x7F25, 0x7F36, 0x7F47, 0x7F58, 0x7F69,
		0x7F7A, 0x7F8B, 0x7F9B, 0x7FAC, 0x7FBD, 0x7FCE,
		0x7FDE, 0x7FEF, 0x8000, 0x8010, 0x8021, 0x8031,
		0x8042, 0x8052, 0x8063, 0x8073, 0x8084, 0x8094,
		0x80A4, 0x80B4, 0x80C5, 0x80D5, 0x80E5, 0x80F5,
		0x8105, 0x8115, 0x8125, 0x8135, 0x8145, 0x8155,
		0x8165, 0x8175, 0x8185, 0x8195, 0x81A5, 0x81B4,
		0x81C4, 0x81D4, 0x81E3, 0x81F3, 0x8203, 0x8212,
		0x8222, 0x8231, 0x8241, 0x8250, 0x8260, 0x826F,
		0x827F, 0x828E, 0x829D, 0x82AD, 0x82BC, 0x82CB,
		0x82DA, 0x82EA, 0x82F9, 0x8308, 0x8317, 0x8326,
		0x8335, 0x8344, 0x8353, 0x8362, 0x8371, 0x8380,
		0x838F, 0x839E, 0x83AD, 0x83BC, 0x83CB, 0x83D9,
		0x83E8, 0x83F7, 0x8406, 0x8414, 0x8423, 0x8432,
		0x8440, 0x844F, 0x845D, 0x846C, 0x847A, 0x8489,
		0x8497, 0x84A6, 0x84B4, 0x84C3, 0x84D1, 0x84DF,
		0x84EE, 0x84FC, 0x850A, 0x8519, 0x8527, 0x8535,
		0x8543, 0x8552, 0x8560, 0x856E, 0x857C, 0x858A,
		0x8598, 0x85A6, 0x85B4, 0x85C2, 0x85D0, 0x85DE,
		0x85EC, 0x85FA, 0x8608, 0x8616, 0x8624, 0x8632,
		0x863F, 0x864D, 0x865B, 0x8669, 0x8677, 0x8684,
		0x8692, 0x86A0, 0x86AD, 0x86BB, 0x86C9, 0x86D6,
		0x86E4, 0x86F1, 0x86FF, 0x870C, 0x871A, 0x8727,
		0x8735, 0x8742, 0x8750, 0x875D, 0x876B, 0x8778,
		0x8785, 0x8793, 0x87A0, 0x87AD, 0x87BA, 0x87C8,
		0x87D5, 0x87E2, 0x87EF, 0x87FD, 0x880A, 0x8817,
		0x8824, 0x8831, 0x883E, 0x884B, 0x8858, 0x8866,
		0x8873, 0x8880, 0x888D, 0x889A, 0x88A7, 0x88B3,
		0x88C0, 0x88CD, 0x88DA, 0x88E7, 0x88F4, 0x8901,
		0x890E, 0x891A, 0x8927, 0x8934, 0x8941, 0x894E,
		0x895A, 0x8967, 0x8974, 0x8980, 0x898D, 0x899A,
		0x89A6, 0x89B3, 0x89BF, 0x89CC, 0x89D9, 0x89E5,
		0x89F2, 0x89FE, 0x8A0B, 0x8A17, 0x8A24, 0x8A30,
		0x8A3D, 0x8A49, 0x8A56, 0x8A62, 0x8A6E, 0x8A7B,
		0x8A87, 0x8A93, 0x8AA0, 0x8AAC, 0x8AB8, 0x8AC5,
		0x8AD1, 0x8ADD, 0x8AE9, 0x8AF6, 0x8B02, 0x8B0E,
		0x8B1A, 0x8B26, 0x8B32, 0x8B3F, 0x8B4B, 0x8B57,
		0x8B63, 0x8B6F, 0x8B7B, 0x8B87, 0x8B93, 0x8B9F,
		0x8BAB, 0x8BB7, 0x8BC3, 0x8BCF, 0x8BDB, 0x8BE7,
		0x8BF3, 0x8BFF, 0x8C0B, 0x8C17, 0x8C23, 0x8C2E,
		0x8C3A, 0x8C46, 0x8C52, 0x8C5E, 0x8C6A, 0x8C75,
		0x8C81, 0x8C8D, 0x8C99, 0x8CA4, 0x8CB0, 0x8CBC,
		0x8CC8, 0x8CD3, 0x8CDF, 0x8CEB, 0x8CF6, 0x8D02,
		0x8D0D, 0x8D19, 0x8D25, 0x8D30, 0x8D3C, 0x8D47,
		0x8D53, 0x8D5E, 0x8D6A, 0x8D75, 0x8D81, 0x8D8C,
		0x8D98, 0x8DA3, 0x8DAF, 0x8DBA, 0x8DC6, 0x8DD1,
		0x8DDC, 0x8DE8, 0x8DF3, 0x8DFF, 0x8E0A, 0x8E15,
		0x8E21, 0x8E2C, 0x8E37, 0x8E43, 0x8E4E, 0x8E59,
		0x8E64, 0x8E70, 0x8E7B, 0x8E86, 0x8E91, 0x8E9C,
		0x8EA8, 0x8EB3, 0x8EBE, 0x8EC9, 0x8ED4, 0x8EDF,
		0x8EEB, 0x8EF6, 0x8F01, 0x8F0C, 0x8F17, 0x8F22,
		0x8F2D, 0x8F38, 0x8F43, 0x8F4E, 0x8F59, 0x8F64,
		0x8F6F, 0x8F7A, 0x8F85, 0x8F90, 0x8F9B, 0x8FA6,
		0x8FB1, 0x8FBC, 0x8FC7, 0x8FD2, 0x8FDD, 0x8FE7,
		0x8FF2, 0x8FFD, 0x9008, 0x9013, 0x901E, 0x9028,
		0x9033, 0x903E, 0x9049, 0x9054, 0x905E, 0x9069,
		0x9074, 0x907F, 0x9089, 0x9094, 0x909F, 0x90A9,
		0x90B4, 0x90BF, 0x90C9, 0x90D4, 0x90DF, 0x90E9,
		0x90F4, 0x90FF, 0x9109, 0x9114, 0x911E, 0x9129,
		0x9134, 0x913E, 0x9149, 0x9153, 0x915E, 0x9168,
		0x9173, 0x917D, 0x9188, 0x9192, 0x919D, 0x91A7,
		0x91B2, 0x91BC, 0x91C6, 0x91D1, 0x91DB, 0x91E6,
		0x91F0, 0x91FB, 0x9205, 0x920F, 0x921A, 0x9224,
		0x922E, 0x9239, 0x9243, 0x924D, 0x9258, 0x9262,
		0x926C, 0x9276, 0x9281, 0x928B, 0x9295, 0x929F,
		0x92AA, 0x92B4, 0x92BE, 0x92C8, 0x92D3, 0x92DD,
		0x92E7, 0x92F1, 0x92FB, 0x9305, 0x9310, 0x931A,
		0x9324, 0x932E, 0x9338, 0x9342, 0x934C, 0x9356,
		0x9360, 0x936B, 0x9375, 0x937F, 0x9389, 0x9393,
		0x939D, 0x93A7, 0x93B1, 0x93BB, 0x93C5, 0x93CF,
		0x93D9, 0x93E3, 0x93ED, 0x93F7, 0x9401, 0x940B,
		0x9415, 0x941E, 0x9428, 0x9432, 0x943C, 0x9446,
		0x9450, 0x945A, 0x9464, 0x946E, 0x9477, 0x9481,
		0x948B, 0x9495, 0x949F, 0x94A9, 0x94B2, 0x94BC,
		0x94C6, 0x94D0, 0x94DA, 0x94E3, 0x94ED, 0x94F7,
		0x9501, 0x950A, 0x9514, 0x951E, 0x9527, 0x9531,
		0x953B, 0x9545, 0x954E, 0x9558, 0x9562, 0x956B,
		0x9575, 0x957F, 0x9588, 0x9592, 0x959B, 0x95A5,
		0x95AF, 0x95B8, 0x95C2, 0x95CB, 0x95D5, 0x95DF,
		0x95E8, 0x95F2, 0x95FB, 0x9605, 0x960E, 0x9618,
		0x9621, 0x962B, 0x9634, 0x963E, 0x9647, 0x9651,
		0x965A, 0x9664, 0x966D, 0x9677, 0x9680, 0x968A,
		0x9693, 0x969D, 0x96A6, 0x96AF, 0x96B9, 0x96C2,
		0x96CC, 0x96D5, 0x96DE, 0x96E8, 0x96F1, 0x96FB,
		0x9704, 0x970D, 0x9717, 0x9720, 0x9729, 0x9733,
		0x973C, 0x9745, 0x974E, 0x9758, 0x9761, 0x976A,
		0x9774, 0x977D, 0x9786, 0x978F, 0x9799, 0x97A2,
		0x97AB, 0x97B4, 0x97BE, 0x97C7, 0x97D0, 0x97D9,
		0x97E2, 0x97EC, 0x97F5, 0x97FE, 0x9807, 0x9810,
		0x9819, 0x9823, 0x982C, 0x9835, 0x983E, 0x9847,
		0x9850, 0x9859, 0x9862, 0x986C, 0x9875, 0x987E,
		0x9887, 0x9890, 0x9899, 0x98A2, 0x98AB, 0x98B4,
		0x98BD, 0x98C6, 0x98CF, 0x98D8, 0x98E1, 0x98EA,
		0x98F3, 0x98FC, 0x9905, 0x990E, 0x9917, 0x9920,
		0x9929, 0x9932, 0x993B, 0x9944, 0x994D, 0x9956,
		0x995F, 0x9968, 0x9971, 0x997A, 0x9983, 0x998B,
		0x9994, 0x999D, 0x99A6, 0x99AF, 0x99B8, 0x99C1,
		0x99CA, 0x99D3, 0x99DB, 0x99E4, 0x99ED, 0x99F6,
		0x99FF, 0x9A08, 0x9A10, 0x9A19, 0x9A22, 0x9A2B,
		0x9A34, 0x9A3C, 0x9A45, 0x9A4E, 0x9A57, 0x9A5F,
		0x9A68, 0x9A71, 0x9A7A, 0x9A82, 0x9A8B, 0x9A94,
		0x9A9D, 0x9AA5, 0x9AAE, 0x9AB7, 0x9ABF, 0x9AC8,
		0x9AD1, 0x9ADA, 0x9AE2, 0x9AEB, 0x9AF4, 0x9AFC,
		0x9B05, 0x9B0E, 0x9B16, 0x9B1F, 0x9B28, 0x9B30,
		0x9B39, 0x9B41, 0x9B4A, 0x9B53, 0x9B5B, 0x9B64,
		0x9B6C, 0x9B75, 0x9B7E, 0x9B86, 0x9B8F, 0x9B97,
		0x9BA0, 0x9BA8, 0x9BB1, 0x9BBA, 0x9BC2, 0x9BCB,
		0x9BD3, 0x9BDC, 0x9BE4, 0x9BED, 0x9BF5, 0x9BFE,
		0x9C06, 0x9C0F, 0x9C17, 0x9C20, 0x9C28, 0x9C31,
		0x9C39, 0x9C42, 0x9C4A, 0x9C52, 0x9C5B, 0x9C63,
		0x9C6C, 0x9C74, 0x9C7D, 0x9C85, 0x9C8D, 0x9C96,
		0x9C9E, 0x9CA7, 0x9CAF, 0x9CB8, 0x9CC0, 0x9CC8,
		0x9CD1, 0x9CD9, 0x9CE1, 0x9CEA, 0x9CF2, 0x9CFA,
		0x9D03, 0x9D0B, 0x9D13, 0x9D1C, 0x9D24, 0x9D2C,
		0x9D35, 0x9D3D, 0x9D45, 0x9D4E, 0x9D56, 0x9D5E,
		0x9D67, 0x9D6F, 0x9D77, 0x9D7F, 0x9D88, 0x9D90,
		0x9D98, 0x9DA0, 0x9DA9, 0x9DB1, 0x9DB9, 0x9DC1,
		0x9DCA, 0x9DD2, 0x9DDA, 0x9DE2, 0x9DEB, 0x9DF3,
		0x9DFB, 0x9E03, 0x9E0B, 0x9E14, 0x9E1C, 0x9E24,
		0x9E2C, 0x9E34, 0x9E3C, 0x9E45, 0x9E4D, 0x9E55,
		0x9E5D, 0x9E65, 0x9E6D, 0x9E75, 0x9E7E, 0x9E86,
		0x9E8E, 0x9E96, 0x9E9E, 0x9EA6, 0x9EAE, 0x9EB6,
		0x9EBE, 0x9EC7, 0x9ECF, 0x9ED7, 0x9EDF, 0x9EE7,
		0x9EEF, 0x9EF7, 0x9EFF, 0x9F07, 0x9F0F, 0x9F17,
		0x9F1F, 0x9F27, 0x9F2F, 0x9F37, 0x9F3F, 0x9F47,
		0x9F4F, 0x9F57, 0x9F5F, 0x9F67, 0x9F6F, 0x9F77,
		0x9F7F, 0x9F87, 0x9F8F, 0x9F97, 0x9F9F, 0x9FA7,
		0x9FAF, 0x9FB7, 0x9FBF, 0x9FC7, 0x9FCF, 0x9FD7,
		0x9FDF, 0x9FE7, 0x9FEF, 0x9FF7, 0x9FFF,
};

int tegra_nvdisp_set_output_lut(struct tegra_dc *dc,
					struct tegra_dc_lut *lut)
{
	tegra_dc_writel(dc,
			tegra_dc_reg_l32(lut->phy_addr),
			nvdisp_output_lut_base_r());
	tegra_dc_writel(dc,
			tegra_dc_reg_h32(lut->phy_addr),
			nvdisp_output_lut_base_hi_r());
	tegra_dc_writel(dc, nvdisp_output_lut_ctl_size_1025_f() |
			    nvdisp_output_lut_ctl_mode_f(0x1), /* interpolate */
			nvdisp_output_lut_ctl_r());

	return 0;
}

static void nvdisp_copy_output_lut(u64 *dst, unsigned int *src, int size)
{
	int i;
	u64 r = 0;

	for (i = 0; i < size; i++) {
		r = src[i];
		dst[i] = (r << 32) | (r << 16) | r;
	}
}

void tegra_nvdisp_get_default_cmu(struct tegra_dc_cmu *default_cmu)
{
	nvdisp_copy_output_lut(default_cmu->rgb, default_srgb_regamma_lut,
			       NVDISP_OUTPUT_LUT_SIZE);
}

static int nvdisp_alloc_output_lut(struct tegra_dc *dc)
{
	struct tegra_dc_lut *lut = &dc->cmu;

	if (!lut)
		return -ENOMEM;

	/* Allocate the memory for LUT */
	lut->size = NVDISP_OUTPUT_LUT_SIZE * sizeof(u64);
	lut->rgb = (u64 *)dma_zalloc_coherent(&dc->ndev->dev, lut->size,
			&lut->phy_addr, GFP_KERNEL);
	if (!lut->rgb)
		return -ENOMEM;

	/* Init LUT with cmu data provided from DT file */
	if (dc->pdata->cmu && dc->pdata->cmu_enable) {
		memcpy(lut->rgb, dc->pdata->cmu, lut->size);
		return 0;
	}

	/* Init the LUT table with default sRGB values */
	nvdisp_copy_output_lut(lut->rgb, default_srgb_regamma_lut,
			       NVDISP_OUTPUT_LUT_SIZE);

	return 0;
}

static int nvdisp_alloc_input_lut(struct tegra_dc *dc,
					struct tegra_dc_win *win,
					bool winlut)
{
	struct tegra_dc_lut *lut;

	if (winlut)
		lut = &win->lut;
	else
		lut = &dc->fb_lut;

	if (!lut)
		return -ENOMEM;

	/* Allocate the memory for LUT */
	lut->size = NVDISP_INPUT_LUT_SIZE * sizeof(u64);
	lut->rgb = (u64 *)dma_zalloc_coherent(&dc->ndev->dev, lut->size,
			&lut->phy_addr, GFP_KERNEL);
	if (!lut->rgb)
		return -ENOMEM;

	return 0;
}

/*	Deassert all the common nvdisplay resets.
 *      Misc and all windows groups are placed in common.
 */
static int __maybe_unused
	tegra_nvdisp_common_reset_deassert(struct tegra_dc *dc)
{
	u8 i;
	int err;


	for ( i = 0; i < DC_N_WINDOWS; i++) {

		if (!nvdisp_common_rst[i]) {
			dev_err(&dc->ndev->dev, "No nvdisp resets available\n");
			return -EINVAL;
		}

		err = reset_control_deassert(nvdisp_common_rst[i]);
		if (err) {
			dev_err(&dc->ndev->dev, "Unable to reset misc\n");
			return err;
		}
	}
	return 0;
}

static int __maybe_unused tegra_nvdisp_common_reset_assert(struct tegra_dc *dc)
{
	u8 i;
	int err;


	for ( i = 0; i < DC_N_WINDOWS; i++) {
		if (!nvdisp_common_rst[i]) {
			dev_err(&dc->ndev->dev, "No Nvdisp resets available\n");
			return -EINVAL;
		}

		err = reset_control_assert(nvdisp_common_rst[i]);
		if (err) {
			dev_err(&dc->ndev->dev, "Unable to reset misc\n");
			return err;
		}
	}
	return 0;
}

static int __maybe_unused tegra_nvdisp_wgrp_reset_assert(struct tegra_dc *dc)
{
	int idx, err = 0;
	for_each_set_bit(idx, &dc->valid_windows, DC_N_WINDOWS) {
		err = reset_control_assert(nvdisp_common_rst[idx+1]);
		if (err)
			dev_err(&dc->ndev->dev, "Failed window %d rst\n", idx);
	}

	return err;
}

static int tegra_nvdisp_wgrp_reset_deassert(struct tegra_dc *dc)
{
	int idx, err = 0;

	/* Misc deassert is common for  all windows */
	err = reset_control_deassert(nvdisp_common_rst[0]);
	if (err)
		dev_err(&dc->ndev->dev, "Failed Misc deassert\n");

	for_each_set_bit(idx, &dc->valid_windows, DC_N_WINDOWS) {
		err = reset_control_deassert(nvdisp_common_rst[idx+1]);
		if (err)
			dev_err(&dc->ndev->dev, "Failed window deassert\n");
	}

	return err;
}

static int tegra_nvdisp_reset_prepare(struct tegra_dc *dc)
{
	char rst_name[6];
	int i;

	/* Use only if bpmp is enabled */
	if (!tegra_bpmp_running())
		return 0;

	nvdisp_common_rst[0] =
		devm_reset_control_get(&dc->ndev->dev, "misc");
	if (IS_ERR(nvdisp_common_rst[0])) {
		dev_err(&dc->ndev->dev, "Unable to get misc reset\n");
		return PTR_ERR(nvdisp_common_rst[0]);
	}

	for ( i = 0; i < DC_N_WINDOWS; i++) {
		snprintf(rst_name, sizeof(rst_name), "wgrp%u", i);
		nvdisp_common_rst[i+1] =
			devm_reset_control_get(&dc->ndev->dev, rst_name);
		if (IS_ERR(nvdisp_common_rst[i+1])) {
			dev_err(&dc->ndev->dev,"Unable to get %s reset\n",
					rst_name);
			return PTR_ERR(nvdisp_common_rst[i+1]);
		}
	}

	return 0;
}

int tegra_nvdisp_set_compclk(struct tegra_dc *dc)
{
	int i, index = 0;
	unsigned long rate = 0;

	/* comp clk will be maximum of head0/1/2 */
	mutex_lock(&tegra_nvdisp_lock);
	for (i = 0; i < TEGRA_MAX_DC; i++) {
		if (clk_client[i].inuse && clk_client[i].dc &&
			rate <= clk_get_rate(clk_client[i].dc->clk)) {
			rate = clk_get_rate(clk_client[i].dc->clk);
			index = i;
		}
	}

	if (rate == 0) {
		/* none of the pclks is on. disable compclk */
		if (compclk_already_on) {
			tegra_disp_clk_disable_unprepare(compclk);
			compclk_already_on = false;
		}

		mutex_unlock(&tegra_nvdisp_lock);
		return 0;
	}

	pr_info(" rate get on compclk %ld\n", rate);

	/* save current clock client index */
	cur_clk_client_index = index;

	/* Set parent for Display clock */
	clk_set_parent(compclk, clk_client[index].dc->clk);

	/* Enable Display comp clock */
	if (!compclk_already_on) {
		tegra_disp_clk_prepare_enable(compclk);
		compclk_already_on = true;
	}

	mutex_unlock(&tegra_nvdisp_lock);
	return 0;
}

int tegra_nvdisp_test_and_set_compclk(unsigned long rate, struct tegra_dc *dc)
{
	bool inuse_state;

	mutex_lock(&tegra_nvdisp_lock);

	/* check if current dc->clk is the source of compclk and whether
	*  rate to which dc->clk is going to be changed is less than current
	*  compclk rate. If yes, it is necessary to switch compclk source to
	*  next highest dc->clk. Otherwise current dc->clk acting as parent to
	*  compclk will lower its rate causing compclk rate to get lowered
	*  and head will underflow for brief period before compclk is again
	*  set to highest of all dc->clks
	*/
	if (cur_clk_client_index == dc->ctrl_num &&
		rate < clk_get_rate(compclk)) {
		inuse_state = clk_client[dc->ctrl_num].inuse;

		/* set inuse flag to false to make sure current
		*  dc->clk will not be used as source for compclk
		*/
		clk_client[dc->ctrl_num].inuse = false;

		mutex_unlock(&tegra_nvdisp_lock);

		tegra_nvdisp_set_compclk(dc);
		clk_client[dc->ctrl_num].inuse = inuse_state;
		return 0;
	}

	mutex_unlock(&tegra_nvdisp_lock);
	return 0;
}

static int _tegra_nvdisp_init_once(struct tegra_dc *dc)
{
	int ret = 0;
	int i;
	char syncpt_name[] = "disp_a";

/*	mutex_lock(&tegra_nvdisp_lock); */

	ret = tegra_nvdisp_reset_prepare(dc);
	if (ret)
		return ret;

	/* Get the nvdisplay_hub and nvdisplay_disp clock and enable
	 * it by default. Change the rates based on requirement later
	 */
	hubclk = tegra_disp_clk_get(&dc->ndev->dev, "nvdisplayhub");
	if (IS_ERR_OR_NULL(hubclk)) {
		dev_err(&dc->ndev->dev, "can't get display hub clock\n");
		ret = -ENOENT;
		goto INIT_EXIT;
	}

	compclk = tegra_disp_clk_get(&dc->ndev->dev, "nvdisplay_disp");
	if (IS_ERR_OR_NULL(compclk)) {
		dev_err(&dc->ndev->dev, "can't get display comp clock\n");
		ret = -ENOENT;
		goto INIT_CLK_ERR;
	}

	/* Init sycpt ids */
	dc->valid_windows = 0x3f; /* Assign all windows to this head */
	for (i = 0; i < DC_N_WINDOWS; ++i, ++syncpt_name[5]) {
		struct tegra_dc_win *win = tegra_dc_get_window(dc, i);

		win->syncpt.id = nvhost_get_syncpt_client_managed(dc->ndev,
								syncpt_name);

		/* allocate input LUT memory and assign to HW */
		if (nvdisp_alloc_input_lut(dc, win, true))
			goto INIT_ERR;
	}

	dc->valid_windows = 0;

	/* Assign powergate id for each partition*/
	nvdisp_pg[NVDISP_PD_INDEX].powergate_id =
			tegra_pd_get_powergate_id(nvdisp_disa_pd);
	nvdisp_pg[NVDISPB_PD_INDEX].powergate_id =
			tegra_pd_get_powergate_id(nvdisp_disb_pd);
	nvdisp_pg[NVDISPC_PD_INDEX].powergate_id =
			tegra_pd_get_powergate_id(nvdisp_disc_pd);

#ifdef CONFIG_TEGRA_ISOMGR
	/* Register with isomgr */
	if (tegra_nvdisp_isomgr_register(TEGRA_ISO_CLIENT_DISP_0,
		tegra_dc_calc_min_bandwidth(dc))) {
		dev_err(&dc->ndev->dev, "could not register isomgr\n");
		goto INIT_ERR;
	}
#endif

	goto INIT_EXIT;

INIT_ERR:
#ifdef CONFIG_TEGRA_ISOMGR
	tegra_nvdisp_isomgr_unregister();
#endif

	for (i = 0; i < DC_N_WINDOWS; ++i) {
		struct tegra_dc_lut *lut;
		struct tegra_dc_win *win = tegra_dc_get_window(dc, i);

		/* Allocate the memory for Input LUT & fb LUT*/
		lut = &win->lut;
		if (lut->rgb)
			dma_free_coherent(&dc->ndev->dev, lut->size,
				(void *)lut->rgb, lut->phy_addr);
	}
INIT_CLK_ERR:
	if (hubclk)
		tegra_disp_clk_put(&dc->ndev->dev, hubclk);

	if (compclk)
		tegra_disp_clk_put(&dc->ndev->dev, compclk);
INIT_EXIT:
/*	mutex_unlock(&tegra_nvdisp_lock); */
	return ret;

}

int tegra_nvdisp_set_chroma_lpf(struct tegra_dc *dc)
{
	/* if color fmt is yuv_422 and postcomp support yuv422
	 * enable chroma lpf by default
	 */
	u32 postcomp_capa = tegra_dc_readl(dc, nvdisp_postcomp_capa_r());
	u32 chroma_lpf = tegra_dc_readl(dc, nvdisp_procamp_r());
	chroma_lpf &= ~nvdisp_procamp_chroma_lpf_enable_f();

	if ((dc->mode.vmode & FB_VMODE_Y422)  &&
		nvdisp_postcomp_capa_is_yuv422_enable_v(postcomp_capa))
		chroma_lpf |= nvdisp_procamp_chroma_lpf_enable_f();

	/* disable chroma for by_pass mode */
	if (dc->yuv_bypass)
		chroma_lpf &= ~nvdisp_procamp_chroma_lpf_enable_f();

	tegra_dc_writel(dc, chroma_lpf, nvdisp_procamp_r());

	return 0;
}

static int _tegra_nvdisp_set_ec_output_lut(struct tegra_dc *dc,
			struct tegra_dc_mode *mode)
{
	struct tegra_dc_lut *lut = &dc->cmu;

	if (!lut)
		return -ENOMEM;

	if (mode->vmode & FB_VMODE_EC_ENABLE) {
		if (mode->vmode & FB_VMODE_Y24 ||
		    mode->vmode & FB_VMODE_Y30) {
			nvdisp_copy_output_lut(lut->rgb,
				yuv8_10bpc_regamma_lut,
				NVDISP_OUTPUT_LUT_SIZE);
		} else if (mode->vmode & FB_VMODE_Y36) {
			nvdisp_copy_output_lut(lut->rgb,
				yuv12bpc_regamma_lut,
				NVDISP_OUTPUT_LUT_SIZE);
		}
	}

	return 0;
}

int tegra_nvdisp_set_ocsc(struct tegra_dc *dc,
			struct tegra_dc_mode *mode)
{
	u32 csc2_control = nvdisp_csc2_control_output_color_sel_rgb_f();

	/* Check whether the extended colorimetry
	 * is requested in mode set for output csc
	 */

	if (mode->vmode & FB_VMODE_EC_ENABLE) {
		u32 ec = mode->vmode & FB_VMODE_EC_MASK;
		if ((ec == FB_VMODE_EC_ADOBE_YCC601) ||
			(ec == FB_VMODE_EC_SYCC601) ||
			(ec == FB_VMODE_EC_XVYCC601))
			csc2_control =
				nvdisp_csc2_control_output_color_sel_y601_f();
		else if ((ec == FB_VMODE_EC_BT2020_CYCC) ||
			(ec == FB_VMODE_EC_BT2020_YCC_RGB))
			csc2_control =
				nvdisp_csc2_control_output_color_sel_y2020_f();
		else if (ec == FB_VMODE_EC_XVYCC709)
			csc2_control =
				nvdisp_csc2_control_output_color_sel_y709_f();
	}

	/* For yuv-bypass - set to default */
	if (dc->yuv_bypass)
		csc2_control = nvdisp_csc2_control_output_color_sel_rgb_f();

	if ((dc->mode.vmode & FB_VMODE_LIMITED_RANGE) && (!dc->yuv_bypass))
		csc2_control |= nvdisp_csc2_control_limit_rgb_enable_f();
	else
		csc2_control |= nvdisp_csc2_control_limit_rgb_disable_f();

	tegra_dc_writel(dc, csc2_control, nvdisp_csc2_control_r());

	return 0;
}

int tegra_nvdisp_program_mode(struct tegra_dc *dc, struct tegra_dc_mode
				     *mode)
{
	unsigned long v_back_porch;
	unsigned long v_front_porch;
	unsigned long v_sync_width;
	unsigned long v_active;

	if (!dc->mode.pclk)
		return 0;

	/* Bypass flag is officially set during flips, but we need to initialize
	 * it here so that the output CSC and chroma LPF blocks are correctly
	 * programmed during modeset for YUV bypass modes. Otherwise, momentary
	 * screen corruption can be observed during the modeset.
	 */
	dc->yuv_bypass = mode->vmode & FB_VMODE_YUV_MASK;
	if ((mode->vmode & FB_VMODE_Y422) |
		(mode->vmode & FB_VMODE_Y444))
		dc->yuv_bypass = 0;

	v_back_porch = mode->v_back_porch;
	v_front_porch = mode->v_front_porch;
	v_sync_width = mode->v_sync_width;
	v_active = mode->v_active;

	if (mode->vmode == FB_VMODE_INTERLACED) {
		v_back_porch /= 2;
		v_front_porch /= 2;
		v_sync_width /= 2;
		v_active /= 2;
	}

	tegra_dc_get(dc);

	tegra_dc_program_bandwidth(dc, true);
	tegra_dc_writel(dc,
		nvdisp_sync_width_h_f(mode->h_sync_width) |
		nvdisp_sync_width_v_f(v_sync_width),
		nvdisp_sync_width_r());
	if ((dc->out->type == TEGRA_DC_OUT_DP) ||
		(dc->out->type == TEGRA_DC_OUT_FAKE_DP) ||
		(dc->out->type == TEGRA_DC_OUT_NVSR_DP) ||
		(dc->out->type == TEGRA_DC_OUT_LVDS)) {
		tegra_dc_writel(dc,
			nvdisp_back_porch_h_f(mode->h_back_porch) |
			nvdisp_back_porch_v_f(
				(v_back_porch - mode->v_ref_to_sync)),
			nvdisp_back_porch_r());
		tegra_dc_writel(dc,
			nvdisp_front_porch_h_f(mode->h_front_porch) |
			nvdisp_front_porch_v_f(
				(v_front_porch + mode->v_ref_to_sync)),
			nvdisp_front_porch_r());
	} else {
		tegra_dc_writel(dc,
			nvdisp_back_porch_h_f(mode->h_back_porch) |
			nvdisp_back_porch_v_f(v_back_porch),
			nvdisp_back_porch_r());
		tegra_dc_writel(dc,
			nvdisp_front_porch_h_f(mode->h_front_porch) |
			nvdisp_front_porch_v_f(v_front_porch),
			nvdisp_front_porch_r());
	}
	tegra_dc_writel(dc,
			nvdisp_active_h_f(mode->h_active) |
			nvdisp_active_v_f(v_active),
			nvdisp_active_r());


#if defined(CONFIG_TEGRA_DC_INTERLACE)
	if (mode->vmode == FB_VMODE_INTERLACED)
		tegra_dc_writel(dc, INTERLACE_MODE_ENABLE |
			INTERLACE_START_FIELD_1
			| INTERLACE_STATUS_FIELD_1,
			nvdisp_interlace_ctl_r());
	else
		tegra_dc_writel(dc, INTERLACE_MODE_DISABLE,
			nvdisp_interlace_ctl_r());

	if (mode->vmode == FB_VMODE_INTERLACED) {
		tegra_dc_writel(dc,
			nvdisp_interlace_fld2_width_v_f(v_sync_width),
			nvdisp_interlace_fld2_width_r());
		tegra_dc_writel(dc,
			nvdisp_interlace_fld2_bporch_v_f(v_back_porch + 1),
			nvdisp_interlace_fld2_bporch_r());
		tegra_dc_writel(dc,
			nvdisp_interlace_fld2_active_v_f(v_active),
			nvdisp_interlace_fld2_active_r());
		tegra_dc_writel(dc,
			nvdisp_interlace_fld2_fporch_v_f(v_front_porch),
			nvdisp_interlace_fld2_fporch_r());
	}
#endif

	/* TODO: MIPI/CRT/HDMI clock cals */
	/* TODO: confirm shift clock still exists in T186 */
	if (dc->mode.pclk != mode->pclk)
		pr_info("Redo Clock pclk 0x%x != dc-pclk 0x%x\n",
				mode->pclk, dc->mode.pclk);

	tegra_nvdisp_set_ocsc(dc, mode);
	_tegra_nvdisp_set_ec_output_lut(dc, mode);

	tegra_nvdisp_set_chroma_lpf(dc);

	/* general-update */
	tegra_dc_writel(dc, nvdisp_cmd_state_ctrl_general_update_enable_f(),
			nvdisp_cmd_state_ctrl_r());
	tegra_dc_readl(dc, nvdisp_cmd_state_ctrl_r()); /* flush */

#ifdef CONFIG_SWITCH
	switch_set_state(&dc->modeset_switch,
			 (mode->h_active << 16) | mode->v_active);
#endif

	tegra_dc_writel(dc, nvdisp_cmd_state_ctrl_general_act_req_enable_f(),
			nvdisp_cmd_state_ctrl_r());
	tegra_dc_readl(dc, nvdisp_cmd_state_ctrl_r()); /* flush */

	if (dc->out_ops && dc->out_ops->modeset_notifier)
		dc->out_ops->modeset_notifier(dc);

	if (dc->mode_dirty)
		memcpy(&dc->cached_mode, &dc->mode, sizeof(dc->mode));

	tegra_dc_put(dc);

	dc->mode_dirty = false;

	trace_display_mode(dc, &dc->mode);
	return 0;
}


int tegra_nvdisp_init(struct tegra_dc *dc)
{
	char rst_name[6];
	int err = 0;
	char vblank_name[32];

	/* Only need init once no matter how many dc objects */
	if (!dc->ndev->id) {
		err = _tegra_nvdisp_init_once(dc);
		if (err)
			return err;
	}

	/*Lut alloc is needed per dc */
	if (!dc->fb_lut.rgb) {
		if (nvdisp_alloc_input_lut(dc, NULL, false))
			return -ENOMEM;
	}

	/* Output LUT is needed per dc */
	if (!(dc->cmu.rgb)) {
		if (nvdisp_alloc_output_lut(dc))
			return -ENOMEM;
	}

	/* Set the valid windows as per mask */
	dc->valid_windows = dc->pdata->win_mask;

	/* Assign powergate id for each partition*/
	dc->powergate_id = nvdisp_pg[dc->ctrl_num].powergate_id;

	/* Save for powermgmt purpose */
	/* valid_windows should be updated on dynamically changing windows */
	nvdisp_pg[dc->ctrl_num].valid_windows = dc->valid_windows;

	/* Allocate a syncpoint for vblank on each head */
	snprintf(vblank_name, sizeof(vblank_name), "vblank%u", dc->ctrl_num);
	dc->vblank_syncpt = nvhost_get_syncpt_client_managed(dc->ndev,
								vblank_name);

#ifdef CONFIG_TEGRA_ISOMGR
	/* Save reference to isohub bw info */
	tegra_nvdisp_isomgr_attach(dc);
#endif

	/* Take the controller out of reset if bpmp is loaded*/
	if (tegra_bpmp_running() && tegra_platform_is_silicon()) {
		snprintf(rst_name, sizeof(rst_name), "head%u", dc->ctrl_num);
		dc->rst = devm_reset_control_get(&dc->ndev->dev, rst_name);
		if (IS_ERR(dc->rst)) {
			dev_err(&dc->ndev->dev,"Unable to get %s reset\n",
				rst_name);
			return PTR_ERR(dc->rst);
		}
	}

	return err;
}

static int tegra_nvdisp_set_control(struct tegra_dc *dc)
{
	u32 protocol = nvdisp_sor_control_protocol_custom_f();
	u32 reg      = nvdisp_sor_control_r();

	/* Set the protocol type in DT and use from there
	 * Current setting are default ones.
	 */

	if (dc->out->type == TEGRA_DC_OUT_HDMI)	{
		protocol = nvdisp_sor1_control_protocol_tmdsa_f();
		if(!strcmp(dc_or_node_names[dc->ndev->id], "/host1x/sor1"))
			reg = nvdisp_sor1_control_r();
		else if(!strcmp(dc_or_node_names[dc->ndev->id], "/host1x/sor"))
			reg = nvdisp_sor_control_r();
	} else if ((dc->out->type == TEGRA_DC_OUT_DP) ||
		(dc->out->type == TEGRA_DC_OUT_NVSR_DP) ||
		(dc->out->type == TEGRA_DC_OUT_FAKE_DP)) {
		protocol = nvdisp_sor_control_protocol_dpa_f();
		reg = nvdisp_sor_control_r();
	} else if ((dc->out->type == TEGRA_DC_OUT_DSI) ||
		(dc->out->type == TEGRA_DC_OUT_FAKE_DSIA) ||
		(dc->out->type == TEGRA_DC_OUT_FAKE_DSIB) ||
		(dc->out->type == TEGRA_DC_OUT_FAKE_DSI_GANGED)) {
		protocol = nvdisp_dsi_control_protocol_dsia_f();
		reg = nvdisp_dsi_control_r();
	}

	tegra_dc_writel(dc, protocol, reg);
	tegra_dc_enable_general_act(dc);
	return 0;
}

static int tegra_nvdisp_head_init(struct tegra_dc *dc)
{
	u32 int_enable;
	u32 int_mask;
	u32 i, val;

	/* Init syncpt */
	tegra_dc_writel(dc, nvdisp_incr_syncpt_cntrl_no_stall_f(1),
		nvdisp_incr_syncpt_cntrl_r());

	/* Disabled this feature as unit fpga hang on enabling this*/
	if (tegra_platform_is_silicon())
		tegra_dc_writel(dc, nvdisp_cont_syncpt_vsync_en_enable_f() |
			nvdisp_cont_syncpt_vsync_indx_f(dc->vblank_syncpt),
			nvdisp_cont_syncpt_vsync_r());

	/* Init interrupts */
	/* Setting Int type. EDGE for most, LEVEL for UF related */
	tegra_dc_writel(dc, 0x3C000000, nvdisp_int_type_r());
	/* Setting all the Int polarity to high */
	tegra_dc_writel(dc, 0x3D8010F6, nvdisp_int_polarity_r());

	/* enable interrupts for vblank, frame_end and underflows */
	int_enable = nvdisp_cmd_int_status_frame_end_f(1) |
			nvdisp_cmd_int_status_v_blank_f(1) |
			nvdisp_cmd_int_status_uf_f(1) |
			nvdisp_cmd_int_status_sd3_f(1);
	/* for panels with one-shot mode enable tearing effect interrupt */
	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE)
		int_enable |= MSF_INT;
	/* Todo: also need to enable interrupts for SD3, DSC etc */

	tegra_dc_writel(dc, int_enable, nvdisp_cmd_int_enable_r());

	int_mask = nvdisp_cmd_int_status_uf_f(1);
	tegra_dc_writel(dc, int_mask, nvdisp_cmd_int_mask_r());

	tegra_dc_writel(dc, nvdisp_state_access_write_mux_assembly_f() |
		nvdisp_state_access_read_mux_active_f(),
		nvdisp_state_access_r());

	tegra_dc_writel(dc, 0x00000000, nvdisp_background_color_r());

	for_each_set_bit(i, &dc->valid_windows, DC_N_WINDOWS) {
		struct tegra_dc_win *win = tegra_dc_get_window(dc, i);

		BUG_ON(!win);

		/* refuse to operate on invalid syncpts */
		if (WARN_ON(win->syncpt.id == NVSYNCPT_INVALID))
			continue;

		if (!nvhost_syncpt_read_ext_check(dc->ndev,
						win->syncpt.id, &val))
			win->syncpt.min = win->syncpt.max = val;
	}

	dc->crc_pending = false;

	/* set mode */
	tegra_nvdisp_program_mode(dc, &dc->mode);

	/*set display control */
	tegra_nvdisp_set_control(dc);

	tegra_nvdisp_set_color_control(dc);

	tegra_dc_enable_general_act(dc);

	return 0;
}

static int tegra_nvdisp_postcomp_init(struct tegra_dc *dc)
{
	/* Set the LUT address in HW register
	 * Enabled the default sRGB_LUT, replace this
	 * with LUT from Panel Characterization through
	 * DT.
	 */
	struct tegra_dc_lut *lut = &dc->cmu;

	if (dc->cmu_enabled) {
		tegra_nvdisp_set_output_lut(dc, lut);
		tegra_nvdisp_set_color_control(dc);
		tegra_dc_enable_general_act(dc);
	}

	return 0;
}

static int tegra_nvdisp_rg_init(struct tegra_dc *dc)
{
	return 0;
}

static int tegra_nvdisp_cursor_init(struct tegra_dc *dc)
{
	return 0;
}

int tegra_nvdisp_head_disable(struct tegra_dc *dc)
{
	int idx;

	/* Detach windows from the head */
	for_each_set_bit(idx, &dc->pdata->win_mask, DC_N_WINDOWS) {
		if (tegra_nvdisp_detach_win(dc, idx))
			dev_err(&dc->ndev->dev,
				"failed to detach window %d\n", idx);
		else
			dev_dbg(&dc->ndev->dev,
				"Window %d detached from head %d\n", idx,
				dc->ctrl_num);
	}

	/* Set comp clock to different pclk since dc->clk will be disabled */
	mutex_lock(&tegra_nvdisp_lock);
	clk_client[dc->ctrl_num].inuse = false;
	mutex_unlock(&tegra_nvdisp_lock);
	tegra_nvdisp_set_compclk(dc);

	/* Disable DC clock */
	tegra_disp_clk_disable_unprepare(dc->clk);

	/* check if any of head is using hub clock */
	mutex_lock(&tegra_nvdisp_lock);
	for (idx = 0; idx < TEGRA_MAX_DC; idx++) {
		if (clk_client[idx].inuse)
			break;
	}

	/* disable hub clock if none of the heads is using it */
	if (idx == TEGRA_MAX_DC && hubclk_already_on) {
		tegra_disp_clk_disable_unprepare(hubclk);
		hubclk_already_on = false;
	}
	mutex_unlock(&tegra_nvdisp_lock);

	return 0;
}

int tegra_nvdisp_head_enable(struct tegra_dc *dc)
{
	int i;
	int res;
	int idx, pclk = 0, ret = 0;
	struct clk *parent_clk = NULL;
	struct clk *hubparent_clk = NULL;

	if (WARN_ON(!dc || !dc->out || !dc->out_ops))
		return false;

	/* Save for powermgmt purpose */
	nvdisp_pg[dc->ctrl_num].valid_windows = dc->pdata->win_mask;

	/* TODO: confirm power domains for parker */
	tegra_dc_unpowergate_locked(dc);

	/* Set HUB CLOCK PARENT as PLLP/ ENABLE */
	/* Change the HUB to HUBPLL for higher clocks */
	hubparent_clk = tegra_disp_clk_get(&dc->ndev->dev, "pllp_display");
	if (IS_ERR_OR_NULL(hubparent_clk)) {
		dev_err(&dc->ndev->dev, "hub parent clock get failed\n");
		ret = -ENOENT;
		return ret; /*TODO: Add proper cleanup later */
	}
	clk_set_parent(hubclk, hubparent_clk);

	/*
	 * WAR for bug 200212319
	 *
	 * IMP will ultimately be responsible for setting the required hubclk
	 * rate in the kernel, but it's not ready yet. As such, explicitly drive
	 * the hubclk rate to 408MHz so that we're not stuck with the rate set
	 * by BL.
	 */
	clk_set_rate(hubclk, 408000000);

	/* set clock status to inuse */
	mutex_lock(&tegra_nvdisp_lock);
	clk_client[dc->ctrl_num].inuse = true;
	clk_client[dc->ctrl_num].dc = dc;

	if (!hubclk_already_on) {
		tegra_disp_clk_prepare_enable(hubclk);
		hubclk_already_on = true;
	}
	mutex_unlock(&tegra_nvdisp_lock);

	pr_info(" rate get on hub %ld\n", clk_get_rate(hubclk));

	/* Enable OR -- need to enable the connection first */
	if (dc->out->enable)
		dc->out->enable(&dc->ndev->dev);

	/* Setting DC clocks, DC, COMPCLK
	 * Set maximum of DC clock for COMPCLK
	 */
	if (dc->out->type == TEGRA_DC_OUT_DSI) {
		parent_clk = tegra_disp_clk_get(&dc->ndev->dev,
						"pll_d_out1");
	} else	{
		parent_clk = tegra_disp_clk_get(&dc->ndev->dev,
						dc->out->parent_clk);
		pr_info("Parent Clock set for DC %s\n",
				dc->out->parent_clk);
	}

	if (IS_ERR_OR_NULL(parent_clk)) {
		dev_err(&dc->ndev->dev,
			"Failed to get DC Parent clock\n");
		ret = -ENOENT;
		return ret; /*TODO: Add proper cleanup later */
	}

	/* Set parent for DC clock */
	clk_set_parent(dc->clk, parent_clk);

	/* Set rate on DC same as pclk */
	clk_set_rate(dc->clk, dc->mode.pclk);

	if (dc->out_ops->setup_clk)
		pclk = dc->out_ops->setup_clk(dc, dc->clk);

	/* Enable DC clock */
	tegra_disp_clk_prepare_enable(dc->clk);

	pr_info(" dc clk %ld\n", clk_get_rate(dc->clk));

	tegra_dc_get(dc);

	/* Deassert the dc reset */
	if (tegra_bpmp_running()) {
		res = reset_control_deassert(dc->rst);
		if (res) {
			dev_err(&dc->ndev->dev, "Unable to deassert dc %d\n",
					dc->ctrl_num);
			return res;
		}

		tegra_nvdisp_wgrp_reset_deassert(dc);
	}

	/* Mask interrupts during init */
	tegra_dc_writel(dc, 0, DC_CMD_INT_MASK);

	enable_irq(dc->irq);

	res = tegra_nvdisp_head_init(dc);
	res |= tegra_nvdisp_postcomp_init(dc);
	res |= tegra_nvdisp_rg_init(dc);
	res |= tegra_nvdisp_cursor_init(dc);

	if (res) {
		dev_err(&dc->ndev->dev, "%s, failed head enable\n", __func__);
		goto failed_enable;
	}

	if (dc->out_ops && dc->out_ops->enable)
		dc->out_ops->enable(dc);

	/* force a full blending update */
	for (i = 0; i < DC_N_WINDOWS; i++)
		dc->blend.z[i] = -1;

	tegra_dc_ext_enable(dc->ext);
	trace_display_enable(dc);

	tegra_dc_dsc_init(dc);

	if (dc->out->postpoweron)
		dc->out->postpoweron(&dc->ndev->dev);

	if (dc->out_ops && dc->out_ops->postpoweron)
		dc->out_ops->postpoweron(dc);

	tegra_nvdisp_set_compclk(dc);

	tegra_log_resume_time();
	/*
	 * We will need to reinitialize the display the next time panel
	 * is enabled.
	 */
	dc->out->flags &= ~TEGRA_DC_OUT_INITIALIZED_MODE;

	i = -1;
	/* Assign windows to this head */
	for_each_set_bit(idx, &dc->pdata->win_mask, DC_N_WINDOWS) {
		if (tegra_nvdisp_assign_win(dc, idx))
			dev_err(&dc->ndev->dev,
				"failed to assign window %d\n", idx);
		else {
			dev_info(&dc->ndev->dev,
				"Window %d assigned to head %d\n", idx,
				dc->ctrl_num);
			if (i == -1)
				i = idx;
		}
	}

	/* Set the fb_index on changing from a zero win_mask to
	 * to a valid one.
	 */
	if ((dc->pdata->fb->win == -1) && dc->pdata->win_mask) {
		tegra_fb_set_win_index(dc, dc->pdata->win_mask);
		dc->pdata->fb->win = i;
	}

	/* Enable RG underflow logging */
	tegra_dc_writel(dc, nvdisp_rg_underflow_enable_enable_f() |
		nvdisp_rg_underflow_mode_red_f(),
		nvdisp_rg_underflow_r());

	tegra_dc_put(dc);
	return 0;

failed_enable:
	tegra_dc_writel(dc, 0, DC_CMD_INT_MASK);
	disable_irq_nosync(dc->irq);
	tegra_dc_clear_bandwidth(dc);
	if (dc->out && dc->out->disable)
		dc->out->disable(&dc->ndev->dev);
	tegra_dc_put(dc);

	/* TODO: disable DC clock */
	return -EINVAL;
}

void tegra_nvdisp_set_vrr_mode(struct tegra_dc *dc)
{
	tegra_dc_writel(dc,
		nvdisp_display_rate_min_refresh_enable_f(1) |
		nvdisp_display_rate_min_refresh_interval_f(
		(u32)(div_s64(dc->frametime_ns, NSEC_PER_MICROSEC))),
		nvdisp_display_rate_r());
	tegra_dc_writel(dc, nvdisp_display_command_control_mode_nc_display_f(),
		nvdisp_display_command_r());
	tegra_dc_writel(dc,  nvdisp_cmd_state_ctrl_host_trig_enable_f()
		, nvdisp_cmd_state_ctrl_r());
}
EXPORT_SYMBOL(tegra_nvdisp_set_vrr_mode);

void tegra_nvdisp_vrr_work(struct work_struct *work)
{
	int reg_val;
	int frame_time_elapsed;
	struct timespec time_now;
	s64 time_now_us;

	struct tegra_dc *dc = container_of(
		to_delayed_work(work), struct tegra_dc, vrr_work);
	struct tegra_vrr *vrr = dc->out->vrr;

	mutex_lock(&dc->lock);
	tegra_dc_get(dc);

	getnstimeofday(&time_now);
	time_now_us = (s64)time_now.tv_sec * 1000000 +
		time_now.tv_nsec / 1000;

	frame_time_elapsed = time_now_us - vrr->curr_frame_us;

	if (frame_time_elapsed < (vrr->frame_len_max - MIN_FRAME_INTERVAL)) {
		reg_val = tegra_dc_readl(dc, nvdisp_cmd_state_ctrl_r());
		reg_val |= nvdisp_cmd_state_ctrl_host_trig_enable_f();
		reg_val |= nvdisp_cmd_state_ctrl_general_act_req_enable_f();
		tegra_dc_writel(dc, reg_val, nvdisp_cmd_state_ctrl_r());
		tegra_dc_readl(dc, nvdisp_cmd_state_ctrl_r()); /* flush */
	}

	tegra_dc_put(dc);
	mutex_unlock(&dc->lock);
	return;
}
EXPORT_SYMBOL(tegra_nvdisp_vrr_work);

void tegra_nvdisp_stop_display(struct tegra_dc *dc)
{
	tegra_dc_writel(dc, nvdisp_display_command_control_mode_stop_f(),
		nvdisp_display_command_r());
}
EXPORT_SYMBOL(tegra_nvdisp_stop_display);

u32 tegra_nvdisp_ihub_read(struct tegra_dc *dc, int win_number,
				int ihub_switch)
{
	u32 reg = 0;
	u32 ret_val = 0;

	mutex_lock(&dc->lock);
	tegra_dc_get(dc);

	switch (ihub_switch) {
	case 0: /* mempool size */
		reg = tegra_dc_readl(dc, nvdisp_ihub_capa_r());
		/* base entry width is 32 bytes */
		ret_val = nvdisp_ihub_capa_mempool_entries_v(reg) *
			(32 << nvdisp_ihub_capa_mempool_width_v(reg));
		break;
	default:
		break;
	}

	tegra_dc_put(dc);
	mutex_unlock(&dc->lock);

	return ret_val;
}

struct tegra_fb_info *tegra_nvdisp_fb_register(struct platform_device *ndev,
	struct tegra_dc *dc, struct tegra_fb_data *fb_data,
	struct resource *fb_mem)
{
	void *virt_addr = NULL;

	/* Check fb_data->win is valid before checking for valid window */
	if (fb_data->win > -1) {
		/* Assign the given window to current dc */
		if (!tegra_dc_get_window(dc, fb_data->win)) {
			dev_err(&ndev->dev, "%s, failed to get window %d for head %d\n",
				__func__, fb_data->win, dc->ctrl_num);
			return ERR_PTR(-ENOENT);
		}

	}

	/* Allocate FBMem if not already allocated */
	if (!fb_mem->start || !fb_mem->end) {
		/* lines must be 64B aligned */
		int stride = round_up(fb_data->xres *
					fb_data->bits_per_pixel / 8, 64);
		/* Add space to permit adjustment of start of buffer.
		 * start of buffer requires 256B alignment. */
		int fb_size = stride * fb_data->yres + 256;

		if (!fb_size)
			return ERR_PTR(-ENOENT);

		virt_addr = dma_alloc_writecombine(&ndev->dev, fb_size,
			&fb_mem->start, GFP_KERNEL);
		if (!virt_addr) {
			dev_err(&ndev->dev, "Failed to allocate FBMem\n");
			return ERR_PTR(-ENOENT);
		}
		fb_mem->end = fb_mem->start + fb_size - 1;
		dev_info(&ndev->dev, "Allocated %d as FBmem\n", fb_size);
	}

	return tegra_fb_register(ndev, dc, fb_data, fb_mem, virt_addr);
}

void tegra_nvdisp_enable_crc(struct tegra_dc *dc)
{
	mutex_lock(&dc->lock);
	tegra_dc_get(dc);

	tegra_dc_writel(dc, nvdisp_crc_control_enable_enable_f() |
		nvdisp_crc_control_input_data_active_data_f(),
		nvdisp_crc_control_r());

	tegra_dc_enable_general_act(dc);
	tegra_dc_put(dc);
	mutex_unlock(&dc->lock);

	/* Register a client of frame_end interrupt */
	tegra_dc_config_frame_end_intr(dc, true);
}

void tegra_nvdisp_disable_crc(struct tegra_dc *dc)
{
	/* Unregister a client of frame_end interrupt */
	tegra_dc_config_frame_end_intr(dc, false);

	mutex_lock(&dc->lock);
	tegra_dc_get(dc);
	tegra_dc_writel(dc, 0x0, nvdisp_crc_control_r());
	tegra_dc_enable_general_act(dc);

	tegra_dc_put(dc);
	mutex_unlock(&dc->lock);
}

u32 tegra_nvdisp_read_rg_crc(struct tegra_dc *dc)
{
	int crc = 0;
	int val = 0;

	if (!dc) {
		pr_err("Failed to get dc: NULL parameter.\n");
		goto crc_error;
	}

	/* If gated quitely return */
	if (tegra_bpmp_running() && !tegra_dc_is_powered(dc))
		return 0;

#ifdef INIT_COMPLETION
	INIT_COMPLETION(dc->crc_complete);
#else
	reinit_completion(&dc->crc_complete);
#endif
	if (dc->crc_pending &&
	    wait_for_completion_interruptible(&dc->crc_complete)) {
		pr_err("CRC read interrupted.\n");
		goto crc_error;
	}

	mutex_lock(&dc->lock);
	tegra_dc_get(dc);
	val = tegra_dc_readl(dc, nvdisp_rg_crca_r());

	/* tegrasim seems to need more time to set the
	 * CRCA valid bit. So adding an infinite
	 * polling loop for tegrasim
	 */
	if (tegra_platform_is_linsim()) {
		while (val <= 0) {
			val = tegra_dc_readl(dc, nvdisp_rg_crca_r());
			msleep(100);
		}
	}

	if (val & nvdisp_rg_crca_valid_true_f())
		crc = tegra_dc_readl(dc, nvdisp_rg_crcb_r());
	/* clear the error bit if set */
	if (val & nvdisp_rg_crca_error_true_f())
		tegra_dc_writel(dc, nvdisp_rg_crca_error_true_f(),
			nvdisp_rg_crca_r());
	tegra_dc_put(dc);
	mutex_unlock(&dc->lock);
crc_error:
	return crc;
}


void tegra_nvdisp_underflow_handler(struct tegra_dc *dc)
{
	u32 reg = tegra_dc_readl(dc, nvdisp_rg_underflow_r());
	dc->stats.underflows++;

	if (dc->underflow_mask & NVDISP_UF_INT)
		dc->stats.underflow_frames +=
				nvdisp_rg_underflow_frames_uflowed_v(reg);

	/* Clear the sticky bit and counter */
	tegra_dc_writel(dc,
		nvdisp_rg_underflow_frames_uflowed_rst_trigger_f() |
		nvdisp_rg_underflow_uflowed_clr_f(),
		nvdisp_rg_underflow_r());

	/* Check whether the reset is done */
	reg = tegra_dc_readl(dc, nvdisp_rg_underflow_r());

	if (reg & nvdisp_rg_underflow_frames_uflowed_rst_pending_f()) {
		pr_err("nvdisp_rg_underflow_frames_uflowed_rst is pending\n");
	} else {
		/* Enable RG underflow logging */
		tegra_dc_writel(dc, nvdisp_rg_underflow_enable_enable_f() |
			nvdisp_rg_underflow_mode_red_f(),
			nvdisp_rg_underflow_r());
	}
}

int tegra_nvdisp_is_powered(int pg_id)
{
	int i, ret;
	int pd_index = -1;

	for (i = 0; i < NVDISP_PD_COUNT; i++) {
		if (nvdisp_pg[i].powergate_id == pg_id) {
			pd_index = i;
			break;
		}
	}

	if (pd_index < 0) {
		pr_info("Not a disp powerdomain\n");
		return 0;
	}

	mutex_lock(&tegra_nvdisp_lock);
	ret = nvdisp_pg[pd_index].ref_cnt;
	mutex_unlock(&tegra_nvdisp_lock);

	return ret;
}

int tegra_nvdisp_powergate_partition(int pg_id)
{
	int i , ret = 0;
	int pd_index = -1;
	bool disable_disp[NVDISP_PD_COUNT] = {
			false, false, false };

	/* DISP - common for all IHUB, ORs, head0, win 0
	 * Request from DISPB or DISPC - disable DISP last
	 * Check DISPB and DISPC valid_windows
	 * in addition to the request for HEAD 1 and 2 powergate
	 */

	for (i = 0; i < NVDISP_PD_COUNT; i++) {
		if (nvdisp_pg[i].powergate_id == pg_id) {
			pd_index = i;
			break;
		}
	}

	if (pd_index < 0) {
		pr_info("Not a disp powerdomain\n");
		return 0;
	}

	if (!nvdisp_pg[pd_index].ref_cnt) {
		pr_info("Already powergated DISP id %d\n",
				 nvdisp_pg[i].powergate_id);
		return 0;
	}

	mutex_lock(&tegra_nvdisp_lock);
	/* Check any valid_windows resides in another PD
	 * the check whether those head are in use before
	 * powering off those PDs
	 */
	for_each_set_bit(i, &nvdisp_pg[pd_index].valid_windows, DC_N_WINDOWS) {
		if (i == 0 && nvdisp_pg[NVDISP_PD_INDEX].windows_inuse) {
			nvdisp_pg[NVDISP_PD_INDEX].windows_inuse -= 1;
			disable_disp[NVDISP_PD_INDEX] = true;
		} else if (((i == 1) || (i == 2)) &&
			nvdisp_pg[NVDISPB_PD_INDEX].windows_inuse) {
			nvdisp_pg[NVDISPB_PD_INDEX].windows_inuse -= 1;
			disable_disp[NVDISPB_PD_INDEX] = true;
		} else if (nvdisp_pg[NVDISPC_PD_INDEX].windows_inuse) {
			nvdisp_pg[NVDISPC_PD_INDEX].windows_inuse -= 1;
			disable_disp[NVDISPC_PD_INDEX] = true;
		}
	}

	/* Check head is in use */
	if (nvdisp_pg[pd_index].head_inuse)
		disable_disp[pd_index] = true;

	nvdisp_pg[pd_index].head_inuse = false;

	disable_disp[pd_index] = true;
	/* Request from DISPB or DISPC - disable DISP also */
	if ((pd_index == 1) || (pd_index == 2))
		disable_disp[NVDISP_PD_INDEX] = true;

	for (i = NVDISP_PD_COUNT - 1; i >= 0; i--) {
		if (disable_disp[i] && (--nvdisp_pg[i].ref_cnt == 0)) {

			if (nvdisp_pg[i].windows_inuse &&
				nvdisp_pg[i].head_inuse)
				pr_err("Error in Windows/Head ref_count\n");

			pr_info("PD DISP%d index%d DOWN\n",
					 i, nvdisp_pg[i].powergate_id);
			/* User when using pg with clk_on */
			ret = tegra_powergate_partition_with_clk_off(
						nvdisp_pg[i].powergate_id);
			/*ret = tegra_powergate_partition(
						nvdisp_pg[i].powergate_id);*/
			if (ret)
				pr_err("Fail to powergate DISP%d\n", i);
		}
	}
	mutex_unlock(&tegra_nvdisp_lock);

	return ret;
}

int tegra_nvdisp_unpowergate_partition(int pg_id)
{
	int i, ret = 0;
	int pd_index = -1;
	bool enable_disp[NVDISP_PD_COUNT] = {
			false , false, false};

	/* DISP - common for all IHUB, ORs, head0, win 0
	 * Request from DISPB or DISPC - enable DISP first
	 * Enable DISPB and DISPC based on valid_windows
	 * checking
	 */

	for (i = 0; i < NVDISP_PD_COUNT; i++) {
		if (nvdisp_pg[i].powergate_id == pg_id) {
			pd_index = i;
			break;
		}
	}

	if (pd_index < 0) {
		pr_info(" Not a disp powerdomain\n");
		return 0;
	}
	mutex_lock(&tegra_nvdisp_lock);

	nvdisp_pg[pd_index].head_inuse = true;
	enable_disp[pd_index] = true;
	/* Request from DISPB or DISPC - enable DISP first */
	if ((pd_index == 1) || (pd_index == 2))
		enable_disp[NVDISP_PD_INDEX] = true;

	/* Check the for valid_windows per head
	 * win0 is in DISP, win1&2 in DISPB and
	 * win3,win4 &win5 in DISPC domains
	 */

	for_each_set_bit(i, &nvdisp_pg[pd_index].valid_windows, DC_N_WINDOWS) {
		if (i == 0) {
			enable_disp[NVDISP_PD_INDEX] = true;
			nvdisp_pg[NVDISP_PD_INDEX].windows_inuse += 1;
		} else if ((i == 1) || (i == 2)) {
			enable_disp[NVDISPB_PD_INDEX] = true;
			nvdisp_pg[NVDISPB_PD_INDEX].windows_inuse += 1;
		} else { /* win 3/4/5 */
			enable_disp[NVDISPC_PD_INDEX] = true;
			nvdisp_pg[NVDISPC_PD_INDEX].windows_inuse += 1;
		}
	}

	for (i = 0; i < NVDISP_PD_COUNT; i++) {
		if (enable_disp[i] && (nvdisp_pg[i].ref_cnt++ == 0)) {
			pr_info("PD DISP%d index%d UP\n",
					i, nvdisp_pg[i].powergate_id);
			/* use clk_off with clk_on  */
			ret = tegra_unpowergate_partition_with_clk_on(
						nvdisp_pg[i].powergate_id);
			/*ret = tegra_unpowergate_partition(
						nvdisp_pg[i].powergate_id);*/
			if (ret) {
				pr_err("Fail to Unpowergate DISP%d\n", i);
				mutex_unlock(&tegra_nvdisp_lock);
				return ret;
			}
		}
	}

	mutex_unlock(&tegra_nvdisp_lock);
	return ret;
}
static int tegra_nvdisp_set_color_control(struct tegra_dc *dc)
{
	u32 color_control;

	switch (dc->out->depth) {

	case 36:
		color_control = nvdisp_color_ctl_base_color_size_36bits_f();
		break;
	case 30:
		color_control = nvdisp_color_ctl_base_color_size_30bits_f();
		break;
	case 24:
		color_control = nvdisp_color_ctl_base_color_size_24bits_f();
		break;
	case 18:
		color_control = nvdisp_color_ctl_base_color_size_18bits_f();
		break;
	default:
		color_control = nvdisp_color_ctl_base_color_size_24bits_f();
		break;
	}

	switch (dc->out->dither) {
	case TEGRA_DC_UNDEFINED_DITHER:
	case TEGRA_DC_DISABLE_DITHER:
		color_control |= nvdisp_color_ctl_dither_ctl_disable_f();
		break;
	case TEGRA_DC_ORDERED_DITHER:
		color_control |= nvdisp_color_ctl_dither_ctl_ordered_f();
		break;
	case TEGRA_DC_TEMPORAL_DITHER:
		color_control |= nvdisp_color_ctl_dither_ctl_temporal_f();
		break;
	case TEGRA_DC_ERRACC_DITHER:
		color_control |= nvdisp_color_ctl_dither_ctl_err_acc_f();
		break;
	default:
		dev_err(&dc->ndev->dev, "Error: Unsupported dithering mode\n");
	}

#if defined(CONFIG_TEGRA_DC_CMU_V2)
	if (dc->cmu_enabled)
		color_control |= nvdisp_color_ctl_cmu_enable_f();
#endif
	/* TO DO - dither rotation, dither offset, dither phase */

	tegra_dc_writel(dc, color_control,
			nvdisp_color_ctl_r());
	return 0;
}

#if defined(CONFIG_TEGRA_DC_CMU_V2)
void tegra_dc_cache_cmu(struct tegra_dc *dc, struct tegra_dc_cmu *src_cmu)
{
	/* copy the data to DC lut */
	memcpy(dc->cmu.rgb, src_cmu->rgb, dc->cmu.size);
	dc->cmu_dirty = true;
}

static void _tegra_nvdisp_update_cmu(struct tegra_dc *dc,
					struct tegra_dc_lut *cmu)
{
	dc->cmu_enabled = dc->pdata->cmu_enable;
	if (!dc->cmu_enabled)
		return;

	/* Not disabling the cmu here - will
	 * consider it if there is any corruption on
	 * updating cmu while it is running
	 */
	tegra_nvdisp_set_output_lut(dc, cmu);
	dc->cmu_dirty = false;
}

int tegra_nvdisp_update_cmu(struct tegra_dc *dc, struct tegra_dc_lut *cmu)
{
	int i;
	u32 reg_val, mask;
	u32 act_req_mask = nvdisp_cmd_state_ctrl_general_act_req_enable_f();

	mutex_lock(&dc->lock);
	if (!dc->enabled) {
		mutex_unlock(&dc->lock);
		return 0;
	}

	tegra_dc_get(dc);

	_tegra_nvdisp_update_cmu(dc, cmu);
	tegra_nvdisp_set_color_control(dc);

	for_each_set_bit(i, &dc->valid_windows, DC_N_WINDOWS) {
		struct tegra_dc_win *win = tegra_dc_get_window(dc, i);

		BUG_ON(!win);

		if (!WIN_IS_ENABLED(win))
			continue;
		mask = win_win_set_params_degamma_range_mask_f();
		reg_val = nvdisp_win_read(win, win_win_set_params_r());
		reg_val &= ~mask;

		if (dc->cmu_enabled) {
			/* enable degamma */
			reg_val |= tegra_nvdisp_get_degamma_config(dc, win);
			nvdisp_win_write(win, reg_val, win_win_set_params_r());

			/* enable csc if csc_enable is true */
			if (win->csc.csc_enable)
				tegra_nvdisp_set_csc(win, &win->csc);
		} else {
			/* disable degamma */
			nvdisp_win_write(win, reg_val, win_win_set_params_r());

			/* disable csc */
			if (win->csc.csc_enable) {
				reg_val =
					win_window_set_control_csc_disable_f();
				nvdisp_win_write(win, reg_val,
					win_window_set_control_r());
			}
		}

		act_req_mask |=
			nvdisp_cmd_state_ctrl_a_act_req_enable_f()
			<< win->idx;
	}

	tegra_dc_writel(dc, act_req_mask, nvdisp_cmd_state_ctrl_r());
	tegra_dc_readl(dc, nvdisp_cmd_state_ctrl_r());

	if (dc->out->flags & TEGRA_DC_OUT_CONTINUOUS_MODE) {
		/* wait for ACT_REQ to complete or time out */
		if (tegra_dc_poll_register(dc, nvdisp_cmd_state_ctrl_r(),
					   act_req_mask, 0, 1,
					   NVDISP_TEGRA_POLL_TIMEOUT_MS))
			dev_err(&dc->ndev->dev,
				"dc timeout waiting to clear ACT_REQ, mask:0x%x\n",
				act_req_mask);
	}

	tegra_dc_put(dc);
	mutex_unlock(&dc->lock);

	return 0;
}
EXPORT_SYMBOL(tegra_nvdisp_update_cmu);
#endif

#define NO_THREAD_GROUP ((u32)-1)
void tegra_nvdisp_get_imp_user_info(struct tegra_dc *dc,
				struct tegra_dc_ext_imp_user_info *info)
{
	int i, j;
	u32 ihub_capa;

	info->current_emcclk = tegra_bwmgr_get_emc_rate();

	ihub_capa = tegra_dc_readl(dc, nvdisp_ihub_capa_r());
	/* base entry width is 32 bytes */
	info->mempool_size = nvdisp_ihub_capa_mempool_entries_v(ihub_capa) *
			(32 << nvdisp_ihub_capa_mempool_width_v(ihub_capa));

	mutex_lock(&tegra_nvdisp_lock);

	for (i = 0; i < DC_N_WINDOWS; i++) {
		int win_capc;
		int win_cape;
		int min_width;
		struct tegra_dc *owner_dc;
		struct tegra_dc_win *win = NULL;

		for (j = 0; j < TEGRA_MAX_DC; j++) {
			owner_dc = tegra_dc_get_dc(j);
			if (!owner_dc)
				continue;

			if (test_bit(i, &owner_dc->valid_windows))
				win = tegra_dc_get_window(owner_dc, i);
		}

		if (!win || !WIN_IS_ENABLED(win))
			continue;

		/*
		 * in_w[i] has 20 bits integer (MSB) and 12 bits
		 * fractional (LSB)
		 */
		win_capc = nvdisp_win_read(win, win_precomp_wgrp_capc_r());
		win_cape = nvdisp_win_read(win, win_precomp_wgrp_cape_r());
		min_width = ((info->in_w[i] >> 12) < info->out_w[i]) ?
				info->in_w[i] >> 12 : info->out_w[i];

		if (min_width <
			win_precomp_wgrp_capc_max_pixels_5tap444_v(win_capc))
			info->v_taps[i] = 5;
		else /* IMP only accepts 2 or 5 for v taps */
			info->v_taps[i] = 2;
	}

	mutex_unlock(&tegra_nvdisp_lock);
}
EXPORT_SYMBOL(tegra_nvdisp_get_imp_user_info);

static void tegra_nvdisp_parse_imp_head_results(
			struct tegra_dc_imp_head_results *res,
			struct tegra_dc_ext_imp_head_results *ext_res)
{
	size_t num_wins;

	res->num_windows = ext_res->num_windows;
	res->cursor_active = ext_res->cursor_active;
	res->total_latency = ext_res->total_latency;
	res->hubclk = ext_res->hubclk;
	res->required_total_bw_kbps = ext_res->required_total_bw_kbps;

	if (res->cursor_active) {
		res->metering_slots_value_cursor =
			ext_res->metering_slots_value_cursor;
		res->pipe_meter_value_cursor =
			ext_res->pipe_meter_value_cursor;
		res->pool_config_entries_cursor =
			ext_res->pool_config_entries_cursor;
		res->cursor_slots_value = ext_res->cursor_slots_value;
	}

	num_wins = res->num_windows;
	if (!num_wins)
		return;

	res->window_slots_value = ext_res->window_slots_value;
	memcpy(res->win_ids, ext_res->win_ids,
		sizeof(*res->win_ids) * num_wins);
	memcpy(res->thread_group_win, ext_res->thread_group_win,
		sizeof(*res->thread_group_win) * num_wins);
	memcpy(res->metering_slots_value_win, ext_res->metering_slots_value_win,
		sizeof(*res->metering_slots_value_win) * num_wins);
	memcpy(res->thresh_lwm_dvfs_win, ext_res->thresh_lwm_dvfs_win,
		sizeof(*res->thresh_lwm_dvfs_win) * num_wins);
	memcpy(res->pipe_meter_value_win, ext_res->pipe_meter_value_win,
		sizeof(*res->pipe_meter_value_win) * num_wins);
	memcpy(res->pool_config_entries_win, ext_res->pool_config_entries_win,
		sizeof(*res->pool_config_entries_win) * num_wins);
}

static int tegra_nvdisp_parse_imp_results(struct tegra_dc *dc,
			struct tegra_dc_ext_flip_user_data *flip_user_data)
{
	void __user *ext_res_ptr =
		(void __user *)flip_user_data->imp_ptr.results;
	struct tegra_dc_ext_imp_head_results ext_res[TEGRA_MAX_DC];
	int i;

	if (copy_from_user(ext_res, ext_res_ptr, sizeof(ext_res))) {
		dev_err(&dc->ndev->dev,
			"Failed to copy IMP results from user\n");
		return -EFAULT;
	}

	for (i = 0; i < TEGRA_MAX_DC; i++)
		tegra_nvdisp_parse_imp_head_results(&dc->imp_results[i],
							&ext_res[i]);

	return 0;
}

static void tegra_nvdisp_activate_common_channel(struct tegra_dc *dc)
{
	if (!dc || !dc->enabled)
		return;

	tegra_dc_writel(dc,
		nvdisp_cmd_state_ctrl_common_act_update_enable_f(),
		nvdisp_cmd_state_ctrl_r());
	tegra_dc_readl(dc, nvdisp_cmd_state_ctrl_r()); /* flush */
	tegra_dc_writel(dc,
		nvdisp_cmd_state_ctrl_common_act_req_enable_f(),
		nvdisp_cmd_state_ctrl_r());
	tegra_dc_readl(dc, nvdisp_cmd_state_ctrl_r()); /* flush */

	dc->common_channel_pending = true;
}

static int tegra_nvdisp_wait_for_common_channel_to_promote(struct tegra_dc *dc)
{
	int ret = 0;

	mutex_lock(&tegra_nvdisp_lock);

	ret = ___wait_event(nvdisp_common_channel_promotion_wq,
		___wait_cond_timeout(!dc->common_channel_pending),
		TASK_INTERRUPTIBLE, 0, HZ,
		mutex_unlock(&tegra_nvdisp_lock);
		__ret = schedule_timeout(__ret);
		mutex_lock(&tegra_nvdisp_lock));

	mutex_unlock(&tegra_nvdisp_lock);
	return ret;
}

static void tegra_nvdisp_notify_common_channel_promoted(struct tegra_dc *dc)
{
	dc->common_channel_pending = false;
	wake_up(&nvdisp_common_channel_promotion_wq);
}

static void tegra_nvdisp_program_dc_mempool(struct tegra_dc *dc,
	struct tegra_dc_imp_head_results *res)
{
	u32 entries;
	int i;

	if (!dc || !dc->enabled)
		return;

	/* program cursor mempool */
	entries = res->pool_config_entries_cursor;
	tegra_dc_writel(dc,
		nvdisp_ihub_cursor_pool_config_entries_f(entries),
		nvdisp_ihub_cursor_pool_config_r());

	for (i = 0; i < res->num_windows; i++) {
		struct tegra_dc_win *win;

		win = tegra_dc_get_window(dc, res->win_ids[i]);
		if (!win)
			continue;

		/* program wgrp mempool */
		entries = res->pool_config_entries_win[i];
		nvdisp_win_write(win,
			win_ihub_pool_config_entries_f(entries),
			win_ihub_pool_config_r());
	}

	tegra_nvdisp_activate_common_channel(dc);
	tegra_nvdisp_wait_for_common_channel_to_promote(dc);
}

static void tegra_nvdisp_program_all_needed_mempool(struct tegra_dc *dc)
{
	int i;

	for (i = 0; i < TEGRA_MAX_DC; i++) {
		struct tegra_dc *other_dc = tegra_dc_get_dc(i);
		if (other_dc && other_dc->new_mempool_needed)
			tegra_nvdisp_program_dc_mempool(other_dc,
				&dc->imp_results[other_dc->ctrl_num]);
	}
}

static void tegra_nvdisp_handle_mempool_allocation(struct tegra_dc *dc)
{
	/*
	 * Per the register manual, SW must follow these rules in order to
	 * dynamically adjust mempool in a safe manner:
	 *
	 * "1. at no time can the sum of the entries allocated to the wgrps
	 *     exceed the number of available entries in the latency buffer
	 *     (IHUB_COMMON_CAPA_MEMPOOL_ENTRIES).
	 *  2. when changing allocations, all allocation decreases must be
	 *     done first, then once the decreases are all effective, the
	 *     increases can be done lest an intermediate state occur which
	 *     violates the first rule above."
	 *
	 * For the following scenario, assume that the state of a head refers to
	 * its head-specific state and the state of any windows that it owns.
	 * Suppose we have three active heads - H0, H1, H2 - and the state of H0
	 * is changing in a manner that affects the mempool allocation for some
	 * subset of the windows on all three heads. Only one PROPOSE can be
	 * active at any given time, which means that the state of H1 and H2
	 * cannot be updated during this period.
	 *
	 * Since the state of H1 and H2 isn't being updated, the mempool for
	 * their windows will either all decrease or all increase:
	 * - In the first case, we will promote the mempool for these windows
	 *   during PROPOSE, before the new window and common channel state for
	 *   H0 is updated in the subsequent flip.
	 * - In the second case, we will phase in the mempool for these windows
	 *   during the flip worker's cleanup, after the new window and common
	 *   channel state for H0 has already been activated.
	 *
	 * Note that it doesn't matter if we're reducing the mempool for some of
	 * H0's windows while increasing the mempool for its other windows since
	 * these updates are all contained to H0 and will be promoted together
	 * on its next vblank.
	 */
	bool mempool_all_decreasing = true;
	int i, j;

	for (i = 0; i < TEGRA_MAX_DC; i++) {
		struct tegra_dc_imp_head_results *new_result;
		struct tegra_dc *other_dc;
		u32 old_val, new_val;

		other_dc = tegra_dc_get_dc(i);
		if (!other_dc || !other_dc->enabled || other_dc == dc)
			continue;

		new_result = &dc->imp_results[other_dc->ctrl_num];
		old_val = nvdisp_ihub_cursor_pool_config_entries_f(
				tegra_dc_readl(other_dc,
				nvdisp_ihub_cursor_pool_config_r()));
		new_val = new_result->pool_config_entries_cursor;

		if (new_val != old_val) {
			other_dc->new_mempool_needed = true;
			mempool_all_decreasing = (new_val < old_val);
			continue;
		}

		for (j = 0; j < new_result->num_windows; j++) {
			struct tegra_dc_win *win;
			int win_id;

			win_id = new_result->win_ids[j];
			win = tegra_dc_get_window(other_dc, win_id);
			if (!win)
				continue;

			old_val = win_ihub_pool_config_entries_f(
					nvdisp_win_read(win,
					win_ihub_pool_config_r()));
			new_val = new_result->pool_config_entries_win[j];
			if (new_val != old_val) {
				other_dc->new_mempool_needed = true;
				mempool_all_decreasing = (new_val < old_val);
				break;
			}
		}
	}

	if (mempool_all_decreasing)
		tegra_nvdisp_program_all_needed_mempool(dc);
}

static void tegra_nvdisp_generate_tg_dep_graph(struct tegra_dc *dc,
					u32 tg_assignments[DC_N_WINDOWS],
					int tg_owners[DC_N_WINDOWS],
					int in_edges[DC_N_WINDOWS],
					int out_edges[DC_N_WINDOWS])
{
	/*
	 * The graph that we use to represent thread group dependencies is very
	 * simple and is defined as follows:
	 *
	 * - Each window is a vertex. Let W[I] represent window I.
	 * - A directed edge (W[I], W[J]) means that W[J] currently owns
	 *   the thread group that we wish to assign to W[I]. Since a thread
	 *   group can only be owned by one active window at any point in time,
	 *   each window vertex can have at most one incoming edge and one
	 *   outgoing edge.
	 */
	int i;

	for (i = 0; i < DC_N_WINDOWS; i++) {
		u32 tg = tg_assignments[i];
		int owner = -1;

		if (tg < DC_N_WINDOWS) {
			owner = tg_owners[tg];
			if (owner >= DC_N_WINDOWS)
				continue;
		}

		/*
		 * If a thread group will not be assigned to this window, or if
		 * the thread group we wish to assign is not owned by any other
		 * window, this window has no dependency. Else, mark the
		 * dependency accordingly.
		 */
		if (tg == NO_THREAD_GROUP || owner < 0 || owner == i) {
			out_edges[i] = -1;
		} else {
			out_edges[i] = owner;
			in_edges[owner] = i;
		}
	}
}

static int tegra_nvdisp_generate_tg_ordering(struct tegra_dc *dc)
{
	/*
	 * This algorithm generates a correct - although not necessarily optimal
	 * - ordering of thread group updates by following these steps:
	 *
	 * 1) Generate a thread group dependency graph. This graph is
	 *    represented as a collection of in-edges and out-edges.
	 * 2) While there are still dependencies left to resolve in the graph,
	 *    locate all vertices which have no dependencies. For each one of
	 *    these vertices V:
	 *    2a) Continue following the trail of incoming edges backwards as
	 *        long as each successive edge originates from a window vertex
	 *        belonging to the same head as V. Let V' represent each such
	 *        window vertex.
	 *    2b) Group each one of the V' found in the previous step, along
	 *        with V, into one batch request. We can group these window
	 *        vertices together since their thread group updates are all
	 *        contained to the same head.
	 *    2c) Mark V and each V' so that we don't recheck them in a future
	 *        iteration. Also update the number of dependencies left.
	 *    2d) Update the out-edges of the graph.
	 *    2e) At the end of each iteration, check which heads have thread
	 *        group updates that can be performed at this stage. Append them
	 *        to a running list.
	 *
	 * Step 2 can fail if either:
	 * A) The algorithm detects a circular dependency. This means that
	 *    there's no way to seamlessly update the required thread groups
	 *    without explicitly disabling at least one window.
	 * B) The thread group updates for the head whose window state is
	 *    changing cannot all fit in one request.
	 */
	int tg_owners[DC_N_WINDOWS], owner_dc_idx[DC_N_WINDOWS];
	int in_edges[DC_N_WINDOWS];
	int cur_out_edges[DC_N_WINDOWS], new_out_edges[DC_N_WINDOWS];
	int dep_left = 0, dep_mask = 0;
	int win_id, req_idx = 0;
	int i, j;
	struct tegra_dc *other_dc;
	bool dc_req_filled = false;
	u32 cur_tg_assignments[DC_N_WINDOWS];
	u32 new_tg_assignments[DC_N_WINDOWS];
	u32 cur_tg, new_tg;

	/* initialize arrays with dummy values */
	memset(tg_owners, -1, sizeof(tg_owners));
	memset(owner_dc_idx, -1, sizeof(owner_dc_idx));
	memset(in_edges, -1, sizeof(in_edges));
	memset(new_out_edges, -1, sizeof(new_out_edges));
	memset(cur_tg_assignments, NO_THREAD_GROUP, sizeof(cur_tg_assignments));
	memset(new_tg_assignments, NO_THREAD_GROUP, sizeof(new_tg_assignments));

	/* fill in the lookup arrays */
	for (i = 0; i < TEGRA_MAX_DC; i++) {
		struct tegra_dc_imp_head_results *res;

		other_dc = tegra_dc_get_dc(i);
		if (!other_dc)
			continue;

		res = &dc->imp_results[i];
		for (j = 0; j < res->num_windows; j++) {
			struct tegra_dc_win *win;

			win_id = res->win_ids[j];
			win = tegra_dc_get_window(other_dc, win_id);
			if (!win)
				continue;

			cur_tg = nvdisp_win_read(win,
						win_ihub_thread_group_r());
			if (cur_tg & win_ihub_thread_group_enable_yes_f())
				cur_tg = (cur_tg >> 1) & 0x1f;
			else
				cur_tg = NO_THREAD_GROUP;

			cur_tg_assignments[win_id] = cur_tg;
			if (cur_tg < DC_N_WINDOWS)
				tg_owners[cur_tg] = win_id;
			new_tg = res->thread_group_win[j];
			new_tg_assignments[win_id] = new_tg;

			owner_dc_idx[win_id] = i;
			dep_left += 1;
		}
	}

	/* Step 1 */
	tegra_nvdisp_generate_tg_dep_graph(dc,
					new_tg_assignments,
					tg_owners,
					in_edges,
					new_out_edges);

	/* Step 2 proper */
	memset(dc->tg_reqs, 0, sizeof(dc->tg_reqs));
	while (dep_left > 0) {
		struct tegra_nvdisp_tg_req reqs[TEGRA_MAX_DC];
		bool dep_decreased = false;
		int dc_idx;

		memset(reqs, 0, sizeof(reqs));
		memcpy(cur_out_edges, new_out_edges, sizeof(cur_out_edges));
		for (i = 0; i < DC_N_WINDOWS; i++) {
			win_id = i;
			if (cur_out_edges[win_id] != -1 ||
				(dep_mask & (1 << win_id)))
				continue;

			/* Step 2a */
			while (win_id != -1) {
				struct tegra_nvdisp_tg_req *req;

				/* If this window isn't active, skip it. */
				dc_idx = owner_dc_idx[win_id];
				if (dc_idx == -1)
					break;

				/* Step 2b */
				cur_tg = cur_tg_assignments[win_id];
				new_tg = new_tg_assignments[win_id];

				/*
				 * Only fill in the thread group request if the
				 * thread group for this window is actually
				 * changing.
				*/
				if (new_tg != cur_tg) {
					req = &reqs[dc_idx];
					req->win_ids[req->num_wins] = win_id;
					req->tgs[req->num_wins] = new_tg;
					req->dc_idx = dc_idx;
					req->num_wins++;
				}

				/* Step 2c */
				dep_mask |= (1 << win_id);
				dep_left -= 1;
				dep_decreased = true;

				/* Step 2d */
				win_id = in_edges[win_id];
				if (win_id != -1) {
					new_out_edges[win_id] = -1;
					if (owner_dc_idx[win_id] != dc_idx)
						win_id = -1;
				}
			}
		}

		/* Error A */
		if (!dep_decreased)
			return -EINVAL;

		/* Step 2e */
		for (i = 0; i < TEGRA_MAX_DC; i++) {
			other_dc = tegra_dc_get_dc(i);
			if (!other_dc || !reqs[i].num_wins)
				continue;

			if (other_dc->ctrl_num == dc->ctrl_num) {
				/* Error B */
				if (dc_req_filled)
					return -EINVAL;
				dc_req_filled = true;
			}

			dc->tg_reqs[req_idx++] = reqs[i];
		}
	}

	return 0;
}

static void tegra_nvdisp_program_thread_group_reqs(struct tegra_dc *dc,
						int start,
						int end)
{
	int i, j;

	for (i = start; i < end; i++) {
		struct tegra_nvdisp_tg_req *req;
		struct tegra_dc *owner_dc;

		req = &dc->tg_reqs[i];
		owner_dc = tegra_dc_get_dc(req->dc_idx);
		if (!owner_dc || !owner_dc->enabled)
			continue;

		if (!req->num_wins)
			break;

		for (j = 0; j < req->num_wins; j++) {
			struct tegra_dc_win *win;
			int win_id;
			u32 tg;

			win_id = req->win_ids[j];
			win = tegra_dc_get_window(owner_dc, win_id);
			if (!win)
				continue;

			tg = req->tgs[j];
			if (tg == NO_THREAD_GROUP)
				nvdisp_win_write(win,
					win_ihub_thread_group_enable_no_f(),
					win_ihub_thread_group_r());
			else
				nvdisp_win_write(win,
					win_ihub_thread_group_num_f(tg) |
					win_ihub_thread_group_enable_yes_f(),
					win_ihub_thread_group_r());
		}

		tegra_nvdisp_activate_common_channel(owner_dc);
		tegra_nvdisp_wait_for_common_channel_to_promote(owner_dc);
	}
}

static void tegra_nvdisp_program_thread_groups(struct tegra_dc *dc,
					bool before_flip)
{
	int start, end;
	int i;

	for (i = 0; i < DC_N_WINDOWS; i++) {
		struct tegra_nvdisp_tg_req *req;
		struct tegra_dc *other_dc;

		req = &dc->tg_reqs[i];
		if (!req->num_wins)
			break;

		other_dc = tegra_dc_get_dc(req->dc_idx);
		if (other_dc && other_dc->ctrl_num == dc->ctrl_num)
			break;
	}

	if (before_flip) {
		start = 0;
		end = i;
	} else {
		start = i + 1;
		end = DC_N_WINDOWS;
	}

	tegra_nvdisp_program_thread_group_reqs(dc, start, end);
}

static int tegra_nvdisp_handle_tg_assignments(struct tegra_dc *dc)
{
	int ret = 0;

	ret = tegra_nvdisp_generate_tg_ordering(dc);
	if (ret)
		return ret;
	tegra_nvdisp_program_thread_groups(dc, true);

	return ret;
}

void tegra_nvdisp_complete_imp_programming(struct tegra_dc *dc)
{
	mutex_lock(&tegra_nvdisp_lock);

	if (!dc->need_to_complete_imp) {
		mutex_unlock(&tegra_nvdisp_lock);
		return;
	}
	dc->need_to_complete_imp = false;

	mutex_unlock(&tegra_nvdisp_lock);

	/* Make sure there's no pending change on the current head. */
	tegra_nvdisp_wait_for_common_channel_to_promote(dc);

	tegra_nvdisp_program_all_needed_mempool(dc);
	tegra_nvdisp_program_thread_groups(dc, false);

	/* Re-enable ihub latency events. */
	tegra_dc_writel(dc,
		tegra_dc_readl(dc, nvdisp_ihub_misc_ctl_r()) |
		nvdisp_ihub_misc_ctl_latency_event_enable_f(),
		nvdisp_ihub_misc_ctl_r());

	/* Release the common channel. */
	tegra_nvdisp_release_common_channel(dc);
}
EXPORT_SYMBOL(tegra_nvdisp_complete_imp_programming);

static bool tegra_nvdisp_common_channel_is_free(void)
{
	int i;

	for (i = 0; i < TEGRA_MAX_DC; i++) {
		struct tegra_dc *dc = tegra_dc_get_dc(i);
		if (dc && dc->common_channel_reserved)
			return false;
	}

	return true;
}

int tegra_nvdisp_reserve_common_channel(struct tegra_dc *dc)
{
	int ret = 0;

	mutex_lock(&tegra_nvdisp_lock);

	ret = ___wait_event(nvdisp_common_channel_reservation_wq,
		___wait_cond_timeout(tegra_nvdisp_common_channel_is_free()),
		TASK_INTERRUPTIBLE, 0, HZ,
		mutex_unlock(&tegra_nvdisp_lock);
		__ret = schedule_timeout(__ret);
		mutex_lock(&tegra_nvdisp_lock));

	dc->common_channel_reserved = true;

	mutex_unlock(&tegra_nvdisp_lock);
	return ret;
}

void tegra_nvdisp_release_common_channel(struct tegra_dc *dc)
{
	dc->common_channel_reserved = false;
	wake_up(&nvdisp_common_channel_reservation_wq);
}

void tegra_nvdisp_handle_common_channel_promotion(struct tegra_dc *dc)
{
	mutex_lock(&tegra_nvdisp_lock);

	if (dc->common_channel_pending &&
		!(tegra_dc_readl(dc, DC_CMD_STATE_CONTROL) & COMMON_ACT_REQ))
		tegra_nvdisp_notify_common_channel_promoted(dc);

	mutex_unlock(&tegra_nvdisp_lock);
}
EXPORT_SYMBOL(tegra_nvdisp_handle_common_channel_promotion);

int tegra_nvdisp_process_imp_results(struct tegra_dc *dc,
			struct tegra_dc_ext_flip_user_data *flip_user_data)
{
	int ret;

	/*
	 * This function is called during the PROPOSE phase. Once the COMMON
	 * channel is reserved, it will eventually be released after the
	 * ihub settings get promoted on vblank.
	 */
	tegra_nvdisp_reserve_common_channel(dc);

	ret = tegra_nvdisp_parse_imp_results(dc, flip_user_data);
	if (ret)
		goto release_and_ret;

	tegra_nvdisp_handle_mempool_allocation(dc);
	ret = tegra_nvdisp_handle_tg_assignments(dc);
	if (ret)
		goto release_and_ret;

	dc->new_imp_results_needed = true;
	dc->need_to_complete_imp = true;

	return ret;
release_and_ret:
	tegra_nvdisp_release_common_channel(dc);
	return ret;
}
EXPORT_SYMBOL(tegra_nvdisp_process_imp_results);

static void tegra_nvdisp_program_imp_head_results(struct tegra_dc *dc,
			struct tegra_dc_imp_head_results *imp_head_results,
			int owner_head)
{
	struct tegra_dc_win *win = NULL;
	u32 val;
	int i;

	if (!dc || !dc->enabled)
		return;

	for (i = 0; i < imp_head_results->num_windows; i++) {
		win = tegra_dc_get_window(dc, imp_head_results->win_ids[i]);
		if (!win)
			continue;

		/* program wgrp fetch meter slots */
		val = imp_head_results->metering_slots_value_win[i];
		nvdisp_win_write(win,
			win_ihub_fetch_meter_slots_f(val),
			win_ihub_fetch_meter_r());

		/*
		 * Since these wgrp latency registers take effect immediately,
		 * re-enable latency events after the rest of the window channel
		 * state for this flip has promoted.
		 */
		val = imp_head_results->thresh_lwm_dvfs_win[i];
		tegra_dc_writel(dc,
			tegra_dc_readl(dc, nvdisp_ihub_misc_ctl_r()) &
			~nvdisp_ihub_misc_ctl_latency_event_enable_f(),
			nvdisp_ihub_misc_ctl_r());
		if (val) {
			nvdisp_win_write(win,
				win_ihub_latency_ctla_ctl_mode_enable_f() |
				win_ihub_latency_ctla_submode_watermark_f(),
				win_ihub_latency_ctla_r());
			nvdisp_win_write(win,
				win_ihub_latency_ctlb_watermark_f(val),
				win_ihub_latency_ctlb_r());
		}

		/* program wgrp pipe meter value */
		val = imp_head_results->pipe_meter_value_win[i];
		nvdisp_win_write(win,
			win_precomp_pipe_meter_val_f(val),
			win_precomp_pipe_meter_r());

		/*
		 * Only program wgrp thread group and mempool for the head whose
		 * window state is changing. Thread group and mempool changes on
		 * other heads will be taken care of separately.
		 */
		if (dc->ctrl_num == owner_head) {
			val = imp_head_results->pool_config_entries_win[i];
			nvdisp_win_write(win,
				win_ihub_pool_config_entries_f(val),
				win_ihub_pool_config_r());

			val = imp_head_results->thread_group_win[i];
			if (val == NO_THREAD_GROUP)
				nvdisp_win_write(win,
					win_ihub_thread_group_enable_no_f(),
					win_ihub_thread_group_r());
			else
				nvdisp_win_write(win,
					win_ihub_thread_group_num_f(val) |
					win_ihub_thread_group_enable_yes_f(),
					win_ihub_thread_group_r());
		}
	}

	/* program cursor fetch meter slots */
	val = imp_head_results->metering_slots_value_cursor;
	tegra_dc_writel(dc,
			nvdisp_ihub_cursor_fetch_meter_slots_f(val),
			nvdisp_ihub_cursor_fetch_meter_r());

	/* program cursor pipe meter value */
	val = imp_head_results->pipe_meter_value_cursor;
	tegra_dc_writel(dc,
			nvdisp_cursor_pipe_meter_val_f(val),
			nvdisp_cursor_pipe_meter_r());

	/*
	 * Only program cursor mempool for this head. See comment for wgrp
	 * mempool above.
	 */
	if (dc->ctrl_num == owner_head) {
		val = imp_head_results->pool_config_entries_cursor;
		tegra_dc_writel(dc,
				nvdisp_ihub_cursor_pool_config_entries_f(val),
				nvdisp_ihub_cursor_pool_config_r());
	}
}

void tegra_nvdisp_program_imp_results(struct tegra_dc *dc)
{
	u32 val = 0;
	int i;

	for (i = 0; i < TEGRA_MAX_DC; i++) {
		struct tegra_dc *other_dc = tegra_dc_get_dc(i);
		if (other_dc && other_dc->enabled)
			tegra_nvdisp_program_imp_head_results(other_dc,
					&dc->imp_results[other_dc->ctrl_num],
					dc->ctrl_num);
	}

	/* program common win and cursor fetch meter slots */
	val = (dc->imp_results[dc->ctrl_num].window_slots_value) |
		(dc->imp_results[dc->ctrl_num].cursor_slots_value << 8);
	tegra_dc_writel(dc, val, nvdisp_ihub_common_fetch_meter_r());
}
#undef NO_THREAD_GROUP

void reg_dump(struct tegra_dc *dc, void *data,
		       void (* print)(void *data, const char *str))
{
	int i;
	char buff[256];
	const char winname[] = "ABCDEFT";
	#if 0
	/* If gated, quietly return. */
	if (!tegra_powergate_is_powered(dc->powergate_id)){
		return;
	}
	#endif

	mutex_lock(&dc->lock);
	tegra_dc_get(dc);

#define DUMP_REG(a) do {			\
	snprintf(buff, sizeof(buff), "%-32s\t%03x\t%08lx\n",  \
		 #a, a, tegra_dc_readl(dc, a));		      \
	print(data, buff);				      \
	} while (0)

#include "hw_nvdisp_nvdisp_regdump.c"

#undef DUMP_REG

#define DUMP_REG(a) do {				\
	snprintf(buff, sizeof(buff), "%-32s\t%03x\t%08x\n",  \
		 #a, a, nvdisp_win_read(win, a));	      \
	print(data, buff);				      \
	} while (0)


	for (i = 0; i < DC_N_WINDOWS; ++i) {
		struct tegra_dc_win *win = tegra_dc_get_window(dc, i);
		if (!win || !win->dc)
			continue;

		print(data, "\n");
		snprintf(buff, sizeof(buff), "WINDOW %c:\n", winname[i]);
		print(data, buff);

#include "hw_nvdisp_win_regdump.c"

#undef DUMP_REG
	}

	tegra_dc_put(dc);
	mutex_unlock(&dc->lock);
}

