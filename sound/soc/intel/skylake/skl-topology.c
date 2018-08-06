/*
 *  skl-topology.c - Implements Platform component ALSA controls/widget
 *  handlers.
 *
 *  Copyright (C) 2014-2015 Intel Corp
 *  Author: Jeeja KP <jeeja.kp@intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/types.h>
#include <linux/firmware.h>
#include <sound/soc.h>
#include <sound/soc-topology.h>
#include <uapi/sound/snd_sst_tokens.h>
#include "skl-sst-dsp.h"
#include "skl-sst-ipc.h"
#include "skl-topology.h"
#include "skl.h"
#include "skl-tplg-interface.h"
#include "../common/sst-dsp.h"
#include "../common/sst-dsp-priv.h"
#include "skl-fwlog.h"

#define SKL_CURVE_NONE	0
#define SKL_MAX_GAIN	0x7FFFFFFF
#define SKL_CH_FIXUP_MASK		(1 << 0)
#define SKL_RATE_FIXUP_MASK		(1 << 1)
#define SKL_FMT_FIXUP_MASK		(1 << 2)
#define SKL_IN_DIR_BIT_MASK		BIT(0)
#define SKL_PIN_COUNT_MASK		GENMASK(7, 4)

static const int mic_mono_list[] = {
0, 1, 2, 3,
};
static const int mic_stereo_list[][SKL_CH_STEREO] = {
{0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3},
};
static const int mic_trio_list[][SKL_CH_TRIO] = {
{0, 1, 2}, {0, 1, 3}, {0, 2, 3}, {1, 2, 3},
};
static const int mic_quatro_list[][SKL_CH_QUATRO] = {
{0, 1, 2, 3},
};

#define CHECK_HW_PARAMS(ch, freq, bps, prm_ch, prm_freq, prm_bps) \
	((ch == prm_ch) && (bps == prm_bps) && (freq == prm_freq))

#define GET_PIPE(ppl, skl, node, pipe_id, pipe) \
	do { list_for_each_entry(ppl, &skl->ppl_list, node) { \
		if (ppl->pipe->ppl_id == pipe_id) { \
			pipe = ppl->pipe; \
			break; } \
		} \
	} while (0)

/*
 * The following table provides the gain in linear scale corresponding to
 * gain in dB scale in the range of -144 dB to 0 dB with 0.1 dB resolution.
 * The real number linear gain is scaled by 0x7FFFFFFFF to convert it to a
 * 32 bit integer as required by FW.
 * linear_gain[i] = 0   for i = 0 ; (Mapped as mute)
 *		  = 0x7FFFFFFF*Round(10^(-144+0.1*i)/20) for i = 1 ... 1440
 */
static u32 linear_gain[] = {
0x00000000, 0x00000089, 0x0000008B, 0x0000008C, 0x0000008E, 0x00000090,
0x00000091, 0x00000093, 0x00000095, 0x00000096, 0x00000098, 0x0000009A,
0x0000009C, 0x0000009D, 0x0000009F, 0x000000A1, 0x000000A3, 0x000000A5,
0x000000A7, 0x000000A9, 0x000000AB, 0x000000AD, 0x000000AF, 0x000000B1,
0x000000B3, 0x000000B5, 0x000000B7, 0x000000B9, 0x000000BB, 0x000000BD,
0x000000BF, 0x000000C2, 0x000000C4, 0x000000C6, 0x000000C8, 0x000000CB,
0x000000CD, 0x000000CF, 0x000000D2, 0x000000D4, 0x000000D7, 0x000000D9,
0x000000DC, 0x000000DE, 0x000000E1, 0x000000E3, 0x000000E6, 0x000000E9,
0x000000EB, 0x000000EE, 0x000000F1, 0x000000F4, 0x000000F7, 0x000000F9,
0x000000FC, 0x000000FF, 0x00000102, 0x00000105, 0x00000108, 0x0000010B,
0x0000010E, 0x00000111, 0x00000115, 0x00000118, 0x0000011B, 0x0000011E,
0x00000122, 0x00000125, 0x00000128, 0x0000012C, 0x0000012F, 0x00000133,
0x00000136, 0x0000013A, 0x0000013E, 0x00000141, 0x00000145, 0x00000149,
0x0000014D, 0x00000150, 0x00000154, 0x00000158, 0x0000015C, 0x00000160,
0x00000164, 0x00000169, 0x0000016D, 0x00000171, 0x00000175, 0x0000017A,
0x0000017E, 0x00000182, 0x00000187, 0x0000018B, 0x00000190, 0x00000195,
0x00000199, 0x0000019E, 0x000001A3, 0x000001A8, 0x000001AC, 0x000001B1,
0x000001B6, 0x000001BC, 0x000001C1, 0x000001C6, 0x000001CB, 0x000001D0,
0x000001D6, 0x000001DB, 0x000001E1, 0x000001E6, 0x000001EC, 0x000001F2,
0x000001F7, 0x000001FD, 0x00000203, 0x00000209, 0x0000020F, 0x00000215,
0x0000021B, 0x00000222, 0x00000228, 0x0000022E, 0x00000235, 0x0000023B,
0x00000242, 0x00000249, 0x0000024F, 0x00000256, 0x0000025D, 0x00000264,
0x0000026B, 0x00000273, 0x0000027A, 0x00000281, 0x00000289, 0x00000290,
0x00000298, 0x0000029F, 0x000002A7, 0x000002AF, 0x000002B7, 0x000002BF,
0x000002C7, 0x000002CF, 0x000002D8, 0x000002E0, 0x000002E9, 0x000002F1,
0x000002FA, 0x00000303, 0x0000030C, 0x00000315, 0x0000031E, 0x00000327,
0x00000330, 0x0000033A, 0x00000343, 0x0000034D, 0x00000357, 0x00000361,
0x0000036B, 0x00000375, 0x0000037F, 0x0000038A, 0x00000394, 0x0000039F,
0x000003A9, 0x000003B4, 0x000003BF, 0x000003CA, 0x000003D6, 0x000003E1,
0x000003EC, 0x000003F8, 0x00000404, 0x00000410, 0x0000041C, 0x00000428,
0x00000434, 0x00000441, 0x0000044D, 0x0000045A, 0x00000467, 0x00000474,
0x00000481, 0x0000048F, 0x0000049C, 0x000004AA, 0x000004B8, 0x000004C6,
0x000004D4, 0x000004E2, 0x000004F1, 0x000004FF, 0x0000050E, 0x0000051D,
0x0000052C, 0x0000053B, 0x0000054B, 0x0000055B, 0x0000056B, 0x0000057B,
0x0000058B, 0x0000059B, 0x000005AC, 0x000005BD, 0x000005CE, 0x000005DF,
0x000005F0, 0x00000602, 0x00000614, 0x00000626, 0x00000638, 0x0000064A,
0x0000065D, 0x00000670, 0x00000683, 0x00000696, 0x000006AA, 0x000006BE,
0x000006D2, 0x000006E6, 0x000006FA, 0x0000070F, 0x00000724, 0x00000739,
0x0000074E, 0x00000764, 0x0000077A, 0x00000790, 0x000007A7, 0x000007BD,
0x000007D4, 0x000007EB, 0x00000803, 0x0000081B, 0x00000833, 0x0000084B,
0x00000863, 0x0000087C, 0x00000896, 0x000008AF, 0x000008C9, 0x000008E3,
0x000008FD, 0x00000918, 0x00000933, 0x0000094E, 0x0000096A, 0x00000985,
0x000009A2, 0x000009BE, 0x000009DB, 0x000009F8, 0x00000A16, 0x00000A34,
0x00000A52, 0x00000A71, 0x00000A90, 0x00000AAF, 0x00000ACE, 0x00000AEF,
0x00000B0F, 0x00000B30, 0x00000B51, 0x00000B72, 0x00000B94, 0x00000BB7,
0x00000BD9, 0x00000BFD, 0x00000C20, 0x00000C44, 0x00000C68, 0x00000C8D,
0x00000CB2, 0x00000CD8, 0x00000CFE, 0x00000D25, 0x00000D4C, 0x00000D73,
0x00000D9B, 0x00000DC3, 0x00000DEC, 0x00000E15, 0x00000E3F, 0x00000E69,
0x00000E94, 0x00000EBF, 0x00000EEB, 0x00000F17, 0x00000F44, 0x00000F71,
0x00000F9F, 0x00000FCD, 0x00000FFC, 0x0000102B, 0x0000105B, 0x0000108C,
0x000010BD, 0x000010EE, 0x00001121, 0x00001153, 0x00001187, 0x000011BB,
0x000011EF, 0x00001224, 0x0000125A, 0x00001291, 0x000012C8, 0x000012FF,
0x00001338, 0x00001371, 0x000013AA, 0x000013E4, 0x0000141F, 0x0000145B,
0x00001497, 0x000014D4, 0x00001512, 0x00001551, 0x00001590, 0x000015D0,
0x00001610, 0x00001652, 0x00001694, 0x000016D7, 0x0000171B, 0x0000175F,
0x000017A4, 0x000017EB, 0x00001831, 0x00001879, 0x000018C2, 0x0000190B,
0x00001955, 0x000019A0, 0x000019EC, 0x00001A39, 0x00001A87, 0x00001AD6,
0x00001B25, 0x00001B76, 0x00001BC7, 0x00001C19, 0x00001C6D, 0x00001CC1,
0x00001D16, 0x00001D6C, 0x00001DC4, 0x00001E1C, 0x00001E75, 0x00001ECF,
0x00001F2B, 0x00001F87, 0x00001FE5, 0x00002043, 0x000020A3, 0x00002103,
0x00002165, 0x000021C8, 0x0000222C, 0x00002292, 0x000022F8, 0x00002360,
0x000023C9, 0x00002433, 0x0000249E, 0x0000250B, 0x00002578, 0x000025E8,
0x00002658, 0x000026CA, 0x0000273D, 0x000027B1, 0x00002827, 0x0000289E,
0x00002916, 0x00002990, 0x00002A0B, 0x00002A88, 0x00002B06, 0x00002B85,
0x00002C06, 0x00002C89, 0x00002D0D, 0x00002D92, 0x00002E19, 0x00002EA2,
0x00002F2C, 0x00002FB8, 0x00003045, 0x000030D5, 0x00003165, 0x000031F8,
0x0000328C, 0x00003322, 0x000033B9, 0x00003453, 0x000034EE, 0x0000358B,
0x00003629, 0x000036CA, 0x0000376C, 0x00003811, 0x000038B7, 0x0000395F,
0x00003A09, 0x00003AB5, 0x00003B63, 0x00003C13, 0x00003CC5, 0x00003D79,
0x00003E30, 0x00003EE8, 0x00003FA2, 0x0000405F, 0x0000411E, 0x000041DF,
0x000042A2, 0x00004368, 0x0000442F, 0x000044FA, 0x000045C6, 0x00004695,
0x00004766, 0x0000483A, 0x00004910, 0x000049E8, 0x00004AC3, 0x00004BA1,
0x00004C81, 0x00004D64, 0x00004E49, 0x00004F32, 0x0000501C, 0x0000510A,
0x000051FA, 0x000052ED, 0x000053E3, 0x000054DC, 0x000055D7, 0x000056D6,
0x000057D7, 0x000058DB, 0x000059E3, 0x00005AED, 0x00005BFB, 0x00005D0B,
0x00005E1F, 0x00005F36, 0x00006050, 0x0000616E, 0x0000628F, 0x000063B3,
0x000064DA, 0x00006605, 0x00006734, 0x00006866, 0x0000699B, 0x00006AD4,
0x00006C11, 0x00006D51, 0x00006E95, 0x00006FDD, 0x00007129, 0x00007278,
0x000073CC, 0x00007523, 0x0000767E, 0x000077DD, 0x00007941, 0x00007AA8,
0x00007C14, 0x00007D83, 0x00007EF7, 0x00008070, 0x000081ED, 0x0000836E,
0x000084F3, 0x0000867D, 0x0000880C, 0x0000899F, 0x00008B37, 0x00008CD4,
0x00008E76, 0x0000901C, 0x000091C7, 0x00009377, 0x0000952C, 0x000096E6,
0x000098A6, 0x00009A6A, 0x00009C34, 0x00009E03, 0x00009FD7, 0x0000A1B1,
0x0000A391, 0x0000A575, 0x0000A760, 0x0000A950, 0x0000AB46, 0x0000AD42,
0x0000AF43, 0x0000B14B, 0x0000B358, 0x0000B56C, 0x0000B786, 0x0000B9A6,
0x0000BBCC, 0x0000BDF9, 0x0000C02C, 0x0000C266, 0x0000C4A6, 0x0000C6ED,
0x0000C93B, 0x0000CB8F, 0x0000CDEA, 0x0000D04D, 0x0000D2B6, 0x0000D527,
0x0000D79F, 0x0000DA1E, 0x0000DCA5, 0x0000DF33, 0x0000E1C8, 0x0000E466,
0x0000E70B, 0x0000E9B7, 0x0000EC6C, 0x0000EF29, 0x0000F1EE, 0x0000F4BB,
0x0000F791, 0x0000FA6F, 0x0000FD55, 0x00010044, 0x0001033C, 0x0001063C,
0x00010945, 0x00010C58, 0x00010F73, 0x00011298, 0x000115C6, 0x000118FD,
0x00011C3E, 0x00011F89, 0x000122DD, 0x0001263B, 0x000129A4, 0x00012D16,
0x00013092, 0x00013419, 0x000137AB, 0x00013B46, 0x00013EED, 0x0001429E,
0x0001465B, 0x00014A22, 0x00014DF5, 0x000151D3, 0x000155BC, 0x000159B1,
0x00015DB2, 0x000161BF, 0x000165D7, 0x000169FC, 0x00016E2D, 0x0001726B,
0x000176B5, 0x00017B0B, 0x00017F6F, 0x000183E0, 0x0001885D, 0x00018CE8,
0x00019181, 0x00019627, 0x00019ADB, 0x00019F9D, 0x0001A46D, 0x0001A94B,
0x0001AE38, 0x0001B333, 0x0001B83E, 0x0001BD57, 0x0001C27F, 0x0001C7B6,
0x0001CCFD, 0x0001D254, 0x0001D7BA, 0x0001DD30, 0x0001E2B7, 0x0001E84E,
0x0001EDF5, 0x0001F3AD, 0x0001F977, 0x0001FF51, 0x0002053D, 0x00020B3A,
0x00021149, 0x0002176A, 0x00021D9D, 0x000223E3, 0x00022A3B, 0x000230A6,
0x00023724, 0x00023DB5, 0x0002445A, 0x00024B12, 0x000251DE, 0x000258BF,
0x00025FB3, 0x000266BD, 0x00026DDB, 0x0002750F, 0x00027C57, 0x000283B6,
0x00028B2A, 0x000292B4, 0x00029A55, 0x0002A20C, 0x0002A9DA, 0x0002B1BF,
0x0002B9BC, 0x0002C1D0, 0x0002C9FD, 0x0002D241, 0x0002DA9E, 0x0002E314,
0x0002EBA3, 0x0002F44B, 0x0002FD0D, 0x000305E9, 0x00030EDF, 0x000317F0,
0x0003211B, 0x00032A62, 0x000333C4, 0x00033D42, 0x000346DC, 0x00035093,
0x00035A66, 0x00036457, 0x00036E65, 0x00037891, 0x000382DB, 0x00038D44,
0x000397CB, 0x0003A271, 0x0003AD38, 0x0003B81E, 0x0003C324, 0x0003CE4B,
0x0003D993, 0x0003E4FD, 0x0003F088, 0x0003FC36, 0x00040806, 0x000413F9,
0x00042010, 0x00042C4B, 0x000438A9, 0x0004452D, 0x000451D5, 0x00045EA4,
0x00046B98, 0x000478B2, 0x000485F3, 0x0004935C, 0x0004A0EC, 0x0004AEA5,
0x0004BC86, 0x0004CA90, 0x0004D8C4, 0x0004E722, 0x0004F5AB, 0x0005045F,
0x0005133E, 0x00052249, 0x00053181, 0x000540E6, 0x00055079, 0x0005603A,
0x0005702A, 0x00058048, 0x00059097, 0x0005A116, 0x0005B1C6, 0x0005C2A7,
0x0005D3BB, 0x0005E501, 0x0005F67A, 0x00060827, 0x00061A08, 0x00062C1F,
0x00063E6B, 0x000650ED, 0x000663A6, 0x00067697, 0x000689BF, 0x00069D21,
0x0006B0BC, 0x0006C491, 0x0006D8A1, 0x0006ECEC, 0x00070174, 0x00071638,
0x00072B3A, 0x0007407A, 0x000755FA, 0x00076BB9, 0x000781B8, 0x000797F9,
0x0007AE7B, 0x0007C541, 0x0007DC49, 0x0007F397, 0x00080B29, 0x00082301,
0x00083B20, 0x00085386, 0x00086C34, 0x0008852C, 0x00089E6E, 0x0008B7FA,
0x0008D1D3, 0x0008EBF8, 0x0009066A, 0x0009212B, 0x00093C3B, 0x0009579C,
0x0009734D, 0x00098F51, 0x0009ABA7, 0x0009C852, 0x0009E552, 0x000A02A7,
0x000A2054, 0x000A3E58, 0x000A5CB6, 0x000A7B6D, 0x000A9A80, 0x000AB9EF,
0x000AD9BB, 0x000AF9E5, 0x000B1A6E, 0x000B3B58, 0x000B5CA4, 0x000B7E52,
0x000BA064, 0x000BC2DB, 0x000BE5B8, 0x000C08FD, 0x000C2CAA, 0x000C50C1,
0x000C7543, 0x000C9A31, 0x000CBF8C, 0x000CE556, 0x000D0B91, 0x000D323C,
0x000D595A, 0x000D80ED, 0x000DA8F4, 0x000DD172, 0x000DFA69, 0x000E23D8,
0x000E4DC3, 0x000E7829, 0x000EA30E, 0x000ECE71, 0x000EFA55, 0x000F26BC,
0x000F53A6, 0x000F8115, 0x000FAF0A, 0x000FDD88, 0x00100C90, 0x00103C23,
0x00106C43, 0x00109CF2, 0x0010CE31, 0x00110003, 0x00113267, 0x00116562,
0x001198F3, 0x0011CD1D, 0x001201E2, 0x00123743, 0x00126D43, 0x0012A3E2,
0x0012DB24, 0x00131309, 0x00134B94, 0x001384C7, 0x0013BEA3, 0x0013F92B,
0x00143460, 0x00147044, 0x0014ACDB, 0x0014EA24, 0x00152824, 0x001566DB,
0x0015A64C, 0x0015E67A, 0x00162765, 0x00166911, 0x0016AB80, 0x0016EEB3,
0x001732AE, 0x00177772, 0x0017BD02, 0x00180361, 0x00184A90, 0x00189292,
0x0018DB69, 0x00192518, 0x00196FA2, 0x0019BB09, 0x001A074F, 0x001A5477,
0x001AA284, 0x001AF179, 0x001B4157, 0x001B9222, 0x001BE3DD, 0x001C368A,
0x001C8A2C, 0x001CDEC6, 0x001D345B, 0x001D8AED, 0x001DE280, 0x001E3B17,
0x001E94B4, 0x001EEF5B, 0x001F4B0F, 0x001FA7D2, 0x002005A9, 0x00206496,
0x0020C49C, 0x002125BE, 0x00218801, 0x0021EB67, 0x00224FF3, 0x0022B5AA,
0x00231C8E, 0x002384A3, 0x0023EDED, 0x0024586F, 0x0024C42C, 0x00253129,
0x00259F69, 0x00260EF0, 0x00267FC1, 0x0026F1E1, 0x00276553, 0x0027DA1C,
0x0028503E, 0x0028C7BF, 0x002940A2, 0x0029BAEB, 0x002A369F, 0x002AB3C1,
0x002B3257, 0x002BB263, 0x002C33EC, 0x002CB6F4, 0x002D3B81, 0x002DC196,
0x002E4939, 0x002ED26E, 0x002F5D3A, 0x002FE9A2, 0x003077A9, 0x00310756,
0x003198AC, 0x00322BB1, 0x0032C06A, 0x003356DC, 0x0033EF0C, 0x003488FF,
0x003524BB, 0x0035C244, 0x003661A0, 0x003702D4, 0x0037A5E6, 0x00384ADC,
0x0038F1BB, 0x00399A88, 0x003A454A, 0x003AF206, 0x003BA0C2, 0x003C5184,
0x003D0452, 0x003DB932, 0x003E702A, 0x003F2940, 0x003FE47B, 0x0040A1E2,
0x00416179, 0x00422349, 0x0042E757, 0x0043ADAA, 0x00447649, 0x0045413B,
0x00460E87, 0x0046DE33, 0x0047B046, 0x004884C9, 0x00495BC1, 0x004A3537,
0x004B1131, 0x004BEFB7, 0x004CD0D1, 0x004DB486, 0x004E9ADE, 0x004F83E1,
0x00506F97, 0x00515E08, 0x00524F3B, 0x00534339, 0x00543A0B, 0x005533B8,
0x00563049, 0x00572FC8, 0x0058323B, 0x005937AD, 0x005A4025, 0x005B4BAE,
0x005C5A4F, 0x005D6C13, 0x005E8102, 0x005F9927, 0x0060B48A, 0x0061D334,
0x0062F531, 0x00641A89, 0x00654347, 0x00666F74, 0x00679F1C, 0x0068D247,
0x006A0901, 0x006B4354, 0x006C814B, 0x006DC2F0, 0x006F084F, 0x00705172,
0x00719E65, 0x0072EF33, 0x007443E8, 0x00759C8E, 0x0076F932, 0x007859DF,
0x0079BEA2, 0x007B2787, 0x007C9499, 0x007E05E6, 0x007F7B79, 0x0080F560,
0x008273A6, 0x0083F65A, 0x00857D89, 0x0087093F, 0x0088998A, 0x008A2E77,
0x008BC815, 0x008D6672, 0x008F099A, 0x0090B19D, 0x00925E89, 0x0094106D,
0x0095C756, 0x00978355, 0x00994478, 0x009B0ACE, 0x009CD667, 0x009EA752,
0x00A07DA0, 0x00A25960, 0x00A43AA2, 0x00A62177, 0x00A80DEE, 0x00AA001A,
0x00ABF80A, 0x00ADF5D1, 0x00AFF97E, 0x00B20324, 0x00B412D4, 0x00B628A1,
0x00B8449C, 0x00BA66D8, 0x00BC8F67, 0x00BEBE5B, 0x00C0F3C9, 0x00C32FC3,
0x00C5725D, 0x00C7BBA9, 0x00CA0BBD, 0x00CC62AC, 0x00CEC08A, 0x00D1256C,
0x00D39167, 0x00D60490, 0x00D87EFC, 0x00DB00C0, 0x00DD89F3, 0x00E01AAB,
0x00E2B2FD, 0x00E55300, 0x00E7FACC, 0x00EAAA77, 0x00ED6218, 0x00F021C7,
0x00F2E99C, 0x00F5B9B0, 0x00F89219, 0x00FB72F2, 0x00FE5C54, 0x01014E57,
0x01044915, 0x01074CA8, 0x010A592A, 0x010D6EB6, 0x01108D67, 0x0113B557,
0x0116E6A2, 0x011A2164, 0x011D65B9, 0x0120B3BC, 0x01240B8C, 0x01276D45,
0x012AD904, 0x012E4EE7, 0x0131CF0B, 0x01355991, 0x0138EE96, 0x013C8E39,
0x0140389A, 0x0143EDD8, 0x0147AE14, 0x014B796F, 0x014F500A, 0x01533205,
0x01571F82, 0x015B18A5, 0x015F1D8E, 0x01632E61, 0x01674B42, 0x016B7454,
0x016FA9BB, 0x0173EB9C, 0x01783A1B, 0x017C955F, 0x0180FD8D, 0x018572CB,
0x0189F540, 0x018E8513, 0x0193226D, 0x0197CD74, 0x019C8651, 0x01A14D2E,
0x01A62234, 0x01AB058D, 0x01AFF764, 0x01B4F7E3, 0x01BA0735, 0x01BF2588,
0x01C45306, 0x01C98FDE, 0x01CEDC3D, 0x01D43850, 0x01D9A447, 0x01DF2050,
0x01E4AC9B, 0x01EA4958, 0x01EFF6B8, 0x01F5B4ED, 0x01FB8428, 0x0201649B,
0x0207567A, 0x020D59F9, 0x02136F4B, 0x021996A5, 0x021FD03D, 0x02261C4A,
0x022C7B01, 0x0232EC9A, 0x0239714D, 0x02400952, 0x0246B4E4, 0x024D743B,
0x02544792, 0x025B2F26, 0x02622B31, 0x02693BF0, 0x027061A1, 0x02779C82,
0x027EECD2, 0x028652D0, 0x028DCEBC, 0x029560D8, 0x029D0964, 0x02A4C8A5,
0x02AC9EDD, 0x02B48C50, 0x02BC9142, 0x02C4ADFB, 0x02CCE2BF, 0x02D52FD7,
0x02DD958A, 0x02E61422, 0x02EEABE8, 0x02F75D27, 0x0300282A, 0x03090D3F,
0x03120CB1, 0x031B26CF, 0x03245BE9, 0x032DAC4D, 0x0337184E, 0x0340A03D,
0x034A446D, 0x03540531, 0x035DE2DF, 0x0367DDCC, 0x0371F64E, 0x037C2CBD,
0x03868173, 0x0390F4C8, 0x039B8719, 0x03A638BF, 0x03B10A19, 0x03BBFB84,
0x03C70D60, 0x03D2400C, 0x03DD93E9, 0x03E9095B, 0x03F4A0C5, 0x04005A8B,
0x040C3714, 0x041836C5, 0x04245A09, 0x0430A147, 0x043D0CEB, 0x04499D60,
0x04565314, 0x04632E76, 0x04702FF4, 0x047D57FF, 0x048AA70B, 0x04981D8B,
0x04A5BBF3, 0x04B382B9, 0x04C17257, 0x04CF8B44, 0x04DDCDFB, 0x04EC3AF8,
0x04FAD2B9, 0x050995BB, 0x05188480, 0x05279F89, 0x0536E758, 0x05465C74,
0x0555FF62, 0x0565D0AB, 0x0575D0D6, 0x05860070, 0x05966005, 0x05A6F023,
0x05B7B15B, 0x05C8A43D, 0x05D9C95D, 0x05EB2150, 0x05FCACAD, 0x060E6C0B,
0x06206006, 0x06328938, 0x0644E841, 0x06577DBE, 0x066A4A53, 0x067D4EA2,
0x06908B50, 0x06A40104, 0x06B7B068, 0x06CB9A26, 0x06DFBEEC, 0x06F41F68,
0x0708BC4C, 0x071D964A, 0x0732AE18, 0x0748046D, 0x075D9A02, 0x07736F92,
0x078985DC, 0x079FDD9F, 0x07B6779E, 0x07CD549C, 0x07E47560, 0x07FBDAB4,
0x08138562, 0x082B7638, 0x0843AE06, 0x085C2D9E, 0x0874F5D6, 0x088E0783,
0x08A76381, 0x08C10AAC, 0x08DAFDE2, 0x08F53E04, 0x090FCBF7, 0x092AA8A2,
0x0945D4EE, 0x096151C6, 0x097D201A, 0x099940DB, 0x09B5B4FE, 0x09D27D79,
0x09EF9B47, 0x0A0D0F64, 0x0A2ADAD1, 0x0A48FE91, 0x0A677BA8, 0x0A865320,
0x0AA58606, 0x0AC51567, 0x0AE50256, 0x0B054DE8, 0x0B25F937, 0x0B47055D,
0x0B68737A, 0x0B8A44AF, 0x0BAC7A24, 0x0BCF1501, 0x0BF21673, 0x0C157FA9,
0x0C3951D8, 0x0C5D8E36, 0x0C8235FF, 0x0CA74A70, 0x0CCCCCCD, 0x0CF2BE5A,
0x0D192061, 0x0D3FF430, 0x0D673B17, 0x0D8EF66D, 0x0DB7278B, 0x0DDFCFCC,
0x0E08F094, 0x0E328B46, 0x0E5CA14C, 0x0E873415, 0x0EB24511, 0x0EDDD5B7,
0x0F09E781, 0x0F367BEE, 0x0F639481, 0x0F9132C3, 0x0FBF583F, 0x0FEE0686,
0x101D3F2D, 0x104D03D0, 0x107D560D, 0x10AE3787, 0x10DFA9E7, 0x1111AEDB,
0x11444815, 0x1177774D, 0x11AB3E3F, 0x11DF9EAE, 0x12149A60, 0x124A3321,
0x12806AC3, 0x12B7431D, 0x12EEBE0C, 0x1326DD70, 0x135FA333, 0x13991141,
0x13D3298C, 0x140DEE0E, 0x144960C5, 0x148583B6, 0x14C258EA, 0x14FFE273,
0x153E2266, 0x157D1AE2, 0x15BCCE07, 0x15FD3E01, 0x163E6CFE, 0x16805D35,
0x16C310E3, 0x17068A4B, 0x174ACBB8, 0x178FD779, 0x17D5AFE8, 0x181C5762,
0x1863D04D, 0x18AC1D17, 0x18F54033, 0x193F3C1D, 0x198A1357, 0x19D5C86C,
0x1A225DED, 0x1A6FD673, 0x1ABE349F, 0x1B0D7B1B, 0x1B5DAC97, 0x1BAECBCA,
0x1C00DB77, 0x1C53DE66, 0x1CA7D768, 0x1CFCC956, 0x1D52B712, 0x1DA9A387,
0x1E0191A9, 0x1E5A8471, 0x1EB47EE7, 0x1F0F8416, 0x1F6B9715, 0x1FC8BB06,
0x2026F30F, 0x20864265, 0x20E6AC43, 0x214833EE, 0x21AADCB6, 0x220EA9F4,
0x22739F0A, 0x22D9BF65, 0x23410E7E, 0x23A98FD5, 0x241346F6, 0x247E3777,
0x24EA64F9, 0x2557D328, 0x25C685BB, 0x26368073, 0x26A7C71D, 0x271A5D91,
0x278E47B3, 0x28038970, 0x287A26C4, 0x28F223B6, 0x296B8457, 0x29E64CC5,
0x2A62812C, 0x2AE025C3, 0x2B5F3ECC, 0x2BDFD098, 0x2C61DF84, 0x2CE56FF9,
0x2D6A866F, 0x2DF12769, 0x2E795779, 0x2F031B3E, 0x2F8E7765, 0x301B70A8,
0x30AA0BCF, 0x313A4DB3, 0x31CC3B37, 0x325FD94F, 0x32F52CFF, 0x338C3B56,
0x34250975, 0x34BF9C8B, 0x355BF9D8, 0x35FA26A9, 0x369A285D, 0x373C0461,
0x37DFC033, 0x38856163, 0x392CED8E, 0x39D66A63, 0x3A81DDA4, 0x3B2F4D22,
0x3BDEBEBF, 0x3C90386F, 0x3D43C038, 0x3DF95C32, 0x3EB11285, 0x3F6AE96F,
0x4026E73C, 0x40E5124F, 0x41A5711B, 0x42680A28, 0x432CE40F, 0x43F4057E,
0x44BD7539, 0x45893A13, 0x46575AF8, 0x4727DEE6, 0x47FACCF0, 0x48D02C3F,
0x49A8040F, 0x4A825BB5, 0x4B5F3A99, 0x4C3EA838, 0x4D20AC29, 0x4E054E17,
0x4EEC95C3, 0x4FD68B07, 0x50C335D3, 0x51B29E2F, 0x52A4CC3A, 0x5399C82D,
0x54919A57, 0x558C4B22, 0x5689E30E, 0x578A6AB7, 0x588DEAD1, 0x59946C2A,
0x5A9DF7AB, 0x5BAA9656, 0x5CBA514A, 0x5DCD31BD, 0x5EE34105, 0x5FFC8890,
0x611911E9, 0x6238E6BA, 0x635C10C5, 0x648299EC, 0x65AC8C2E, 0x66D9F1A7,
0x680AD491, 0x693F3F45, 0x6A773C39, 0x6BB2D603, 0x6CF2175A, 0x6E350B13,
0x6F7BBC23, 0x70C6359F, 0x721482BF, 0x7366AEDB, 0x74BCC56B, 0x7616D20D,
0x7774E07D, 0x78D6FC9E, 0x7A3D3271, 0x7BA78E21, 0x7D161BF7, 0x7E88E865,
0x7FFFFFFF};

