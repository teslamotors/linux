// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2015 - 2018 Intel Corporation
 * Copyright (C) ON Semiconductor
 *
 */

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include "lc898122.h"
#include "lc898122-oisinit.h"
#include "lc898122-regs.h"

void lc898122_GyrCon(struct lc898122_device *lc898122_dev, u8);
void lc898122_SetSineWave(struct lc898122_device *lc898122_dev, u8, u8);
void lc898122_StartSineWave(struct lc898122_device *lc898122_dev);
void lc898122_StopSineWave(struct lc898122_device *lc898122_dev);

void lc898122_SetMeasFil(struct lc898122_device *lc898122_dev, u8);
void lc898122_ClrMeasFil(struct lc898122_device *lc898122_dev);

u8 lc898122_TstActMov(struct lc898122_device *lc898122_dev, u8);

void lc898122_StbOnn(struct lc898122_device *lc898122_dev);
void lc898122_StbOnnN(struct lc898122_device *lc898122_dev, u8 UcStbY,
		u8 UcStbX);
void lc898122_SetGcf(struct lc898122_device *lc898122_dev, u8	UcSetNum);


#define		LC898122_MAXLMT_20M		0x40400000
#define		LC898122_MINLMT_20M		0x40000000
#define		LC898122_CHGCOEF_20M		0xB9400000
#define		LC898122_MINLMT_MOV_20M	0x00000000
#define		LC898122_CHGCOEF_MOV_20M	0xB8800000

#define		LC898122_MAXLMT_13M		0x40333333
#define		LC898122_MINLMT_13M		0x3FC51EB9
#define		LC898122_CHGCOEF_13M		0xBA120820
#define		LC898122_MINLMT_MOV_13M	0x00000000
#define		LC898122_CHGCOEF_MOV_13M	0xB92B6DB7

#define		LC898122_ZOOMTBL	16
const u32	ClGyxZom[LC898122_ZOOMTBL]	= {
	0x3F800000,
	0x3F800000,
	0x3F800000,
	0x3F800000,
	0x3F800000,
	0x3F800000,
	0x3F800000,
	0x3F800000,
	0x3F800000,
	0x3F800000,
	0x3F800000,
	0x3F800000,
	0x3F800000,
	0x3F800000,
	0x3F800000,
	0x3F800000
};

const u32	ClGyyZom[LC898122_ZOOMTBL]	= {
	0x3F800000,
	0x3F800000,
	0x3F800000,
	0x3F800000,
	0x3F800000,
	0x3F800000,
	0x3F800000,
	0x3F800000,
	0x3F800000,
	0x3F800000,
	0x3F800000,
	0x3F800000,
	0x3F800000,
	0x3F800000,
	0x3F800000,
	0x3F800000
};

#define		LC898122_COEFTBL	7
const u32	ClDiCof_20M[LC898122_COEFTBL]	= {
	LC898122_DIFIL_S2_20M,
	LC898122_DIFIL_S2_20M,
	LC898122_DIFIL_S2_20M,
	LC898122_DIFIL_S2_20M,
	LC898122_DIFIL_S2_20M,
	LC898122_DIFIL_S2_20M,
	LC898122_DIFIL_S2_20M
};
const u32	ClDiCof_13M[LC898122_COEFTBL]	= {
	LC898122_DIFIL_S2_13M,
	LC898122_DIFIL_S2_13M,
	LC898122_DIFIL_S2_13M,
	LC898122_DIFIL_S2_13M,
	LC898122_DIFIL_S2_13M,
	LC898122_DIFIL_S2_13M,
	LC898122_DIFIL_S2_13M
};

void lc898122_MesFil(struct lc898122_device *lc898122_dev, u8	UcMesMod)
{
	struct i2c_client *client = lc898122_dev->client;

	if (lc898122_dev->state.flags & LC898122_EXTCLK_ALL) {
		if (!UcMesMod) {
			RamWrite32A(client, LC898122_mes1aa, 0x3D1E5A40);
			RamWrite32A(client, LC898122_mes1ab, 0x3D1E5A40);
			RamWrite32A(client, LC898122_mes1ac, 0x3F6C34C0);
			RamWrite32A(client, LC898122_mes1ad, 0x00000000);
			RamWrite32A(client, LC898122_mes1ae, 0x00000000);
			RamWrite32A(client, LC898122_mes1ba, 0x3F800000);
			RamWrite32A(client, LC898122_mes1bb, 0x00000000);
			RamWrite32A(client, LC898122_mes1bc, 0x00000000);
			RamWrite32A(client, LC898122_mes1bd, 0x00000000);
			RamWrite32A(client, LC898122_mes1be, 0x00000000);

			RamWrite32A(client, LC898122_mes2aa, 0x3D1E5A40);
			RamWrite32A(client, LC898122_mes2ab, 0x3D1E5A40);
			RamWrite32A(client, LC898122_mes2ac, 0x3F6C34C0);
			RamWrite32A(client, LC898122_mes2ad, 0x00000000);
			RamWrite32A(client, LC898122_mes2ae, 0x00000000);
			RamWrite32A(client, LC898122_mes2ba, 0x3F800000);
			RamWrite32A(client, LC898122_mes2bb, 0x00000000);
			RamWrite32A(client, LC898122_mes2bc, 0x00000000);
			RamWrite32A(client, LC898122_mes2bd, 0x00000000);
			RamWrite32A(client, LC898122_mes2be, 0x00000000);

		} else if (UcMesMod == LC898122_LOOPGAIN) {
			RamWrite32A(client, LC898122_mes1aa, 0x3E587E00);
			RamWrite32A(client, LC898122_mes1ab, 0x3E587E00);
			RamWrite32A(client, LC898122_mes1ac, 0x3F13C100);
			RamWrite32A(client, LC898122_mes1ad, 0x00000000);
			RamWrite32A(client, LC898122_mes1ae, 0x00000000);
			RamWrite32A(client, LC898122_mes1ba, 0x3F7FD400);
			RamWrite32A(client, LC898122_mes1bb, 0xBF7FD400);
			RamWrite32A(client, LC898122_mes1bc, 0x3F7FA840);
			RamWrite32A(client, LC898122_mes1bd, 0x00000000);
			RamWrite32A(client, LC898122_mes1be, 0x00000000);


			RamWrite32A(client, LC898122_mes2aa, 0x3E587E00);
			RamWrite32A(client, LC898122_mes2ab, 0x3E587E00);
			RamWrite32A(client, LC898122_mes2ac, 0x3F13C100);
			RamWrite32A(client, LC898122_mes2ad, 0x00000000);
			RamWrite32A(client, LC898122_mes2ae, 0x00000000);
			RamWrite32A(client, LC898122_mes2ba, 0x3F7FD400);
			RamWrite32A(client, LC898122_mes2bb, 0xBF7FD400);
			RamWrite32A(client, LC898122_mes2bc, 0x3F7FA840);
			RamWrite32A(client, LC898122_mes2bd, 0x00000000);
			RamWrite32A(client, LC898122_mes2be, 0x00000000);

		} else if (UcMesMod == LC898122_THROUGH) {
			RamWrite32A(client, LC898122_mes1aa, 0x3F800000);
			RamWrite32A(client, LC898122_mes1ab, 0x00000000);
			RamWrite32A(client, LC898122_mes1ac, 0x00000000);
			RamWrite32A(client, LC898122_mes1ad, 0x00000000);
			RamWrite32A(client, LC898122_mes1ae, 0x00000000);
			RamWrite32A(client, LC898122_mes1ba, 0x3F800000);
			RamWrite32A(client, LC898122_mes1bb, 0x00000000);
			RamWrite32A(client, LC898122_mes1bc, 0x00000000);
			RamWrite32A(client, LC898122_mes1bd, 0x00000000);
			RamWrite32A(client, LC898122_mes1be, 0x00000000);


			RamWrite32A(client, LC898122_mes2aa, 0x3F800000);
			RamWrite32A(client, LC898122_mes2ab, 0x00000000);
			RamWrite32A(client, LC898122_mes2ac, 0x00000000);
			RamWrite32A(client, LC898122_mes2ad, 0x00000000);
			RamWrite32A(client, LC898122_mes2ae, 0x00000000);
			RamWrite32A(client, LC898122_mes2ba, 0x3F800000);
			RamWrite32A(client, LC898122_mes2bb, 0x00000000);
			RamWrite32A(client, LC898122_mes2bc, 0x00000000);
			RamWrite32A(client, LC898122_mes2bd, 0x00000000);
			RamWrite32A(client, LC898122_mes2be, 0x00000000);

		} else if (UcMesMod == LC898122_NOISE) {
			RamWrite32A(client, LC898122_mes1aa, 0x3D1E5A40);
			RamWrite32A(client, LC898122_mes1ab, 0x3D1E5A40);
			RamWrite32A(client, LC898122_mes1ac, 0x3F6C34C0);
			RamWrite32A(client, LC898122_mes1ad, 0x00000000);
			RamWrite32A(client, LC898122_mes1ae, 0x00000000);
			RamWrite32A(client, LC898122_mes1ba, 0x3D1E5A40);
			RamWrite32A(client, LC898122_mes1bb, 0x3D1E5A40);
			RamWrite32A(client, LC898122_mes1bc, 0x3F6C34C0);
			RamWrite32A(client, LC898122_mes1bd, 0x00000000);
			RamWrite32A(client, LC898122_mes1be, 0x00000000);

			RamWrite32A(client, LC898122_mes2aa, 0x3D1E5A40);
			RamWrite32A(client, LC898122_mes2ab, 0x3D1E5A40);
			RamWrite32A(client, LC898122_mes2ac, 0x3F6C34C0);
			RamWrite32A(client, LC898122_mes2ad, 0x00000000);
			RamWrite32A(client, LC898122_mes2ae, 0x00000000);
			RamWrite32A(client, LC898122_mes2ba, 0x3D1E5A40);
			RamWrite32A(client, LC898122_mes2bb, 0x3D1E5A40);
			RamWrite32A(client, LC898122_mes2bc, 0x3F6C34C0);
			RamWrite32A(client, LC898122_mes2bd, 0x00000000);
			RamWrite32A(client, LC898122_mes2be, 0x00000000);
		}
	} else {
		if (!UcMesMod) {
			RamWrite32A(client, LC898122_mes1aa, 0x3CA175C0);
			RamWrite32A(client, LC898122_mes1ab, 0x3CA175C0);
			RamWrite32A(client, LC898122_mes1ac, 0x3F75E8C0);
			RamWrite32A(client, LC898122_mes1ad, 0x00000000);
			RamWrite32A(client, LC898122_mes1ae, 0x00000000);
			RamWrite32A(client, LC898122_mes1ba, 0x3F800000);
			RamWrite32A(client, LC898122_mes1bb, 0x00000000);
			RamWrite32A(client, LC898122_mes1bc, 0x00000000);
			RamWrite32A(client, LC898122_mes1bd, 0x00000000);
			RamWrite32A(client, LC898122_mes1be, 0x00000000);

			RamWrite32A(client, LC898122_mes2aa, 0x3CA175C0);
			RamWrite32A(client, LC898122_mes2ab, 0x3CA175C0);
			RamWrite32A(client, LC898122_mes2ac, 0x3F75E8C0);
			RamWrite32A(client, LC898122_mes2ad, 0x00000000);
			RamWrite32A(client, LC898122_mes2ae, 0x00000000);
			RamWrite32A(client, LC898122_mes2ba, 0x3F800000);
			RamWrite32A(client, LC898122_mes2bb, 0x00000000);
			RamWrite32A(client, LC898122_mes2bc, 0x00000000);
			RamWrite32A(client, LC898122_mes2bd, 0x00000000);
			RamWrite32A(client, LC898122_mes2be, 0x00000000);

		} else if (UcMesMod == LC898122_LOOPGAIN) {
			RamWrite32A(client, LC898122_mes1aa, 0x3DF21080);
			RamWrite32A(client, LC898122_mes1ab, 0x3DF21080);
			RamWrite32A(client, LC898122_mes1ac, 0x3F437BC0);
			RamWrite32A(client, LC898122_mes1ad, 0x00000000);
			RamWrite32A(client, LC898122_mes1ae, 0x00000000);
			RamWrite32A(client, LC898122_mes1ba, 0x3F7EF980);
			RamWrite32A(client, LC898122_mes1bb, 0xBF7EF980);
			RamWrite32A(client, LC898122_mes1bc, 0x3F7DF300);
			RamWrite32A(client, LC898122_mes1bd, 0x00000000);
			RamWrite32A(client, LC898122_mes1be, 0x00000000);

			RamWrite32A(client, LC898122_mes2aa, 0x3DF21080);
			RamWrite32A(client, LC898122_mes2ab, 0x3DF21080);
			RamWrite32A(client, LC898122_mes2ac, 0x3F437BC0);
			RamWrite32A(client, LC898122_mes2ad, 0x00000000);
			RamWrite32A(client, LC898122_mes2ae, 0x00000000);
			RamWrite32A(client, LC898122_mes2ba, 0x3F7EF980);
			RamWrite32A(client, LC898122_mes2bb, 0xBF7EF980);
			RamWrite32A(client, LC898122_mes2bc, 0x3F7DF300);
			RamWrite32A(client, LC898122_mes2bd, 0x00000000);
			RamWrite32A(client, LC898122_mes2be, 0x00000000);
		} else if (UcMesMod == LC898122_THROUGH) {
			RamWrite32A(client, LC898122_mes1aa, 0x3F800000);
			RamWrite32A(client, LC898122_mes1ab, 0x00000000);
			RamWrite32A(client, LC898122_mes1ac, 0x00000000);
			RamWrite32A(client, LC898122_mes1ad, 0x00000000);
			RamWrite32A(client, LC898122_mes1ae, 0x00000000);
			RamWrite32A(client, LC898122_mes1ba, 0x3F800000);
			RamWrite32A(client, LC898122_mes1bb, 0x00000000);
			RamWrite32A(client, LC898122_mes1bc, 0x00000000);
			RamWrite32A(client, LC898122_mes1bd, 0x00000000);
			RamWrite32A(client, LC898122_mes1be, 0x00000000);


			RamWrite32A(client, LC898122_mes2aa, 0x3F800000);
			RamWrite32A(client, LC898122_mes2ab, 0x00000000);
			RamWrite32A(client, LC898122_mes2ac, 0x00000000);
			RamWrite32A(client, LC898122_mes2ad, 0x00000000);
			RamWrite32A(client, LC898122_mes2ae, 0x00000000);
			RamWrite32A(client, LC898122_mes2ba, 0x3F800000);
			RamWrite32A(client, LC898122_mes2bb, 0x00000000);
			RamWrite32A(client, LC898122_mes2bc, 0x00000000);
			RamWrite32A(client, LC898122_mes2bd, 0x00000000);
			RamWrite32A(client, LC898122_mes2be, 0x00000000);
		} else if (UcMesMod == LC898122_NOISE) {
			RamWrite32A(client, LC898122_mes1aa, 0x3CA175C0);
			RamWrite32A(client, LC898122_mes1ab, 0x3CA175C0);
			RamWrite32A(client, LC898122_mes1ac, 0x3F75E8C0);
			RamWrite32A(client, LC898122_mes1ad, 0x00000000);
			RamWrite32A(client, LC898122_mes1ae, 0x00000000);
			RamWrite32A(client, LC898122_mes1ba, 0x3CA175C0);
			RamWrite32A(client, LC898122_mes1bb, 0x3CA175C0);
			RamWrite32A(client, LC898122_mes1bc, 0x3F75E8C0);
			RamWrite32A(client, LC898122_mes1bd, 0x00000000);
			RamWrite32A(client, LC898122_mes1be, 0x00000000);

			RamWrite32A(client, LC898122_mes2aa, 0x3CA175C0);
			RamWrite32A(client, LC898122_mes2ab, 0x3CA175C0);
			RamWrite32A(client, LC898122_mes2ac, 0x3F75E8C0);
			RamWrite32A(client, LC898122_mes2ad, 0x00000000);
			RamWrite32A(client, LC898122_mes2ae, 0x00000000);
			RamWrite32A(client, LC898122_mes2ba, 0x3CA175C0);
			RamWrite32A(client, LC898122_mes2bb, 0x3CA175C0);
			RamWrite32A(client, LC898122_mes2bc, 0x3F75E8C0);
			RamWrite32A(client, LC898122_mes2bd, 0x00000000);
			RamWrite32A(client, LC898122_mes2be, 0x00000000);
		}
	}
}

void lc898122_SrvCon(struct lc898122_device *lc898122_dev, u8	UcDirSel,
	    u8	UcSwcCon)
{
	struct i2c_client *client = lc898122_dev->client;

	if (UcSwcCon) {
		if (!UcDirSel) {
			RegWriteA(client, LC898122_WH_EQSWX, 0x03);
			RamWrite32A(client, LC898122_sxggf, 0x00000000);
		} else {
			RegWriteA(client, LC898122_WH_EQSWY, 0x03);
			RamWrite32A(client, LC898122_syggf, 0x00000000);
		}
	} else {
		if (!UcDirSel) {
			RegWriteA(client, LC898122_WH_EQSWX, 0x02);
			RamWrite32A(client, LC898122_SXLMT, 0x00000000);
		} else {
			RegWriteA(client, LC898122_WH_EQSWY, 0x02);
			RamWrite32A(client, LC898122_SYLMT, 0x00000000);
		}
	}
}