static void skl_init_single_module_pipe(struct snd_soc_dapm_widget *w,
						struct skl *skl);

void skl_tplg_d0i3_get(struct skl *skl, enum d0i3_capability caps)
{
	struct skl_d0i3_data *d0i3 =  &skl->skl_sst->d0i3;

	switch (caps) {
	case SKL_D0I3_NONE:
		d0i3->non_d0i3++;
		break;

	case SKL_D0I3_STREAMING:
		d0i3->streaming++;
		break;

	case SKL_D0I3_NON_STREAMING:
		d0i3->non_streaming++;
		break;
	}
}

void skl_tplg_d0i3_put(struct skl *skl, enum d0i3_capability caps)
{
	struct skl_d0i3_data *d0i3 =  &skl->skl_sst->d0i3;

	switch (caps) {
	case SKL_D0I3_NONE:
		d0i3->non_d0i3--;
		break;

	case SKL_D0I3_STREAMING:
		d0i3->streaming--;
		break;

	case SKL_D0I3_NON_STREAMING:
		d0i3->non_streaming--;
		break;
	}
}

/*
 * SKL DSP driver modelling uses only few DAPM widgets so for rest we will
 * ignore. This helpers checks if the SKL driver handles this widget type
 */
int is_skl_dsp_widget_type(struct snd_soc_dapm_widget *w,
				  struct device *dev)
{
	if (w->dapm->dev == dev) {
		switch (w->id) {
		case snd_soc_dapm_dai_link:
		case snd_soc_dapm_dai_in:
		case snd_soc_dapm_aif_in:
		case snd_soc_dapm_aif_out:
		case snd_soc_dapm_dai_out:
		case snd_soc_dapm_switch:
			return false;
		default:
			return true;
		}
	}

	return false;
}

/*
 * Each pipelines needs memory to be allocated. Check if we have free memory
 * from available pool.
 */
static bool skl_is_pipe_mem_avail(struct skl *skl,
				struct skl_module_cfg *mconfig)
{
	struct skl_sst *ctx = skl->skl_sst;

	if (skl->resource.mem + mconfig->pipe->memory_pages >
				skl->resource.max_mem) {
		dev_err(ctx->dev,
				"%s: module_id %d instance %d\n", __func__,
				mconfig->id.module_id,
				mconfig->id.instance_id);
		dev_err(ctx->dev,
				"exceeds ppl memory available %d mem %d\n",
				skl->resource.max_mem, skl->resource.mem);
		return false;
	} else {
		return true;
	}
}

/*
 * Add the mem to the mem pool. This is freed when pipe is deleted.
 * Note: DSP does actual memory management we only keep track for complete
 * pool
 */
static void skl_tplg_alloc_pipe_mem(struct skl *skl,
				struct skl_module_cfg *mconfig)
{
	skl->resource.mem += mconfig->pipe->memory_pages;
}

/*
 * Pipeline needs needs DSP CPU resources for computation, this is
 * quantified in MCPS (Million Clocks Per Second) required for module/pipe
 *
 * Each pipelines needs mcps to be allocated. Check if we have mcps for this
 * pipe.
 */

static bool skl_is_pipe_mcps_avail(struct skl *skl,
				struct skl_module_cfg *mconfig)
{
	struct skl_sst *ctx = skl->skl_sst;
	u8 res_idx = mconfig->res_idx;
	struct skl_module_res *res = &mconfig->module->resources[res_idx];

	if (skl->resource.mcps + res->cps > skl->resource.max_mcps) {
		dev_err(ctx->dev,
			"%s: module_id %d instance %d\n", __func__,
			mconfig->id.module_id, mconfig->id.instance_id);
		dev_err(ctx->dev,
			"exceeds ppl mcps available %d > mem %d\n",
			skl->resource.max_mcps, skl->resource.mcps);
		return false;
	} else {
		return true;
	}
}

static void skl_tplg_alloc_pipe_mcps(struct skl *skl,
				struct skl_module_cfg *mconfig)
{
	u8 res_idx = mconfig->res_idx;
	struct skl_module_res *res = &mconfig->module->resources[res_idx];

	skl->resource.mcps += res->cps;
}

/*
 * Free the mcps when tearing down
 */
static void
skl_tplg_free_pipe_mcps(struct skl *skl, struct skl_module_cfg *mconfig)
{
	u8 res_idx = mconfig->res_idx;
	struct skl_module_res *res = &mconfig->module->resources[res_idx];

	res = &mconfig->module->resources[res_idx];
	skl->resource.mcps -= res->cps;
}

/*
 * Free the memory when tearing down
 */
static void
skl_tplg_free_pipe_mem(struct skl *skl, struct skl_module_cfg *mconfig)
{
	skl->resource.mem -= mconfig->pipe->memory_pages;
}


static void skl_dump_mconfig(struct skl_sst *ctx,
					struct skl_module_cfg *mcfg)
{
	struct skl_module_iface *iface = &mcfg->module->formats[0];

	dev_dbg(ctx->dev, "Dumping config\n");
	dev_dbg(ctx->dev, "Input Format:\n");
	dev_dbg(ctx->dev, "channels = %d\n", iface->inputs[0].fmt.channels);
	dev_dbg(ctx->dev, "s_freq = %d\n", iface->inputs[0].fmt.s_freq);
	dev_dbg(ctx->dev, "ch_cfg = %d\n", iface->inputs[0].fmt.ch_cfg);
	dev_dbg(ctx->dev, "valid bit depth = %d\n",
				iface->inputs[0].fmt.valid_bit_depth);
	dev_dbg(ctx->dev, "Output Format:\n");
	dev_dbg(ctx->dev, "channels = %d\n", iface->outputs[0].fmt.channels);
	dev_dbg(ctx->dev, "s_freq = %d\n", iface->outputs[0].fmt.s_freq);
	dev_dbg(ctx->dev, "valid bit depth = %d\n",
				iface->outputs[0].fmt.valid_bit_depth);
	dev_dbg(ctx->dev, "ch_cfg = %d\n", iface->outputs[0].fmt.ch_cfg);
}

static void skl_tplg_update_chmap(struct skl_module_fmt *fmt, int chs)
{
	int slot_map = 0xFFFFFFFF;
	int start_slot = 0;
	int i;

	for (i = 0; i < chs; i++) {
		/*
		 * For 2 channels with starting slot as 0, slot map will
		 * look like 0xFFFFFF10.
		 */
		slot_map &= (~(0xF << (4 * i)) | (start_slot << (4 * i)));
		start_slot++;
	}
	fmt->ch_map = slot_map;
}

static void skl_tplg_update_params(struct skl_module_fmt *fmt,
			struct skl_pipe_params *params, int fixup)
{
	if (fixup & SKL_RATE_FIXUP_MASK)
		fmt->s_freq = params->s_freq;
	if (fixup & SKL_CH_FIXUP_MASK) {
		fmt->channels = params->ch;
		skl_tplg_update_chmap(fmt, fmt->channels);
		if (fmt->channels == 1)
			fmt->ch_cfg = SKL_CH_CFG_MONO;
		else if (fmt->channels == 2)
			fmt->ch_cfg = SKL_CH_CFG_STEREO;

	}
	if (fixup & SKL_FMT_FIXUP_MASK) {
		fmt->valid_bit_depth = skl_get_bit_depth(params->s_fmt);

		/*
		 * 16 bit is 16 bit container whereas 24 bit is in 32 bit
		 * container so update bit depth accordingly
		 */
		switch (fmt->valid_bit_depth) {
		case SKL_DEPTH_16BIT:
			fmt->bit_depth = fmt->valid_bit_depth;
			break;

		default:
			fmt->bit_depth = SKL_DEPTH_32BIT;
			break;
		}
	}

}

/*
 * A pipeline may have modules which impact the pcm parameters, like SRC,
 * channel converter, format converter.
 * We need to calculate the output params by applying the 'fixup'
 * Topology will tell driver which type of fixup is to be applied by
 * supplying the fixup mask, so based on that we calculate the output
 *
 * Now In FE the pcm hw_params is source/target format. Same is applicable
 * for BE with its hw_params invoked.
 * here based on FE, BE pipeline and direction we calculate the input and
 * outfix and then apply that for a module
 */
static void skl_tplg_update_params_fixup(struct skl_module_cfg *m_cfg,
		struct skl_pipe_params *params, bool is_fe)
{
	int in_fixup, out_fixup;
	struct skl_module_fmt *in_fmt, *out_fmt;

	/* Fixups will be applied to pin 0 only */
	in_fmt = &m_cfg->module->formats[0].inputs[0].fmt;
	out_fmt = &m_cfg->module->formats[0].outputs[0].fmt;

	if (params->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (is_fe) {
			in_fixup = m_cfg->params_fixup;
			out_fixup = (~m_cfg->converter) &
					m_cfg->params_fixup;
		} else {
			out_fixup = m_cfg->params_fixup;
			in_fixup = (~m_cfg->converter) &
					m_cfg->params_fixup;
		}
	} else {
		if (is_fe) {
			out_fixup = m_cfg->params_fixup;
			in_fixup = (~m_cfg->converter) &
					m_cfg->params_fixup;
		} else {
			in_fixup = m_cfg->params_fixup;
			out_fixup = (~m_cfg->converter) &
					m_cfg->params_fixup;
		}
	}

	skl_tplg_update_params(in_fmt, params, in_fixup);
	skl_tplg_update_params(out_fmt, params, out_fixup);
}

/*
 * A module needs input and output buffers, which are dependent upon pcm
 * params, so once we have calculate params, we need buffer calculation as
 * well.
 */
static void skl_tplg_update_buffer_size(struct skl_sst *ctx,
				struct skl_module_cfg *mcfg)
{
	int multiplier = 1;
	struct skl_module_fmt *in_fmt, *out_fmt;
	struct skl_module_res *res;

	/* Since fixups is applied to pin 0 only, ibs, obs needs
	 * change for pin 0 only
	 */
	res = &mcfg->module->resources[0];
	in_fmt = &mcfg->module->formats[0].inputs[0].fmt;
	out_fmt = &mcfg->module->formats[0].outputs[0].fmt;

	if (mcfg->m_type == SKL_MODULE_TYPE_SRCINT)
		multiplier = 5;

	res->ibs = DIV_ROUND_UP(in_fmt->s_freq, 1000) *
			in_fmt->channels * (in_fmt->bit_depth >> 3) *
			multiplier;

	res->obs = DIV_ROUND_UP(out_fmt->s_freq, 1000) *
			out_fmt->channels * (out_fmt->bit_depth >> 3) *
			multiplier;
}

static u8 skl_tplg_be_dev_type(int dev_type)
{
	int ret;

	switch (dev_type) {
	case SKL_DEVICE_BT:
		ret = NHLT_DEVICE_BT;
		break;

	case SKL_DEVICE_DMIC:
		ret = NHLT_DEVICE_DMIC;
		break;

	case SKL_DEVICE_I2S:
		ret = NHLT_DEVICE_I2S;
		break;

	default:
		ret = NHLT_DEVICE_INVALID;
		break;
	}

	return ret;
}

static int skl_tplg_update_be_blob(struct snd_soc_dapm_widget *w,
						struct skl_sst *ctx)
{
	struct skl_module_cfg *m_cfg = w->priv;
	int link_type, dir;
	u32 ch, s_freq, s_fmt;
	struct nhlt_specific_cfg *cfg;
	struct skl *skl = get_skl_ctx(ctx->dev);
	u8 dev_type = skl_tplg_be_dev_type(m_cfg->dev_type);
	int fmt_idx = m_cfg->fmt_idx;
	struct skl_module_iface *m_iface = &m_cfg->module->formats[fmt_idx];

	/* check if we already have blob */
	if (m_cfg->formats_config[SKL_PARAM_INIT].caps_size > 0)
		return 0;

	dev_dbg(ctx->dev, "Applying default cfg blob\n");
	switch (m_cfg->dev_type) {
	case SKL_DEVICE_DMIC:
		link_type = NHLT_LINK_DMIC;
		dir = SNDRV_PCM_STREAM_CAPTURE;
		s_freq = m_iface->inputs[0].fmt.s_freq;
		s_fmt = m_iface->inputs[0].fmt.bit_depth;
		ch = m_iface->inputs[0].fmt.channels;
		break;

	case SKL_DEVICE_I2S:
		link_type = NHLT_LINK_SSP;
		if (m_cfg->hw_conn_type == SKL_CONN_SOURCE) {
			dir = SNDRV_PCM_STREAM_PLAYBACK;
			s_freq = m_iface->outputs[0].fmt.s_freq;
			s_fmt = m_iface->outputs[0].fmt.bit_depth;
			ch = m_iface->outputs[0].fmt.channels;
		} else {
			dir = SNDRV_PCM_STREAM_CAPTURE;
			s_freq = m_iface->inputs[0].fmt.s_freq;
			s_fmt = m_iface->inputs[0].fmt.bit_depth;
			ch = m_iface->inputs[0].fmt.channels;
		}
		break;

	default:
		return -EINVAL;
	}

	/* update the blob based on virtual bus_id and default params */
	cfg = skl_get_ep_blob(skl, m_cfg->vbus_id, link_type,
					s_fmt, ch, s_freq, dir, dev_type);
	if (cfg) {
		m_cfg->formats_config[SKL_PARAM_INIT].caps_size = cfg->size;
		m_cfg->formats_config[SKL_PARAM_INIT].caps = (u32 *) &cfg->caps;
	} else {
		dev_err(ctx->dev, "Blob NULL for id %x type %d dirn %d\n",
					m_cfg->vbus_id, link_type, dir);
		dev_err(ctx->dev, "PCM: ch %d, freq %d, fmt %d\n",
					ch, s_freq, s_fmt);
		return -EIO;
	}

	return 0;
}

static void skl_tplg_update_module_params(struct snd_soc_dapm_widget *w,
							struct skl_sst *ctx)
{
	struct skl_module_cfg *m_cfg = w->priv;
	struct skl_pipe_params *params = m_cfg->pipe->p_params;
	int p_conn_type = m_cfg->pipe->conn_type;
	bool is_fe;

	if (!m_cfg->params_fixup)
		return;

	dev_dbg(ctx->dev, "Mconfig for widget=%s BEFORE updation\n",
				w->name);

	skl_dump_mconfig(ctx, m_cfg);

	if (p_conn_type == SKL_PIPE_CONN_TYPE_FE)
		is_fe = true;
	else
		is_fe = false;

	skl_tplg_update_params_fixup(m_cfg, params, is_fe);
	skl_tplg_update_buffer_size(ctx, m_cfg);

	dev_dbg(ctx->dev, "Mconfig for widget=%s AFTER updation\n",
				w->name);

	skl_dump_mconfig(ctx, m_cfg);
}