u8 lc898122_RtnCen(struct lc898122_device *lc898122_dev,
		     u8 UcCmdPar)
{
	u8	UcCmdSts = LC898122_EXE_END;

	lc898122_GyrCon(lc898122_dev, OFF);

	if (!UcCmdPar) {
		lc898122_StbOnn(lc898122_dev);
	} else if (UcCmdPar == 0x01) {
		lc898122_SrvCon(lc898122_dev, LC898122_X_DIR, ON);
		lc898122_SrvCon(lc898122_dev, LC898122_Y_DIR, OFF);
	} else if (UcCmdPar == 0x02) {
		lc898122_SrvCon(lc898122_dev, LC898122_X_DIR, OFF);
		lc898122_SrvCon(lc898122_dev, LC898122_Y_DIR, ON);
	}

	return UcCmdSts;
}

void lc898122_GyrCon(struct lc898122_device *lc898122_dev, u8 UcGyrCon)
{
	struct i2c_client *client = lc898122_dev->client;

	RegWriteA(client, LC898122_WG_SHTON, 0x00);

	if (UcGyrCon == ON) {

		if (lc898122_dev->state.flags & LC898122_GAIN_CONT)
			lc898122_autogaincontrol(lc898122_dev, ON);

		lc898122_cleargyro(lc898122_dev, 0x000E, LC898122_CLR_FRAM1);

		RamWrite32A(client, LC898122_sxggf, 0x3F800000);
		RamWrite32A(client, LC898122_syggf, 0x3F800000);

	} else if (UcGyrCon == SPC) {

		if (lc898122_dev->state.flags & LC898122_GAIN_CONT)
			lc898122_autogaincontrol(lc898122_dev, ON);

		RamWrite32A(client, LC898122_sxggf, 0x3F800000);
		RamWrite32A(client, LC898122_syggf, 0x3F800000);
	} else {
		RamWrite32A(client, LC898122_sxggf, 0x00000000);
		RamWrite32A(client, LC898122_syggf, 0x00000000);

		if (lc898122_dev->state.flags & LC898122_GAIN_CONT)
			lc898122_autogaincontrol(lc898122_dev, OFF);
	}
}

void lc898122_OisEna(struct lc898122_device *lc898122_dev)
{
	lc898122_SrvCon(lc898122_dev, LC898122_X_DIR, ON);
	lc898122_SrvCon(lc898122_dev, LC898122_Y_DIR, ON);

	lc898122_GyrCon(lc898122_dev, ON);
}

void OisEnaLin(struct lc898122_device *lc898122_dev)
{
	lc898122_SrvCon(lc898122_dev, LC898122_X_DIR, ON);
	lc898122_SrvCon(lc898122_dev, LC898122_Y_DIR, ON);

	lc898122_GyrCon(lc898122_dev, SPC);
}

void lc898122_S2cPro(struct lc898122_device *lc898122_dev, u8 uc_mode)
{
	struct i2c_client *client = lc898122_dev->client;
	u32	DIFIL_S2;

	if (lc898122_dev->state.UcModule == LC898122_MODULE_20M)
		DIFIL_S2	= LC898122_DIFIL_S2_20M;
	else
		DIFIL_S2	= LC898122_DIFIL_S2_13M;

	if (uc_mode == 1) {
		if (lc898122_dev->state.flags & LC898122_H1COEF_CHANGER)
			lc898122_SetH1cMod(lc898122_dev, LC898122_S2MODE);
		RegWriteA(client, LC898122_WG_SHTON, 0x11);
		RamWrite32A(client, LC898122_gxh1c, DIFIL_S2);
		RamWrite32A(client, LC898122_gyh1c, DIFIL_S2);
	} else {
		RamWrite32A(client, LC898122_gxh1c,
				lc898122_dev->state.UlH1Coefval);
		RamWrite32A(client, LC898122_gyh1c,
				lc898122_dev->state.UlH1Coefval);
		RegWriteA(client, LC898122_WG_SHTON, 0x00);

		if (lc898122_dev->state.flags & LC898122_H1COEF_CHANGER)
			lc898122_SetH1cMod(lc898122_dev,
					lc898122_dev->state.UcH1LvlMod);
	}
}

short lc898122_GenMes(struct lc898122_device *lc898122_dev, u16 UsRamAdd,
	     u8 UcMesMod)
{
	struct i2c_client *client = lc898122_dev->client;

	short	SsMesRlt;

	RegWriteA(client, LC898122_WC_MES1ADD0, (u8)UsRamAdd);
	RegWriteA(client, LC898122_WC_MES1ADD1, (u8)((UsRamAdd >> 8) & 0x0001));
	RamWrite32A(client, LC898122_MSABS1AV, 0x00000000);

	if (!UcMesMod) {
		RegWriteA(client, LC898122_WC_MESLOOP1, 0x04);
		RegWriteA(client, LC898122_WC_MESLOOP0, 0x00);
		RamWrite32A(client, LC898122_msmean, 0x3A7FFFF7);
	} else {
		RegWriteA(client, LC898122_WC_MESLOOP1, 0x00);
		RegWriteA(client, LC898122_WC_MESLOOP0, 0x01);
		RamWrite32A(client, LC898122_msmean, 0x3F800000);
	}

	RegWriteA(client, LC898122_WC_MESABS, 0x00);
	lc898122_BsyWit(lc898122_dev, LC898122_WC_MESMODE, 0x01);
	lc898122_RamAccFixMod(lc898122_dev, ON);
	RamReadA(client, LC898122_MSABS1AV, (u16 *)&SsMesRlt);
	lc898122_RamAccFixMod(lc898122_dev, OFF);

	return SsMesRlt;
}

const u16 CucFreqVal_extclk[17] = {
	0xFFFF, /*  0:  Stop */
	0x0059, /*  1: 0.994653Hz */
	0x00B2, /*  2: 1.989305Hz */
	0x010C, /*  3: 2.995133Hz */
	0x0165, /*  4: 3.989786Hz */
	0x01BF, /*  5: 4.995614Hz */
	0x0218, /*  6: 5.990267Hz */
	0x0272, /*  7: 6.996095Hz */
	0x02CB, /*  8: 7.990748Hz */
	0x0325, /*  9: 8.996576Hz */
	0x037E, /*  A: 9.991229Hz */
	0x03D8, /*  B: 10.99706Hz */
	0x0431, /*  C: 11.99171Hz */
	0x048B, /*  D: 12.99754Hz */
	0x04E4, /*  E: 13.99219Hz */
	0x053E, /*  F: 14.99802Hz */
	0x0597  /* 10: 15.99267Hz */
};

const u16 CucFreqVal[17] = {
	0xFFFF, /*  0:  Stop */
	0x002C, /*  1: 0.983477Hz */
	0x0059, /*  2: 1.989305Hz */
	0x0086, /*  3: 2.995133Hz */
	0x00B2, /*  4: 3.97861Hz */
	0x00DF, /*  5: 4.984438Hz */
	0x010C, /*  6: 5.990267Hz */
	0x0139, /*  7: 6.996095Hz */
	0x0165, /*  8: 7.979572Hz */
	0x0192, /*  9: 8.9854Hz */
	0x01BF, /*  A: 9.991229Hz */
	0x01EC, /*  B: 10.99706Hz */
	0x0218, /*  C: 11.98053Hz */
	0x0245, /*  D: 12.98636Hz */
	0x0272, /*  E: 13.99219Hz */
	0x029F, /*  F: 14.99802Hz */
	0x02CB /* 10: 15.9815Hz */
};

void lc898122_SetSinWavePara(struct lc898122_device *lc898122_dev,
		    u8 UcTableVal, u8 UcMethodVal)
{
	struct i2c_client *client = lc898122_dev->client;

	u16	UsFreqDat;
	u8	UcEqSwX, UcEqSwY;

	if (UcTableVal > 0x10)
		UcTableVal = 0x10;			/* Limit */

	if (lc898122_dev->state.flags & LC898122_EXTCLK_ALL)
		UsFreqDat = CucFreqVal_extclk[UcTableVal];
	else
		UsFreqDat = CucFreqVal[UcTableVal];

	if (UcMethodVal == LC898122_SINEWAVE) {
		RegWriteA(client, LC898122_WC_SINPHSX, 0x00);
		RegWriteA(client, LC898122_WC_SINPHSY, 0x00);
	} else if (UcMethodVal == LC898122_CIRCWAVE) {
		RegWriteA(client, LC898122_WC_SINPHSX,	0x00);
		RegWriteA(client, LC898122_WC_SINPHSY,	0x20);
	} else {
		RegWriteA(client, LC898122_WC_SINPHSX, 0x00);
		RegWriteA(client, LC898122_WC_SINPHSY, 0x00);
	}


	if (lc898122_dev->state.flags & LC898122_USE_SINLPF) {
		if (((lc898122_dev->state.flags & LC898122_ACCEPTANCE) &&
		     ((UcMethodVal == LC898122_CIRCWAVE) ||
		      (UcMethodVal == LC898122_SINEWAVE))) ||
		    (!(lc898122_dev->state.flags & LC898122_ACCEPTANCE) &&
		     ((UcMethodVal != LC898122_XHALWAVE) &&
		      (UcMethodVal != LC898122_YHALWAVE)))) {
			lc898122_MesFil(lc898122_dev, LC898122_NOISE);
			/* LPF */
		}
	}


	if (UsFreqDat == 0xFFFF) {
		RegReadA(client, LC898122_WH_EQSWX, &UcEqSwX);
		RegReadA(client, LC898122_WH_EQSWY, &UcEqSwY);
		UcEqSwX &= ~LC898122_EQSINSW;
		UcEqSwY &= ~LC898122_EQSINSW;
		RegWriteA(client, LC898122_WH_EQSWX, UcEqSwX);
		RegWriteA(client, LC898122_WH_EQSWY, UcEqSwY);


	if (lc898122_dev->state.flags & LC898122_USE_SINLPF) {
		if (((lc898122_dev->state.flags & LC898122_ACCEPTANCE) &&
		     ((UcMethodVal == LC898122_CIRCWAVE) ||
		      (UcMethodVal == LC898122_SINEWAVE) ||
		      (UcMethodVal == LC898122_XACTTEST) ||
		      (UcMethodVal == LC898122_YACTTEST))) ||
		    !((lc898122_dev->state.flags & LC898122_ACCEPTANCE) &&
		      (UcMethodVal != LC898122_XHALWAVE) &&
		      (UcMethodVal != LC898122_YHALWAVE))) {
			RegWriteA(client, LC898122_WC_DPON, 0x00);
			RegWriteA(client, LC898122_WC_DPO1ADD0, 0x00);
			RegWriteA(client, LC898122_WC_DPO1ADD1, 0x00);
			RegWriteA(client, LC898122_WC_DPO2ADD0, 0x00);
			RegWriteA(client, LC898122_WC_DPO2ADD1, 0x00);
			RegWriteA(client, LC898122_WC_DPI1ADD0, 0x00);
			RegWriteA(client, LC898122_WC_DPI1ADD1, 0x00);
			RegWriteA(client, LC898122_WC_DPI2ADD0, 0x00);
			RegWriteA(client, LC898122_WC_DPI2ADD1, 0x00);

			lc898122_RamAccFixMod(lc898122_dev, ON);

			RamWriteA(client, LC898122_SXOFFZ1,
					lc898122_dev->state.UsCntXof);
			RamWriteA(client, LC898122_SYOFFZ1,
					lc898122_dev->state.UsCntYof);

			lc898122_RamAccFixMod(lc898122_dev, OFF);

			RegWriteA(client, LC898122_WC_MES1ADD0, 0x00);
			RegWriteA(client, LC898122_WC_MES1ADD1, 0x00);
			RegWriteA(client, LC898122_WC_MES2ADD0, 0x00);
			RegWriteA(client, LC898122_WC_MES2ADD1, 0x00);
		}
	}
	RegWriteA(client, LC898122_WC_SINON, 0x00);

	} else {
		RegReadA(client, LC898122_WH_EQSWX, &UcEqSwX);
		RegReadA(client, LC898122_WH_EQSWY, &UcEqSwY);

		if (((lc898122_dev->state.flags & LC898122_ACCEPTANCE) &&
		     ((UcMethodVal == LC898122_CIRCWAVE) ||
		      (UcMethodVal == LC898122_SINEWAVE))) ||
		    (!(lc898122_dev->state.flags & LC898122_ACCEPTANCE) &&
		     ((UcMethodVal != LC898122_XHALWAVE) &&
		      (UcMethodVal != LC898122_YHALWAVE)))) {

			if (lc898122_dev->state.flags & LC898122_USE_SINLPF) {
				RegWriteA(client, LC898122_WC_DPI1ADD0,
					(u8)LC898122_MES1BZ2);
				RegWriteA(client, LC898122_WC_DPI1ADD1,
					(u8)((LC898122_MES1BZ2 >> 8) & 0x0001));
				RegWriteA(client, LC898122_WC_DPI2ADD0,
					(u8)LC898122_MES2BZ2);
				RegWriteA(client, LC898122_WC_DPI2ADD1,
					(u8)((LC898122_MES2BZ2 >> 8) & 0x0001));
				RegWriteA(client, LC898122_WC_DPO1ADD0,
					(u8)LC898122_SXOFFZ1);
				RegWriteA(client, LC898122_WC_DPO1ADD1,
					(u8)((LC898122_SXOFFZ1 >> 8) & 0x0001));
				RegWriteA(client, LC898122_WC_DPO2ADD0,
					(u8)LC898122_SYOFFZ1);
				RegWriteA(client, LC898122_WC_DPO2ADD1,
					(u8)((LC898122_SYOFFZ1 >> 8) & 0x0001));

				RegWriteA(client, LC898122_WC_MES1ADD0,
					(u8)LC898122_SINXZ);
				RegWriteA(client, LC898122_WC_MES1ADD1,
					(u8)((LC898122_SINXZ >> 8) & 0x0001));
				RegWriteA(client, LC898122_WC_MES2ADD0,
					(u8)LC898122_SINYZ);
				RegWriteA(client, LC898122_WC_MES2ADD1,
					(u8)((LC898122_SINYZ >> 8) & 0x0001));

				RegWriteA(client, LC898122_WC_DPON, 0x03);

				UcEqSwX &= ~LC898122_EQSINSW;
				UcEqSwY &= ~LC898122_EQSINSW;
			} else {
				UcEqSwX |= 0x08;
				UcEqSwY |= 0x08;
			}
		} else if ((lc898122_dev->state.flags & LC898122_ACCEPTANCE) &&
			   ((UcMethodVal == LC898122_XACTTEST) ||
			    (UcMethodVal == LC898122_YACTTEST))) {
			RegWriteA(client, LC898122_WC_DPI2ADD0,
				(u8)LC898122_MES2BZ2);
			RegWriteA(client, LC898122_WC_DPI2ADD1,
				(u8)((LC898122_MES2BZ2 >> 8) & 0x0001));
			if (UcMethodVal == LC898122_XACTTEST) {
				RegWriteA(client, LC898122_WC_DPO2ADD0,
					(u8)LC898122_SXOFFZ1);
				RegWriteA(client, LC898122_WC_DPO2ADD1,
					(u8)((LC898122_SXOFFZ1 >> 8) & 0x0001));
				RegWriteA(client, LC898122_WC_MES2ADD0,
					(u8)LC898122_SINXZ);
				RegWriteA(client, LC898122_WC_MES2ADD1,
					(u8)((LC898122_SINXZ >> 8) & 0x0001));
			} else {
				RegWriteA(client, LC898122_WC_DPO2ADD0,
					(u8)LC898122_SYOFFZ1);
				RegWriteA(client, LC898122_WC_DPO2ADD1,
					(u8)((LC898122_SYOFFZ1 >> 8) & 0x0001));
				RegWriteA(client, LC898122_WC_MES2ADD0,
					(u8)LC898122_SINYZ);
				RegWriteA(client, LC898122_WC_MES2ADD1,
					(u8)((LC898122_SINYZ >> 8) & 0x0001));
			}

			RegWriteA(client, LC898122_WC_DPON, 0x02);

			UcEqSwX &= ~LC898122_EQSINSW;
			UcEqSwY &= ~LC898122_EQSINSW;
		} else {
			if (UcMethodVal == LC898122_XHALWAVE)
				UcEqSwX = 0x22;
			else
				UcEqSwY = 0x22;
		}

		RegWriteA(client, LC898122_WC_SINFRQ0, (u8)UsFreqDat);
		RegWriteA(client, LC898122_WC_SINFRQ1, (u8)(UsFreqDat >> 8));
		RegWriteA(client, LC898122_WC_MESSINMODE, 0x00);

		RegWriteA(client, LC898122_WH_EQSWX, UcEqSwX);
		RegWriteA(client, LC898122_WH_EQSWY, UcEqSwY);

		RegWriteA(client, LC898122_WC_SINON,     0x01);
	}
}