int skl_probe_get_index(struct snd_soc_dai *dai,
				struct skl_probe_config *pconfig)
{
	int i, ret = -1;
	char pos[4];

	for (i = 0; i < pconfig->no_injector; i++) {
		snprintf(pos, 4, "%d", i);
		if (strstr(dai->name, pos))
			return i;
	}
	return ret;
}

int skl_probe_attach_inj_dma(struct snd_soc_dapm_widget *w,
					struct skl_sst *ctx, int index)
{
	int ret = -EINVAL;

	struct skl_module_cfg *mconfig = w->priv;
	struct skl_probe_attach_inj_dma ad;
	struct skl_probe_config *pconfig = &ctx->probe_config;

	if (pconfig->iprobe[index].state == SKL_PROBE_STATE_INJ_NONE) {
		dev_dbg(ctx->dev, "Attaching injector DMA\n");
		ad.node_id.node.vindex = pconfig->iprobe[index].dma_id;
		ad.node_id.node.dma_type = SKL_DMA_HDA_HOST_OUTPUT_CLASS;
		ad.node_id.node.rsvd = 0;
		ad.dma_buff_size = pconfig->edma_buffsize;

		ret = skl_set_module_params(ctx, (void *)&ad,
					sizeof(struct skl_probe_attach_inj_dma),
					SKL_PROBE_INJECT_DMA_ATTACH, mconfig);
		if (ret < 0)
			return -EINVAL;

		pconfig->iprobe[index].state = SKL_PROBE_STATE_INJ_DMA_ATTACHED;
		dev_dbg(ctx->dev, "iprobe[%d].state %d\n", index,
					pconfig->iprobe[index].state);
	}

	return ret;

}

int skl_probe_detach_inj_dma(struct skl_sst *ctx, struct snd_soc_dapm_widget *w,
								int index)
{
	struct skl_module_cfg *mconfig = w->priv;
	struct skl_probe_config *pconfig = &ctx->probe_config;
	struct skl_ipc_large_config_msg msg;
	union skl_connector_node_id node_id;
	int ret = -EINVAL;

	if (pconfig->iprobe[index].state == SKL_PROBE_STATE_INJ_DISCONNECTED) {
		dev_dbg(ctx->dev, "Detaching injector DMA\n");
		node_id.node.vindex = pconfig->iprobe[index].dma_id;
		node_id.node.dma_type = SKL_DMA_HDA_HOST_OUTPUT_CLASS;
		node_id.node.rsvd = 0;

		msg.module_id = mconfig->id.module_id;
		msg.instance_id = mconfig->id.instance_id;
		msg.large_param_id = SKL_PROBE_INJECT_DMA_DETACH;
		msg.param_data_size = sizeof(union skl_connector_node_id);

		dev_dbg(ctx->dev, "setting module params size=%d\n",
						msg.param_data_size);
		ret = skl_ipc_set_large_config(&ctx->ipc, &msg,
						(u32 *)&node_id);
		if (ret < 0)
			return -EINVAL;

		pconfig->iprobe[index].state = SKL_PROBE_STATE_INJ_NONE;
		dev_dbg(ctx->dev, "iprobe[%d].state %d\n", index,
					pconfig->iprobe[index].state);
	}
	return ret;
}


int skl_probe_point_set_config(struct snd_soc_dapm_widget *w,
					struct skl_sst *ctx, int direction,
					struct snd_soc_dai *dai)
{
	int i, ret = -EIO, n = 0;
	struct skl_module_cfg *mconfig = w->priv;
	const struct snd_kcontrol_new *k;
	struct skl_probe_config *pconfig = &ctx->probe_config;
	struct probe_pt_param prb_pt_param[8] = {{0}};
	int store_prb_pt_index[8] = {0};

	if (direction == SND_COMPRESS_PLAYBACK) {

		/* only one injector point can be set at a time*/
		n = skl_probe_get_index(dai, pconfig);
		if (n < 0)
			return -EINVAL;

		k = &w->kcontrol_news[pconfig->no_extractor + n];
		dev_dbg(dai->dev, "operation = %d, purpose = %d, probe_point_id = %d\n",
		pconfig->iprobe[n].operation, pconfig->iprobe[n].purpose,
					pconfig->iprobe[n].probe_point_id);

		if ((k->access & SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK)
			&& (pconfig->iprobe[n].state ==
				SKL_PROBE_STATE_INJ_DMA_ATTACHED)
			&& (pconfig->iprobe[n].operation ==
						SKL_PROBE_CONNECT)
			&& (pconfig->iprobe[n].purpose ==
						SKL_PROBE_INJECT ||
			pconfig->iprobe[n].purpose ==
					SKL_PROBE_INJECT_REEXTRACT)) {

			prb_pt_param[0].params =
					pconfig->iprobe[n].probe_point_id;
			prb_pt_param[0].connection = pconfig->iprobe[n].purpose;
			prb_pt_param[0].node_id =  pconfig->iprobe[n].dma_id;
			ret = skl_set_module_params(ctx, (void *)prb_pt_param,
				sizeof(struct probe_pt_param),
				SKL_PROBE_CONNECT, mconfig);
			if (ret < 0) {
				dev_dbg(dai->dev, "failed to set injector probe point\n");
				return -EINVAL;
			}

			pconfig->iprobe[n].state =
					SKL_PROBE_STATE_INJ_CONNECTED;
			dev_dbg(dai->dev, "iprobe[%d].state %d\n", n,
						pconfig->iprobe[n].state);
		}

	} else if (direction == SND_COMPRESS_CAPTURE) {

		/*multiple extractor points can be set simultaneously*/
		for (i = 0; i < pconfig->no_extractor; i++) {
			k = &w->kcontrol_news[i];
			dev_dbg(dai->dev, "operation = %d, purpose = %d, probe_point_id = %d\n",
					pconfig->eprobe[i].operation,
					pconfig->eprobe[i].purpose,
					pconfig->eprobe[i].probe_point_id);
			if ((k->access & SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK)
				&& (pconfig->eprobe[i].state ==
						SKL_PROBE_STATE_EXT_NONE)
				&& (pconfig->eprobe[i].operation ==
						SKL_PROBE_CONNECT)
				&& (pconfig->eprobe[i].purpose ==
						SKL_PROBE_EXTRACT ||
				pconfig->eprobe[i].purpose ==
						SKL_PROBE_INJECT_REEXTRACT)) {

				dev_dbg(dai->dev, "Retrieving the exractor params\n");
				prb_pt_param[n].params =
					pconfig->eprobe[i].probe_point_id;
				prb_pt_param[n].connection =
					pconfig->eprobe[i].purpose;
				prb_pt_param[n].node_id = -1;
				store_prb_pt_index[i] = 1;
				n++;
			}
		}

		if (n > 0) {
			ret = skl_set_module_params(ctx, (void *)prb_pt_param, n * sizeof(struct probe_pt_param),
						SKL_PROBE_CONNECT, mconfig);

			if (ret < 0) {
				dev_dbg(dai->dev, "failed to set extractor probe point\n");
				return -EINVAL;
			}
		}

		for (i = 0; i < pconfig->no_extractor; i++) {
			if (store_prb_pt_index[i]) {
				pconfig->eprobe[i].state =
					SKL_PROBE_STATE_EXT_CONNECTED;
				dev_dbg(dai->dev, "eprobe[%d].state %d\n",
						n, pconfig->eprobe[i].state);
			}
		}

	}
	return ret;
}

/*
 * some modules can have multiple params set from user control and
 * need to be set after module is initialized. If set_param flag is
 * set module params will be done after module is initialised.
 */
int skl_tplg_set_module_params(struct snd_soc_dapm_widget *w,
						struct skl_sst *ctx)
{
	int i, ret;
	struct skl_module_cfg *mconfig = w->priv;
	const struct snd_kcontrol_new *k;
	struct soc_bytes_ext *sb;
	struct skl_algo_data *bc;
	struct skl_specific_cfg *sp_cfg;

	if (mconfig->formats_config[SKL_PARAM_SET].caps_size > 0 &&
		mconfig->formats_config[SKL_PARAM_SET].set_params ==
							SKL_PARAM_SET) {
		sp_cfg = &mconfig->formats_config[SKL_PARAM_SET];
		ret = skl_set_module_params(ctx, sp_cfg->caps,
					sp_cfg->caps_size,
					sp_cfg->param_id, mconfig);
		if (ret < 0)
			return ret;
	}

	for (i = 0; i < w->num_kcontrols; i++) {
		k = &w->kcontrol_news[i];
		if (k->access & SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK) {
			sb = (void *) k->private_value;
			bc = (struct skl_algo_data *)sb->dobj.private;

			if (bc->set_params == SKL_PARAM_SET) {
				ret = skl_set_module_params(ctx,
						(u32 *)bc->params, bc->size,
						bc->param_id, mconfig);
				if (ret < 0)
					return ret;
			}
		}
	}

	return 0;
}

/*
 * some module param can set from user control and this is required as
 * when module is initailzed. if module param is required in init it is
 * identifed by set_param flag. if set_param flag is not set, then this
 * parameter needs to set as part of module init.
 */
static int skl_tplg_set_module_init_data(struct snd_soc_dapm_widget *w)
{
	const struct snd_kcontrol_new *k;
	struct soc_bytes_ext *sb;
	struct skl_algo_data *bc;
	struct skl_module_cfg *mconfig = w->priv;
	int i;

	for (i = 0; i < w->num_kcontrols; i++) {
		k = &w->kcontrol_news[i];
		if (k->access & SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK) {
			sb = (struct soc_bytes_ext *)k->private_value;
			bc = (struct skl_algo_data *)sb->dobj.private;

			if (bc->set_params != SKL_PARAM_INIT)
				continue;

			mconfig->formats_config[SKL_PARAM_INIT].caps =
							(u32 *)bc->params;
			mconfig->formats_config[SKL_PARAM_INIT].caps_size =
								bc->size;

			break;
		}
	}

	return 0;
}

static int skl_tplg_module_prepare(struct skl_sst *ctx, struct skl_pipe *pipe,
		struct snd_soc_dapm_widget *w, struct skl_module_cfg *mcfg)
{
	switch (mcfg->dev_type) {
	case SKL_DEVICE_HDAHOST:
		return skl_pcm_host_dma_prepare(ctx->dev, pipe->p_params);

	case SKL_DEVICE_HDALINK:
		return skl_pcm_link_dma_prepare(ctx->dev, pipe->p_params);
	}

	return 0;
}

/*
 * Inside a pipe instance, we can have various modules. These modules need
 * to instantiated in DSP by invoking INIT_MODULE IPC, which is achieved by
 * skl_init_module() routine, so invoke that for all modules in a pipeline
 */
static int
skl_tplg_init_pipe_modules(struct skl *skl, struct skl_pipe *pipe)
{
	struct skl_pipe_module *w_module;
	struct snd_soc_dapm_widget *w;
	struct skl_module_cfg *mconfig;
	struct skl_sst *ctx = skl->skl_sst;
	u8 cfg_idx;
	int ret = 0;

	list_for_each_entry(w_module, &pipe->w_list, node) {
		uuid_le *uuid_mod;
		w = w_module->w;
		mconfig = w->priv;

		/* check if module ids are populated */
		if (mconfig->id.module_id < 0) {
			dev_err(skl->skl_sst->dev,
					"module %pUL id not populated\n",
					(uuid_le *)mconfig->guid);
			return -EIO;
		}

		cfg_idx = mconfig->pipe->cur_config_idx;
		mconfig->fmt_idx = mconfig->mod_cfg[cfg_idx].fmt_idx;
		mconfig->res_idx = mconfig->mod_cfg[cfg_idx].res_idx;

		/* check resource available */
		if (!skl_is_pipe_mcps_avail(skl, mconfig))
			return -ENOMEM;

		if (mconfig->module->loadable && ctx->dsp->fw_ops.load_mod) {
			ret = ctx->dsp->fw_ops.load_mod(ctx->dsp,
				mconfig->id.module_id, mconfig->guid);
			if (ret < 0)
				return ret;

			mconfig->m_state = SKL_MODULE_LOADED;
		}

		/* prepare the DMA if the module is gateway cpr */
		ret = skl_tplg_module_prepare(ctx, pipe, w, mconfig);
		if (ret < 0)
			return ret;

		/* update blob if blob is null for be with default value */
		skl_tplg_update_be_blob(w, ctx);

		/*
		 * apply fix/conversion to module params based on
		 * FE/BE params
		 */
		skl_tplg_update_module_params(w, ctx);
		uuid_mod = (uuid_le *)mconfig->guid;
		mconfig->id.pvt_id = skl_get_pvt_id(ctx, uuid_mod,
						mconfig->id.instance_id);
		if (mconfig->id.pvt_id < 0)
			return ret;
		skl_tplg_set_module_init_data(w);

		ret = skl_dsp_get_core(ctx->dsp, mconfig->core_id);
		if (ret < 0) {
			dev_err(ctx->dev, "Failed to wake up core %d ret=%d\n",
						mconfig->core_id, ret);
			return ret;
		}

		ret = skl_init_module(ctx, mconfig);
		if (ret < 0) {
			skl_put_pvt_id(ctx, uuid_mod, &mconfig->id.pvt_id);
			goto err;
		}
		skl_tplg_alloc_pipe_mcps(skl, mconfig);
		ret = skl_tplg_set_module_params(w, ctx);
		if (ret < 0)
			goto err;
	}

	return 0;
err:
	skl_dsp_put_core(ctx->dsp, mconfig->core_id);
	return ret;
}

static int skl_tplg_unload_pipe_modules(struct skl_sst *ctx,
	 struct skl_pipe *pipe)
{
	int ret = 0;
	struct skl_pipe_module *w_module = NULL;
	struct skl_module_cfg *mconfig = NULL;

	list_for_each_entry(w_module, &pipe->w_list, node) {
		uuid_le *uuid_mod;
		mconfig  = w_module->w->priv;
		uuid_mod = (uuid_le *)mconfig->guid;

		if (mconfig->module->loadable && ctx->dsp->fw_ops.unload_mod &&
			mconfig->m_state > SKL_MODULE_UNINIT) {
			ret = ctx->dsp->fw_ops.unload_mod(ctx->dsp,
						mconfig->id.module_id);
			if (ret < 0)
				return -EIO;
		}
		skl_put_pvt_id(ctx, uuid_mod, &mconfig->id.pvt_id);

		ret = skl_dsp_put_core(ctx->dsp, mconfig->core_id);
		if (ret < 0) {
			/* don't return; continue with other modules */
			dev_err(ctx->dev, "Failed to sleep core %d ret=%d\n",
				mconfig->core_id, ret);
		}
	}

	/* no modules to unload in this path, so return */
	return ret;
}

static bool is_skl_tplg_multi_fmt(struct skl *skl, struct skl_pipe *pipe)
{
	int i;
	struct skl_pipe_fmt *cur_fmt;
	struct skl_pipe_fmt *next_fmt;

	if (pipe->conn_type == SKL_PIPE_CONN_TYPE_FE &&
			pipe->nr_cfgs > 1) {
		for (i = 0; i < pipe->nr_cfgs-1; i++) {
			if (pipe->direction == SNDRV_PCM_STREAM_PLAYBACK) {
				cur_fmt = &pipe->configs[i].out_fmt;
				next_fmt = &pipe->configs[i+1].out_fmt;
			} else {
				cur_fmt = &pipe->configs[i].in_fmt;
				next_fmt = &pipe->configs[i+1].in_fmt;
			}
			if (!CHECK_HW_PARAMS(cur_fmt->channels, cur_fmt->freq,
						cur_fmt->bps,
						next_fmt->channels,
						next_fmt->freq,	next_fmt->bps))
				return true;
		}
	} else if (pipe->nr_cfgs > 1) {
		return true;
	}

	return false;
}

/*
 * Here, we select pipe format based on the pipe type and pipe
 * direction to determine the current config index for the pipeline.
 * The config index is then used to select proper module resources.
 * Intermediate pipes currently have a fixed format hence we select the
 * 0th configuratation by default for such pipes.
 */
static int
skl_tplg_get_pipe_config(struct skl *skl, struct skl_module_cfg *mconfig)
{
	struct skl_sst *ctx = skl->skl_sst;
	struct skl_pipe *pipe = mconfig->pipe;
	struct skl_pipe_params *params = pipe->p_params;
	struct skl_path_config *pconfig = &pipe->configs[0];
	struct skl_pipe_fmt *fmt = NULL;
	bool in_fmt = false;
	int i;
	bool ret;

	if (pipe->nr_cfgs == 0) {
		pipe->cur_config_idx = 0;
		return 0;
	}

	ret = is_skl_tplg_multi_fmt(skl, pipe);
	if (ret) {
		pipe->cur_config_idx = pipe->pipe_config_idx;
		pipe->memory_pages = pconfig->mem_pages;
		dev_dbg(ctx->dev, "found pipe config idx:%d\n",
				pipe->cur_config_idx);
		return 0;
	}
	if (pipe->conn_type == SKL_PIPE_CONN_TYPE_NONE) {
		dev_dbg(ctx->dev, "No conn_type detected, take 0th config\n");
		pipe->cur_config_idx = 0;
		pipe->memory_pages = pconfig->mem_pages;

		return 0;
	}

	if ((pipe->conn_type == SKL_PIPE_CONN_TYPE_FE &&
	     pipe->direction == SNDRV_PCM_STREAM_PLAYBACK) ||
	     (pipe->conn_type == SKL_PIPE_CONN_TYPE_BE &&
	     pipe->direction == SNDRV_PCM_STREAM_CAPTURE))
		in_fmt = true;

	for (i = 0; i < pipe->nr_cfgs; i++) {
		pconfig = &pipe->configs[i];
		if (in_fmt)
			fmt = &pconfig->in_fmt;
		else
			fmt = &pconfig->out_fmt;

		if (CHECK_HW_PARAMS(params->ch, params->s_freq, params->s_fmt,
				    fmt->channels, fmt->freq, fmt->bps)) {
			pipe->cur_config_idx = i;
			pipe->memory_pages = pconfig->mem_pages;
			dev_dbg(ctx->dev, "Using pipe config: %d\n", i);

			return 0;
		}
	}

	dev_err(ctx->dev, "Invalid pipe config: %d %d %d for pipe: %d\n",
		params->ch, params->s_freq, params->s_fmt, pipe->ppl_id);
	return -EINVAL;
}

/*
 * Mixer module represents a pipeline. So in the Pre-PMU event of mixer we
 * need create the pipeline. So we do following:
 *   - check the resources
 *   - Create the pipeline
 *   - Initialize the modules in pipeline
 *   - finally bind all modules together
 */
static int skl_tplg_mixer_dapm_pre_pmu_event(struct snd_soc_dapm_widget *w,
							struct skl *skl)
{
	int ret;
	struct skl_module_cfg *mconfig = w->priv;
	struct skl_pipe_module *w_module;
	struct skl_pipe *s_pipe = mconfig->pipe;
	struct skl_module_cfg *src_module = NULL, *dst_module, *module;
	struct skl_sst *ctx = skl->skl_sst;
	struct skl_module_deferred_bind *modules;

	if (mconfig->pipe->state >= SKL_PIPE_CREATED)
		return 0;

	/*
	 * This will check for single module in source pipeline. If single
	 * module pipeline  exists then its going to create source pipeline
	 * first. This will handle/satisfy source-to-sink pipeline creation
	 * scenario for single module in any stream
	 */
	skl_init_single_module_pipe(w, skl);

	ret = skl_tplg_get_pipe_config(skl, mconfig);
	if (ret < 0)
		return ret;

	/* check resource available */
	if (!skl_is_pipe_mcps_avail(skl, mconfig))
		return -EBUSY;

	if (!skl_is_pipe_mem_avail(skl, mconfig))
		return -ENOMEM;

	/*
	 * Create a list of modules for pipe.
	 * This list contains modules from source to sink
	 */
	ret = skl_create_pipeline(ctx, mconfig->pipe);
	if (ret < 0)
		return ret;

	skl_tplg_alloc_pipe_mem(skl, mconfig);
	skl_tplg_alloc_pipe_mcps(skl, mconfig);

	/* Init all pipe modules from source to sink */
	ret = skl_tplg_init_pipe_modules(skl, s_pipe);
	if (ret < 0)
		return ret;

	/* Bind modules from source to sink */
	list_for_each_entry(w_module, &s_pipe->w_list, node) {
		dst_module = w_module->w->priv;

		if (src_module == NULL) {
			src_module = dst_module;
			continue;
		}

		ret = skl_bind_modules(ctx, src_module, dst_module);
		if (ret < 0)
			return ret;

		src_module = dst_module;
	}

	/*
	 * When the destination module is initialized, check for these modules
	 * in deferred bind list. If found, bind them.
	 */
	list_for_each_entry(w_module, &s_pipe->w_list, node) {
		if (list_empty(&skl->bind_list))
			break;

		list_for_each_entry(modules, &skl->bind_list, node) {
			module = w_module->w->priv;
			if (modules->dst == module)
				skl_bind_modules(ctx, modules->src,
							modules->dst);
		}
	}

	return 0;
}

/*
 * This function returns pipe order in given stream
 */
static int skl_get_pipe_order(struct skl_module_cfg *mcfg)
{
	struct skl_pipe *pipe = mcfg->pipe;

	switch (pipe->conn_type) {
	case SKL_PIPE_CONN_TYPE_FE:
		if (pipe->direction == SNDRV_PCM_STREAM_CAPTURE)
			return SKL_LAST_PIPE;
		else if (pipe->direction == SNDRV_PCM_STREAM_PLAYBACK)
			return SKL_FIRST_PIPE;
		break;
	case SKL_PIPE_CONN_TYPE_BE:
		if (pipe->direction == SNDRV_PCM_STREAM_PLAYBACK)
			return SKL_LAST_PIPE;
		else if (pipe->direction == SNDRV_PCM_STREAM_CAPTURE)
			return SKL_FIRST_PIPE;
		break;
	}
	return SKL_INTERMEDIATE_PIPE;
}

/*
 * This function checks for single module source pipeline. If found any then
 * it will initialize source pipeline and its module
 */
static void skl_init_single_module_pipe(struct snd_soc_dapm_widget *w,
							struct skl *skl)
{
	struct snd_soc_dapm_path *p;
	struct snd_soc_dapm_widget *src_w = NULL;
	struct skl_module_cfg *mcfg;

	snd_soc_dapm_widget_for_each_source_path(w, p) {
		src_w = p->source;

		if ((src_w->priv != NULL) && is_skl_dsp_widget_type(src_w, skl->skl_sst->dev)) {
			mcfg = src_w->priv;
			if ((list_is_singular(&mcfg->pipe->w_list)) &&
						(src_w->power_check(src_w)))
				skl_tplg_mixer_dapm_pre_pmu_event(src_w, skl);
		}
		skl_init_single_module_pipe(src_w, skl);
	}
}

static int skl_fill_sink_instance_id(struct skl_sst *ctx, u32 *params,
				int size, struct skl_module_cfg *mcfg)
{
	int i, pvt_id;

	if (mcfg->m_type == SKL_MODULE_TYPE_KPB) {
		struct skl_kpb_params *kpb_params =
				(struct skl_kpb_params *)params;
		struct skl_mod_inst_map *inst = kpb_params->u.map;

		for (i = 0; i < kpb_params->num_modules; i++) {
			pvt_id = skl_get_pvt_instance_id_map(ctx, inst->mod_id,
								inst->inst_id);
			if (pvt_id < 0)
				return -EINVAL;

			inst->inst_id = pvt_id;
			inst++;
		}
	}

	return 0;
}
/*
 * Some modules require params to be set after the module is bound to
 * all pins connected.
 *
 * The module provider initializes set_param flag for such modules and we
 * send params after binding
 */