void lc898122_SetStandby(struct lc898122_device *lc898122_dev, u8 UcContMode)
{
	struct i2c_client *client = lc898122_dev->client;
	u8	UcStbb0, UcClkon;

	switch (UcContMode) {
	case LC898122_STB1_ON:
		if (!(lc898122_dev->state.flags & LC898122_AF_PWMMODE))
			RegWriteA(client, LC898122_DRVFCAF, 0x00);
		RegWriteA(client, LC898122_STBB0, 0x00);
		RegWriteA(client, LC898122_STBB1, 0x00);
		RegWriteA(client, LC898122_PWMA, 0x00);
		RegWriteA(client, LC898122_PWMAAF, 0x00);
		RegWriteA(client, LC898122_CVA,  0x00);
		lc898122_driversw(lc898122_dev, OFF);
		lc898122_afdriversw(lc898122_dev, OFF);
		if (!(lc898122_dev->state.flags & LC898122_MONITOR_OFF))
			RegWriteA(client, LC898122_PWMMONA, 0x00);
		lc898122_selectgyrosleep(lc898122_dev, ON);
		break;
	case LC898122_STB1_OFF:
		lc898122_selectgyrosleep(lc898122_dev, OFF);
		RegWriteA(client, LC898122_PWMMONA, 0x80);
		lc898122_driversw(lc898122_dev, ON);
		lc898122_afdriversw(lc898122_dev, ON);
		RegWriteA(client, LC898122_CVA, 0xC0);
		if (lc898122_dev->state.flags & LC898122_AF_PWMMODE)
			RegWriteA(client, LC898122_PWMAAF, 0x80);
		else
			RegWriteA(client, LC898122_PWMAAF, 0x00);
		RegWriteA(client, LC898122_PWMA, 0xC0);
		RegWriteA(client, LC898122_STBB1, 0x05);
		RegWriteA(client, LC898122_STBB0, 0xDF);

		break;
	case  LC898122_STB2_ON:
		if (!(lc898122_dev->state.flags & LC898122_AF_PWMMODE))
			RegWriteA(client, LC898122_DRVFCAF, 0x00);
		RegWriteA(client, LC898122_STBB0, 0x00);
		RegWriteA(client, LC898122_STBB1, 0x00);
		RegWriteA(client, LC898122_PWMA, 0x00);
		RegWriteA(client, LC898122_PWMAAF, 0x00);
		RegWriteA(client, LC898122_CVA,  0x00);
		lc898122_driversw(lc898122_dev, OFF);
		lc898122_afdriversw(lc898122_dev, OFF);
		if (!(lc898122_dev->state.flags & LC898122_MONITOR_OFF))
			RegWriteA(client, LC898122_PWMMONA, 0x00);
		lc898122_selectgyrosleep(lc898122_dev, ON);
		RegWriteA(client, LC898122_CLKON, 0x00);
		break;
	case LC898122_STB2_OFF:
		RegWriteA(client, LC898122_CLKON, 0x1F);
		lc898122_selectgyrosleep(lc898122_dev, OFF);
		RegWriteA(client, LC898122_PWMMONA, 0x80);
		lc898122_driversw(lc898122_dev, ON);
		lc898122_afdriversw(lc898122_dev, ON);
		RegWriteA(client, LC898122_CVA, 0xC0);
		if (lc898122_dev->state.flags & LC898122_AF_PWMMODE)
			RegWriteA(client, LC898122_PWMAAF, 0x80);
		else
			RegWriteA(client, LC898122_PWMAAF,    0x00);
		RegWriteA(client, LC898122_PWMA, 0xC0);
		RegWriteA(client, LC898122_STBB1, 0x05);
		RegWriteA(client, LC898122_STBB0, 0xDF);

		break;
	case LC898122_STB3_ON:

		if (!(lc898122_dev->state.flags & LC898122_AF_PWMMODE))
			RegWriteA(client, LC898122_DRVFCAF, 0x00);
		RegWriteA(client, LC898122_STBB0, 0x00);
		RegWriteA(client, LC898122_STBB1, 0x00);
		RegWriteA(client, LC898122_PWMA, 0x00);
		RegWriteA(client, LC898122_PWMAAF, 0x00);
		RegWriteA(client, LC898122_CVA, 0x00);
		lc898122_driversw(lc898122_dev, OFF);
		lc898122_afdriversw(lc898122_dev, OFF);
		if (!(lc898122_dev->state.flags & LC898122_MONITOR_OFF))
			RegWriteA(client, LC898122_PWMMONA, 0x00);
		lc898122_selectgyrosleep(lc898122_dev, ON);
		RegWriteA(client, LC898122_CLKON, 0x00);
		RegWriteA(client, LC898122_I2CSEL, 0x01);
		RegWriteA(client, LC898122_OSCSTOP, 0x02);
		break;
	case LC898122_STB3_OFF:
		RegWriteA(client, LC898122_OSCSTOP, 0x00);
		RegWriteA(client, LC898122_I2CSEL, 0x00);
		RegWriteA(client, LC898122_CLKON, 0x1F);
		lc898122_selectgyrosleep(lc898122_dev, OFF);
		RegWriteA(client, LC898122_PWMMONA, 0x80);
		lc898122_driversw(lc898122_dev, ON);
		lc898122_afdriversw(lc898122_dev, ON);
		RegWriteA(client, LC898122_CVA, 0xC0);
		if (lc898122_dev->state.flags & LC898122_AF_PWMMODE)
			RegWriteA(client, LC898122_PWMAAF, 0x80);
		else
			RegWriteA(client, LC898122_PWMAAF, 0x00);
		RegWriteA(client, LC898122_PWMA, 0xC0);
		RegWriteA(client, LC898122_STBB1, 0x05);
		RegWriteA(client, LC898122_STBB0, 0xDF);

		break;

	case LC898122_STB4_ON:

		if (!(lc898122_dev->state.flags & LC898122_AF_PWMMODE))
			RegWriteA(client, LC898122_DRVFCAF, 0x00);
		RegWriteA(client, LC898122_STBB0, 0x00);
		RegWriteA(client, LC898122_STBB1, 0x00);
		RegWriteA(client, LC898122_PWMA, 0x00);
		RegWriteA(client, LC898122_PWMAAF, 0x00);
		RegWriteA(client, LC898122_CVA, 0x00);
		lc898122_driversw(lc898122_dev, OFF);
		lc898122_afdriversw(lc898122_dev, OFF);
		if (!(lc898122_dev->state.flags & LC898122_MONITOR_OFF))
			RegWriteA(client, LC898122_PWMMONA, 0x00);
		lc898122_GyOutSignalCont(lc898122_dev);
		RegWriteA(client, LC898122_CLKON, 0x04);
		break;
	case LC898122_STB4_OFF:
		RegWriteA(client, LC898122_CLKON, 0x1F);
		lc898122_selectgyrosleep(lc898122_dev, OFF);
		RegWriteA(client, LC898122_PWMMONA, 0x80);
		lc898122_driversw(lc898122_dev, ON);
		lc898122_afdriversw(lc898122_dev, ON);
		RegWriteA(client, LC898122_CVA, 0xC0);
		if (lc898122_dev->state.flags & LC898122_AF_PWMMODE)
			RegWriteA(client, LC898122_PWMAAF, 0x80);
		else
			RegWriteA(client, LC898122_PWMAAF, 0x00);
		RegWriteA(client, LC898122_PWMA, 0xC0);
		RegWriteA(client, LC898122_STBB1, 0x05);
		RegWriteA(client, LC898122_STBB0, 0xDF);

		break;

		/************** special mode ************/
	case LC898122_STB2_OISON:

		RegReadA(client, LC898122_STBB0, &UcStbb0);
		UcStbb0 &= 0x80;
		RegWriteA(client, LC898122_STBB0, UcStbb0);
		RegWriteA(client, LC898122_PWMA, 0x00);
		RegWriteA(client, LC898122_CVA,  0x00);
		lc898122_driversw(lc898122_dev, OFF);
		if (!(lc898122_dev->state.flags & LC898122_MONITOR_OFF))
			RegWriteA(client, LC898122_PWMMONA, 0x00);
		lc898122_selectgyrosleep(lc898122_dev, ON);
		RegReadA(client, LC898122_CLKON, &UcClkon);
		UcClkon &= 0x1A;
		RegWriteA(client, LC898122_CLKON, UcClkon);
		break;
	case LC898122_STB2_OISOFF:
		RegReadA(client, LC898122_CLKON, &UcClkon);
		UcClkon |= 0x05;
		RegWriteA(client, LC898122_CLKON, UcClkon);
		lc898122_selectgyrosleep(lc898122_dev, OFF);

		RegWriteA(client, LC898122_PWMMONA, 0x80);
		lc898122_driversw(lc898122_dev, ON);
		RegWriteA(client, LC898122_CVA, 0xC0);
		RegWriteA(client, LC898122_PWMA, 0xC0);
		RegReadA(client, LC898122_STBB0, &UcStbb0);
		UcStbb0 |= 0x5F;
		RegWriteA(client, LC898122_STBB0, UcStbb0);

		break;

	case LC898122_STB2_AFON:
		if (!(lc898122_dev->state.flags & LC898122_AF_PWMMODE))
			RegWriteA(client, LC898122_DRVFCAF, 0x00);

		RegReadA(client, LC898122_STBB0, &UcStbb0);
		UcStbb0 &= 0x7F;
		RegWriteA(client, LC898122_STBB0, UcStbb0);
		RegWriteA(client, LC898122_STBB1, 0x00);
		if (lc898122_dev->state.flags & LC898122_AF_PWMMODE)
			RegWriteA(client, LC898122_PWMAAF, 0x80);
		else
			RegWriteA(client, LC898122_PWMAAF, 0x00);
		lc898122_afdriversw(lc898122_dev, OFF);
		if (!(lc898122_dev->state.flags & LC898122_MONITOR_OFF))
			RegWriteA(client, LC898122_PWMMONA, 0x00);
		RegReadA(client, LC898122_CLKON, &UcClkon);
		UcClkon &= 0x07;
		RegWriteA(client, LC898122_CLKON, UcClkon);
		break;
	case LC898122_STB2_AFOFF:
		RegReadA(client, LC898122_CLKON, &UcClkon);
		UcClkon |= 0x18;
		RegWriteA(client, LC898122_CLKON,	UcClkon);
		lc898122_afdriversw(lc898122_dev, ON);
		RegWriteA(client, LC898122_PWMAAF, 0x00);
		RegWriteA(client, LC898122_STBB1, 0x05);
		RegReadA(client, LC898122_STBB0, &UcStbb0);
		UcStbb0 |= 0x80;
		RegWriteA(client, LC898122_STBB0, UcStbb0);

		break;
	}
}