static int skl_tplg_set_module_bind_params(struct snd_soc_dapm_widget *w,
			struct skl_module_cfg *mcfg, struct skl_sst *ctx)
{
	int i, ret;
	struct skl_module_cfg *mconfig = w->priv;
	const struct snd_kcontrol_new *k;
	struct soc_bytes_ext *sb;
	struct skl_algo_data *bc;
	struct skl_specific_cfg *sp_cfg;
	u32 *params;

	/*
	 * check all out/in pins are in bind state.
	 * if so set the module param
	 */
	for (i = 0; i < mcfg->module->max_output_pins; i++) {
		if (mcfg->m_out_pin[i].pin_state != SKL_PIN_BIND_DONE)
			return 0;
	}

	for (i = 0; i < mcfg->module->max_input_pins; i++) {
		if (mcfg->m_in_pin[i].pin_state != SKL_PIN_BIND_DONE)
			return 0;
	}

	if (mconfig->formats_config[SKL_PARAM_BIND].caps_size > 0 &&
		mconfig->formats_config[SKL_PARAM_BIND].set_params ==
							SKL_PARAM_BIND) {
		sp_cfg = &mconfig->formats_config[SKL_PARAM_BIND];
		ret = skl_set_module_params(ctx, sp_cfg->caps,
					sp_cfg->caps_size,
					sp_cfg->param_id, mconfig);
		if (ret < 0)
			return ret;
	}

	for (i = 0; i < w->num_kcontrols; i++) {
		k = &w->kcontrol_news[i];
		if (k->access & SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK) {
			sb = (void *) k->private_value;
			bc = (struct skl_algo_data *)sb->dobj.private;

			if (bc->set_params == SKL_PARAM_BIND) {
				params = kzalloc(bc->max, GFP_KERNEL);
				if (!params)
					return -ENOMEM;

				memcpy(params, bc->params, bc->max);
				skl_fill_sink_instance_id(ctx, params, bc->max,
								mconfig);

				ret = skl_set_module_params(ctx, params,
						bc->max, bc->param_id, mconfig);
				kfree(params);

				if (ret < 0)
					return ret;
			}
		}
	}

	return 0;
}

static int skl_tplg_find_moduleid_from_uuid(struct skl *skl,
					const struct snd_kcontrol_new *k)
{
	struct soc_bytes_ext *sb = (void *) k->private_value;
	struct skl_algo_data *bc = (struct skl_algo_data *)sb->dobj.private;
	struct skl_kpb_params *uuid_params, *params;
	struct hdac_bus *bus = ebus_to_hbus(skl_to_ebus(skl));
	int i, size, module_id;

	if (bc->set_params == SKL_PARAM_BIND && bc->max) {
		uuid_params = (struct skl_kpb_params *)bc->params;
		size = uuid_params->num_modules *
			sizeof(struct skl_mod_inst_map) +
			sizeof(uuid_params->num_modules);

		params = devm_kzalloc(bus->dev, size, GFP_KERNEL);
		if (!params)
			return -ENOMEM;

		params->num_modules = uuid_params->num_modules;

		for (i = 0; i < uuid_params->num_modules; i++) {
			module_id = skl_get_module_id(skl->skl_sst,
				&uuid_params->u.map_uuid[i].mod_uuid);
			if (module_id < 0) {
				devm_kfree(bus->dev, params);
				return -EINVAL;
			}

			params->u.map[i].mod_id = module_id;
			params->u.map[i].inst_id =
				uuid_params->u.map_uuid[i].inst_id;
		}

		devm_kfree(bus->dev, bc->params);
		bc->params = (char *)params;
		bc->max = size;
	}

	return 0;
}

/*
 * Retrieve the module id from UUID mentioned in the
 * post bind params
 */
void skl_tplg_add_moduleid_in_bind_params(struct skl *skl,
				struct snd_soc_dapm_widget *w)
{
	struct skl_module_cfg *mconfig = w->priv;
	int i;

	/*
	 * Post bind params are used for only for KPB
	 * to set copier instances to drain the data
	 * in fast mode
	 */
	if (mconfig->m_type != SKL_MODULE_TYPE_KPB)
		return;

	for (i = 0; i < w->num_kcontrols; i++)
		if ((w->kcontrol_news[i].access &
			SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK) &&
			(skl_tplg_find_moduleid_from_uuid(skl,
			&w->kcontrol_news[i]) < 0))
			dev_err(skl->skl_sst->dev,
				"%s: invalid kpb post bind params\n",
				__func__);
}

static int skl_tplg_module_add_deferred_bind(struct skl *skl,
	struct skl_module_cfg *src, struct skl_module_cfg *dst)
{
	struct skl_module_deferred_bind *m_list, *modules;
	int i;

	/* only supported for module with static pin connection */
	for (i = 0; i < dst->module->max_input_pins; i++) {
		struct skl_module_pin *pin = &dst->m_in_pin[i];

		if (pin->is_dynamic)
			continue;

		if ((pin->id.module_id  == src->id.module_id) &&
			(pin->id.instance_id  == src->id.instance_id)) {

			if (!list_empty(&skl->bind_list)) {
				list_for_each_entry(modules, &skl->bind_list, node) {
					if (modules->src == src && modules->dst == dst)
						return 0;
				}
			}

			m_list = kzalloc(sizeof(*m_list), GFP_KERNEL);
			if (!m_list)
				return -ENOMEM;

			m_list->src = src;
			m_list->dst = dst;

			list_add(&m_list->node, &skl->bind_list);
		}
	}

	return 0;
}

static int skl_tplg_bind_sinks(struct snd_soc_dapm_widget *w,
				struct skl *skl,
				struct snd_soc_dapm_widget *src_w,
				struct skl_module_cfg *src_mconfig)
{
	struct snd_soc_dapm_path *p;
	struct snd_soc_dapm_widget *sink = NULL, *next_sink = NULL;
	struct skl_module_cfg *sink_mconfig;
	struct skl_sst *ctx = skl->skl_sst;
	int ret;

	snd_soc_dapm_widget_for_each_sink_path(w, p) {
		if (!p->connect)
			continue;

		dev_dbg(ctx->dev, "%s: src widget=%s\n", __func__, w->name);
		dev_dbg(ctx->dev, "%s: sink widget=%s\n", __func__, p->sink->name);

		next_sink = p->sink;

		if (!is_skl_dsp_widget_type(p->sink, ctx->dev))
			return skl_tplg_bind_sinks(p->sink, skl, src_w, src_mconfig);

		/*
		 * here we will check widgets in sink pipelines, so that
		 * can be any widgets type and we are only interested if
		 * they are ones used for SKL so check that first
		 */
		if ((p->sink->priv != NULL) &&
				is_skl_dsp_widget_type(p->sink, ctx->dev)) {

			sink = p->sink;
			sink_mconfig = sink->priv;

			/*
			 * Modules other than PGA leaf can be connected
			 * directly or via switch to a module in another
			 * pipeline. EX: reference path
			 * when the path is enabled, the dst module that needs
			 * to be bound may not be initialized. if the module is
			 * not initialized, add these modules in the deferred
			 * bind list and when the dst module is initialised,
			 * bind this module to the dst_module in deferred list.
			 */
			if (((src_mconfig->m_state == SKL_MODULE_INIT_DONE)
				&& (sink_mconfig->m_state == SKL_MODULE_UNINIT))) {

				ret = skl_tplg_module_add_deferred_bind(skl,
						src_mconfig, sink_mconfig);

				if (ret < 0)
					return ret;

			}


			if (src_mconfig->m_state == SKL_MODULE_UNINIT ||
				sink_mconfig->m_state == SKL_MODULE_UNINIT)
				continue;

			/* Bind source to sink, mixin is always source */
			ret = skl_bind_modules(ctx, src_mconfig, sink_mconfig);
			if (ret)
				return ret;

			/* set module params after bind */
			skl_tplg_set_module_bind_params(src_w, src_mconfig, ctx);
			skl_tplg_set_module_bind_params(sink, sink_mconfig, ctx);

			/* Start sinks pipe first */
			if (sink_mconfig->pipe->state != SKL_PIPE_STARTED) {
				if (sink_mconfig->pipe->conn_type !=
							SKL_PIPE_CONN_TYPE_FE)
					ret = skl_run_pipe(ctx,
							sink_mconfig->pipe);
				if (ret)
					return ret;
			}
		}
	}

	/* NOTE: While selecting the multiple pipeline confguration next_sink
	 * become NULL. So sink and next_sink checking is required for muliple
	 * pipeline configuration.
	 */
	if ((!sink) && (next_sink))
		return skl_tplg_bind_sinks(next_sink, skl, src_w, src_mconfig);

	return 0;
}

/*
 * A PGA represents a module in a pipeline. So in the Pre-PMU event of PGA
 * we need to do following:
 *   - Bind to sink pipeline
 *      Since the sink pipes can be running and we don't get mixer event on
 *      connect for already running mixer, we need to find the sink pipes
 *      here and bind to them. This way dynamic connect works.
 *   - Start sink pipeline, if not running
 *   - Then run current pipe
 */
static int skl_tplg_pga_dapm_pre_pmu_event(struct snd_soc_dapm_widget *w,
								struct skl *skl)
{
	struct skl_module_cfg *src_mconfig;
	struct skl_sst *ctx = skl->skl_sst;
	int ret = 0;

	src_mconfig = w->priv;

	/*
	 * find which sink it is connected to, bind with the sink,
	 * if sink is not started, start sink pipe first, then start
	 * this pipe
	 */
	ret = skl_tplg_bind_sinks(w, skl, w, src_mconfig);
	if (ret)
		return ret;

	/* Start source pipe last after starting all sinks */
	if (src_mconfig->pipe->conn_type != SKL_PIPE_CONN_TYPE_FE)
		return skl_run_pipe(ctx, src_mconfig->pipe);

	return 0;
}

static struct snd_soc_dapm_widget *skl_get_src_dsp_widget(
		struct snd_soc_dapm_widget *w, struct skl *skl)
{
	struct snd_soc_dapm_path *p;
	struct snd_soc_dapm_widget *src_w = NULL;
	struct skl_sst *ctx = skl->skl_sst;

	snd_soc_dapm_widget_for_each_source_path(w, p) {
		src_w = p->source;
		if (!p->connect)
			continue;

		dev_dbg(ctx->dev, "sink widget=%s\n", w->name);
		dev_dbg(ctx->dev, "src widget=%s\n", p->source->name);

		/*
		 * here we will check widgets in sink pipelines, so that can
		 * be any widgets type and we are only interested if they are
		 * ones used for SKL so check that first
		 */
		if ((p->source->priv != NULL) &&
				is_skl_dsp_widget_type(p->source, ctx->dev)) {
			return p->source;
		}
	}

	if (src_w != NULL)
		return skl_get_src_dsp_widget(src_w, skl);

	return NULL;
}

/*
 * in the Post-PMU event of mixer we need to do following:
 *   - Check if this pipe is running
 *   - if not, then
 *	- bind this pipeline to its source pipeline
 *	  if source pipe is already running, this means it is a dynamic
 *	  connection and we need to bind only to that pipe
 *	- start this pipeline
 */
static int skl_tplg_mixer_dapm_post_pmu_event(struct snd_soc_dapm_widget *w,
							struct skl *skl)
{
	int ret = 0;
	struct snd_soc_dapm_widget *source, *sink;
	struct skl_module_cfg *src_mconfig, *sink_mconfig;
	struct skl_sst *ctx = skl->skl_sst;
	int src_pipe_started = 0;

	sink = w;
	sink_mconfig = sink->priv;

	/*
	 * If source pipe is already started, that means source is driving
	 * one more sink before this sink got connected, Since source is
	 * started, bind this sink to source and start this pipe.
	 */
	source = skl_get_src_dsp_widget(w, skl);
	if (source != NULL) {
		src_mconfig = source->priv;
		sink_mconfig = sink->priv;
		src_pipe_started = 1;

		/*
		 * check pipe state, then no need to bind or start the
		 * pipe
		 */
		if (src_mconfig->pipe->state != SKL_PIPE_STARTED)
			src_pipe_started = 0;
	}

	if (src_pipe_started) {
		ret = skl_bind_modules(ctx, src_mconfig, sink_mconfig);
		if (ret)
			return ret;

		/* set module params after bind */
		skl_tplg_set_module_bind_params(source, src_mconfig, ctx);
		skl_tplg_set_module_bind_params(sink, sink_mconfig, ctx);

		if (sink_mconfig->pipe->conn_type != SKL_PIPE_CONN_TYPE_FE)
			ret = skl_run_pipe(ctx, sink_mconfig->pipe);
	}

	return ret;
}

/*
 * in the Pre-PMD event of mixer we need to do following:
 *   - Stop the pipe
 *   - find the source connections and remove that from dapm_path_list
 *   - unbind with source pipelines if still connected
 */
static int skl_tplg_mixer_dapm_pre_pmd_event(struct snd_soc_dapm_widget *w,
							struct skl *skl)
{
	struct skl_module_cfg *src_mconfig, *sink_mconfig;
	int ret = 0, i;
	struct skl_sst *ctx = skl->skl_sst;

	sink_mconfig = w->priv;

	/* Stop the pipe */
	ret = skl_stop_pipe(ctx, sink_mconfig->pipe);
	if (ret)
		return ret;

	for (i = 0; i < sink_mconfig->module->max_input_pins; i++) {
		if (sink_mconfig->m_in_pin[i].pin_state == SKL_PIN_BIND_DONE) {
			src_mconfig = sink_mconfig->m_in_pin[i].tgt_mcfg;
			if (!src_mconfig)
				continue;

			ret = skl_unbind_modules(ctx,
						src_mconfig, sink_mconfig);
		}
	}

	return ret;
}

/*
 * in the Post-PMD event of mixer we need to do following:
 *   - Free the mcps used
 *   - Free the mem used
 *   - Unbind the modules within the pipeline
 *   - Delete the pipeline (modules are not required to be explicitly
 *     deleted, pipeline delete is enough here
 */
static int skl_tplg_mixer_dapm_post_pmd_event(struct snd_soc_dapm_widget *w,
							struct skl *skl)
{
	struct skl_module_cfg *mconfig = w->priv;
	struct skl_pipe_module *w_module;
	struct skl_module_cfg *src_module = NULL, *dst_module;
	struct skl_sst *ctx = skl->skl_sst;
	struct skl_pipe *s_pipe = mconfig->pipe;
	struct skl_module_deferred_bind *modules, *tmp;

	if (s_pipe->state == SKL_PIPE_INVALID)
		return -EINVAL;

	skl_tplg_free_pipe_mcps(skl, mconfig);
	skl_tplg_free_pipe_mem(skl, mconfig);

	list_for_each_entry(w_module, &s_pipe->w_list, node) {
		if (list_empty(&skl->bind_list))
			break;

		src_module = w_module->w->priv;

		list_for_each_entry_safe(modules, tmp, &skl->bind_list, node) {
			/*
			 * When the destination module is deleted, Unbind the
			 * modules from deferred bind list.
			 */
			if (modules->dst == src_module) {
				skl_unbind_modules(ctx, modules->src,
						modules->dst);
			}

			/*
			 * When the source module is deleted, remove this entry
			 * from the deferred bind list.
			 */
			if (modules->src == src_module) {
				list_del(&modules->node);
				modules->src = NULL;
				modules->dst = NULL;
				kfree(modules);
			}
		}
	}

	list_for_each_entry(w_module, &s_pipe->w_list, node) {
		dst_module = w_module->w->priv;

		if (mconfig->m_state >= SKL_MODULE_INIT_DONE)
			skl_tplg_free_pipe_mcps(skl, dst_module);
		if (src_module == NULL) {
			src_module = dst_module;
			continue;
		}

		skl_unbind_modules(ctx, src_module, dst_module);
		src_module = dst_module;
	}

	skl_delete_pipe(ctx, mconfig->pipe);

	list_for_each_entry(w_module, &s_pipe->w_list, node) {
		src_module = w_module->w->priv;
		src_module->m_state = SKL_MODULE_UNINIT;
	}

	return skl_tplg_unload_pipe_modules(ctx, s_pipe);
}

/*
 * in the Post-PMD event of PGA we need to do following:
 *   - Free the mcps used
 *   - Stop the pipeline
 *   - In source pipe is connected, unbind with source pipelines
 */
static int skl_tplg_pga_dapm_post_pmd_event(struct snd_soc_dapm_widget *w,
								struct skl *skl)
{
	struct skl_module_cfg *src_mconfig, *sink_mconfig;
	int ret = 0, i;
	struct skl_sst *ctx = skl->skl_sst;

	src_mconfig = w->priv;

	/* Stop the pipe since this is a mixin module */
	ret = skl_stop_pipe(ctx, src_mconfig->pipe);
	if (ret)
		return ret;

	for (i = 0; i < src_mconfig->module->max_output_pins; i++) {
		if (src_mconfig->m_out_pin[i].pin_state == SKL_PIN_BIND_DONE) {
			sink_mconfig = src_mconfig->m_out_pin[i].tgt_mcfg;
			if (!sink_mconfig)
				continue;
			/*
			 * This is a connecter and if path is found that means
			 * unbind between source and sink has not happened yet
			 */
			ret = skl_unbind_modules(ctx, src_mconfig,
							sink_mconfig);
		}
	}

	return ret;
}

/*
 * In modelling, we assume there will be ONLY one mixer in a pipeline. If a
 * second one is required that is created as another pipe entity.
 * The mixer is responsible for pipe management and represent a pipeline
 * instance
 */
static int skl_tplg_mixer_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *k, int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct skl *skl = get_skl_ctx(dapm->dev);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		return skl_tplg_mixer_dapm_pre_pmu_event(w, skl);

	case SND_SOC_DAPM_POST_PMU:
		return skl_tplg_mixer_dapm_post_pmu_event(w, skl);

	case SND_SOC_DAPM_PRE_PMD:
		return skl_tplg_mixer_dapm_pre_pmd_event(w, skl);

	case SND_SOC_DAPM_POST_PMD:
		return skl_tplg_mixer_dapm_post_pmd_event(w, skl);
	}

	return 0;
}

/*
 * In modelling, we assumed rest of the modules in pipeline are PGA. But we
 * are interested in last PGA (leaf PGA) in a pipeline to disconnect with
 * the sink when it is running (two FE to one BE or one FE to two BE)
 * scenarios
 */
static int skl_tplg_pga_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *k, int event)

{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct skl *skl = get_skl_ctx(dapm->dev);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		return skl_tplg_pga_dapm_pre_pmu_event(w, skl);

	case SND_SOC_DAPM_POST_PMD:
		return skl_tplg_pga_dapm_post_pmd_event(w, skl);
	}

	return 0;
}

/*
 * In modelling, we assumed that all single module will be PGA leaf. Have
 * added new event flag POST_PMU. PRE_PMU is going to handle dynamic connection
 * i.e (dynamic FE or BE connection to already running stream). POST_PMU will
 * handle the pipeline binding and running from sink to source. POST_PMD
 * will handle the cleanup of single module pipe.
 */
static int skl_tplg_pga_single_module_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *k, int event)

{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct skl *skl = get_skl_ctx(dapm->dev);
	struct skl_module_cfg *mcfg = w->priv;
	int ret;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = skl_tplg_mixer_dapm_pre_pmu_event(w, skl);
		if ((skl_get_pipe_order(mcfg) == SKL_LAST_PIPE) && (ret == 0))
			ret = skl_tplg_mixer_dapm_post_pmu_event(w, skl);
		return ret;

	case SND_SOC_DAPM_POST_PMU:
		return skl_tplg_pga_dapm_pre_pmu_event(w, skl);

	case SND_SOC_DAPM_POST_PMD:
		ret = skl_tplg_pga_dapm_post_pmd_event(w, skl);
		if (ret >= 0)
			ret = skl_tplg_mixer_dapm_post_pmd_event(w, skl);
		return ret;
	}

	return 0;
}

int skl_tplg_dsp_log_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_soc_kcontrol_platform(kcontrol);
	struct hdac_ext_bus *ebus = snd_soc_component_get_drvdata
					(&(platform->component));
	struct skl *skl = ebus_to_skl(ebus);

	ucontrol->value.integer.value[0] = get_dsp_log_priority(skl);

	return 0;
}

int skl_tplg_dsp_log_set(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_soc_kcontrol_platform(kcontrol);
	struct hdac_ext_bus *ebus = snd_soc_component_get_drvdata
					(&(platform->component));
	struct skl *skl = ebus_to_skl(ebus);

	update_dsp_log_priority(ucontrol->value.integer.value[0], skl);

	return 0;
}

static int skl_tplg_send_gain_ipc(struct snd_soc_dapm_context *dapm,
					struct skl_module_cfg *mconfig)
{
	struct skl_gain_config *gain_cfg;
	struct skl *skl = get_skl_ctx(dapm->dev);
	struct skl_module_iface *m_intf;
	int num_channel, i, ret = 0;

	m_intf = &mconfig->module->formats[mconfig->fmt_idx];
	num_channel = (m_intf->outputs[0].fmt.channels >
				MAX_NUM_CHANNELS) ? MAX_NUM_CHANNELS :
					m_intf->outputs[0].fmt.channels;

	gain_cfg = kzalloc(sizeof(*gain_cfg), GFP_KERNEL);
	if (!gain_cfg)
		return -ENOMEM;

	gain_cfg->ramp_type = mconfig->gain_data->ramp_type;
	gain_cfg->ramp_duration = mconfig->gain_data->ramp_duration;
	for (i = 0; i < num_channel; i++) {
		gain_cfg->channel_id = i;
		gain_cfg->target_volume = mconfig->gain_data->volume[i];
		ret = skl_set_module_params(skl->skl_sst, (u32 *)gain_cfg,
				sizeof(*gain_cfg), 0, mconfig);
		if (ret < 0) {
			dev_err(dapm->dev,
				"set gain for channel:%d failed\n", i);
			break;
		}
	}
	kfree(gain_cfg);

	return ret;
}

static int skl_tplg_ramp_duration_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *w = snd_soc_dapm_kcontrol_widget(kcontrol);
	struct skl_module_cfg *mconfig = w->priv;

	ucontrol->value.integer.value[0] = mconfig->gain_data->ramp_duration;

	return 0;
}

static int skl_tplg_ramp_type_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *w = snd_soc_dapm_kcontrol_widget(kcontrol);
	struct skl_module_cfg *mconfig = w->priv;

	ucontrol->value.integer.value[0] = mconfig->gain_data->ramp_type;

	return 0;
}

static int skl_tplg_get_linear_toindex(int val)
{
	int i, index = -EINVAL;

	for (i = 0; i < ARRAY_SIZE(linear_gain); i++) {
		if (val == linear_gain[i]) {
			index = i;
			break;
		}
	}

	return index;
}

static int skl_tplg_volume_ctl_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	struct soc_mixer_control *mc;
	struct snd_soc_dapm_widget *w;
	struct skl_module_iface *m_intf;
	struct skl_module_cfg *mconfig;

	mc = (struct soc_mixer_control *)kcontrol->private_value;
	w = snd_soc_dapm_kcontrol_widget(kcontrol);
	mconfig = w->priv;

	m_intf = &mconfig->module->formats[mconfig->fmt_idx];
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = m_intf->outputs[0].fmt.channels;
	uinfo->value.integer.min = mc->min;
	uinfo->value.integer.max = mc->max;

	return 0;
}