void lc898122_SetZsp(struct lc898122_device *lc898122_dev,
	    u8	UcZoomStepDat)
{
	struct i2c_client *client = lc898122_dev->client;
	u32	UlGyrZmx, UlGyrZmy, UlGyrZrx, UlGyrZry;

	/* Zoom Step */
	if (UcZoomStepDat > (LC898122_ZOOMTBL - 1))
		UcZoomStepDat = (LC898122_ZOOMTBL - 1);

	if (UcZoomStepDat == 0) {
		UlGyrZmx	= ClGyxZom[0];
		UlGyrZmy	= ClGyyZom[0];
	} else {
		UlGyrZmx	= ClGyxZom[UcZoomStepDat];
		UlGyrZmy	= ClGyyZom[UcZoomStepDat];
	}

	RamWrite32A(client, LC898122_gxlens, UlGyrZmx);
	RamWrite32A(client, LC898122_gylens, UlGyrZmy);

	RamRead32A(client, LC898122_gxlens, &UlGyrZrx);
	RamRead32A(client, LC898122_gylens, &UlGyrZry);

	if (UlGyrZmx != UlGyrZrx)
		RamWrite32A(client, LC898122_gxlens, UlGyrZmx);

	if (UlGyrZmy != UlGyrZry)
		RamWrite32A(client, LC898122_gylens, UlGyrZmy);
}

void lc898122_StbOnn(struct lc898122_device *lc898122_dev)
{
	struct i2c_client *client = lc898122_dev->client;
	u8	UcRegValx, UcRegValy;
	u8	UcRegIni;
	u8	UcCnt;

	RegReadA(client, LC898122_WH_EQSWX, &UcRegValx);
	RegReadA(client, LC898122_WH_EQSWY, &UcRegValy);

	if (((UcRegValx & 0x01) != 0x01) && ((UcRegValy & 0x01) != 0x01)) {
		RegWriteA(client, LC898122_WH_SMTSRVON,	0x01);

		lc898122_SrvCon(lc898122_dev, LC898122_X_DIR, ON);
		lc898122_SrvCon(lc898122_dev, LC898122_Y_DIR, ON);

		for (UcCnt = 0; UcCnt < 60; UcCnt++) {
			RegReadA(client, LC898122_RH_SMTSRVSTT,	&UcRegIni);
			if ((UcRegIni & 0x77) == 0x66)
				break;
			msleep(10);
		}

		RegWriteA(client, LC898122_WH_SMTSRVON,	0x00);

		if (UcCnt == 60) {
			RamWriteA(client, LC898122_SXOFFZ2, 0);
			RamWriteA(client, LC898122_SYOFFZ2, 0);
		}
	} else {
		lc898122_SrvCon(lc898122_dev, LC898122_X_DIR, ON);
		lc898122_SrvCon(lc898122_dev, LC898122_Y_DIR, ON);
	}
}

void lc898122_StbOnnN(struct lc898122_device *lc898122_dev, u8 UcStbY,
		u8 UcStbX)
{
	struct i2c_client *client = lc898122_dev->client;

	u8	UcRegIni;
	u8	UcSttMsk = 0;
	u8	UcCnt;

	RegWriteA(client, LC898122_WH_SMTSRVON,	0x01);
	if (UcStbX == ON)
		UcSttMsk |= 0x07;
	if (UcStbY == ON)
		UcSttMsk |= 0x70;

	lc898122_SrvCon(lc898122_dev, LC898122_X_DIR, UcStbX);
	lc898122_SrvCon(lc898122_dev, LC898122_Y_DIR, UcStbY);

	for (UcCnt = 0; UcCnt < 60; UcCnt++) {
		RegReadA(client, LC898122_RH_SMTSRVSTT,	&UcRegIni);
		if ((UcRegIni & UcSttMsk) == (0x66 & UcSttMsk))
			break;
		msleep(10);
	}

	RegWriteA(client, LC898122_WH_SMTSRVON,	0x00);

	if (UcCnt == 60) {
		if (UcStbX == ON)
			RamWriteA(client, LC898122_SXOFFZ2, 0);
		if (UcStbY == ON)
			RamWriteA(client, LC898122_SYOFFZ2, 0);
	}
}

void lc898122_OptCen(struct lc898122_device *lc898122_dev, u8 UcOptmode,
	       u16 UsOptXval, u16 UsOptYval)
{
	struct i2c_client *client = lc898122_dev->client;

	lc898122_RamAccFixMod(lc898122_dev, ON);

	switch (UcOptmode) {
	case LC898122_VAL_SET:
		RamWriteA(client, LC898122_SXOFFZ1, UsOptXval);
		RamWriteA(client, LC898122_SYOFFZ1, UsOptYval);
		break;
	case LC898122_VAL_FIX:
		lc898122_dev->state.UsCntXof = UsOptXval;
		lc898122_dev->state.UsCntYof = UsOptYval;
		RamWriteA(client, LC898122_SXOFFZ1,
				lc898122_dev->state.UsCntXof);
		RamWriteA(client, LC898122_SYOFFZ1,
				lc898122_dev->state.UsCntYof);
		break;
	case LC898122_VAL_SPC:
		RamReadA(client, LC898122_SXOFFZ1, &UsOptXval);
		RamReadA(client, LC898122_SYOFFZ1, &UsOptYval);
		lc898122_dev->state.UsCntXof = UsOptXval;
		lc898122_dev->state.UsCntYof = UsOptYval;
		break;
	}

	lc898122_RamAccFixMod(lc898122_dev, OFF);

}

void lc898122_SetSineWave(struct lc898122_device *lc898122_dev, u8 UcJikuSel,
		 u8 UcMeasMode)
{
	struct i2c_client *client = lc898122_dev->client;

	u16  UsFRQ_extclk[]   = { 0x30EE/*139.9Hz*/, 0x037E/*10Hz*/ };
	u16  UsFRQ_osc[]   = { 0x1877/*139.9Hz*/, 0x01BF/*10Hz*/ };
	u32   UlAMP[2][2]   = {{ 0x3CA3D70A, 0x3CA3D70A },
					 { 0x3F800000, 0x3F800000 } };
	u8	UcEqSwX, UcEqSwY;
	u16  *UsFRQ;

	if (lc898122_dev->state.flags & LC898122_EXTCLK_ALL)
		UsFRQ = UsFRQ_extclk;
	else
		UsFRQ = UsFRQ_osc;

	UcMeasMode &= 0x01;
	UcJikuSel  &= 0x01;

	RegWriteA(client, LC898122_WC_SINPHSX, 0x00);
	RegWriteA(client, LC898122_WC_SINPHSY, 0x00);

	RegWriteA(client, LC898122_WC_MESSINMODE,     0x00);
	RegWriteA(client, LC898122_WC_MESWAIT,     0x00);

	RamWrite32A(client, LC898122_sxsin, UlAMP[UcMeasMode][LC898122_X_DIR]);
	RamWrite32A(client, LC898122_sysin, UlAMP[UcMeasMode][LC898122_Y_DIR]);

	RegWriteA(client, LC898122_WC_SINFRQ0, (u8)UsFRQ[UcMeasMode]);
	RegWriteA(client, LC898122_WC_SINFRQ1, (u8)(UsFRQ[UcMeasMode] >> 8));

	RegReadA(client, LC898122_WH_EQSWX, &UcEqSwX);
	RegReadA(client, LC898122_WH_EQSWY, &UcEqSwY);
	if (!UcMeasMode && !UcJikuSel) {
		UcEqSwX |= 0x10;
		UcEqSwY &= ~LC898122_EQSINSW;
	} else if (!UcMeasMode && UcJikuSel) {
		UcEqSwX &= ~LC898122_EQSINSW;
		UcEqSwY |= 0x10;
	} else if (UcMeasMode && !UcJikuSel) {
		UcEqSwX = 0x22;
		UcEqSwY = 0x03;
	} else {
		UcEqSwX = 0x03;
		UcEqSwY = 0x22;
	}

	RegWriteA(client, LC898122_WH_EQSWX, UcEqSwX);
	RegWriteA(client, LC898122_WH_EQSWY, UcEqSwY);
}

void lc898122_StartSineWave(struct lc898122_device *lc898122_dev)
{
	struct i2c_client *client = lc898122_dev->client;

	RegWriteA(client, LC898122_WC_SINON, 0x01);
}

void lc898122_StopSineWave(struct lc898122_device *lc898122_dev)
{
	struct i2c_client *client = lc898122_dev->client;

	u8		UcEqSwX, UcEqSwY;

	RegWriteA(client, LC898122_WC_SINON,     0x00);
	RegReadA(client, LC898122_WH_EQSWX, &UcEqSwX);
	RegReadA(client, LC898122_WH_EQSWY, &UcEqSwY);
	UcEqSwX &= ~LC898122_EQSINSW;
	UcEqSwY &= ~LC898122_EQSINSW;
	RegWriteA(client, LC898122_WH_EQSWX, UcEqSwX);
	RegWriteA(client, LC898122_WH_EQSWY, UcEqSwY);

}

void lc898122_SetMeasFil(struct lc898122_device *lc898122_dev, u8 UcFilSel)
{
	lc898122_MesFil(lc898122_dev, UcFilSel);
}

void lc898122_ClrMeasFil(struct lc898122_device *lc898122_dev)
{
	lc898122_cleargyro(lc898122_dev, 0x1000, LC898122_CLR_FRAM1);
}

void lc898122_SetPanTiltMode(struct lc898122_device *lc898122_dev, u8 UcPnTmod)
{
	struct i2c_client *client = lc898122_dev->client;

	switch (UcPnTmod) {
	case OFF:
		RegWriteA(client, LC898122_WG_PANON, 0x00);
		break;
	case ON:
#ifdef	NEW_PTST
		RegWriteA(client, LC898122_WG_PANON, 0x10);
#else
		RegWriteA(client, LC898122_WG_PANON, 0x01);
#endif
		break;
	}

}

u8 lc898122_TriSts(struct lc898122_device *lc898122_dev)
{
	struct i2c_client *client = lc898122_dev->client;

	u8 UcRsltSts = 0;
	u8 UcVal;

	RegReadA(client, LC898122_WG_ADJGANGXATO, &UcVal);
	if (UcVal & 0x03) {
		RegReadA(client, LC898122_RG_LEVJUGE, &UcVal);
		UcRsltSts = UcVal & 0x11;
		UcRsltSts |= 0x80;
	}

	return UcRsltSts;
}

u8 lc898122_DrvPwmSw(struct lc898122_device *lc898122_dev, u8 UcSelPwmMod)
{
	struct i2c_client *client = lc898122_dev->client;

	switch (UcSelPwmMod) {
	case LC898122_Mlnp:
		RegWriteA(client, LC898122_DRVFC, 0xF0);
		lc898122_dev->state.UcPwmMod = LC898122_PWMMOD_CVL;
		break;

	case LC898122_Mpwm:
		if (lc898122_dev->state.flags & LC898122_PWM_BREAK)
			RegWriteA(client, LC898122_DRVFC, 0x00);
		else
			RegWriteA(client, LC898122_DRVFC, 0xC0);
		lc898122_dev->state.UcPwmMod = LC898122_PWMMOD_PWM;
		break;
	}

	return UcSelPwmMod << 4;
}

void lc898122_SetGcf(struct lc898122_device *lc898122_dev, u8 UcSetNum)
{
	struct i2c_client *client = lc898122_dev->client;

	if (UcSetNum > (LC898122_COEFTBL - 1))
		UcSetNum = (LC898122_COEFTBL - 1);

	if (lc898122_dev->state.UcModule == LC898122_MODULE_20M)
		lc898122_dev->state.UlH1Coefval	= ClDiCof_20M[UcSetNum];
	else
		lc898122_dev->state.UlH1Coefval	= ClDiCof_13M[UcSetNum];

	RamWrite32A(client, LC898122_gxh1c, lc898122_dev->state.UlH1Coefval);
	RamWrite32A(client, LC898122_gyh1c, lc898122_dev->state.UlH1Coefval);

	if (lc898122_dev->state.flags & LC898122_H1COEF_CHANGER)
		lc898122_SetH1cMod(lc898122_dev, UcSetNum);
}

void lc898122_SetH1cMod(struct lc898122_device *lc898122_dev,
		u8	UcSetNum)
{
	struct i2c_client *client = lc898122_dev->client;

	u32	MAXLMT;
	u32	MINLMT;
	u32	CHGCOEF;
	u32	MINLMT_MOV;
	u32	CHGCOEF_MOV;

	if (lc898122_dev->state.UcModule == LC898122_MODULE_20M) {
		MAXLMT		= LC898122_MAXLMT_20M;
		MINLMT		= LC898122_MINLMT_20M;
		CHGCOEF		= LC898122_CHGCOEF_20M;
		MINLMT_MOV	= LC898122_MINLMT_MOV_20M;
		CHGCOEF_MOV	= LC898122_CHGCOEF_MOV_20M;
	} else{
		MAXLMT		= LC898122_MAXLMT_13M;
		MINLMT		= LC898122_MINLMT_13M;
		CHGCOEF		= LC898122_CHGCOEF_13M;
		MINLMT_MOV	= LC898122_MINLMT_MOV_13M;
		CHGCOEF_MOV	= LC898122_CHGCOEF_MOV_13M;
	}

	switch (UcSetNum) {
	case (LC898122_ACTMODE):
		lc898122_IniPtMovMod(lc898122_dev, OFF);

		if (lc898122_dev->state.UcModule == LC898122_MODULE_20M)
			lc898122_dev->state.UlH1Coefval	= ClDiCof_20M[0];
		else
			lc898122_dev->state.UlH1Coefval	= ClDiCof_13M[0];

		lc898122_dev->state.UcH1LvlMod = 0;

		RamWrite32A(client, LC898122_gxlmt6L, MINLMT);
		RamWrite32A(client, LC898122_gxlmt6H, MAXLMT);

		RamWrite32A(client, LC898122_gylmt6L, MINLMT);
		RamWrite32A(client, LC898122_gylmt6H, MAXLMT);

		RamWrite32A(client, LC898122_gxhc_tmp,
				lc898122_dev->state.UlH1Coefval);
		RamWrite32A(client, LC898122_gxmg, CHGCOEF);

		RamWrite32A(client, LC898122_gyhc_tmp,
				lc898122_dev->state.UlH1Coefval);
		RamWrite32A(client, LC898122_gymg,		CHGCOEF);

		RegWriteA(client, LC898122_WG_HCHR, 0x12);
		break;

	case(LC898122_S2MODE):
		RegWriteA(client, LC898122_WG_HCHR, 0x10);
		break;

	case(LC898122_MOVMODE):
		lc898122_IniPtMovMod(lc898122_dev, ON);

		RamWrite32A(client, LC898122_gxlmt6L, MINLMT_MOV);
		RamWrite32A(client, LC898122_gylmt6L, MINLMT_MOV);

		RamWrite32A(client, LC898122_gxmg, CHGCOEF_MOV);
		RamWrite32A(client, LC898122_gymg, CHGCOEF_MOV);

		RamWrite32A(client, LC898122_gxhc_tmp,
				lc898122_dev->state.UlH1Coefval);
		RamWrite32A(client, LC898122_gyhc_tmp,
				lc898122_dev->state.UlH1Coefval);

		RegWriteA(client, LC898122_WG_HCHR, 0x12);
		break;

	default:
		lc898122_IniPtMovMod(lc898122_dev, OFF);

		lc898122_dev->state.UcH1LvlMod = UcSetNum;

		RamWrite32A(client, LC898122_gxlmt6L, MINLMT);
		RamWrite32A(client, LC898122_gylmt6L, MINLMT);

		RamWrite32A(client, LC898122_gxmg,	CHGCOEF);
		RamWrite32A(client, LC898122_gymg,	CHGCOEF);

		RamWrite32A(client, LC898122_gxhc_tmp,
				lc898122_dev->state.UlH1Coefval);
		RamWrite32A(client, LC898122_gyhc_tmp,
				lc898122_dev->state.UlH1Coefval);

		RegWriteA(client, LC898122_WG_HCHR, 0x12);
		break;
	}
}