static int skl_tplg_volume_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *w = snd_soc_dapm_kcontrol_widget(kcontrol);
	struct skl_module_cfg *mconfig = w->priv;
	struct skl_module_iface *m_intf;
	int i, max_channels;

	m_intf = &mconfig->module->formats[mconfig->fmt_idx];
	max_channels = (m_intf->outputs[0].fmt.channels >
				MAX_NUM_CHANNELS) ? MAX_NUM_CHANNELS :
					m_intf->outputs[0].fmt.channels;
	for (i = 0; i < max_channels; i++)
		ucontrol->value.integer.value[i] =
			skl_tplg_get_linear_toindex(
				mconfig->gain_data->volume[i]);

	return 0;
}

static int skl_tplg_ramp_duration_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm;
	struct snd_soc_dapm_widget *w;
	struct skl_module_cfg *mconfig;
	int ret = 0;

	dapm = snd_soc_dapm_kcontrol_dapm(kcontrol);
	w = snd_soc_dapm_kcontrol_widget(kcontrol);
	mconfig = w->priv;
	mconfig->gain_data->ramp_duration = ucontrol->value.integer.value[0];

	if (w->power)
		ret = skl_tplg_send_gain_ipc(dapm, mconfig);
	return ret;
}

static int skl_tplg_ramp_type_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm;
	struct snd_soc_dapm_widget *w;
	struct skl_module_cfg *mconfig;
	int ret = 0;

	dapm = snd_soc_dapm_kcontrol_dapm(kcontrol);
	w = snd_soc_dapm_kcontrol_widget(kcontrol);
	mconfig = w->priv;
	mconfig->gain_data->ramp_type = ucontrol->value.integer.value[0];

	if (w->power)
		ret = skl_tplg_send_gain_ipc(dapm, mconfig);

	return ret;
}

static int skl_tplg_volume_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm;
	struct snd_soc_dapm_widget *w;
	struct skl_module_cfg *mconfig;
	struct skl_module_iface *m_intf;
	int ret = 0, i, max_channels;

	dapm = snd_soc_dapm_kcontrol_dapm(kcontrol);
	w = snd_soc_dapm_kcontrol_widget(kcontrol);
	mconfig = w->priv;

	m_intf = &mconfig->module->formats[mconfig->fmt_idx];
	max_channels = (m_intf->outputs[0].fmt.channels >
				MAX_NUM_CHANNELS) ? MAX_NUM_CHANNELS :
				m_intf->outputs[0].fmt.channels;

	for (i = 0; i < max_channels; i++)
		if (ucontrol->value.integer.value[i] >=
					ARRAY_SIZE(linear_gain)) {
			dev_err(dapm->dev,
				"Volume requested is out of range!!!\n");
			return -EINVAL;
		}

	for (i = 0; i < max_channels; i++)
		mconfig->gain_data->volume[i] =
			linear_gain[ucontrol->value.integer.value[i]];

	if (w->power)
		ret = skl_tplg_send_gain_ipc(dapm, mconfig);

	return ret;
}

static int skl_tplg_multi_config_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_soc_kcontrol_platform(kcontrol);
	struct hdac_ext_bus *ebus = snd_soc_component_get_drvdata
					(&(platform->component));
	struct skl *skl = ebus_to_skl(ebus);
	struct skl_pipeline *ppl;
	struct skl_pipe *pipe = NULL;
	u32 *pipe_id;
	struct soc_enum *ec = (struct soc_enum *)kcontrol->private_value;

	if (!ec)
		return -EINVAL;

	pipe_id = ec->dobj.private;
	GET_PIPE(ppl, skl, node, *pipe_id, pipe);
	if (!pipe)
		return -EIO;

	ucontrol->value.enumerated.item[0]  =  pipe->pipe_config_idx;

	return 0;
}

static int skl_tplg_multi_config_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_soc_kcontrol_platform(kcontrol);
	struct hdac_ext_bus *ebus = snd_soc_component_get_drvdata
					(&(platform->component));
	struct skl *skl = ebus_to_skl(ebus);
	struct skl_pipeline *ppl;
	struct skl_pipe *pipe = NULL;
	struct soc_enum *ec = (struct soc_enum *)kcontrol->private_value;
	u32 *pipe_id;

	if (!ec)
		return -EINVAL;

	if (ucontrol->value.enumerated.item[0] > ec->items)
		return -EINVAL;

	pipe_id = ec->dobj.private;
	GET_PIPE(ppl, skl, node, *pipe_id, pipe);
	if (!pipe)
		return -EIO;

	pipe->pipe_config_idx = ucontrol->value.enumerated.item[0];

	return 0;
}


static int skl_tplg_tlv_control_get(struct snd_kcontrol *kcontrol,
			unsigned int __user *data, unsigned int size)
{
	struct soc_bytes_ext *sb =
			(struct soc_bytes_ext *)kcontrol->private_value;
	struct skl_algo_data *bc = (struct skl_algo_data *)sb->dobj.private;
	struct snd_soc_dapm_widget *w = snd_soc_dapm_kcontrol_widget(kcontrol);
	struct skl_module_cfg *mconfig = w->priv;
	struct skl *skl = get_skl_ctx(w->dapm->dev);

	if (w->power)
		skl_get_module_params(skl->skl_sst, (u32 *)bc->params,
				      bc->size, bc->param_id, mconfig);

	/* decrement size for TLV header */
	size -= 2 * sizeof(u32);

	/* check size as we don't want to send kernel data */
	if (size > bc->max)
		size = bc->max;

	if (bc->params) {
		if (copy_to_user(data, &bc->param_id, sizeof(u32)))
			return -EFAULT;
		if (copy_to_user(data + 1, &size, sizeof(u32)))
			return -EFAULT;
		if (copy_to_user(data + 2, bc->params, size))
			return -EFAULT;
	}

	return 0;
}

#define SKL_PARAM_VENDOR_ID 0xff

static int skl_tplg_tlv_control_set(struct snd_kcontrol *kcontrol,
			const unsigned int __user *data, unsigned int size)
{
	struct snd_soc_dapm_widget *w = snd_soc_dapm_kcontrol_widget(kcontrol);
	struct skl_module_cfg *mconfig = w->priv;
	struct soc_bytes_ext *sb =
			(struct soc_bytes_ext *)kcontrol->private_value;
	struct skl_algo_data *ac = (struct skl_algo_data *)sb->dobj.private;
	struct skl *skl = get_skl_ctx(w->dapm->dev);

	if (ac->params) {
		if (size > ac->max)
			return -EINVAL;

		ac->size = size;
		/*
		 * if the param_is is of type Vendor, firmware expects actual
		 * parameter id and size from the control.
		 */
		if (ac->param_id == SKL_PARAM_VENDOR_ID) {
			if (copy_from_user(ac->params, data, size))
				return -EFAULT;
		} else {
			if (copy_from_user(ac->params,
					   data + 2, size))
				return -EFAULT;
		}

		if (w->power)
			return skl_set_module_params(skl->skl_sst,
						(u32 *)ac->params, ac->size,
						ac->param_id, mconfig);
	}

	return 0;
}

static int skl_tplg_mic_control_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *w = snd_soc_dapm_kcontrol_widget(kcontrol);
	struct skl_module_cfg *mconfig = w->priv;
	struct soc_enum *ec = (struct soc_enum *)kcontrol->private_value;
	u32 ch_type = *((u32 *)ec->dobj.private);

	if (mconfig->dmic_ch_type == ch_type)
		ucontrol->value.enumerated.item[0] =
					mconfig->dmic_ch_combo_index;
	else
		ucontrol->value.enumerated.item[0] = 0;

	return 0;
}

static int skl_fill_mic_sel_params(struct skl_module_cfg *mconfig,
	struct skl_mic_sel_config *mic_cfg, struct device *dev)
{
	struct skl_specific_cfg *sp_cfg =
				&mconfig->formats_config[SKL_PARAM_INIT];

	sp_cfg->caps_size = sizeof(struct skl_mic_sel_config);
	sp_cfg->set_params = SKL_PARAM_SET;
	sp_cfg->param_id = 0x00;
	if (!sp_cfg->caps) {
		sp_cfg->caps = devm_kzalloc(dev, sp_cfg->caps_size, GFP_KERNEL);
		if (!sp_cfg->caps)
			return -ENOMEM;
	}

	mic_cfg->mic_switch = SKL_MIC_SEL_SWITCH;
	mic_cfg->flags = 0;
	memcpy(sp_cfg->caps, mic_cfg, sp_cfg->caps_size);

	return 0;
}

static int skl_tplg_mic_control_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *w = snd_soc_dapm_kcontrol_widget(kcontrol);
	struct skl_module_cfg *mconfig = w->priv;
	struct skl_mic_sel_config mic_cfg = {0};
	struct soc_enum *ec = (struct soc_enum *)kcontrol->private_value;
	u32 ch_type = *((u32 *)ec->dobj.private);
	const int *list;
	u8 in_ch, out_ch, index;

	mconfig->dmic_ch_type = ch_type;
	mconfig->dmic_ch_combo_index = ucontrol->value.enumerated.item[0];

	/* enum control index 0 is INVALID, so no channels to be set */
	if (mconfig->dmic_ch_combo_index == 0)
		return 0;

	/* No valid channel selection map for index 0, so offset by 1 */
	index = mconfig->dmic_ch_combo_index - 1;

	switch (ch_type) {
	case SKL_CH_MONO:
		if (mconfig->dmic_ch_combo_index > ARRAY_SIZE(mic_mono_list))
			return -EINVAL;

		list = &mic_mono_list[index];
		break;

	case SKL_CH_STEREO:
		if (mconfig->dmic_ch_combo_index > ARRAY_SIZE(mic_stereo_list))
			return -EINVAL;

		list = mic_stereo_list[index];
		break;

	case SKL_CH_TRIO:
		if (mconfig->dmic_ch_combo_index > ARRAY_SIZE(mic_trio_list))
			return -EINVAL;

		list = mic_trio_list[index];
		break;

	case SKL_CH_QUATRO:
		if (mconfig->dmic_ch_combo_index > ARRAY_SIZE(mic_quatro_list))
			return -EINVAL;

		list = mic_quatro_list[index];
		break;

	default:
		dev_err(w->dapm->dev,
				"Invalid channel %d for mic_select module\n",
				ch_type);
		return -EINVAL;

	}

	/* channel type enum map to number of chanels for that type */
	for (out_ch = 0; out_ch < ch_type; out_ch++) {
		in_ch = list[out_ch];
		mic_cfg.blob[out_ch][in_ch] = SKL_DEFAULT_MIC_SEL_GAIN;
	}

	return skl_fill_mic_sel_params(mconfig, &mic_cfg, w->dapm->dev);
}

/*
 * Fill the dma id for host and link. In case of passthrough
 * pipeline, this will both host and link in the same
 * pipeline, so need to copy the link and host based on dev_type
 */
static void skl_tplg_fill_dma_id(struct skl_module_cfg *mcfg,
				struct skl_pipe_params *params)
{
	struct skl_pipe *pipe = mcfg->pipe;

	if (pipe->passthru) {
		switch (mcfg->dev_type) {
		case SKL_DEVICE_HDALINK:
			pipe->p_params->link_dma_id = params->link_dma_id;
			pipe->p_params->link_index = params->link_index;
			pipe->p_params->link_bps = params->link_bps;
			break;

		case SKL_DEVICE_HDAHOST:
			pipe->p_params->host_dma_id = params->host_dma_id;
			pipe->p_params->host_bps = params->host_bps;
			break;

		default:
			break;
		}
		pipe->p_params->s_fmt = params->s_fmt;
		pipe->p_params->ch = params->ch;
		pipe->p_params->s_freq = params->s_freq;
		pipe->p_params->stream = params->stream;
		pipe->p_params->format = params->format;

	} else {
		memcpy(pipe->p_params, params, sizeof(*params));
	}
}

static int skl_probe_set_tlv_ext(struct snd_kcontrol *kcontrol)
{
	struct snd_soc_dapm_context *dapm =
			snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_dapm_widget *w = snd_soc_dapm_kcontrol_widget(kcontrol);
	struct skl_module_cfg *mconfig = w->priv;
	struct soc_bytes_ext *sb = (void *) kcontrol->private_value;
	struct skl_probe_data *ap = (struct skl_probe_data *)sb->dobj.private;
	struct skl *skl = get_skl_ctx(dapm->dev);
	struct skl_probe_config *pconfig = &skl->skl_sst->probe_config;
	struct probe_pt_param connect_point;
	int disconnect_point;
	int ret = 0;
	int index = -1, i;
	char buf[20], pos[10];


	for (i = 0; i < pconfig->no_extractor; i++) {
		strcpy(buf, "Extractor");
		snprintf(pos, 4, "%d", i);
		if (strstr(kcontrol->id.name, strcat(buf, pos))) {
			index = i;
			break;
		}
	}
	if (index < 0)
		return -EINVAL;

	if ((ap->operation == SKL_PROBE_CONNECT) &&
		(pconfig->eprobe[index].state == SKL_PROBE_STATE_EXT_NONE)) {
		/* cache extractor params */
		pconfig->eprobe[index].operation = ap->operation;
		pconfig->eprobe[index].purpose = ap->purpose;
		pconfig->eprobe[index].probe_point_id = ap->probe_point_id;

		/* Below check ensures that atleast one extractor stream is in
		 * progress in which case the driver can send the CONNECT IPC
		 */
		if (pconfig->e_refc > 0) {
			memcpy(&connect_point.params, &ap->probe_point_id,
								sizeof(u32));
			connect_point.connection = ap->purpose;
			connect_point.node_id = -1;
			ret = skl_set_module_params(skl->skl_sst,
					(void *)&connect_point,
					sizeof(struct probe_pt_param),
					SKL_PROBE_CONNECT, mconfig);
			if (ret < 0) {
				dev_err(dapm->dev, "failed to connect extractor probe point\n");
				return -EINVAL;
			}
			pconfig->eprobe[index].state =
						SKL_PROBE_STATE_EXT_CONNECTED;
			dev_dbg(dapm->dev, "eprobe[%d].state %d\n", index,
						pconfig->eprobe[index].state);
		}
	} else if ((ap->operation == SKL_PROBE_DISCONNECT) &&
				(pconfig->eprobe[index].state ==
				SKL_PROBE_STATE_EXT_CONNECTED) &&
				(pconfig->e_refc > 0)) {
		disconnect_point = (int)ap->probe_point_id;
		ret = skl_set_module_params(skl->skl_sst,
			(void *)&disconnect_point, sizeof(disconnect_point),
						SKL_PROBE_DISCONNECT, mconfig);
		if (ret < 0) {
			dev_err(dapm->dev, "failed to disconnect extractor probe point\n");
			return -EINVAL;
		}
		pconfig->eprobe[index].state = SKL_PROBE_STATE_EXT_NONE;
		dev_dbg(dapm->dev, "eprobe[%d].state %d\n", index,
					pconfig->eprobe[index].state);
		} else
			ret = -EINVAL;

	return ret;
}

static int skl_probe_set_tlv_inj(struct snd_kcontrol *kcontrol)
{
	struct snd_soc_dapm_context *dapm =
				snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_dapm_widget *w = snd_soc_dapm_kcontrol_widget(kcontrol);
	struct skl_module_cfg *mconfig = w->priv;
	struct soc_bytes_ext *sb = (void *) kcontrol->private_value;
	struct skl_probe_data *ap = (struct skl_probe_data *)sb->dobj.private;
	struct skl *skl = get_skl_ctx(dapm->dev);
	struct skl_probe_config *pconfig = &skl->skl_sst->probe_config;
	int disconnect_point;
	int ret = 0;
	int index = -1, i;
	char buf[20], pos[10];

	for (i = 0; i < pconfig->no_injector; i++) {
		strcpy(buf, "Injector");
		snprintf(pos, 4, "%d", i);
		if (strstr(kcontrol->id.name, strcat(buf, pos))) {
			index = i;
			break;
		}
	}
	if (index < 0)
		return -EINVAL;

	if ((ap->operation == SKL_PROBE_CONNECT) &&
		(pconfig->iprobe[index].state == SKL_PROBE_STATE_INJ_NONE)) {
		/* cache injector params */
		pconfig->iprobe[index].operation = ap->operation;
		pconfig->iprobe[index].purpose = ap->purpose;
		pconfig->iprobe[index].probe_point_id = ap->probe_point_id;
	} else if ((ap->operation == SKL_PROBE_DISCONNECT) &&

		(pconfig->iprobe[index].state ==
				SKL_PROBE_STATE_INJ_CONNECTED) &&
		(pconfig->i_refc > 0)) {
		disconnect_point = (int)ap->probe_point_id;
		ret = skl_set_module_params(skl->skl_sst,
				(void *)&disconnect_point,
				sizeof(disconnect_point),
				SKL_PROBE_DISCONNECT, mconfig);
		if (ret < 0) {
			dev_err(dapm->dev, "failed to disconnect injector probe point\n");
			return -EINVAL;
		}
		pconfig->iprobe[index].state = SKL_PROBE_STATE_INJ_DISCONNECTED;
		dev_dbg(dapm->dev, "iprobe[%d].state %d\n", index,
					pconfig->iprobe[index].state);
	} else
		ret = -EINVAL;

	return ret;
}

static int skl_tplg_tlv_probe_set(struct snd_kcontrol *kcontrol,
			const unsigned int __user *data, unsigned int size)
{
	struct snd_soc_dapm_context *dapm =
				snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct soc_bytes_ext *sb = (void *) kcontrol->private_value;
	struct skl_probe_data *ap = (struct skl_probe_data *)sb->dobj.private;
	void *offset;
	int ret = -EIO, ret1;

	dev_dbg(dapm->dev, "In %s control=%s\n", __func__, kcontrol->id.name);
	dev_dbg(dapm->dev, "size = %u, %#x\n", size, size);

	if (data) {
		offset = (unsigned char *)data;
		offset += 2 * sizeof(u32); /* To skip TLV heeader */
		if (copy_from_user(&ap->operation,
					offset, sizeof(ap->operation)))
			return -EIO;

		offset += sizeof(ap->operation);
		if (copy_from_user(&ap->purpose,
					offset, sizeof(ap->purpose)))
			return -EIO;

		offset += sizeof(ap->purpose);
		if (copy_from_user(&ap->probe_point_id,
					offset, sizeof(ap->probe_point_id)))
			return -EIO;

		dev_dbg(dapm->dev, "operation = %d, purpose = %d, probe_point_id = %d\n",
					ap->operation, ap->purpose, ap->probe_point_id);

		/* In the case of extraction, additional probe points can
		 * be set when the stream is in progress and the driver can
		 * immediately send the connect IPC. But in the case of
		 * injector, for each probe point connection a new stream with
		 * the DAI number corresponding to that control has to be
		 * opened. Hence below implementation ensures that the connect
		 * IPC is sent only in case of extractor.
		 */
		switch (ap->purpose) {
		case SKL_PROBE_EXTRACT:
			ret = skl_probe_set_tlv_ext(kcontrol);
			break;

		case SKL_PROBE_INJECT:
			ret = skl_probe_set_tlv_inj(kcontrol);
			break;

		case SKL_PROBE_INJECT_REEXTRACT:
		/* Injector and extractor control will be set one by one
		 * for Inject_Reextract
		 */
			ret = skl_probe_set_tlv_ext(kcontrol);
			ret1 = skl_probe_set_tlv_inj(kcontrol);
			if (ret == 0 || ret1 == 0)
				ret = 0;
			else
				ret = -EINVAL;
			break;

		default:
			ret = -EINVAL;
		}
	}
	return ret;
}

/*
 * The FE params are passed by hw_params of the DAI.
 * On hw_params, the params are stored in Gateway module of the FE and we
 * need to calculate the format in DSP module configuration, that
 * conversion is done here
 */
int skl_tplg_update_pipe_params(struct device *dev,
			struct skl_module_cfg *mconfig,
			struct skl_pipe_params *params,
			snd_pcm_format_t fmt)
{
	struct skl_module_res *res = &mconfig->module->resources[0];
	struct skl *skl = get_skl_ctx(dev);
	struct skl_module_fmt *format = NULL;
	u8 cfg_idx = mconfig->pipe->cur_config_idx;

	skl_tplg_fill_dma_id(mconfig, params);
	mconfig->fmt_idx = mconfig->mod_cfg[cfg_idx].fmt_idx;
	mconfig->res_idx = mconfig->mod_cfg[cfg_idx].res_idx;

	if (skl->nr_modules)
		return 0;

	if (params->stream == SNDRV_PCM_STREAM_PLAYBACK)
		format = &mconfig->module->formats[0].inputs[0].fmt;
	else
		format = &mconfig->module->formats[0].outputs[0].fmt;

	/* set the hw_params */
	format->s_freq = params->s_freq;
	format->channels = params->ch;

	/*
		* set copier sample type as follows
		* 0 : if data is MSB aligned in the container
		* 1 : if data is LSB aligned in the container
		* 4 : if float
	*/
	switch (fmt) {
	case SKL_FMT_S16LE:
		format->valid_bit_depth = SKL_DEPTH_16BIT;
		format->bit_depth = SKL_DEPTH_16BIT;
		format->sample_type = SKL_SAMPLE_TYPE_INT_MSB;
		break;

	case SKL_FMT_S24LE:
		format->valid_bit_depth = SKL_DEPTH_24BIT;
		format->bit_depth = SKL_DEPTH_32BIT;
		format->sample_type = SKL_SAMPLE_TYPE_INT_LSB;
		break;

	case SKL_FMT_S32LE:
		format->valid_bit_depth = SKL_DEPTH_32BIT;
		format->bit_depth = SKL_DEPTH_32BIT;
		format->sample_type = SKL_SAMPLE_TYPE_INT_MSB;
		break;

	case SKL_FMT_FLOATLE:
		format->valid_bit_depth = SKL_DEPTH_32BIT;
		format->bit_depth = SKL_DEPTH_32BIT;
		format->sample_type = SKL_SAMPLE_TYPE_FLOAT;
		break;

	case SKL_FMT_S24_3LE:
		format->valid_bit_depth = SKL_DEPTH_24BIT;
		format->bit_depth = SKL_DEPTH_24BIT;
		format->sample_type = SKL_SAMPLE_TYPE_INT_MSB;
		break;

	default:
		format->bit_depth = SKL_DEPTH_32BIT;
		format->sample_type = SKL_SAMPLE_TYPE_INT_MSB;
	}

	if (params->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		res->ibs = (format->s_freq / 1000) *
				(format->channels) *
				(format->bit_depth >> 3);
	} else {
		res->obs = (format->s_freq / 1000) *
				(format->channels) *
				(format->bit_depth >> 3);
	}

	return 0;
}

/*
 * Query the module config for the FE DAI
 * This is used to find the hw_params set for that DAI and apply to FE
 * pipeline
 */
struct skl_module_cfg *
skl_tplg_fe_get_cpr_module(struct snd_soc_dai *dai, int stream)
{
	struct snd_soc_dapm_widget *w;
	struct snd_soc_dapm_path *p = NULL;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		w = dai->playback_widget;
		snd_soc_dapm_widget_for_each_sink_path(w, p) {
			if (p->connect && p->sink->power &&
				!is_skl_dsp_widget_type(p->sink, dai->dev))
				continue;

			if (p->sink->priv) {
				dev_dbg(dai->dev, "set params for %s\n",
						p->sink->name);
				return p->sink->priv;
			}
		}
	} else {
		w = dai->capture_widget;
		snd_soc_dapm_widget_for_each_source_path(w, p) {
			if (p->connect && p->source->power &&
				!is_skl_dsp_widget_type(p->source, dai->dev))
				continue;

			if (p->source->priv) {
				dev_dbg(dai->dev, "set params for %s\n",
						p->source->name);
				return p->source->priv;
			}
		}
	}

	return NULL;
}

static struct skl_module_cfg *skl_get_mconfig_pb_cpr(
		struct snd_soc_dai *dai, struct snd_soc_dapm_widget *w)
{
	struct snd_soc_dapm_path *p;
	struct skl_module_cfg *mconfig = NULL;

	snd_soc_dapm_widget_for_each_source_path(w, p) {
		if (w->endpoints[SND_SOC_DAPM_DIR_OUT] > 0) {
			if (p->connect &&
				    (p->sink->id == snd_soc_dapm_aif_out) &&
				    p->source->priv) {
				mconfig = p->source->priv;
				return mconfig;
			}
			mconfig = skl_get_mconfig_pb_cpr(dai, p->source);
			if (mconfig)
				return mconfig;
		}
	}
	return mconfig;
}

static struct skl_module_cfg *skl_get_mconfig_cap_cpr(
		struct snd_soc_dai *dai, struct snd_soc_dapm_widget *w)
{
	struct snd_soc_dapm_path *p;
	struct skl_module_cfg *mconfig = NULL;

	snd_soc_dapm_widget_for_each_sink_path(w, p) {
		if (w->endpoints[SND_SOC_DAPM_DIR_IN] > 0) {
			if (p->connect &&
				    (p->source->id == snd_soc_dapm_aif_in) &&
				    p->sink->priv) {
				mconfig = p->sink->priv;
				return mconfig;
			}
			mconfig = skl_get_mconfig_cap_cpr(dai, p->sink);
			if (mconfig)
				return mconfig;
		}
	}
	return mconfig;
}

struct skl_module_cfg *
skl_tplg_be_get_cpr_module(struct snd_soc_dai *dai, int stream)
{
	struct snd_soc_dapm_widget *w;
	struct skl_module_cfg *mconfig;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		w = dai->playback_widget;
		mconfig = skl_get_mconfig_pb_cpr(dai, w);
	} else {
		w = dai->capture_widget;
		mconfig = skl_get_mconfig_cap_cpr(dai, w);
	}
	return mconfig;
}

static u8 skl_tplg_be_link_type(int dev_type)
{
	int ret;

	switch (dev_type) {
	case SKL_DEVICE_BT:
		ret = NHLT_LINK_SSP;
		break;

	case SKL_DEVICE_DMIC:
		ret = NHLT_LINK_DMIC;
		break;

	case SKL_DEVICE_I2S:
		ret = NHLT_LINK_SSP;
		break;

	case SKL_DEVICE_HDALINK:
		ret = NHLT_LINK_HDA;
		break;
	case SKL_DEVICE_SDW_PCM:
	case SKL_DEVICE_SDW_PDM:
		ret = NHLT_LINK_SDW;
		break;
	default:
		ret = NHLT_LINK_INVALID;
		break;
	}

	return ret;
}
struct skl_sdw_agg_data_caps {
	u32 alh_stream_num;
	u32 ch_mask;
} __packed;

struct skl_sdw_caps_cfg {
	u32 gw_attributes;
	u32 count;
	struct skl_sdw_agg_data_caps data[0];

} __packed;

/*
 * Fill the BE gateway parameters
 * The BE gateway expects a blob of parameters which are kept in the ACPI
 * NHLT blob, so query the blob for interface type (i2s/pdm) and instance.
 * The port can have multiple settings so pick based on the PCM
 * parameters
 */
static int skl_tplg_be_fill_pipe_params(struct snd_soc_dai *dai,
				struct skl_module_cfg *mconfig,
				struct skl_pipe_params *params)
{
	int i, j;
	struct nhlt_specific_cfg *cfg;
	struct skl_sdw_caps_cfg *sdw_cfg;
	struct skl *skl = get_skl_ctx(dai->dev);
	int link_type = skl_tplg_be_link_type(mconfig->dev_type);
	u8 dev_type = skl_tplg_be_dev_type(mconfig->dev_type);

	skl_tplg_fill_dma_id(mconfig, params);

	if (link_type == NHLT_LINK_HDA)
		return 0;
	if (link_type == NHLT_LINK_SDW) {
		sdw_cfg = kzalloc((((sizeof(struct skl_sdw_agg_data_caps)) *
				(mconfig->sdw_agg.num_masters)) + 2),
				GFP_KERNEL);
		if (!sdw_cfg)
			return -ENOMEM;
		mconfig->formats_config[SKL_PARAM_INIT].caps_size =
			(((sizeof(u32)) * (mconfig->sdw_agg.num_masters) * 2)
			+ (2 * (sizeof(u32))));

		sdw_cfg->count = mconfig->sdw_agg.num_masters;
		j = 0;
		for (i = 0; i < SDW_MAX_MASTERS; i++) {
			if (mconfig->sdw_agg.agg_data[i].ch_mask) {
				sdw_cfg->data[j].ch_mask =
					mconfig->sdw_agg.agg_data[i].ch_mask;
				sdw_cfg->data[j].alh_stream_num =
					mconfig->sdw_agg.agg_data[i].alh_stream_num;
				j++;
			}
		}
		sdw_cfg->count = mconfig->sdw_agg.num_masters;
		mconfig->formats_config[SKL_PARAM_INIT].caps = (u32 *) sdw_cfg;
		return 0;
	}

	/* update the blob based on virtual bus_id*/
	cfg = skl_get_nhlt_specific_cfg(skl, mconfig->vbus_id, link_type,
					params->s_fmt, params->ch,
					params->s_freq, params->stream,
					dev_type);

	if (cfg) {
		mconfig->formats_config[SKL_PARAM_INIT].caps_size = cfg->size;
		mconfig->formats_config[SKL_PARAM_INIT].caps =
							(u32 *) &cfg->caps;
	} else {
		dev_err(dai->dev, "Blob NULL for id %x type %d dirn %d\n",
					mconfig->vbus_id, link_type,
					params->stream);
		dev_err(dai->dev, "PCM: ch %d, freq %d, fmt %d\n",
				 params->ch, params->s_freq, params->s_fmt);
		return -EINVAL;
	}

	return 0;
}

static int skl_tplg_be_set_src_pipe_params(struct snd_soc_dai *dai,
				struct snd_soc_dapm_widget *w,
				struct skl_pipe_params *params)
{
	struct snd_soc_dapm_path *p;
	int ret = -EIO;

	snd_soc_dapm_widget_for_each_source_path(w, p) {
		if (p->connect && is_skl_dsp_widget_type(p->source, dai->dev) &&
						p->source->priv) {

			ret = skl_tplg_be_fill_pipe_params(dai,
						p->source->priv, params);
			if (ret < 0)
				return ret;
		} else {
			ret = skl_tplg_be_set_src_pipe_params(dai,
						p->source, params);
			if (ret < 0)
				return ret;
		}
	}

	return ret;
}

static int skl_tplg_be_set_sink_pipe_params(struct snd_soc_dai *dai,
	struct snd_soc_dapm_widget *w, struct skl_pipe_params *params)
{
	struct snd_soc_dapm_path *p = NULL;
	int ret = -EIO;

	snd_soc_dapm_widget_for_each_sink_path(w, p) {
		if (p->connect && is_skl_dsp_widget_type(p->sink, dai->dev) &&
						p->sink->priv) {

			ret = skl_tplg_be_fill_pipe_params(dai,
						p->sink->priv, params);
			if (ret < 0)
				return ret;
		} else {
			ret = skl_tplg_be_set_sink_pipe_params(
						dai, p->sink, params);
			if (ret < 0)
				return ret;
		}
	}

	return ret;
}

/*
 * BE hw_params can be a source parameters (capture) or sink parameters
 * (playback). Based on sink and source we need to either find the source
 * list or the sink list and set the pipeline parameters
 */
int skl_tplg_be_update_params(struct snd_soc_dai *dai,
				struct skl_pipe_params *params)
{
	struct snd_soc_dapm_widget *w;

	if (params->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		w = dai->playback_widget;

		return skl_tplg_be_set_src_pipe_params(dai, w, params);

	} else {
		w = dai->capture_widget;

		return skl_tplg_be_set_sink_pipe_params(dai, w, params);
	}

	return 0;
}

/*
 * This function searches notification kcontrol list present in skl_sst
 * context against unique notify_id and returns kcontrol pointer if match
 * found.
 */
struct snd_kcontrol *skl_search_notify_kctl(struct skl_sst *skl,
							u32 notify_id)
{
	struct skl_notify_kctrl_info *kctl_info;

	list_for_each_entry(kctl_info, &skl->notify_kctls, list) {
		if (notify_id == kctl_info->notify_id)
			return kctl_info->notify_kctl;
	}
	return NULL;
}

/*
 * This function creates notification kcontrol list by searching control
 * list present in snd_card context. It compares kcontrol name with specific
 * string "notify params" to get notification kcontrols and add it up to the
 * notification list present in skl_sst context.
 * NOTE: To use module notification feature, new kcontrol named "notify" should
 * be added in topology XML for that particular module.
 */
int skl_create_notify_kctl_list(struct skl_sst *skl_sst,
				struct snd_card *card)
{
	struct snd_kcontrol *kctl;
	struct snd_soc_dapm_widget *w;
	struct skl_module_cfg *mconfig;
	struct skl_notify_kctrl_info *info;
	u32 size = sizeof(*info);

	list_for_each_entry(kctl, &card->controls, list) {
		if (strnstr(kctl->id.name, "notify params",
						strlen(kctl->id.name))) {
			info = kzalloc(size, GFP_KERNEL);
			if (!info)
				return -ENOMEM;

			w = snd_soc_dapm_kcontrol_widget(kctl);
			mconfig = w->priv;

			/* Module ID (MS word) + Module Instance ID (LS word) */
			info->notify_id = ((mconfig->id.module_id << 16) |
					   (mconfig->id.instance_id));
			info->notify_kctl = kctl;

			list_add_tail(&info->list, &skl_sst->notify_kctls);
		}
	}
	return 0;
}

/*
 * This function deletes notification kcontrol list from skl_sst
 * context.
 */
void skl_delete_notify_kctl_list(struct skl_sst *skl_sst)
{
	struct skl_notify_kctrl_info *info, *tmp;

	list_for_each_entry_safe(info, tmp, &skl_sst->notify_kctls, list) {
		list_del(&info->list);
		kfree(info);
	}
}

/*
 * This function creates notification kcontrol list on first module
 * notification from firmware. It also search notification kcontrol
 * list against unique notify_id sent from firmware and returns the
 * corresponding kcontrol pointer.
 */
struct snd_kcontrol *skl_get_notify_kcontrol(struct skl_sst *skl,
			struct snd_card *card, u32 notify_id)
{
	struct snd_kcontrol *kctl = NULL;

	if (list_empty(&skl->notify_kctls))
		skl_create_notify_kctl_list(skl, card);

	kctl = skl_search_notify_kctl(skl, notify_id);

	return kctl;
}



/*
 * Get the events along with data stored in notify_data and pass
 * to kcontrol private data.
 */
int skl_dsp_cb_event(struct skl_sst *ctx, unsigned int event,
				struct skl_notify_data *notify_data)
{
	struct snd_soc_card *card;
	struct soc_bytes_ext *sb;
	struct skl *skl = get_skl_ctx(ctx->dev);
	struct snd_soc_platform *soc_platform = skl->platform;
	struct skl_module_notify *m_notification = NULL;
	struct snd_kcontrol *kcontrol;
	struct skl_algo_data *bc;
	u8 param_length;