u16 lc898122_RdFwVr(void)
{
	return LC898122_FW_VER;
}

u8 lc898122_GetDOFSTDAF(struct lc898122_device *lc898122_dev)
{
	struct i2c_client *client = lc898122_dev->client;

	u8	ucDRVFC3AF, ucDRVFC4AF, ucRegDat;

	if (lc898122_dev->state.UcCvrCod == LC898122_CVER122) {
		RegReadA(client, LC898122_DRVFC3AF, &ucDRVFC3AF);
		RegReadA(client, LC898122_DRVFC4AF, &ucDRVFC4AF);
		ucRegDat = ((ucDRVFC4AF & 0xC0) >> 3) | (ucDRVFC3AF & 0x07);
	} else {
		RegReadA(client, LC898122_DRVFC4AF, &ucDRVFC4AF);
		ucRegDat = ucDRVFC4AF >> 3;
	}

	return ucRegDat;
}

void lc898122_SetDOFSTDAF(struct lc898122_device *lc898122_dev, u8 ucSetDat)
{
	struct i2c_client *client = lc898122_dev->client;

	u8 ucRegDat;

	if (lc898122_dev->state.UcCvrCod == LC898122_CVER122) {
		RegReadA(client, LC898122_DRVFC3AF, &ucRegDat);
		RegWriteA(client, LC898122_DRVFC4AF, (ucSetDat & 0x18) << 3);
		RegWriteA(client, LC898122_DRVFC3AF,
				(ucRegDat & 0x70) | (ucSetDat & 0x07));
	} else {
		RegWriteA(client, LC898122_DRVFC4AF, ucSetDat << 3);
	}
}

u8 lc898122_TstActMov(struct lc898122_device *lc898122_dev, u8 UcDirSel)
{
	struct i2c_client *client = lc898122_dev->client;

	u8 UcRsltSts;
	u16	UsMsppVal;

	lc898122_MesFil(lc898122_dev, LC898122_NOISE);

	if (!UcDirSel) {
		RamWrite32A(client, LC898122_sxsin, LC898122_ACT_CHK_LVL);
		RamWrite32A(client, LC898122_sysin, 0x000000);
		lc898122_SetSinWavePara(lc898122_dev, 0x05, LC898122_XACTTEST);
	} else {
		RamWrite32A(client, LC898122_sxsin, 0x000000);
		RamWrite32A(client, LC898122_sysin, LC898122_ACT_CHK_LVL);
		lc898122_SetSinWavePara(lc898122_dev, 0x05, LC898122_YACTTEST);
	}

	if (!UcDirSel) {
		RegWriteA(client, LC898122_WC_MES1ADD0,  (u8)LC898122_SXINZ1);
		RegWriteA(client, LC898122_WC_MES1ADD1,
				(u8)((LC898122_SXINZ1 >> 8) & 0x0001));
	} else {
		RegWriteA(client, LC898122_WC_MES1ADD0,
				(u8)LC898122_SYINZ1);
		RegWriteA(client, LC898122_WC_MES1ADD1,
				(u8)((LC898122_SYINZ1 >> 8) & 0x0001));
	}

	RegWriteA(client, LC898122_WC_MESSINMODE, 0x00);
	RegWriteA(client, LC898122_WC_MESLOOP1, 0x00);
	RegWriteA(client, LC898122_WC_MESLOOP0, 0x02);
	RamWrite32A(client, LC898122_msmean, 0x3F000000);
	RegWriteA(client, LC898122_WC_MESABS, 0x00);
	lc898122_BsyWit(lc898122_dev, LC898122_WC_MESMODE, 0x02);

	lc898122_RamAccFixMod(lc898122_dev, ON);
	RamReadA(client, LC898122_MSPP1AV, &UsMsppVal);
	lc898122_RamAccFixMod(lc898122_dev, OFF);

	if (!UcDirSel)
		lc898122_SetSinWavePara(lc898122_dev, 0x00, LC898122_XACTTEST);
	else
		lc898122_SetSinWavePara(lc898122_dev, 0x00, LC898122_YACTTEST);

	UcRsltSts = LC898122_EXE_END;
	if (UsMsppVal > LC898122_ACT_THR) {
		if (!UcDirSel)
			UcRsltSts = LC898122_EXE_HXMVER;
		else
			UcRsltSts = LC898122_EXE_HYMVER;
	}

	return UcRsltSts;
}

u8 lc898122_RunHea(struct lc898122_device *lc898122_dev)
{
	u8	UcRst;

	UcRst = LC898122_EXE_END;
	UcRst |= lc898122_TstActMov(lc898122_dev, LC898122_X_DIR);
	UcRst |= lc898122_TstActMov(lc898122_dev, LC898122_Y_DIR);

	return UcRst;
}

u8 lc898122_RunGea(struct lc898122_device *lc898122_dev)
{
	struct i2c_client *client = lc898122_dev->client;

	u8	UcRst, UcCnt, UcXLowCnt, UcYLowCnt, UcXHigCnt, UcYHigCnt;
	u16	UsGxoVal[10], UsGyoVal[10], UsDif;

	UcRst = LC898122_EXE_END;
	UcXLowCnt = UcYLowCnt = UcXHigCnt = UcYHigCnt = 0;

	lc898122_MesFil(lc898122_dev, LC898122_THROUGH);

	for (UcCnt = 0; UcCnt < 10; UcCnt++) {
		RegWriteA(client, LC898122_WC_MES1ADD0, 0x00);
		RegWriteA(client, LC898122_WC_MES1ADD1, 0x00);
		lc898122_cleargyro(lc898122_dev, 0x1000, LC898122_CLR_FRAM1);
		UsGxoVal[UcCnt] =
			(u16)lc898122_GenMes(lc898122_dev,
							LC898122_AD2Z, 0);

		RegWriteA(client, LC898122_WC_MES1ADD0, 0x00);
		RegWriteA(client, LC898122_WC_MES1ADD1, 0x00);
		lc898122_cleargyro(lc898122_dev, 0x1000, LC898122_CLR_FRAM1);
		UsGyoVal[UcCnt] =
			(u16)lc898122_GenMes(lc898122_dev,
							LC898122_AD3Z, 0);

		if (UcCnt > 0) {
			if ((short)UsGxoVal[0] > (short)UsGxoVal[UcCnt])
				UsDif = (u16)((short)UsGxoVal[0] -
						(short)UsGxoVal[UcCnt]);
			else
				UsDif = (u16)((short)UsGxoVal[UcCnt] -
						(short)UsGxoVal[0]);

			if (UsDif > LC898122_GEA_DIF_HIG)
				UcXHigCnt++;

			if (UsDif < LC898122_GEA_DIF_LOW)
				UcXLowCnt++;

			if ((short)UsGyoVal[0] > (short)UsGyoVal[UcCnt])
				UsDif = (u16)((short)UsGyoVal[0] -
						(short)UsGyoVal[UcCnt]);
			else
				UsDif = (u16)((short)UsGyoVal[UcCnt] -
						(short)UsGyoVal[0]);

			if (UsDif > LC898122_GEA_DIF_HIG)
				UcYHigCnt++;

			if (UsDif < LC898122_GEA_DIF_LOW)
				UcYLowCnt++;
		}
	}

	if (UcXHigCnt >= 1)
		UcRst = UcRst | LC898122_EXE_GXABOVE;

	if (UcXLowCnt > 8)
		UcRst = UcRst | LC898122_EXE_GXBELOW;

	if (UcYHigCnt >= 1)
		UcRst = UcRst | LC898122_EXE_GYABOVE;

	if (UcYLowCnt > 8)
		UcRst = UcRst | LC898122_EXE_GYBELOW;

	return UcRst;
}