	switch (event) {
	case SKL_TPLG_CHG_NOTIFY:
		card = soc_platform->component.card;

		kcontrol = snd_soc_card_get_kcontrol(card,
				"Topology Change Notification");
		if (!kcontrol) {
			dev_warn(ctx->dev,
				"NOTIFICATION Controls not found\n");
			return -EINVAL;
		}

		sb = (struct soc_bytes_ext *)kcontrol->private_value;
		if (!sb->dobj.private) {
			sb->dobj.private = devm_kzalloc(ctx->dev,
				sizeof(*notify_data), GFP_KERNEL);
			if (!sb->dobj.private)
				return -ENOMEM;
		}

		memcpy(sb->dobj.private, notify_data, sizeof(*notify_data));
		snd_ctl_notify(card->snd_card, SNDRV_CTL_EVENT_MASK_VALUE,
							&kcontrol->id);
		break;
	case SKL_EVENT_GLB_MODULE_NOTIFICATION:
		m_notification = (struct skl_module_notify *)notify_data->data;
		card = soc_platform->component.card;
		kcontrol = skl_get_notify_kcontrol(ctx, card->snd_card,
					m_notification->unique_id);
		if (!kcontrol) {
			dev_warn(ctx->dev, "Module notify control not found\n");
			return -EINVAL;
		}

		sb = (struct soc_bytes_ext *)kcontrol->private_value;
		bc = (struct skl_algo_data *)sb->dobj.private;
		param_length = sizeof(struct skl_notify_data)
					+ notify_data->length;
		memcpy(bc->params, (char *)notify_data, param_length);
		snd_ctl_notify(card->snd_card,
				SNDRV_CTL_EVENT_MASK_VALUE, &kcontrol->id);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/*
 * Get last topology change events like pipeline start, pipeline delete,
 * DSP D0/D3 and notify to user along with time at which last change occurred
 * in topology.
 */
int skl_tplg_change_notification_get(struct snd_kcontrol *kcontrol,
			unsigned int __user *data, unsigned int size)
{
	struct skl_notify_data *notify_data;
	struct soc_bytes_ext *sb =
			(struct soc_bytes_ext *)kcontrol->private_value;

	if (sb->dobj.private) {
		notify_data = (struct skl_notify_data *)sb->dobj.private;
		if (copy_to_user(data, notify_data, sizeof(*notify_data)))
			return -EFAULT;
		/* Clear the data after copy to user as per requirement */
		memset(notify_data, 0, sizeof(*notify_data));
	}

	return 0;
}

static const struct snd_soc_tplg_widget_events skl_tplg_widget_ops[] = {
	{SKL_MIXER_EVENT, skl_tplg_mixer_event},
	{SKL_VMIXER_EVENT, skl_tplg_mixer_event},
	{SKL_PGA_EVENT, skl_tplg_pga_event},
};

static const struct snd_soc_tplg_bytes_ext_ops skl_tlv_ops[] = {
	{SKL_CONTROL_TYPE_BYTE_TLV, skl_tplg_tlv_control_get,
					skl_tplg_tlv_control_set},
	{SKL_CONTROL_TYPE_BYTE_PROBE, skl_tplg_tlv_control_get,
					skl_tplg_tlv_probe_set},
};

static const struct snd_soc_tplg_kcontrol_ops skl_tplg_kcontrol_ops[] = {
	{
		.id = SKL_CONTROL_TYPE_MIC_SELECT,
		.get = skl_tplg_mic_control_get,
		.put = skl_tplg_mic_control_set,
	},
	{
		.id = SKL_CONTROL_TYPE_MULTI_IO_SELECT,
		.get = skl_tplg_multi_config_get,
		.put = skl_tplg_multi_config_set,
	},
	{
		.id = SKL_CONTROL_TYPE_VOLUME,
		.info = skl_tplg_volume_ctl_info,
		.get = skl_tplg_volume_get,
		.put = skl_tplg_volume_set,
	},
	{
		.id = SKL_CONTROL_TYPE_RAMP_DURATION,
		.get = skl_tplg_ramp_duration_get,
		.put = skl_tplg_ramp_duration_set,
	},
	{
		.id = SKL_CONTROL_TYPE_RAMP_TYPE,
		.get = skl_tplg_ramp_type_get,
		.put = skl_tplg_ramp_type_set,
	},
};

static int skl_tplg_fill_pipe_cfg(struct device *dev,
			struct skl_pipe *pipe, u32 tkn,
			u32 tkn_val, int conf_idx, int dir)
{
	struct skl_pipe_fmt *fmt;
	struct skl_path_config *config;

	switch (dir) {
	case SKL_DIR_IN:
		fmt = &pipe->configs[conf_idx].in_fmt;
		break;

	case SKL_DIR_OUT:
		fmt = &pipe->configs[conf_idx].out_fmt;
		break;

	default:
		dev_err(dev, "Invalid direction: %d\n", dir);
		return -EINVAL;
	}

	config = &pipe->configs[conf_idx];

	switch (tkn) {
	case SKL_TKN_U32_CFG_FREQ:
		fmt->freq = tkn_val;
		break;

	case SKL_TKN_U8_CFG_CHAN:
		fmt->channels = tkn_val;
		break;

	case SKL_TKN_U8_CFG_BPS:
		fmt->bps = tkn_val;
		break;

	case SKL_TKN_U32_PATH_MEM_PGS:
		config->mem_pages = tkn_val;
		break;

	default:
		dev_err(dev, "Invalid token config: %d\n", tkn);
		return -EINVAL;
	}

	return 0;
}

static int skl_tplg_fill_pipe_tkn(struct device *dev,
			struct skl_pipe *pipe, u32 tkn,
			u32 tkn_val)
{

	switch (tkn) {
	case SKL_TKN_U32_PIPE_CONN_TYPE:
		pipe->conn_type = tkn_val;
		break;

	case SKL_TKN_U32_PIPE_PRIORITY:
		pipe->pipe_priority = tkn_val;
		break;

	case SKL_TKN_U32_PIPE_MEM_PGS:
		pipe->memory_pages = tkn_val;
		break;

	case SKL_TKN_U32_PMODE:
		pipe->lp_mode = tkn_val;
		break;

	case SKL_TKN_U32_PIPE_DIRECTION:
		pipe->direction = tkn_val;
		break;

	case SKL_TKN_U32_NUM_CONFIGS:
		pipe->nr_cfgs = tkn_val;
		break;

	default:
		dev_err(dev, "Token not handled %d\n", tkn);
		return -EINVAL;
	}

	return 0;
}

/*
 * Add pipeline by parsing the relevant tokens
 * Return an existing pipe if the pipe already exists.
 */
static int skl_tplg_add_pipe(struct device *dev,
		struct skl_module_cfg *mconfig, struct skl *skl,
		struct snd_soc_tplg_vendor_value_elem *tkn_elem)
{
	struct skl_pipeline *ppl;
	struct skl_pipe *pipe;
	struct skl_pipe_params *params;

	list_for_each_entry(ppl, &skl->ppl_list, node) {
		if (ppl->pipe->ppl_id == tkn_elem->value) {
			mconfig->pipe = ppl->pipe;
			return -EEXIST;
		}
	}

	ppl = devm_kzalloc(dev, sizeof(*ppl), GFP_KERNEL);
	if (!ppl)
		return -ENOMEM;

	pipe = devm_kzalloc(dev, sizeof(*pipe), GFP_KERNEL);
	if (!pipe)
		return -ENOMEM;

	params = devm_kzalloc(dev, sizeof(*params), GFP_KERNEL);
	if (!params)
		return -ENOMEM;

	pipe->p_params = params;
	pipe->ppl_id = tkn_elem->value;
	INIT_LIST_HEAD(&pipe->w_list);

	ppl->pipe = pipe;
	list_add(&ppl->node, &skl->ppl_list);

	mconfig->pipe = pipe;
	mconfig->pipe->state = SKL_PIPE_INVALID;

	return 0;
}

static int skl_tplg_get_uuid(struct device *dev, u8 *guid,
	      struct snd_soc_tplg_vendor_uuid_elem *uuid_tkn)
{
	if (uuid_tkn->token == SKL_TKN_UUID)
		memcpy(guid, &uuid_tkn->uuid, 16);
	else {
		dev_err(dev, "Not an UUID token tkn %d\n", uuid_tkn->token);
		return -EINVAL;
	}

	return 0;
}

static int skl_tplg_fill_pin(struct device *dev,
			struct snd_soc_tplg_vendor_value_elem *tkn_elem,
			struct skl_module_pin *m_pin,
			int pin_index)
{
	int ret;

	switch (tkn_elem->token) {
	case SKL_TKN_U32_PIN_MOD_ID:
		m_pin[pin_index].id.module_id = tkn_elem->value;
		break;

	case SKL_TKN_U32_PIN_INST_ID:
		m_pin[pin_index].id.instance_id = tkn_elem->value;
		break;

	case SKL_TKN_UUID:
		ret = skl_tplg_get_uuid(dev,
			m_pin[pin_index].id.mod_uuid.b,
			(struct snd_soc_tplg_vendor_uuid_elem *) tkn_elem);

		if (ret < 0)
			return ret;

		break;

	default:
		dev_err(dev, "%d Not a pin token\n", tkn_elem->token);
		return -EINVAL;
	}

	return 0;
}

/*
 * Parse for pin config specific tokens to fill up the
 * module private data
 */
static int skl_tplg_fill_pins_info(struct device *dev,
		struct skl_module_cfg *mconfig,
		struct snd_soc_tplg_vendor_value_elem *tkn_elem,
		int dir, int pin_count)
{
	int ret;
	struct skl_module_pin *m_pin;

	switch (dir) {
	case SKL_DIR_IN:
		m_pin = mconfig->m_in_pin;
		break;

	case SKL_DIR_OUT:
		m_pin = mconfig->m_out_pin;
		break;

	default:
		dev_err(dev, "Invalid direction value\n");
		return -EINVAL;
	}

	ret = skl_tplg_fill_pin(dev, tkn_elem,
			m_pin, pin_count);

	if (ret < 0)
		return ret;

	m_pin[pin_count].in_use = false;
	m_pin[pin_count].pin_state = SKL_PIN_UNBIND;

	return 0;
}

/*
 * Fill up input/output module config format based
 * on the direction
 */
static int skl_tplg_fill_fmt(struct device *dev,
		struct skl_module_fmt *dst_fmt,
		u32 tkn, u32 value)
{
	switch (tkn) {
	case SKL_TKN_U32_FMT_CH:
		dst_fmt->channels  = value;
		break;

	case SKL_TKN_U32_FMT_FREQ:
		dst_fmt->s_freq = value;
		break;

	case SKL_TKN_U32_FMT_BIT_DEPTH:
		dst_fmt->bit_depth = value;
		break;

	case SKL_TKN_U32_FMT_SAMPLE_SIZE:
		dst_fmt->valid_bit_depth = value;
		break;

	case SKL_TKN_U32_FMT_CH_CONFIG:
		dst_fmt->ch_cfg = value;
		break;

	case SKL_TKN_U32_FMT_INTERLEAVE:
		dst_fmt->interleaving_style = value;
		break;

	case SKL_TKN_U32_FMT_SAMPLE_TYPE:
		dst_fmt->sample_type = value;
		break;

	case SKL_TKN_U32_FMT_CH_MAP:
		dst_fmt->ch_map = value;
		break;

	default:
		dev_err(dev, "Invalid token %d\n", tkn);
		return -EINVAL;
	}

	return 0;
}

static int skl_tplg_widget_fill_fmt(struct device *dev,
		struct skl_module_iface *fmt,
		u32 tkn, u32 val, u32 dir, int fmt_idx)
{
	struct skl_module_fmt *dst_fmt;

	if (!fmt)
		return -EINVAL;

	switch (dir) {
	case SKL_DIR_IN:
		dst_fmt = &fmt->inputs[fmt_idx].fmt;
		break;

	case SKL_DIR_OUT:
		dst_fmt = &fmt->outputs[fmt_idx].fmt;
		break;

	default:
		dev_err(dev, "Invalid direction: %d\n", dir);
		return -EINVAL;
	}

	return skl_tplg_fill_fmt(dev, dst_fmt, tkn, val);
}

static void skl_tplg_fill_pin_dynamic_val(
		struct skl_module_pin *mpin, u32 pin_count, u32 value)
{
	int i;

	for (i = 0; i < pin_count; i++)
		mpin[i].is_dynamic = value;
}

/*
 * Resource table in the manifest has pin specific resources
 * like pin and pin buffer size
 */
static int skl_tplg_manifest_pin_res_tkn(struct device *dev,
		struct snd_soc_tplg_vendor_value_elem *tkn_elem,
		struct skl_module_res *res, int pin_idx, int dir)
{
	struct skl_module_pin_resources *m_pin;

	switch (dir) {
	case SKL_DIR_IN:
		m_pin = &res->input[pin_idx];
		break;

	case SKL_DIR_OUT:
		m_pin = &res->output[pin_idx];
		break;

	default:
		dev_err(dev, "Invalid pin direction: %d\n", dir);
		return -EINVAL;
	}

	switch (tkn_elem->token) {
	case SKL_TKN_MM_U32_RES_PIN_ID:
		m_pin->pin_index = tkn_elem->value;
		break;

	case SKL_TKN_MM_U32_PIN_BUF:
		m_pin->buf_size = tkn_elem->value;
		break;

	default:
		dev_err(dev, "Invalid token: %d\n", tkn_elem->token);
		return -EINVAL;
	}

	return 0;
}

/*
 * Fill module specific resources from the manifest's resource
 * table like CPS, DMA size, mem_pages.
 */
static int skl_tplg_fill_res_tkn(struct device *dev,
		struct snd_soc_tplg_vendor_value_elem *tkn_elem,
		struct skl_module_res *res,
		int pin_idx, int dir)
{
	int ret, tkn_count = 0;

	if (!res)
		return -EINVAL;

	switch (tkn_elem->token) {
	case SKL_TKN_MM_U32_CPS:
		res->cps = tkn_elem->value;
		break;

	case SKL_TKN_MM_U32_DMA_SIZE:
		res->dma_buffer_size = tkn_elem->value;
		break;

	case SKL_TKN_MM_U32_CPC:
		res->cpc = tkn_elem->value;
		break;

	case SKL_TKN_U32_MEM_PAGES:
		res->is_pages = tkn_elem->value;
		break;

	case SKL_TKN_U32_OBS:
		res->obs = tkn_elem->value;
		break;

	case SKL_TKN_U32_IBS:
		res->ibs = tkn_elem->value;
		break;

	case SKL_TKN_U32_MAX_MCPS:
		res->cps = tkn_elem->value;
		break;

	case SKL_TKN_MM_U32_RES_PIN_ID:
	case SKL_TKN_MM_U32_PIN_BUF:
		ret = skl_tplg_manifest_pin_res_tkn(dev, tkn_elem, res,
						    pin_idx, dir);
		if (ret < 0)
			return ret;
		break;

	default:
		dev_err(dev, "Not a res type token: %d", tkn_elem->token);
		return -EINVAL;

	}
	tkn_count++;

	return tkn_count;
}

/*
 * Parse tokens to fill up the module private data
 */
static int skl_tplg_get_token(struct device *dev,
		struct snd_soc_tplg_vendor_value_elem *tkn_elem,
		struct skl *skl, struct skl_module_cfg *mconfig)
{
	int tkn_count = 0;
	int ret;
	static int is_pipe_exists;
	static int pin_index, dir, conf_idx, agg_id;
	struct skl_module_iface *iface = NULL;
	struct skl_module_res *res = NULL;
	int res_idx = mconfig->res_idx;
	int fmt_idx = mconfig->fmt_idx;

	/*
	 * If the manifest structure contains no modules, fill all
	 * the module data to 0th index.
	 * res_idx and fmt_idx are default set to 0.
	 */
	if (skl->nr_modules == 0) {
		res = &mconfig->module->resources[res_idx];
		iface = &mconfig->module->formats[fmt_idx];
	}

	if (tkn_elem->token > SKL_TKN_MAX)
		return -EINVAL;

	switch (tkn_elem->token) {
	case SKL_TKN_U8_IN_QUEUE_COUNT:
		mconfig->module->max_input_pins = tkn_elem->value;
		break;

	case SKL_TKN_U8_OUT_QUEUE_COUNT:
		mconfig->module->max_output_pins = tkn_elem->value;
		break;

	case SKL_TKN_U8_DYN_IN_PIN:
		if (!mconfig->m_in_pin)
			mconfig->m_in_pin = devm_kzalloc(dev, MAX_IN_QUEUE *
					sizeof(*mconfig->m_in_pin), GFP_KERNEL);
		if (!mconfig->m_in_pin)
			return -ENOMEM;

		skl_tplg_fill_pin_dynamic_val(mconfig->m_in_pin, MAX_IN_QUEUE,
					      tkn_elem->value);
		break;

	case SKL_TKN_U8_DYN_OUT_PIN:
		if (!mconfig->m_out_pin)
			mconfig->m_out_pin = devm_kzalloc(dev, MAX_IN_QUEUE *
					sizeof(*mconfig->m_in_pin), GFP_KERNEL);
		if (!mconfig->m_out_pin)
			return -ENOMEM;

		skl_tplg_fill_pin_dynamic_val(mconfig->m_out_pin, MAX_OUT_QUEUE,
					      tkn_elem->value);
		break;

	case SKL_TKN_U8_TIME_SLOT:
		mconfig->time_slot = tkn_elem->value;
		break;

	case SKL_TKN_U8_CORE_ID:
		mconfig->core_id = tkn_elem->value;

	case SKL_TKN_U8_MOD_TYPE:
		mconfig->m_type = tkn_elem->value;
		break;

	case SKL_TKN_U8_DEV_TYPE:
		mconfig->dev_type = tkn_elem->value;
		break;

	case SKL_TKN_U8_HW_CONN_TYPE:
		mconfig->hw_conn_type = tkn_elem->value;
		break;

	case SKL_TKN_U16_MOD_INST_ID:
		mconfig->id.instance_id =
		tkn_elem->value;
		break;

	case SKL_TKN_U32_MEM_PAGES:
	case SKL_TKN_U32_MAX_MCPS:
	case SKL_TKN_U32_OBS:
	case SKL_TKN_U32_IBS:
		ret = skl_tplg_fill_res_tkn(dev, tkn_elem, res, dir, pin_index);
		if (ret < 0)
			return ret;

		break;

	case SKL_TKN_U32_VBUS_ID:
		mconfig->vbus_id = tkn_elem->value;
		break;

	case SKL_TKN_U32_PARAMS_FIXUP:
		mconfig->params_fixup = tkn_elem->value;
		break;

	case SKL_TKN_U32_CONVERTER:
		mconfig->converter = tkn_elem->value;
		break;

	case SKL_TKN_U32_D0I3_CAPS:
		mconfig->d0i3_caps = tkn_elem->value;
		break;

	case SKL_TKN_U32_PIPE_ID:
		ret = skl_tplg_add_pipe(dev,
				mconfig, skl, tkn_elem);

		if (ret < 0) {
			if (ret == -EEXIST) {
				is_pipe_exists = 1;
				break;
			}
			return is_pipe_exists;
		}
		is_pipe_exists = 0;

		break;

	case SKL_TKN_U32_PIPE_CONFIG_ID:
		conf_idx = tkn_elem->value;
		break;

	case SKL_TKN_U32_PIPE_CONN_TYPE:
	case SKL_TKN_U32_PIPE_PRIORITY:
	case SKL_TKN_U32_PIPE_MEM_PGS:
	case SKL_TKN_U32_PMODE:
	case SKL_TKN_U32_PIPE_DIRECTION:
	case SKL_TKN_U32_NUM_CONFIGS:
		if (!is_pipe_exists) {
			ret = skl_tplg_fill_pipe_tkn(dev, mconfig->pipe,
					tkn_elem->token, tkn_elem->value);
			if (ret < 0)
				return ret;
		}

		break;

	case SKL_TKN_U32_PATH_MEM_PGS:
	case SKL_TKN_U32_CFG_FREQ:
	case SKL_TKN_U8_CFG_CHAN:
	case SKL_TKN_U8_CFG_BPS:
		if (mconfig->pipe->nr_cfgs) {
			ret = skl_tplg_fill_pipe_cfg(dev, mconfig->pipe,
					tkn_elem->token, tkn_elem->value,
					conf_idx, dir);
			if (ret < 0)
				return ret;
		}
		break;

	case SKL_TKN_CFG_MOD_RES_ID:
		mconfig->mod_cfg[conf_idx].res_idx = tkn_elem->value;
		break;

	case SKL_TKN_CFG_MOD_FMT_ID:
		mconfig->mod_cfg[conf_idx].fmt_idx = tkn_elem->value;
		break;

	/*
	 * SKL_TKN_U32_DIR_PIN_COUNT token has the value for both
	 * direction and the pin count. The first four bits represent
	 * direction and next four the pin count.
	 */
	case SKL_TKN_U32_DIR_PIN_COUNT:
		dir = tkn_elem->value & SKL_IN_DIR_BIT_MASK;
		pin_index = (tkn_elem->value &
			SKL_PIN_COUNT_MASK) >> 4;

		break;

	case SKL_TKN_U32_FMT_CH:
	case SKL_TKN_U32_FMT_FREQ:
	case SKL_TKN_U32_FMT_BIT_DEPTH:
	case SKL_TKN_U32_FMT_SAMPLE_SIZE:
	case SKL_TKN_U32_FMT_CH_CONFIG:
	case SKL_TKN_U32_FMT_INTERLEAVE:
	case SKL_TKN_U32_FMT_SAMPLE_TYPE:
	case SKL_TKN_U32_FMT_CH_MAP:
		ret = skl_tplg_widget_fill_fmt(dev, iface, tkn_elem->token,
				tkn_elem->value, dir, pin_index);

		if (ret < 0)
			return ret;

		break;

	case SKL_TKN_U32_PIN_MOD_ID:
	case SKL_TKN_U32_PIN_INST_ID:
	case SKL_TKN_UUID:
		ret = skl_tplg_fill_pins_info(dev,
				mconfig, tkn_elem, dir,
				pin_index);
		if (ret < 0)
			return ret;

		break;

	case SKL_TKN_U32_FMT_CFG_IDX:
		if (tkn_elem->value > SKL_MAX_PARAMS_TYPES)
			return -EINVAL;

		mconfig->fmt_cfg_idx = tkn_elem->value;
		break;

	case SKL_TKN_U32_CAPS_SIZE:
		mconfig->formats_config[mconfig->fmt_cfg_idx].caps_size =
			tkn_elem->value;

		break;

	case SKL_TKN_U32_CAPS_SET_PARAMS:
		mconfig->formats_config[mconfig->fmt_cfg_idx].set_params =
				tkn_elem->value;
		break;

	case SKL_TKN_U32_CAPS_PARAMS_ID:
		mconfig->formats_config[mconfig->fmt_cfg_idx].param_id =
				tkn_elem->value;
		break;

	case SKL_TKN_U32_PROC_DOMAIN:
		mconfig->domain =
			tkn_elem->value;

		break;

	case SKL_TKN_U32_AGG_LINK_ID:
		agg_id = tkn_elem->value;
		if (agg_id > SDW_MAX_MASTERS)
			return -EINVAL;
		break;

	case SKL_TKN_U32_AGG_NUM_MASTERS:
		mconfig->sdw_agg.num_masters = tkn_elem->value;
		mconfig->sdw_agg_enable = (tkn_elem->value > 1)
					? true : false;
		break;

	case SKL_TKN_U32_AGG_CH_MASK:
		mconfig->sdw_agg.agg_data[agg_id].ch_mask =
				tkn_elem->value;
		break;

	case SKL_TKN_U32_DMA_BUF_SIZE:
		mconfig->dma_buffer_size = tkn_elem->value;
		break;

	case SKL_TKN_U8_IN_PIN_TYPE:
	case SKL_TKN_U8_OUT_PIN_TYPE:
	case SKL_TKN_U8_CONN_TYPE:
		break;

	default:
		dev_err(dev, "Token %d not handled\n",
				tkn_elem->token);
		return -EINVAL;
	}

	tkn_count++;

	return tkn_count;
}

/*
 * Parse the vendor array for specific tokens to construct
 * module private data
 */
static int skl_tplg_get_tokens(struct device *dev,
		char *pvt_data,	struct skl *skl,
		struct skl_module_cfg *mconfig, int block_size)
{
	struct snd_soc_tplg_vendor_array *array;
	struct snd_soc_tplg_vendor_value_elem *tkn_elem;
	int tkn_count = 0, ret;
	int off = 0, tuple_size = 0;
	bool is_mod_guid = true;

	if (block_size <= 0)
		return -EINVAL;

	while (tuple_size < block_size) {
		array = (struct snd_soc_tplg_vendor_array *)(pvt_data + off);

		off += array->size;

		switch (array->type) {
		case SND_SOC_TPLG_TUPLE_TYPE_STRING:
			dev_warn(dev, "no string tokens expected for skl tplg\n");
			continue;

		case SND_SOC_TPLG_TUPLE_TYPE_UUID:
			if (is_mod_guid) {
				ret = skl_tplg_get_uuid(dev, mconfig->guid,
					array->uuid);
				is_mod_guid = false;
			} else
				ret = skl_tplg_get_token(dev,
					array->value,
					skl, mconfig);

			if (ret < 0)
				return ret;

			tuple_size += sizeof(*array->uuid);

			continue;

		default:
			tkn_elem = array->value;
			tkn_count = 0;
			break;
		}

		while (tkn_count <= (array->num_elems - 1)) {
			ret = skl_tplg_get_token(dev, tkn_elem,
					skl, mconfig);

			if (ret < 0)
				return ret;

			tkn_count = tkn_count + ret;
			tkn_elem++;
		}

		tuple_size += tkn_count * sizeof(*tkn_elem);
	}

	return off;
}

/*
 * Every data block is preceded by a descriptor to read the number
 * of data blocks, they type of the block and it's size
 */
static int skl_tplg_get_desc_blocks(struct device *dev,
		struct snd_soc_tplg_vendor_array *array)
{
	struct snd_soc_tplg_vendor_value_elem *tkn_elem;

	tkn_elem = array->value;

	switch (tkn_elem->token) {
	case SKL_TKN_U8_NUM_BLOCKS:
	case SKL_TKN_U8_BLOCK_TYPE:
	case SKL_TKN_U16_BLOCK_SIZE:
		return tkn_elem->value;

	default:
		dev_err(dev, "Invalid descriptor token %d\n", tkn_elem->token);
		break;
	}

	return -EINVAL;
}

static int skl_tplg_get_caps_data(struct device *dev, char *data,
					struct skl_module_cfg *mconfig)
{
	int idx;

	idx = mconfig->fmt_cfg_idx;
	if (mconfig->formats_config[idx].caps_size > 0) {
		mconfig->formats_config[idx].caps = (u32 *)devm_kzalloc(dev,
					mconfig->formats_config[idx].caps_size,
					GFP_KERNEL);
		if (mconfig->formats_config[idx].caps == NULL)
			return -ENOMEM;
		memcpy(mconfig->formats_config[idx].caps, data,
				mconfig->formats_config[idx].caps_size);
	}

	return mconfig->formats_config[idx].caps_size;
}

/*
 * Parse the private data for the token and corresponding value.
 * The private data can have multiple data blocks. So, a data block
 * is preceded by a descriptor for number of blocks and a descriptor
 * for the type and size of the suceeding data block.
 */
static int skl_tplg_get_pvt_data(struct snd_soc_tplg_dapm_widget *tplg_w,
				struct skl *skl, struct device *dev,
				struct skl_module_cfg *mconfig)
{
	struct snd_soc_tplg_vendor_array *array;
	int num_blocks, block_size = 0, block_type, off = 0;
	char *data;
	int ret;

	/* Read the NUM_DATA_BLOCKS descriptor */
	array = (struct snd_soc_tplg_vendor_array *)tplg_w->priv.data;
	ret = skl_tplg_get_desc_blocks(dev, array);
	if (ret < 0)
		return ret;
	num_blocks = ret;

	off += array->size;
	/* Read the BLOCK_TYPE and BLOCK_SIZE descriptor */
	while (num_blocks > 0) {
		array = (struct snd_soc_tplg_vendor_array *)
				(tplg_w->priv.data + off);

		ret = skl_tplg_get_desc_blocks(dev, array);

		if (ret < 0)
			return ret;
		block_type = ret;
		off += array->size;

		array = (struct snd_soc_tplg_vendor_array *)
			(tplg_w->priv.data + off);

		ret = skl_tplg_get_desc_blocks(dev, array);

		if (ret < 0)
			return ret;
		block_size = ret;
		off += array->size;

		array = (struct snd_soc_tplg_vendor_array *)
			(tplg_w->priv.data + off);

		data = (tplg_w->priv.data + off);

		if (block_type == SKL_TYPE_TUPLE) {
			ret = skl_tplg_get_tokens(dev, data,
					skl, mconfig, block_size);
		} else {
			ret = skl_tplg_get_caps_data(dev, data, mconfig);
		}

		if (ret < 0)
			return ret;

		--num_blocks;
		off += ret;
	}

	return 0;
}

static void skl_clear_pin_config(struct snd_soc_platform *platform,
				struct snd_soc_dapm_widget *w)
{
	int i;
	struct skl_module_cfg *mconfig;
	struct skl_pipe *pipe;

	if (!strncmp(w->dapm->component->name, platform->component.name,
					strlen(platform->component.name))) {
		mconfig = w->priv;
		pipe = mconfig->pipe;
		for (i = 0; i < mconfig->module->max_input_pins; i++) {
			mconfig->m_in_pin[i].in_use = false;
			mconfig->m_in_pin[i].pin_state = SKL_PIN_UNBIND;
		}
		for (i = 0; i < mconfig->module->max_output_pins; i++) {
			mconfig->m_out_pin[i].in_use = false;
			mconfig->m_out_pin[i].pin_state = SKL_PIN_UNBIND;
		}
		pipe->state = SKL_PIPE_INVALID;
		mconfig->m_state = SKL_MODULE_UNINIT;
	}
}

void skl_cleanup_resources(struct skl *skl)
{
	struct skl_sst *ctx = skl->skl_sst;
	struct snd_soc_platform *soc_platform = skl->platform;
	struct snd_soc_dapm_widget *w;
	struct snd_soc_card *card;

	if (soc_platform == NULL)
		return;

	card = soc_platform->component.card;
	if (!card || !card->instantiated)
		return;

	skl->resource.mem = 0;
	skl->resource.mcps = 0;

	list_for_each_entry(w, &card->widgets, list) {
		if (is_skl_dsp_widget_type(w, ctx->dev) && w->priv != NULL)
			skl_clear_pin_config(soc_platform, w);
	}

	skl_clear_module_cnt(ctx->dsp);
}

/*
 * Topology core widget load callback
 *
 * This is used to save the private data for each widget which gives
 * information to the driver about module and pipeline parameters which DSP
 * FW expects like ids, resource values, formats etc
 */
static int skl_tplg_widget_load(struct snd_soc_component *cmpnt, int index,
				struct snd_soc_dapm_widget *w,
				struct snd_soc_tplg_dapm_widget *tplg_w)
{
	int ret, i;
	struct hdac_ext_bus *ebus = snd_soc_component_get_drvdata(cmpnt);
	struct skl *skl = ebus_to_skl(ebus);
	struct hdac_bus *bus = ebus_to_hbus(ebus);
	struct skl_module_cfg *mconfig;

	if (!tplg_w->priv.size)
		goto bind_event;

	mconfig = devm_kzalloc(bus->dev, sizeof(*mconfig), GFP_KERNEL);

	if (!mconfig)
		return -ENOMEM;

	if (skl->nr_modules == 0) {
		mconfig->module = devm_kzalloc(bus->dev,
				sizeof(*mconfig->module), GFP_KERNEL);
		if (!mconfig->module)
			return -ENOMEM;
	}

	w->priv = mconfig;

	/*
	 * module binary can be loaded later, so set it to query when
	 * module is load for a use case
	 */
	mconfig->id.module_id = -1;

	/* To provide backward compatibility, set default as SKL_PARAM_INIT */
	mconfig->fmt_cfg_idx = SKL_PARAM_INIT;

	/* Parse private data for tuples */
	ret = skl_tplg_get_pvt_data(tplg_w, skl, bus->dev, mconfig);
	if (ret < 0)
		return ret;

	if (mconfig->m_type == SKL_MODULE_TYPE_GAIN) {
		mconfig->gain_data = devm_kzalloc(bus->dev,
				sizeof(*mconfig->gain_data), GFP_KERNEL);

		if (!mconfig->gain_data)
			return -ENOMEM;

		mconfig->gain_data->ramp_duration = 0;
		mconfig->gain_data->ramp_type = SKL_CURVE_NONE;
		for (i = 0; i < MAX_NUM_CHANNELS; i++)
			mconfig->gain_data->volume[i] = SKL_MAX_GAIN;
	}

	skl_debug_init_module(skl->debugfs, w, mconfig);

bind_event:
	if (tplg_w->event_type == 0) {
		dev_dbg(bus->dev, "ASoC: No event handler required\n");
		return 0;
	}

	ret = snd_soc_tplg_widget_bind_event(w, skl_tplg_widget_ops,
					ARRAY_SIZE(skl_tplg_widget_ops),
					tplg_w->event_type);

	if (ret) {
		dev_err(bus->dev, "%s: No matching event handlers found for %d\n",
					__func__, tplg_w->event_type);
		return -EINVAL;
	}

	return 0;
}

static int skl_init_algo_data(struct device *dev, struct soc_bytes_ext *be,
					struct snd_soc_tplg_bytes_control *bc)
{
	struct skl_algo_data *ac;
	struct skl_dfw_algo_data *dfw_ac =
				(struct skl_dfw_algo_data *)bc->priv.data;

	ac = devm_kzalloc(dev, sizeof(*ac), GFP_KERNEL);
	if (!ac)
		return -ENOMEM;

	/* Fill private data */
	ac->max = dfw_ac->max;
	ac->param_id = dfw_ac->param_id;
	ac->set_params = dfw_ac->set_params;
	ac->size = dfw_ac->max;

	if (ac->max) {
		ac->params = (char *) devm_kzalloc(dev, ac->max, GFP_KERNEL);
		if (!ac->params)
			return -ENOMEM;

		memcpy(ac->params, dfw_ac->params, ac->max);
	}

	be->dobj.private  = ac;
	return 0;
}

static int skl_init_enum_data(struct device *dev, struct soc_enum *se,
				struct snd_soc_tplg_enum_control *ec)
{

	void *data;

	if (ec->priv.size) {
		data = devm_kzalloc(dev, sizeof(ec->priv.size), GFP_KERNEL);
		if (!data)
			return -ENOMEM;
		memcpy(data, ec->priv.data, ec->priv.size);
		se->dobj.private = data;
	}

	return 0;

}

static int skl_tplg_control_load(struct snd_soc_component *cmpnt,
				int index,
				struct snd_kcontrol_new *kctl,
				struct snd_soc_tplg_ctl_hdr *hdr)
{
	struct soc_bytes_ext *sb;
	struct snd_soc_tplg_bytes_control *tplg_bc;
	struct snd_soc_tplg_enum_control *tplg_ec;
	struct hdac_ext_bus *ebus  = snd_soc_component_get_drvdata(cmpnt);
	struct hdac_bus *bus = ebus_to_hbus(ebus);
	struct soc_enum *se;

	switch (hdr->ops.info) {
	case SND_SOC_TPLG_CTL_BYTES:
		tplg_bc = container_of(hdr,
				struct snd_soc_tplg_bytes_control, hdr);
		if (kctl->access & SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK) {
			sb = (struct soc_bytes_ext *)kctl->private_value;
			if (tplg_bc->priv.size)
				return skl_init_algo_data(
						bus->dev, sb, tplg_bc);
		}
		break;

	case SND_SOC_TPLG_CTL_ENUM:
		tplg_ec = container_of(hdr,
				struct snd_soc_tplg_enum_control, hdr);
		if (kctl->access & SNDRV_CTL_ELEM_ACCESS_READWRITE) {
			se = (struct soc_enum *)kctl->private_value;
			if (tplg_ec->priv.size)
				return skl_init_enum_data(bus->dev, se,
						tplg_ec);
		}
		break;

	default:
		dev_warn(bus->dev, "Control load not supported %d:%d:%d\n",
			hdr->ops.get, hdr->ops.put, hdr->ops.info);
		break;
	}

	return 0;
}

static int skl_tplg_fill_str_mfest_tkn(struct device *dev,
		struct snd_soc_tplg_vendor_string_elem *str_elem,
		struct skl *skl)
{
	int tkn_count = 0;
	static int ref_count;

	switch (str_elem->token) {
	case SKL_TKN_STR_LIB_NAME:
		if (ref_count > skl->skl_sst->lib_count - 1) {
			ref_count = 0;
			return -EINVAL;
		}

		strncpy(skl->skl_sst->lib_info[ref_count].name,
			str_elem->string,
			ARRAY_SIZE(skl->skl_sst->lib_info[ref_count].name));
		ref_count++;
		break;

	default:
		dev_err(dev, "Not a string token %d\n", str_elem->token);
		break;
	}
	tkn_count++;

	return tkn_count;
}

static int skl_tplg_get_str_tkn(struct device *dev,
		struct snd_soc_tplg_vendor_array *array,
		struct skl *skl)
{
	int tkn_count = 0, ret;
	struct snd_soc_tplg_vendor_string_elem *str_elem;

	str_elem = (struct snd_soc_tplg_vendor_string_elem *)array->value;
	while (tkn_count < array->num_elems) {
		ret = skl_tplg_fill_str_mfest_tkn(dev, str_elem, skl);
		str_elem++;

		if (ret < 0)
			return ret;

		tkn_count = tkn_count + ret;
	}

	return tkn_count;
}

static int skl_tplg_mfest_fill_dmactrl(struct device *dev,
		struct skl_dmactrl_config *dmactrl_cfg,
		struct snd_soc_tplg_vendor_value_elem *tkn_elem)
{

	u32 cfg_idx = dmactrl_cfg->idx;
	struct skl_dmctrl_hdr *hdr = &dmactrl_cfg->hdr[cfg_idx];

	switch (tkn_elem->token) {
	case SKL_TKN_U32_FMT_CH:
		hdr->ch = tkn_elem->value;
		break;

	case SKL_TKN_U32_FMT_FREQ:
		hdr->freq = tkn_elem->value;
		break;

	case SKL_TKN_U32_FMT_BIT_DEPTH:
		hdr->fmt = tkn_elem->value;
		break;

	case SKL_TKN_U32_PIPE_DIRECTION:
		hdr->direction = tkn_elem->value;
		break;

	case SKL_TKN_U8_TIME_SLOT:
		hdr->tdm_slot = tkn_elem->value;
		break;

	case SKL_TKN_U32_VBUS_ID:
		hdr->vbus_id = tkn_elem->value;
		break;

	case SKL_TKN_U32_DMACTRL_CFG_IDX:
		dmactrl_cfg->idx  = tkn_elem->value;
		break;

	case SKL_TKN_U32_DMACTRL_CFG_SIZE:
		if (tkn_elem->value && !hdr->data) {
			hdr->data = devm_kzalloc(dev,
				tkn_elem->value, GFP_KERNEL);
			if (!hdr->data)
				return -ENOMEM;
			hdr->data_size = tkn_elem->value;
		} else {
			hdr->data_size = 0;
			dev_err(dev, "Invalid dmactrl info \n");
		}
		break;
	default:
		dev_err(dev, "Invalid token %d\n", tkn_elem->token);
		return -EINVAL;
	}

	return 0;
}

static int skl_tplg_manifest_fill_fmt(struct device *dev,
		struct skl_module_iface *fmt,
		struct snd_soc_tplg_vendor_value_elem *tkn_elem,
		u32 dir, int fmt_idx)
{
	struct skl_module_pin_fmt *dst_fmt;
	struct skl_module_fmt *mod_fmt;
	int ret;

	if (!fmt)
		return -EINVAL;

	switch (dir) {
	case SKL_DIR_IN:
		dst_fmt = &fmt->inputs[fmt_idx];
		break;

	case SKL_DIR_OUT:
		dst_fmt = &fmt->outputs[fmt_idx];
		break;

	default:
		dev_err(dev, "Invalid direction: %d\n", dir);
		return -EINVAL;
	}

	mod_fmt = &dst_fmt->fmt;

	switch (tkn_elem->token) {
	case SKL_TKN_MM_U32_INTF_PIN_ID:
		dst_fmt->id = tkn_elem->value;
		break;

	default:
		ret = skl_tplg_fill_fmt(dev, mod_fmt, tkn_elem->token,
					tkn_elem->value);
		if (ret < 0)
			return ret;
		break;
	}

	return 0;
}

static int skl_tplg_fill_mod_info(struct device *dev,
		struct snd_soc_tplg_vendor_value_elem *tkn_elem,
		struct skl_module *mod)
{

	if (!mod)
		return -EINVAL;

	switch (tkn_elem->token) {
	case SKL_TKN_U8_IN_PIN_TYPE:
		mod->input_pin_type = tkn_elem->value;
		break;

	case SKL_TKN_U8_OUT_PIN_TYPE:
		mod->output_pin_type = tkn_elem->value;
		break;

	case SKL_TKN_U8_IN_QUEUE_COUNT:
		mod->max_input_pins = tkn_elem->value;
		break;

	case SKL_TKN_U8_OUT_QUEUE_COUNT:
		mod->max_output_pins = tkn_elem->value;
		break;

	case SKL_TKN_MM_U8_NUM_RES:
		mod->nr_resources = tkn_elem->value;
		break;

	case SKL_TKN_MM_U8_NUM_INTF:
		mod->nr_interfaces = tkn_elem->value;
		break;

	default:
		dev_err(dev, "Invalid mod info token %d", tkn_elem->token);
		return -EINVAL;
	}

	return 0;
}


static int skl_tplg_get_int_tkn(struct device *dev,
		struct snd_soc_tplg_vendor_value_elem *tkn_elem,
		struct skl *skl)
{
	int tkn_count = 0, ret, size;
	static int mod_idx, res_val_idx, intf_val_idx, dir, pin_idx;
	static int dma_cfg_idx;
	struct skl_module_res *res = NULL;
	struct skl_module_iface *fmt = NULL;
	struct skl_module *mod = NULL;
	static struct skl_astate_config *astate_table;
	static int astate_cfg_idx, count;
	int i;

	if (skl->modules) {
		mod = skl->modules[mod_idx];
		res = &mod->resources[res_val_idx];
		fmt = &mod->formats[intf_val_idx];
	}

	switch (tkn_elem->token) {
	case SKL_TKN_U32_LIB_COUNT:
		skl->skl_sst->lib_count = tkn_elem->value;
		break;

	case SKL_TKN_U8_NUM_MOD:
		skl->nr_modules = tkn_elem->value;
		skl->modules = devm_kcalloc(dev, skl->nr_modules,
				sizeof(*skl->modules), GFP_KERNEL);
		if (!skl->modules)
			return -ENOMEM;

		for (i = 0; i < skl->nr_modules; i++) {
			skl->modules[i] = devm_kzalloc(dev,
					sizeof(struct skl_module), GFP_KERNEL);
			if (!skl->modules[i])
				return -ENOMEM;
		}
		break;

	case SKL_TKN_MM_U8_MOD_IDX:
		mod_idx = tkn_elem->value;
		break;

	case SKL_TKN_U32_ASTATE_COUNT:
		if (astate_table != NULL) {
			dev_err(dev, "More than one entry for A-State count");
			return -EINVAL;
		}

		if (tkn_elem->value > SKL_MAX_ASTATE_CFG) {
			dev_err(dev, "Invalid A-State count %d\n",
				tkn_elem->value);
			return -EINVAL;
		}

		size = tkn_elem->value * sizeof(struct skl_astate_config) +
				sizeof(count);
		skl->cfg.astate_cfg = devm_kzalloc(dev, size, GFP_KERNEL);
		if (!skl->cfg.astate_cfg)
			return -ENOMEM;

		astate_table = skl->cfg.astate_cfg->astate_table;
		count = skl->cfg.astate_cfg->count = tkn_elem->value;
		break;

	case SKL_TKN_U32_ASTATE_IDX:
		if (tkn_elem->value >= count) {
			dev_err(dev, "Invalid A-State index %d\n",
				tkn_elem->value);
			return -EINVAL;
		}

		astate_cfg_idx = tkn_elem->value;
		break;

	case SKL_TKN_U32_ASTATE_KCPS:
		astate_table[astate_cfg_idx].kcps = tkn_elem->value;
		break;

	case SKL_TKN_U32_ASTATE_CLK_SRC:
		astate_table[astate_cfg_idx].clk_src = tkn_elem->value;
		break;

	case SKL_TKN_U32_DMA_TYPE:
		skl->cfg.dmacfg.type = tkn_elem->value;
		break;

	case SKL_TKN_U32_DMA_SIZE:
		skl->cfg.dmacfg.size = tkn_elem->value;
		break;

	case SKL_TKN_U32_DMA_IDX:
		dma_cfg_idx = tkn_elem->value;
		break;

	case SKL_TKN_U32_DMA_MIN_SIZE:
		skl->cfg.dmacfg.dma_cfg[dma_cfg_idx].min_size =
							tkn_elem->value;
		break;

	case SKL_TKN_U32_DMA_MAX_SIZE:
		skl->cfg.dmacfg.dma_cfg[dma_cfg_idx].max_size =
							tkn_elem->value;
		break;

	case SKL_TKN_U8_IN_PIN_TYPE:
	case SKL_TKN_U8_OUT_PIN_TYPE:
	case SKL_TKN_U8_IN_QUEUE_COUNT:
	case SKL_TKN_U8_OUT_QUEUE_COUNT:
	case SKL_TKN_MM_U8_NUM_RES:
	case SKL_TKN_MM_U8_NUM_INTF:
		ret = skl_tplg_fill_mod_info(dev, tkn_elem, mod);
		if (ret < 0)
			return ret;
		break;

	case SKL_TKN_U32_DIR_PIN_COUNT:
		dir = tkn_elem->value & SKL_IN_DIR_BIT_MASK;
		pin_idx = (tkn_elem->value & SKL_PIN_COUNT_MASK) >> 4;
		break;

	case SKL_TKN_MM_U32_RES_ID:
		if (!res)
			return -EINVAL;

		res->id = tkn_elem->value;
		res_val_idx = tkn_elem->value;
		break;

	case SKL_TKN_MM_U32_FMT_ID:
		if (!fmt)
			return -EINVAL;

		fmt->fmt_idx = tkn_elem->value;
		intf_val_idx = tkn_elem->value;
		break;

	case SKL_TKN_MM_U32_CPS:
	case SKL_TKN_MM_U32_DMA_SIZE:
	case SKL_TKN_MM_U32_CPC:
	case SKL_TKN_U32_MEM_PAGES:
	case SKL_TKN_U32_OBS:
	case SKL_TKN_U32_IBS:
	case SKL_TKN_MM_U32_RES_PIN_ID:
	case SKL_TKN_MM_U32_PIN_BUF:
		ret = skl_tplg_fill_res_tkn(dev, tkn_elem, res, pin_idx, dir);
		if (ret < 0)
			return ret;

		break;

	case SKL_TKN_MM_U32_NUM_IN_FMT:
		if (!fmt)
			return -EINVAL;

		res->nr_input_pins = tkn_elem->value;
		break;

	case SKL_TKN_MM_U32_NUM_OUT_FMT:
		if (!fmt)
			return -EINVAL;

		res->nr_output_pins = tkn_elem->value;
		break;

	case SKL_TKN_U32_FMT_CH:
	case SKL_TKN_U32_FMT_FREQ:
	case SKL_TKN_U32_FMT_BIT_DEPTH:
	case SKL_TKN_U32_FMT_SAMPLE_SIZE:
	case SKL_TKN_U32_FMT_CH_CONFIG:
	case SKL_TKN_U32_FMT_INTERLEAVE:
	case SKL_TKN_U32_FMT_SAMPLE_TYPE:
	case SKL_TKN_U32_FMT_CH_MAP:
	case SKL_TKN_MM_U32_INTF_PIN_ID:
	case SKL_TKN_U32_PIPE_DIRECTION:
	case SKL_TKN_U8_TIME_SLOT:
	case SKL_TKN_U32_VBUS_ID:
	case SKL_TKN_U32_DMACTRL_CFG_IDX:
	case SKL_TKN_U32_DMACTRL_CFG_SIZE:
		if (skl->modules)
			ret = skl_tplg_manifest_fill_fmt(dev, fmt, tkn_elem,
							 dir, pin_idx);
		else
			ret = skl_tplg_mfest_fill_dmactrl(dev, &skl->cfg.dmactrl_cfg,
					 tkn_elem);
		if (ret < 0)
			return ret;
		break;

	default:
		dev_err(dev, "Not a manifest token %d\n", tkn_elem->token);
		return -EINVAL;
	}
	tkn_count++;

	return tkn_count;
}

static int skl_tplg_get_manifest_uuid(struct device *dev,
				struct skl *skl,
				struct snd_soc_tplg_vendor_uuid_elem *uuid_tkn)
{
	static int ref_count;
	struct skl_module *mod;

	if (uuid_tkn->token == SKL_TKN_UUID) {
		mod = skl->modules[ref_count];
		memcpy(&mod->uuid, &uuid_tkn->uuid, sizeof(uuid_tkn->uuid));
		ref_count++;
	} else {
		dev_err(dev, "Not an UUID token tkn %d\n", uuid_tkn->token);
		return -EINVAL;
	}

	return 0;
}

/*
 * Fill the manifest structure by parsing the tokens based on the
 * type.
 */
static int skl_tplg_get_manifest_tkn(struct device *dev,
		char *pvt_data, struct skl *skl,
		int block_size)
{
	int tkn_count = 0, ret;
	int off = 0, tuple_size = 0;
	struct snd_soc_tplg_vendor_array *array;
	struct snd_soc_tplg_vendor_value_elem *tkn_elem;

	if (block_size <= 0)
		return -EINVAL;

	while (tuple_size < block_size) {
		array = (struct snd_soc_tplg_vendor_array *)(pvt_data + off);
		off += array->size;
		switch (array->type) {
		case SND_SOC_TPLG_TUPLE_TYPE_STRING:
			ret = skl_tplg_get_str_tkn(dev, array, skl);

			if (ret < 0)
				return ret;
			tkn_count = ret;

			tuple_size += tkn_count *
				sizeof(struct snd_soc_tplg_vendor_string_elem);
			continue;

		case SND_SOC_TPLG_TUPLE_TYPE_UUID:
			ret = skl_tplg_get_manifest_uuid(dev, skl, array->uuid);
			if (ret < 0)
				return ret;

			tuple_size += sizeof(*array->uuid);
			continue;

		default:
			tkn_elem = array->value;
			tkn_count = 0;
			break;
		}

		while (tkn_count <= array->num_elems - 1) {
			ret = skl_tplg_get_int_tkn(dev,
					tkn_elem, skl);
			if (ret < 0)
				return ret;

			tkn_count = tkn_count + ret;
			tkn_elem++;
		}
		tuple_size += (tkn_count * sizeof(*tkn_elem));
		tkn_count = 0;
	}

	return off;
}

/*
 * Parse manifest private data for tokens. The private data block is
 * preceded by descriptors for type and size of data block.
 */
static int skl_tplg_get_manifest_data(struct snd_soc_tplg_manifest *manifest,
			struct device *dev, struct skl *skl)
{
	struct snd_soc_tplg_vendor_array *array;
	int num_blocks, block_size = 0, block_type, off = 0;
	struct skl_dmctrl_hdr *dmactrl_hdr;
	int cfg_idx, ret;
	char *data;

	/* Read the NUM_DATA_BLOCKS descriptor */
	array = (struct snd_soc_tplg_vendor_array *)manifest->priv.data;
	ret = skl_tplg_get_desc_blocks(dev, array);
	if (ret < 0)
		return ret;
	num_blocks = ret;

	off += array->size;
	/* Read the BLOCK_TYPE and BLOCK_SIZE descriptor */
	while (num_blocks > 0) {
		array = (struct snd_soc_tplg_vendor_array *)
				(manifest->priv.data + off);
		ret = skl_tplg_get_desc_blocks(dev, array);

		if (ret < 0)
			return ret;
		block_type = ret;
		off += array->size;

		array = (struct snd_soc_tplg_vendor_array *)
			(manifest->priv.data + off);

		ret = skl_tplg_get_desc_blocks(dev, array);

		if (ret < 0)
			return ret;
		block_size = ret;
		off += array->size;

		array = (struct snd_soc_tplg_vendor_array *)
			(manifest->priv.data + off);

		data = (manifest->priv.data + off);

		if (block_type == SKL_TYPE_TUPLE) {
			ret = skl_tplg_get_manifest_tkn(dev, data, skl,
					block_size);

			if (ret < 0)
				return ret;

			--num_blocks;
		} else {
			cfg_idx = skl->cfg.dmactrl_cfg.idx;
			if (cfg_idx < SKL_MAX_DMACTRL_CFG) {
				dmactrl_hdr = &skl->cfg.dmactrl_cfg.hdr[cfg_idx];
				if (dmactrl_hdr->data && (dmactrl_hdr->data_size == block_size))
					memcpy(dmactrl_hdr->data, data, block_size);
			} else {
				dev_err(dev, "error block_idx value exceeding %d\n", cfg_idx);
				return -EINVAL;
			}
			ret = block_size;
			--num_blocks;
		}
		off += ret;
	}

	return 0;
}

static int skl_manifest_load(struct snd_soc_component *cmpnt, int index,
				struct snd_soc_tplg_manifest *manifest)
{
	struct hdac_ext_bus *ebus = snd_soc_component_get_drvdata(cmpnt);
	struct hdac_bus *bus = ebus_to_hbus(ebus);
	struct skl *skl = ebus_to_skl(ebus);

	/* proceed only if we have private data defined */
	if (manifest->priv.size == 0)
		return 0;

	skl_tplg_get_manifest_data(manifest, bus->dev, skl);

	if (skl->skl_sst->lib_count > SKL_MAX_LIB) {
		dev_err(bus->dev, "Exceeding max Library count. Got:%d\n",
					skl->skl_sst->lib_count);
		return  -EINVAL;
	}

	return 0;
}

/*
 * This function updates the event flag and fucntiona handler for single module
 */
static void skl_update_single_module_event(struct skl *skl,
					struct skl_pipe *pipe)
{
	struct skl_module_cfg *mcfg;
	struct skl_pipe_module *w_module;
	struct snd_soc_dapm_widget *w;

	list_for_each_entry(w_module, &pipe->w_list, node) {
		w = w_module->w;
		mcfg = w->priv;

		if (list_is_singular(&pipe->w_list)) {

			/*
			 * If module pipe order is last then we dont need
			 * POST_PMU, as POST_PMU bind/run sink to source.
			 * For last pipe order there is no sink pipelne.
			 */
			if (skl_get_pipe_order(mcfg) == SKL_LAST_PIPE)
				w->event_flags = SND_SOC_DAPM_PRE_PMU |
						 SND_SOC_DAPM_POST_PMD;
			else
				w->event_flags = SND_SOC_DAPM_PRE_PMU |
						 SND_SOC_DAPM_POST_PMU |
						 SND_SOC_DAPM_POST_PMD;

			w->event = skl_tplg_pga_single_module_event;
		}
	}
}

static struct snd_soc_tplg_ops skl_tplg_ops  = {
	.widget_load = skl_tplg_widget_load,
	.control_load = skl_tplg_control_load,
	.bytes_ext_ops = skl_tlv_ops,
	.bytes_ext_ops_count = ARRAY_SIZE(skl_tlv_ops),
	.io_ops = skl_tplg_kcontrol_ops,
	.io_ops_count = ARRAY_SIZE(skl_tplg_kcontrol_ops),
	.manifest = skl_manifest_load,
	.dai_load = skl_dai_load,
};

/*
 * A pipe can have multiple modules, each of them will be a DAPM widget as
 * well. While managing a pipeline we need to get the list of all the
 * widgets in a pipelines, so this helper - skl_tplg_create_pipe_widget_list()
 * helps to get the SKL type widgets in that pipeline
 */
static int skl_tplg_create_pipe_widget_list(struct snd_soc_platform *platform)
{
	struct snd_soc_dapm_widget *w;
	struct skl_module_cfg *mcfg = NULL;
	struct skl_pipe_module *p_module = NULL;
	struct skl_pipe *pipe;

	list_for_each_entry(w, &platform->component.card->widgets, list) {
		if (is_skl_dsp_widget_type(w, platform->dev) && w->priv) {
			mcfg = w->priv;
			pipe = mcfg->pipe;

			p_module = devm_kzalloc(platform->dev,
						sizeof(*p_module), GFP_KERNEL);
			if (!p_module)
				return -ENOMEM;

			p_module->w = w;
			list_add_tail(&p_module->node, &pipe->w_list);
		}
	}

	return 0;
}

static void skl_tplg_set_pipe_type(struct skl *skl, struct skl_pipe *pipe)
{
	struct skl_pipe_module *w_module;
	struct snd_soc_dapm_widget *w;
	struct skl_module_cfg *mconfig;
	bool host_found = false, link_found = false;

	list_for_each_entry(w_module, &pipe->w_list, node) {
		w = w_module->w;
		mconfig = w->priv;

		if (mconfig->dev_type == SKL_DEVICE_HDAHOST)
			host_found = true;
		else if (mconfig->dev_type != SKL_DEVICE_NONE)
			link_found = true;
	}

	if (host_found && link_found)
		pipe->passthru = true;
	else
		pipe->passthru = false;
}

/* This will be read from topology manifest, currently defined here */
#define SKL_MAX_MCPS 350000000
#define SKL_FW_MAX_MEM 1000000

/*
 * SKL topology init routine
 */
int skl_tplg_init(struct snd_soc_platform *platform, struct hdac_ext_bus *ebus)
{
	int ret;
	const struct firmware *fw;
	struct hdac_bus *bus = ebus_to_hbus(ebus);
	struct skl *skl = ebus_to_skl(ebus);
	struct skl_pipeline *ppl;

	ret = request_firmware(&fw, skl->tplg_name, bus->dev);
	if (ret < 0) {
		dev_err(bus->dev, "tplg fw %s load failed with %d\n",
				skl->tplg_name, ret);
		ret = request_firmware(&fw, "dfw_sst.bin", bus->dev);
		if (ret < 0) {
			dev_err(bus->dev, "Fallback tplg fw %s load failed with %d\n",
					"dfw_sst.bin", ret);
			return ret;
		}
	}

	/*
	 * The complete tplg for SKL is loaded as index 0, we don't use
	 * any other index
	 */
	ret = snd_soc_tplg_component_load(&platform->component,
					&skl_tplg_ops, fw, 0);
	if (ret < 0) {
		dev_err(bus->dev, "tplg component load failed%d\n", ret);
		release_firmware(fw);
		return -EINVAL;
	}

	skl->resource.max_mcps = SKL_MAX_MCPS;
	skl->resource.max_mem = SKL_FW_MAX_MEM;

	skl->tplg = fw;
	ret = skl_tplg_create_pipe_widget_list(platform);
	if (ret < 0)
		return ret;

	list_for_each_entry(ppl, &skl->ppl_list, node)
		skl_tplg_set_pipe_type(skl, ppl->pipe);

	list_for_each_entry(ppl, &skl->ppl_list, node)
		skl_update_single_module_event(skl, ppl->pipe);

	return 0;
}
