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
#include "lc898122-oisfil.h"

#define	LC898122_TRI_LEVEL 0x3A83126F
#define	LC898122_TIMELOW   0x50
#define	LC898122_TIMEHGH   0x05
#define	LC898122_TIMEBSE_EXT 0x2F
#define	LC898122_TIMEBSE   0x5D
#define	LC898122_MONADR	   LC898122_GXXFZ
#define LC898122_GANADR	   LC898122_gxadj
#define	LC898122_XMINGAIN  0x00000000
#define	LC898122_XMAXGAIN  0x3F800000
#define	LC898122_YMINGAIN  0x00000000
#define	LC898122_YMAXGAIN  0x3F800000
#define	LC898122_XSTEPUP   0x38D1B717
#define	LC898122_XSTEPDN   0xBD4CCCCD
#define	LC898122_YSTEPUP   0x38D1B717
#define	LC898122_YSTEPDN   0xBD4CCCCD

#define LC898122_LOOP_TIMEOUT 10

#define LC898122_AFTYPE_UNIDIR 0x1
#define LC898122_AFTYPE_BI_DIR 0x2

#define LC898122_MODULE_13M    0x01
#define LC898122_MODULE_20M    0x02

static void lc898122_gyoutsignal(struct lc898122_device *lc898122_dev)
{
	struct i2c_client *client = lc898122_dev->client;

	if (lc898122_dev->state.flags & LC898122_USE_INVENSENSE) {
		RegWriteA(client, LC898122_GRADR0,
			LC898122_INVENSENSE_GYROX_INI);
		RegWriteA(client, LC898122_GRADR1,
			LC898122_INVENSENSE_GYROY_INI);
	} else if (lc898122_dev->state.flags & LC898122_USE_STMICRO_L3G4IS) {
		RegWriteA(client, LC898122_GRADR0,
			LC898122_STMICRO_GYROX_INI);
		RegWriteA(client, LC898122_GRADR1,
			LC898122_STMICRO_GYROY_INI);
	} else if (lc898122_dev->state.flags & LC898122_USE_PANASONIC) {
		RegWriteA(client, LC898122_GRADR0,
			LC898122_PANASONIC_GYROX_INI);
		RegWriteA(client, LC898122_GRADR1,
			LC898122_PANASONIC_GYROY_INI);
	} else {
		/* ERROR */
	}
	RegWriteA(client, LC898122_GRSEL, 0x02);
}

void lc898122_accwait(struct lc898122_device *lc898122_dev, u8 UcTrgDat)
{
	struct i2c_client *client = lc898122_dev->client;
	u8 UcFlgVal;
	int UcCnt;

	for (UcCnt = 0; UcCnt < 60; UcCnt++) {
		RegReadA(client, LC898122_GRACC, &UcFlgVal);
		UcFlgVal &= UcTrgDat;
		if (UcFlgVal == 0)
			break;
		msleep(LC898122_LOOP_TIMEOUT);
	}
}

void lc898122_selectgyrosleep(struct lc898122_device *lc898122_dev,
			u8 UcSelMode)
{
	struct i2c_client *client = lc898122_dev->client;
	u8	UcRamIni;
	u8	UcGrini;

	if (lc898122_dev->state.flags & LC898122_USE_INVENSENSE) {
		if (UcSelMode == ON) {
			RegWriteA(client, LC898122_WC_EQON, 0x00);
			RegWriteA(client, LC898122_GRSEL, 0x01);

			RegReadA(client, LC898122_GRINI, &UcGrini);
			RegWriteA(client, LC898122_GRINI, (UcGrini |
				LC898122_SLOWMODE));

			RegWriteA(client, LC898122_GRADR0, 0x6B);
			RegWriteA(client, LC898122_GRACC, 0x01);

			lc898122_accwait(lc898122_dev, 0x01);

			RegReadA(client, LC898122_GRDAT0H, &UcRamIni);
			UcRamIni |= 0x40;

			if (lc898122_dev->state.flags & LC898122_GYROSTANDBY)
				UcRamIni &= ~0x01;

			RegWriteA(client, LC898122_GRADR0, 0x6B);
			RegWriteA(client, LC898122_GSETDT, UcRamIni);
			RegWriteA(client, LC898122_GRACC, 0x10);
			lc898122_accwait(lc898122_dev, 0x10);

			if (lc898122_dev->state.flags & LC898122_GYROSTANDBY) {
				RegWriteA(client, LC898122_GRADR0, 0x6C);
				RegWriteA(client, LC898122_GSETDT, 0x07);
				RegWriteA(client, LC898122_GRACC,	0x10);
				lc898122_accwait(lc898122_dev, 0x10);
			}
		} else {
			if (lc898122_dev->state.flags & LC898122_GYROSTANDBY) {
				RegWriteA(client, LC898122_GRADR0, 0x6C);
				RegWriteA(client, LC898122_GSETDT, 0x00);
				RegWriteA(client, LC898122_GRACC, 0x10);
				lc898122_accwait(lc898122_dev, 0x10);
			}

			RegWriteA(client, LC898122_GRADR0, 0x6B);
			RegWriteA(client, LC898122_GRACC, 0x01);
			lc898122_accwait(lc898122_dev, 0x01);
			RegReadA(client, LC898122_GRDAT0H, &UcRamIni);

			UcRamIni &= ~0x40;
			if (lc898122_dev->state.flags & LC898122_GYROSTANDBY)
				UcRamIni |= 0x01;

			RegWriteA(client, LC898122_GSETDT, UcRamIni);
			RegWriteA(client, LC898122_GRACC,	0x10);
			lc898122_accwait(lc898122_dev, 0x10);

			RegReadA(client, LC898122_GRINI, &UcGrini);
			RegWriteA(client, LC898122_GRINI,
				(UcGrini & ~LC898122_SLOWMODE));

			lc898122_gyoutsignal(lc898122_dev);

			msleep(50);

			RegWriteA(client, LC898122_WC_EQON, 0x01);

			lc898122_cleargyro(lc898122_dev,
				0x007F,
				LC898122_CLR_FRAM1);
		}
	} else {
		if (UcSelMode == ON) {
			RegWriteA(client, LC898122_WC_EQON, 0x00);
			RegWriteA(client, LC898122_GRSEL,	0x01);
			RegWriteA(client, LC898122_GRADR0, 0x4C);
			RegWriteA(client, LC898122_GSETDT, 0x02);
			RegWriteA(client, LC898122_GRACC,	0x10);
			lc898122_accwait(lc898122_dev, 0x10);
		} else {
			RegWriteA(client, LC898122_GRADR0, 0x4C);
			RegWriteA(client, LC898122_GSETDT, 0x00);
			RegWriteA(client, LC898122_GRACC,	0x10);
			lc898122_accwait(lc898122_dev, 0x10);
			lc898122_gyoutsignal(lc898122_dev);

			msleep(50);

			RegWriteA(client, LC898122_WC_EQON, 0x01);
			lc898122_cleargyro(lc898122_dev, 0x007F,
				LC898122_CLR_FRAM1);
		}
	}
}

void lc898122_cleargyro(struct lc898122_device *lc898122_dev,
			u16 UsClrFil, u8 UcClrMod)
{
	struct i2c_client *client = lc898122_dev->client;
	u8 UcRamClr;
	int UcCnt;

	/* Select Filter to clear */
	RegWriteA(client, LC898122_WC_RAMDLYMOD1, (u8)(UsClrFil >> 8));
	RegWriteA(client, LC898122_WC_RAMDLYMOD0, (u8)UsClrFil);

	/* Enable Clear */
	RegWriteA(client, LC898122_WC_RAMINITON, UcClrMod);

	/* Check RAM Clear complete */
	for (UcCnt = 0; UcCnt < 60; UcCnt++) {
		RegReadA(client, LC898122_WC_RAMINITON, &UcRamClr);
		UcRamClr &= UcClrMod;
		if (UcRamClr == 0)
			break;
		msleep(LC898122_LOOP_TIMEOUT);
	}
}

void lc898122_driversw(struct lc898122_device *lc898122_dev, u8 UcDrvSw)
{
	struct i2c_client *client = lc898122_dev->client;

	if (UcDrvSw == ON) {
		if (lc898122_dev->state.UcPwmMod == LC898122_PWMMOD_CVL) {
			RegWriteA(client, LC898122_DRVFC, 0xF0);
		} else {
			if (lc898122_dev->state.flags & LC898122_PWM_BREAK)
				RegWriteA(client, LC898122_DRVFC, 0x00);
			else
				RegWriteA(client, LC898122_DRVFC, 0xC0);
		}
	} else {
		if (lc898122_dev->state.UcPwmMod == LC898122_PWMMOD_CVL) {
			RegWriteA(client, LC898122_DRVFC, 0x30);
		} else {
			if (lc898122_dev->state.flags & LC898122_PWM_BREAK)
				RegWriteA(client, LC898122_DRVFC, 0x00);
			else
				RegWriteA(client, LC898122_DRVFC, 0x00);
		}
	}
}

void lc898122_GyOutSignalCont(struct lc898122_device *lc898122_dev)
{
	struct i2c_client *client = lc898122_dev->client;

	RegWriteA(client, LC898122_GRSEL, 0x04);
}


void lc898122_BsyWit(struct lc898122_device *lc898122_dev,
	u16 UsTrgAdr,
	u8 UcTrgDat)
{
	struct i2c_client *client = lc898122_dev->client;
	u8 UcFlgVal;
	u8 UcCnt;

	RegWriteA(client, UsTrgAdr, UcTrgDat);

	for (UcCnt = 0; UcCnt < 60; UcCnt++) {
		RegReadA(client, UsTrgAdr, &UcFlgVal);
		UcFlgVal &= (UcTrgDat & 0x0F);
		if (UcFlgVal == 0)
			break;
		msleep(LC898122_LOOP_TIMEOUT);
	}
}

void lc898122_IniPtAve(struct lc898122_device *lc898122_dev)
{
	struct i2c_client *client = lc898122_dev->client;

	RegWriteA(client, LC898122_WG_PANSTT1DWNSMP0, 0x00);
	RegWriteA(client, LC898122_WG_PANSTT1DWNSMP1, 0x00);
	RegWriteA(client, LC898122_WG_PANSTT2DWNSMP0, 0x90);
	RegWriteA(client, LC898122_WG_PANSTT2DWNSMP1, 0x01);
	RegWriteA(client, LC898122_WG_PANSTT3DWNSMP0, 0x64);
	RegWriteA(client, LC898122_WG_PANSTT3DWNSMP1, 0x00);
	RegWriteA(client, LC898122_WG_PANSTT4DWNSMP0, 0x00);
	RegWriteA(client, LC898122_WG_PANSTT4DWNSMP1, 0x00);

	RamWrite32A(client, LC898122_st1mean, 0x3f800000);
	RamWrite32A(client, LC898122_st2mean, 0x3B23D700);
	RamWrite32A(client, LC898122_st3mean, 0x3C23D700);
	RamWrite32A(client, LC898122_st4mean, 0x3f800000);
}

void lc898122_RamAccFixMod(struct lc898122_device *lc898122_dev, u8 UcAccMod)
{
	struct i2c_client *client = lc898122_dev->client;

	switch (UcAccMod) {
	case OFF:
		RegWriteA(client, LC898122_WC_RAMACCMOD, 0x00);
		break;
	case ON:
		RegWriteA(client, LC898122_WC_RAMACCMOD, 0x31);
		break;
	}
}

void lc898122_IniPtMovMod(struct lc898122_device *lc898122_dev, u8 UcPtMod)
{
	struct i2c_client *client = lc898122_dev->client;

	switch (UcPtMod) {
	case OFF:
		RegWriteA(client, LC898122_WG_PANSTTSETGYRO, 0x00);
		RegWriteA(client, LC898122_WG_PANSTTSETGAIN, 0x54);
		RegWriteA(client, LC898122_WG_PANSTTSETISTP, 0x14);
		RegWriteA(client, LC898122_WG_PANSTTSETIFTR, 0x94);
		RegWriteA(client, LC898122_WG_PANSTTSETLFTR, 0x00);
		break;
	case ON:
		RegWriteA(client, LC898122_WG_PANSTTSETGYRO, 0x00);
		RegWriteA(client, LC898122_WG_PANSTTSETGAIN, 0x00);
		RegWriteA(client, LC898122_WG_PANSTTSETISTP, 0x14);
		RegWriteA(client, LC898122_WG_PANSTTSETIFTR, 0x94);
		RegWriteA(client, LC898122_WG_PANSTTSETLFTR, 0x00);
		break;
	}
}

void lc898122_ChkCvr(struct lc898122_device *lc898122_dev)
{
	struct i2c_client *client = lc898122_dev->client;

	RegReadA(client, LC898122_CVER,	&lc898122_dev->state.UcCvrCod);
	RegWriteA(client, LC898122_VRREG, (u8)LC898122_FW_VER);
}

void lc898122_RemOff(struct lc898122_device *lc898122_dev, u8 UcMod)
{
	struct i2c_client *client = lc898122_dev->client;

	switch (UcMod) {
	case OFF:
		RegWriteA(client, LC898122_WG_PANSTT6, 0x11);
		RegWriteA(client, LC898122_WC_RAMACCXY, 0x01);
		RamWrite32A(client, LC898122_gxia_1, 0x39D2BD40);
		RamWrite32A(client, LC898122_gxib_1, 0x39D2BD40);
		RamWrite32A(client, LC898122_gxic_1, 0x3F7FCB40);
		RegWriteA(client, LC898122_WC_RAMACCXY,	0x00);
		RegWriteA(client, LC898122_WG_PANSTT6, 0x00);
		break;

	case ON:
		lc898122_cleargyro(lc898122_dev, 0x007F, LC898122_CLR_FRAM1);
		RegWriteA(client, LC898122_WC_RAMACCXY, 0x01);
		RamWrite32A(client, LC898122_gxia_1, 0x3B038040);
		RamWrite32A(client, LC898122_gxib_1, 0x3B038040);
		RamWrite32A(client, LC898122_gxic_1, 0x3F7EF900);
		RegWriteA(client, LC898122_WC_RAMACCXY,	0x00);
		lc898122_SetPanTiltMode(lc898122_dev, ON);
		RegWriteA(client, LC898122_WG_PANSTT6, 0x44);
		break;
	}
}

void lc898122_afdriversw(struct lc898122_device *lc898122_dev, u8 UcDrvSw)
{
	struct i2c_client *client = lc898122_dev->client;

	if (UcDrvSw == ON) {
		if (lc898122_dev->state.UcAfType == LC898122_BI_DIR) {
			RegWriteA(client, LC898122_DRVFCAF, 0x10);
		} else {
			if (lc898122_dev->state.flags & LC898122_AF_PWMMODE)
				RegWriteA(client, LC898122_DRVFCAF, 0x00);
			else
				RegWriteA(client, LC898122_DRVFCAF, 0x20);
		}
		RegWriteA(client, LC898122_CCAAF, 0x80);
	} else {
		RegWriteA(client, LC898122_CCAAF, 0x00);
	}
}

void lc898122_settregaf(struct lc898122_device *lc898122_dev, u16 UsTregAf)
{
	struct i2c_client *client = lc898122_dev->client;

	if (lc898122_dev->state.UcAfType == LC898122_BI_DIR)
		RamWriteA(client, LC898122_TREG_H, (UsTregAf << 5));
	else
		RamWriteA(client, LC898122_TREG_H, (UsTregAf << 6));
}

void lc898122_autogaincontrol(struct lc898122_device *lc898122_dev,
	u8 UcModeSw)
{
	struct i2c_client *client = lc898122_dev->client;

	if (UcModeSw == OFF) {
		RegWriteA(client, LC898122_WG_ADJGANGXATO, 0xA0);
		RegWriteA(client, LC898122_WG_ADJGANGYATO, 0xA0);
		RamWrite32A(client, LC898122_GANADR, LC898122_XMAXGAIN);
		RamWrite32A(client, LC898122_GANADR |
			0x0100,  LC898122_YMAXGAIN);
	} else {
		RegWriteA(client, LC898122_WG_ADJGANGXATO, 0xA3);
		RegWriteA(client, LC898122_WG_ADJGANGYATO, 0xA3);
	}
}

static void lc898122_afinitialsetting(struct lc898122_device *lc898122_dev)
{
	struct i2c_client *client = lc898122_dev->client;
	u8	UcStbb0;

	struct af_cfg_t {
		u16	RWEXD1_L_AF;
		u16	RWEXD2_L_AF;
		u16	RWEXD3_L_AF;
		u8	FSTCTIME_AF;
		u8	FSTMODE_AF;
	};

	const struct af_cfg_t *af_cfg = NULL;

	const struct af_cfg_t af_cfg_20M = {
		.RWEXD1_L_AF = LC898122_RWEXD1_L_AF_20M,
		.RWEXD2_L_AF = LC898122_RWEXD2_L_AF_20M,
		.RWEXD3_L_AF = LC898122_RWEXD3_L_AF_20M,
		.FSTCTIME_AF = LC898122_FSTCTIME_AF_20M,
		.FSTMODE_AF  = LC898122_FSTMODE_AF_20M,
	};

	const struct af_cfg_t af_cfg_13M = {
		.RWEXD1_L_AF = LC898122_RWEXD1_L_AF_13M,
		.RWEXD2_L_AF = LC898122_RWEXD2_L_AF_13M,
		.RWEXD3_L_AF = LC898122_RWEXD3_L_AF_13M,
		.FSTCTIME_AF = LC898122_FSTCTIME_AF_13M,
		.FSTMODE_AF  = LC898122_FSTMODE_AF_13M,
	};


	if (lc898122_dev->state.UcModule == LC898122_MODULE_20M)
		af_cfg = &af_cfg_20M;
	else
		af_cfg = &af_cfg_13M;

	lc898122_afdriversw(lc898122_dev, OFF);

	if (lc898122_dev->state.UcAfType == LC898122_BI_DIR) {
		lc898122_settregaf(lc898122_dev, 0x0400);
		RegWriteA(client, LC898122_DRVFCAF, 0x10);
		RegWriteA(client, LC898122_DRVFC3AF, 0x40);
		RegWriteA(client, LC898122_DRVFC4AF, 0x80);
		RegWriteA(client, LC898122_AFFC, 0x90);
	} else {
		if (lc898122_dev->state.flags & LC898122_AF_PWMMODE)
			RegWriteA(client, LC898122_DRVFCAF, 0x00);
		else
			RegWriteA(client, LC898122_DRVFCAF, 0x20);

		RegWriteA(client, LC898122_DRVFC3AF, 0x00);
		RegWriteA(client, LC898122_DRVFC4AF, 0x80);
		RegWriteA(client, LC898122_PWMAAF, 0x00);
		RegWriteA(client, LC898122_AFFC, 0x80);
	}

	if (lc898122_dev->state.flags & LC898122_AF_PWMMODE) {
		RegWriteA(client, LC898122_DRVFC2AF, 0x82);
		RegWriteA(client, LC898122_DRVCH3SEL, 0x02);
		RegWriteA(client, LC898122_PWMFCAF, 0x89);
		RegWriteA(client, LC898122_PWMPERIODAF, 0xA0);
	} else {
		RegWriteA(client, LC898122_DRVFC2AF, 0x00);
		RegWriteA(client, LC898122_DRVCH3SEL, 0x00);
		RegWriteA(client, LC898122_PWMFCAF, 0x01);
		RegWriteA(client, LC898122_PWMPERIODAF, 0x20);
	}

	if (lc898122_dev->state.UcAfType == LC898122_BI_DIR)
		RegWriteA(client, LC898122_CCFCAF, 0x08);
	else
		RegWriteA(client, LC898122_CCFCAF, 0x40);

	RegReadA(client, LC898122_STBB0, &UcStbb0);
	UcStbb0 &= 0x7F;
	RegWriteA(client, LC898122_STBB0, UcStbb0);
	RegWriteA(client, LC898122_STBB1, 0x00);

	/* AF Initial setting */
	RegWriteA(client, LC898122_FSTMODE, af_cfg->FSTMODE_AF);
	RamWriteA(client, LC898122_RWEXD1_L, af_cfg->RWEXD1_L_AF);
	RamWriteA(client, LC898122_RWEXD2_L, af_cfg->RWEXD2_L_AF);
	RamWriteA(client, LC898122_RWEXD3_L, af_cfg->RWEXD3_L_AF);
	RegWriteA(client, LC898122_FSTCTIME, af_cfg->FSTCTIME_AF);
	RegWriteA(client, LC898122_TCODEH, 0x04);

	if (lc898122_dev->state.flags & LC898122_AF_PWMMODE)
		RegWriteA(client, LC898122_PWMAAF, 0x80);

	UcStbb0 |= 0x80;
	RegWriteA(client, LC898122_STBB0, UcStbb0);
	RegWriteA(client, LC898122_STBB1, 0x05);

	lc898122_afdriversw(lc898122_dev, ON);
}

static void lc898122_init_clock(struct lc898122_device *lc898122_dev)
{
	struct i2c_client *client = lc898122_dev->client;
	struct clkcfg {
		u8 pwmdiv;
		u8 srvdiv;
		u8 gifdiv;
		u8 afpwmdiv;
		u8 opafdiv;
		u8 clksel;
	} clkregs = {
		.pwmdiv = 0x00,
		.srvdiv = 0x00,
		.gifdiv = 0x02,
		.afpwmdiv = 0x00,
		.opafdiv = 0x02,
		.clksel = 0x00,
	};

	lc898122_ChkCvr(lc898122_dev); /* Read Cver */

	/*OSC Enables*/
	lc898122_dev->state.UcOscAdjFlg = 0;

	if (lc898122_dev->state.flags & LC898122_SET_DEFAULTS) {
		/*OSC ENABLE*/
		RegWriteA(client, LC898122_OSCSTOP, 0x00);
		RegWriteA(client, LC898122_OSCSET, 0x90);
		RegWriteA(client, LC898122_OSCCNTEN, 0x00);
	}
	/*Clock Enables*/
	RegWriteA(client, LC898122_CLKON, 0x1F);

	if (lc898122_dev->state.flags & LC898122_EXTCLK_ALL) {
		clkregs.clksel = 0x07;
		clkregs.gifdiv = 0x02;
	} else if (lc898122_dev->state.flags & LC898122_EXTCLK_PWM) {
		clkregs.clksel = 0x01;
		clkregs.gifdiv = 0x03;
	}

	if (lc898122_dev->state.UcModule == LC898122_MODULE_20M)
		clkregs.opafdiv = 0x06;
	else
		clkregs.opafdiv = 0x04;

	RegWriteA(client, LC898122_CLKSEL, clkregs.clksel);
	RegWriteA(client, LC898122_PWMDIV, clkregs.pwmdiv);
	RegWriteA(client, LC898122_SRVDIV, clkregs.srvdiv);
	RegWriteA(client, LC898122_GIFDIV, clkregs.gifdiv);
	RegWriteA(client, LC898122_AFPWMDIV, clkregs.afpwmdiv);
	RegWriteA(client, LC898122_OPAFDIV, clkregs.opafdiv);

}

static void lc898122_init_iop(struct lc898122_device *lc898122_dev)
{
	struct i2c_client *client = lc898122_dev->client;

	if (lc898122_dev->state.flags & LC898122_SET_DEFAULTS) {
		RegWriteA(client, LC898122_P0LEV, 0x00);
		RegWriteA(client, LC898122_P0DIR, 0x00);
		RegWriteA(client, LC898122_P0PON, 0x0F);
		RegWriteA(client, LC898122_P0PUD, 0x0F);
	}

	if (lc898122_dev->state.flags & LC898122_USE_3W_DGYRO)
		RegWriteA(client, LC898122_IOP1SEL, 0x02);
	else
		RegWriteA(client, LC898122_IOP1SEL, 0x00);

	if (lc898122_dev->state.flags & LC898122_SET_DEFAULTS) {
		RegWriteA(client, LC898122_IOP0SEL, 0x02);
		RegWriteA(client, LC898122_IOP2SEL, 0x02);
		RegWriteA(client, LC898122_IOP3SEL, 0x00);
		RegWriteA(client, LC898122_IOP4SEL, 0x00);
		RegWriteA(client, LC898122_IOP5SEL, 0x00);
		RegWriteA(client, LC898122_DGINSEL, 0x00);
		RegWriteA(client, LC898122_I2CSEL, 0x00);
		RegWriteA(client, LC898122_DLMODE, 0x00);
	}
}

static void lc898122_init_dgyro(struct lc898122_device *lc898122_dev)
{
	struct i2c_client *client = lc898122_dev->client;
	u8 UcGrini;

	if (lc898122_dev->state.flags & LC898122_USE_3W_DGYRO)
		RegWriteA(client, LC898122_SPIM, 0x00);
	else
		RegWriteA(client, LC898122_SPIM, 0x01);

	RegWriteA(client, LC898122_GRSEL, 0x01);
	RegWriteA(client, LC898122_GRINI, 0x80);

	if (lc898122_dev->state.flags & LC898122_USE_STMICRO_L3G4IS) {

		RegWriteA(client, LC898122_LSBF, 0x03);
		RegWriteA(client, LC898122_GRADR0, 0x0B);
		RegWriteA(client, LC898122_GSETDT, 0x0F);
		RegWriteA(client, LC898122_GRACC, 0x10);
		lc898122_accwait(lc898122_dev, 0x10);
		RegWriteA(client, LC898122_GRADR0, 0x0D);
		RegWriteA(client, LC898122_GSETDT, 0x01);
		RegWriteA(client, LC898122_GRACC, 0x10);
		lc898122_accwait(lc898122_dev, 0x10);

	} else if (lc898122_dev->state.flags & LC898122_USE_INVENSENSE) {

		RegReadA(client, LC898122_GRINI, &UcGrini);
		RegWriteA(client, LC898122_GRINI, (UcGrini |
			LC898122_SLOWMODE));
		RegWriteA(client, LC898122_GRADR0, 0x6A);
		RegWriteA(client, LC898122_GSETDT, 0x10);
		RegWriteA(client, LC898122_GRACC, 0x10);
		lc898122_accwait(lc898122_dev, 0x10);
		RegWriteA(client, LC898122_GRADR0, 0x1B);
		RegWriteA(client, LC898122_GSETDT,
			(LC898122_INVENSENSE_FS_SEL << 3));
		RegWriteA(client, LC898122_GRACC, 0x10);
		lc898122_accwait(lc898122_dev, 0x10);
		RegReadA(client, LC898122_GRINI, &UcGrini);
		RegWriteA(client, LC898122_GRINI,
			(UcGrini & ~LC898122_SLOWMODE));

	} else if (lc898122_dev->state.flags & LC898122_USE_PANASONIC) {

		RegWriteA(client, LC898122_PANAM, 0x09);
		RegWriteA(client, LC898122_REVB7, 0x03);

	} else {
		dev_err(&client->dev, "No gyro configured\n");
	}
	RegWriteA(client, LC898122_RDSEL, 0x7C);
	lc898122_gyoutsignal(lc898122_dev);
}

static void lc898122_init_monitor(struct lc898122_device *lc898122_dev)
{
	struct i2c_client *client = lc898122_dev->client;

	RegWriteA(client, LC898122_PWMMONA, 0x00);
	RegWriteA(client, LC898122_MONSELA, 0x5C);
	RegWriteA(client, LC898122_MONSELB, 0x5D);
	RegWriteA(client, LC898122_MONSELC, 0x00);
	RegWriteA(client, LC898122_MONSELD, 0x00);

	/* Monitor Circuit */
	RegWriteA(client, LC898122_WC_PINMON1, 0x00);
	RegWriteA(client, LC898122_WC_PINMON2, 0x00);
	RegWriteA(client, LC898122_WC_PINMON3, 0x00);
	RegWriteA(client, LC898122_WC_PINMON4, 0x00);

	/* Delay Monitor */
	RegWriteA(client, LC898122_WC_DLYMON11, 0x04);
	RegWriteA(client, LC898122_WC_DLYMON10, 0x40);
	RegWriteA(client, LC898122_WC_DLYMON21, 0x04);
	RegWriteA(client, LC898122_WC_DLYMON20, 0xC0);
	RegWriteA(client, LC898122_WC_DLYMON31, 0x00);
	RegWriteA(client, LC898122_WC_DLYMON30, 0x00);
	RegWriteA(client, LC898122_WC_DLYMON41, 0x00);
	RegWriteA(client, LC898122_WC_DLYMON40, 0x00);

	RegWriteA(client, LC898122_PWMMONA, 0x80);
}

static void lc898122_init_servo(struct lc898122_device *lc898122_dev)
{
	struct i2c_client *client = lc898122_dev->client;
	u8	UcStbb0;

	struct actuator {
		u32 A3_IEXP3;
		u32 A1_IEXP1;
	};

	const struct actuator *actuator_param = NULL;

	const struct actuator actuator_6p5ohm = {
		.A3_IEXP3 = ACTREG_6P5OHM_A3_IEXP3,
		.A1_IEXP1 = ACTREG_6P5OHM_A1_IEXP1,
	};


	const struct actuator actuator_9p2ohm = {
		.A3_IEXP3 = ACTREG_9P2OHM_A3_IEXP3,
		.A1_IEXP1 = ACTREG_9P2OHM_A1_IEXP1,
	};

	const struct actuator actuator_15p0ohm = {
		.A3_IEXP3 = ACTREG_15P0OHM_A3_IEXP3,
		.A1_IEXP1 = ACTREG_15P0OHM_A1_IEXP1,
	};

	if (lc898122_dev->state.flags & LC898122_ACTREG_15P0OHM)
		actuator_param = &actuator_15p0ohm;
	else if (lc898122_dev->state.flags & LC898122_ACTREG_9P2OHM)
		actuator_param = &actuator_9p2ohm;
	else if (lc898122_dev->state.flags & LC898122_ACTREG_6P5OHM)
		actuator_param = &actuator_6p5ohm;
	else
		actuator_param = &actuator_6p5ohm;

	lc898122_dev->state.UcPwmMod = LC898122_INIT_PWMMODE;

	RegWriteA(client, LC898122_WC_EQON, 0x00);
	RegWriteA(client, LC898122_WC_RAMINITON, 0x00);

	lc898122_cleargyro(lc898122_dev, 0x0000, LC898122_CLR_ALL_RAM);

	RegWriteA(client, LC898122_WH_EQSWX, 0x02);
	RegWriteA(client, LC898122_WH_EQSWY, 0x02);

	lc898122_RamAccFixMod(lc898122_dev, OFF);

	/* Monitor Gain */
	RamWrite32A(client, LC898122_dm1g, 0x3F800000);
	RamWrite32A(client, LC898122_dm2g, 0x3F800000);
	RamWrite32A(client, LC898122_dm3g, 0x3F800000);
	RamWrite32A(client, LC898122_dm4g, 0x3F800000);

	/* Hall output limitter */
	RamWrite32A(client, LC898122_sxlmta1, 0x3F800000);
	RamWrite32A(client, LC898122_sylmta1, 0x3F800000);

	/* Emargency Stop */
	RegWriteA(client, LC898122_WH_EMGSTPON,	0x00);
	RegWriteA(client, LC898122_WH_EMGSTPTMR, 0xFF);
	RamWrite32A(client, LC898122_sxemglev, 0x3F800000);
	RamWrite32A(client, LC898122_syemglev, 0x3F800000);

	/* Hall Servo smoothing */
	RegWriteA(client, LC898122_WH_SMTSRVON,	0x00);

	if (lc898122_dev->state.flags & LC898122_EXTCLK_ALL) {
		RegWriteA(client, LC898122_WH_SMTSRVSMP, 0x03);
		RegWriteA(client, LC898122_WH_SMTTMR, 0x07);
	} else {
		RegWriteA(client, LC898122_WH_SMTSRVSMP, 0x06);
		RegWriteA(client, LC898122_WH_SMTTMR, 0x0f);
	}

	RamWrite32A(client, LC898122_sxsmtav, 0xBC800000);
	RamWrite32A(client, LC898122_sysmtav, 0xBC800000);
	RamWrite32A(client, LC898122_sxsmtstp, 0x3AE90466);
	RamWrite32A(client, LC898122_sysmtstp, 0x3AE90466);
	/* High-dimensional correction  */
	RegWriteA(client, LC898122_WH_HOFCON, 0x11);

	/* Front */
	RamWrite32A(client, LC898122_sxiexp3, actuator_param->A3_IEXP3);
	RamWrite32A(client, LC898122_sxiexp2, 0x00000000);
	RamWrite32A(client, LC898122_sxiexp1, actuator_param->A1_IEXP1);
	RamWrite32A(client, LC898122_sxiexp0, 0x00000000);
	RamWrite32A(client, LC898122_sxiexp, 0x3F800000);
	RamWrite32A(client, LC898122_syiexp3, actuator_param->A3_IEXP3);
	RamWrite32A(client, LC898122_syiexp2, 0x00000000);
	RamWrite32A(client, LC898122_syiexp1, actuator_param->A1_IEXP1);
	RamWrite32A(client, LC898122_syiexp0, 0x00000000);
	RamWrite32A(client, LC898122_syiexp, 0x3F800000);

	/* Back */
	RamWrite32A(client, LC898122_sxoexp3, actuator_param->A3_IEXP3);
	RamWrite32A(client, LC898122_sxoexp2, 0x00000000);
	RamWrite32A(client, LC898122_sxoexp1, actuator_param->A1_IEXP1);
	RamWrite32A(client, LC898122_sxoexp0, 0x00000000);
	RamWrite32A(client, LC898122_sxoexp, 0x3F800000);
	RamWrite32A(client, LC898122_syoexp3, actuator_param->A3_IEXP3);
	RamWrite32A(client, LC898122_syoexp2, 0x00000000);
	RamWrite32A(client, LC898122_syoexp1, actuator_param->A1_IEXP1);
	RamWrite32A(client, LC898122_syoexp0, 0x00000000);
	RamWrite32A(client, LC898122_syoexp, 0x3F800000);

	/* Sine wave */
	if (lc898122_dev->state.flags & LC898122_SET_DEFAULTS) {
		RegWriteA(client, LC898122_WC_SINON, 0x00);
		RegWriteA(client, LC898122_WC_SINFRQ0, 0x00);
		RegWriteA(client, LC898122_WC_SINFRQ1, 0x60);
		RegWriteA(client, LC898122_WC_SINPHSX, 0x00);
		RegWriteA(client, LC898122_WC_SINPHSY, 0x20);

		/* AD over sampling */
		RegWriteA(client, LC898122_WC_ADMODE, 0x06);

		/* Measure mode */
		RegWriteA(client, LC898122_WC_MESMODE, 0x00);
		RegWriteA(client, LC898122_WC_MESSINMODE, 0x00);
		RegWriteA(client, LC898122_WC_MESLOOP0,	0x08);
		RegWriteA(client, LC898122_WC_MESLOOP1,	0x02);
		RegWriteA(client, LC898122_WC_MES1ADD0,	0x00);
		RegWriteA(client, LC898122_WC_MES1ADD1,	0x00);
		RegWriteA(client, LC898122_WC_MES2ADD0,	0x00);
		RegWriteA(client, LC898122_WC_MES2ADD1,	0x00);
		RegWriteA(client, LC898122_WC_MESABS, 0x00);
		RegWriteA(client, LC898122_WC_MESWAIT, 0x00);

		/* auto measure */
		RegWriteA(client, LC898122_WC_AMJMODE, 0x00);
		RegWriteA(client, LC898122_WC_AMJLOOP0, 0x08);
		RegWriteA(client, LC898122_WC_AMJLOOP1, 0x02);
		RegWriteA(client, LC898122_WC_AMJIDL0, 0x02);
		RegWriteA(client, LC898122_WC_AMJIDL1, 0x00);
		RegWriteA(client, LC898122_WC_AMJ1ADD0, 0x00);
		RegWriteA(client, LC898122_WC_AMJ1ADD1, 0x00);
		RegWriteA(client, LC898122_WC_AMJ2ADD0,	0x00);
		RegWriteA(client, LC898122_WC_AMJ2ADD1,	0x00);

		/* Data Pass */
		RegWriteA(client, LC898122_WC_DPI1ADD0, 0x00);
		RegWriteA(client, LC898122_WC_DPI1ADD1,	0x00);
		RegWriteA(client, LC898122_WC_DPI2ADD0,	0x00);
		RegWriteA(client, LC898122_WC_DPI2ADD1,	0x00);
		RegWriteA(client, LC898122_WC_DPI3ADD0,	0x00);
		RegWriteA(client, LC898122_WC_DPI3ADD1,	0x00);
		RegWriteA(client, LC898122_WC_DPI4ADD0,	0x00);
		RegWriteA(client, LC898122_WC_DPI4ADD1,	0x00);
		RegWriteA(client, LC898122_WC_DPO1ADD0,	0x00);
		RegWriteA(client, LC898122_WC_DPO1ADD1,	0x00);
		RegWriteA(client, LC898122_WC_DPO2ADD0,	0x00);
		RegWriteA(client, LC898122_WC_DPO2ADD1,	0x00);
		RegWriteA(client, LC898122_WC_DPO3ADD0,	0x00);
		RegWriteA(client, LC898122_WC_DPO3ADD1,	0x00);
		RegWriteA(client, LC898122_WC_DPO4ADD0,	0x00);
		RegWriteA(client, LC898122_WC_DPO4ADD1,	0x00);
		RegWriteA(client, LC898122_WC_DPON, 0x00);

		/* Interrupt Flag */
		RegWriteA(client, LC898122_WC_INTMSK, 0xFF);
	}

	/* Ram Access */
	lc898122_RamAccFixMod(lc898122_dev, OFF);

	/* PWM Signal Generate */
	lc898122_driversw(lc898122_dev, OFF);

	RegWriteA(client, LC898122_DRVFC2, 0x90);
	RegWriteA(client, LC898122_DRVSELX, 0xFF);
	RegWriteA(client, LC898122_DRVSELY, 0xFF);


	if (lc898122_dev->state.flags & LC898122_PWM_BREAK) {
		if (lc898122_dev->state.UcCvrCod == LC898122_CVER122)
			RegWriteA(client, LC898122_PWMFC, 0x2D);
		else
			RegWriteA(client, LC898122_PWMFC, 0x3D);
	} else {
		RegWriteA(client, LC898122_PWMFC, 0x21);
	}

	if (lc898122_dev->state.flags & LC898122_USE_VH_SYNC) {
		RegWriteA(client, LC898122_STROBEFC, 0x80);
		RegWriteA(client, LC898122_STROBEDLYX, 0x00);
		RegWriteA(client, LC898122_STROBEDLYY, 0x00);
	}

	RegWriteA(client, LC898122_PWMA, 0x00);
	RegWriteA(client, LC898122_PWMDLYX, 0x04);
	RegWriteA(client, LC898122_PWMDLYY, 0x04);

	if (lc898122_dev->state.flags & LC898122_SET_DEFAULTS) {
		RegWriteA(client, LC898122_DRVCH1SEL, 0x00);
		RegWriteA(client, LC898122_DRVCH2SEL, 0x00);

		RegWriteA(client, LC898122_PWMDLYTIMX, 0x00);
		RegWriteA(client, LC898122_PWMDLYTIMY, 0x00);
	}

	if (lc898122_dev->state.UcCvrCod == LC898122_CVER122) {
		RegWriteA(client, LC898122_PWMPERIODY,	0x00);
		RegWriteA(client, LC898122_PWMPERIODY2,	0x00);
	} else {
		if (lc898122_dev->state.flags & LC898122_EXTCLK_PWM) {
			RegWriteA(client, LC898122_PWMPERIODX,	0x84);
			RegWriteA(client, LC898122_PWMPERIODX2,	0x00);
			RegWriteA(client, LC898122_PWMPERIODY,	0x84);
			RegWriteA(client, LC898122_PWMPERIODY2,	0x00);
		} else {
			RegWriteA(client, LC898122_PWMPERIODX,	0x00);
			RegWriteA(client, LC898122_PWMPERIODX2,	0x00);
			RegWriteA(client, LC898122_PWMPERIODY,	0x00);
			RegWriteA(client, LC898122_PWMPERIODY2,	0x00);
		}
	}

	/* Linear PWM circuit setting */
	RegWriteA(client, LC898122_CVA, 0xC0);

	if (lc898122_dev->state.UcCvrCod == LC898122_CVER122)
		RegWriteA(client, LC898122_CVFC, 0x22);

	RegWriteA(client, LC898122_CVFC2, 0x80);
	if (lc898122_dev->state.UcCvrCod == LC898122_CVER122) {
		RegWriteA(client, LC898122_CVSMTHX, 0x00);
		RegWriteA(client, LC898122_CVSMTHY, 0x00);
	}

	RegReadA(client, LC898122_STBB0, &UcStbb0);
	UcStbb0 &= 0x80;
	RegWriteA(client, LC898122_STBB0, UcStbb0);
}

static void lc898122_init_gyro(struct lc898122_device *lc898122_dev)
{
	struct i2c_client *client = lc898122_dev->client;

	struct gyro_cfg {
		u32	GYRLMT1H;
		u32	GYRLMT3_S1;
		u32	GYRLMT3_S2;
		u32	GYRLMT4_S1;
		u32	GYRLMT4_S2;
		u32	GYRA12_HGH;
		u32	GYRA12_MID;
		u32	GYRA34_HGH;
		u32	GYRA34_MID;
		u32	GYRB12_HGH;
		u32	GYRB12_MID;
		u32	GYRB34_HGH;
		u32	GYRB34_MID;
	};

	const struct gyro_cfg gyro_13M = {
		.GYRLMT1H	= LC898122_GYRLMT1H_13M,
		.GYRLMT3_S1	= LC898122_GYRLMT3_S1_13M,
		.GYRLMT3_S2	= LC898122_GYRLMT3_S2_13M,
		.GYRLMT4_S1	= LC898122_GYRLMT4_S1_13M,
		.GYRLMT4_S2	= LC898122_GYRLMT4_S2_13M,
		.GYRA12_HGH	= LC898122_GYRA12_HGH_13M,
		.GYRA12_MID	= LC898122_GYRA12_MID_13M,
		.GYRA34_HGH	= LC898122_GYRA34_HGH_13M,
		.GYRA34_MID	= LC898122_GYRA34_MID_13M,
		.GYRB12_HGH	= LC898122_GYRB12_HGH_13M,
		.GYRB12_MID	= LC898122_GYRB12_MID_13M,
		.GYRB34_HGH	= LC898122_GYRB34_HGH_13M,
		.GYRB34_MID	= LC898122_GYRB34_MID_13M,
	};

	const struct gyro_cfg gyro_20M = {
		.GYRLMT1H	= LC898122_GYRLMT1H_20M,
		.GYRLMT3_S1	= LC898122_GYRLMT3_S1_20M,
		.GYRLMT3_S2	= LC898122_GYRLMT3_S2_20M,
		.GYRLMT4_S1	= LC898122_GYRLMT4_S1_20M,
		.GYRLMT4_S2	= LC898122_GYRLMT4_S2_20M,
		.GYRA12_HGH	= LC898122_GYRA12_HGH_20M,
		.GYRA12_MID	= LC898122_GYRA12_MID_20M,
		.GYRA34_HGH	= LC898122_GYRA34_HGH_20M,
		.GYRA34_MID	= LC898122_GYRA34_MID_20M,
		.GYRB12_HGH	= LC898122_GYRB12_HGH_20M,
		.GYRB12_MID	= LC898122_GYRB12_MID_20M,
		.GYRB34_HGH	= LC898122_GYRB34_HGH_20M,
		.GYRB34_MID	= LC898122_GYRB34_MID_20M,
	};

	const struct gyro_cfg *gyro;

	if (lc898122_dev->state.UcModule == LC898122_MODULE_20M)
		gyro = &gyro_20M;
	else
		gyro = &gyro_13M;

	/* Gyro Filter Setting */
	RegWriteA(client, LC898122_WG_EQSW, 0x03);

	/* Gyro Filter Down Sampling */
	RegWriteA(client, LC898122_WG_SHTON, 0x10);

	if (lc898122_dev->state.flags & LC898122_SET_DEFAULTS) {
		RegWriteA(client, LC898122_WG_SHTDLYTMR, 0x00);
		RegWriteA(client, LC898122_WG_GADSMP, 0x00);
		RegWriteA(client, LC898122_WG_HCHR, 0x00);
		RegWriteA(client, LC898122_WG_LMT3MOD, 0x00);
		RegWriteA(client, LC898122_WG_VREFADD, 0x12);
	}

	RegWriteA(client, LC898122_WG_SHTMOD, 0x06);

	/* Limiter */
	RamWrite32A(client, LC898122_gxlmt1H, gyro->GYRLMT1H);
	RamWrite32A(client, LC898122_gylmt1H, gyro->GYRLMT1H);

	RamWrite32A(client, LC898122_gxlmt3HS0, gyro->GYRLMT3_S1);
	RamWrite32A(client, LC898122_gylmt3HS0, gyro->GYRLMT3_S1);
	RamWrite32A(client, LC898122_gxlmt3HS1, gyro->GYRLMT3_S2);
	RamWrite32A(client, LC898122_gylmt3HS1, gyro->GYRLMT3_S2);
	RamWrite32A(client, LC898122_gylmt4HS0, gyro->GYRLMT4_S1);
	RamWrite32A(client, LC898122_gxlmt4HS0, gyro->GYRLMT4_S1);
	RamWrite32A(client, LC898122_gxlmt4HS1, gyro->GYRLMT4_S2);
	RamWrite32A(client, LC898122_gylmt4HS1, gyro->GYRLMT4_S2);

	/* Pan/Tilt parameter */
	RegWriteA(client, LC898122_WG_PANADDA, 0x12);
	RegWriteA(client, LC898122_WG_PANADDB, 0x09);

	/* Threshold */
	RamWrite32A(client, LC898122_SttxHis, 0x00000000);
	RamWrite32A(client, LC898122_SttxaL, 0x00000000);
	RamWrite32A(client, LC898122_SttxbL, 0x00000000);
	RamWrite32A(client, LC898122_Sttx12aM, gyro->GYRA12_MID);
	RamWrite32A(client, LC898122_Sttx12aH, gyro->GYRA12_HGH);
	RamWrite32A(client, LC898122_Sttx12bM, gyro->GYRB12_MID);
	RamWrite32A(client, LC898122_Sttx12bH, gyro->GYRB12_HGH);
	RamWrite32A(client, LC898122_Sttx34aM, gyro->GYRA34_MID);
	RamWrite32A(client, LC898122_Sttx34aH, gyro->GYRA34_HGH);
	RamWrite32A(client, LC898122_Sttx34bM, gyro->GYRB34_MID);
	RamWrite32A(client, LC898122_Sttx34bH, gyro->GYRB34_HGH);
	RamWrite32A(client, LC898122_SttyaL, 0x00000000);
	RamWrite32A(client, LC898122_SttybL, 0x00000000);
	RamWrite32A(client, LC898122_Stty12aM, gyro->GYRA12_MID);
	RamWrite32A(client, LC898122_Stty12aH, gyro->GYRA12_HGH);
	RamWrite32A(client, LC898122_Stty12bM, gyro->GYRB12_MID);
	RamWrite32A(client, LC898122_Stty12bH, gyro->GYRB12_HGH);
	RamWrite32A(client, LC898122_Stty34aM, gyro->GYRA34_MID);
	RamWrite32A(client, LC898122_Stty34aH, gyro->GYRA34_HGH);
	RamWrite32A(client, LC898122_Stty34bM, gyro->GYRB34_MID);
	RamWrite32A(client, LC898122_Stty34bH, gyro->GYRB34_HGH);
	/*  Pan level */
	RegWriteA(client, LC898122_WG_PANLEVABS, 0x00);
	/* Average parameter are set IniAdj */

	/* Phase Transition Settings */
	RegWriteA(client, LC898122_WG_PANSTT21JUG0, 0x00);
	RegWriteA(client, LC898122_WG_PANSTT21JUG1, 0x00);
	RegWriteA(client, LC898122_WG_PANSTT31JUG0, 0x00);
	RegWriteA(client, LC898122_WG_PANSTT31JUG1, 0x00);
	RegWriteA(client, LC898122_WG_PANSTT41JUG0, 0x01);
	RegWriteA(client, LC898122_WG_PANSTT41JUG1, 0x00);
	RegWriteA(client, LC898122_WG_PANSTT12JUG0, 0x00);
	RegWriteA(client, LC898122_WG_PANSTT12JUG1, 0x07);
	RegWriteA(client, LC898122_WG_PANSTT13JUG0, 0x00);
	RegWriteA(client, LC898122_WG_PANSTT13JUG1, 0x00);
	RegWriteA(client, LC898122_WG_PANSTT23JUG0, 0x11);
	RegWriteA(client, LC898122_WG_PANSTT23JUG1, 0x00);
	RegWriteA(client, LC898122_WG_PANSTT43JUG0, 0x00);
	RegWriteA(client, LC898122_WG_PANSTT43JUG1, 0x00);
	RegWriteA(client, LC898122_WG_PANSTT34JUG0, 0x01);
	RegWriteA(client, LC898122_WG_PANSTT34JUG1, 0x00);
	RegWriteA(client, LC898122_WG_PANSTT24JUG0, 0x00);
	RegWriteA(client, LC898122_WG_PANSTT24JUG1, 0x00);
	RegWriteA(client, LC898122_WG_PANSTT42JUG0, 0x44);
	RegWriteA(client, LC898122_WG_PANSTT42JUG1, 0x04);

	/* State Timer */
	RegWriteA(client, LC898122_WG_PANSTT1LEVTMR, 0x00);
	RegWriteA(client, LC898122_WG_PANSTT2LEVTMR, 0x00);
	RegWriteA(client, LC898122_WG_PANSTT3LEVTMR, 0x00);
	RegWriteA(client, LC898122_WG_PANSTT4LEVTMR, 0x03);

	/* Control filter */
	RegWriteA(client, LC898122_WG_PANTRSON0, 0x11);
	/* State Setting */
	lc898122_IniPtMovMod(lc898122_dev, OFF);
	/* Hold */
	RegWriteA(client, LC898122_WG_PANSTTSETILHLD, 0x00);
	/* State2,4 Step Time Setting */
	RegWriteA(client, LC898122_WG_PANSTT2TMR0, 0x01);
	RegWriteA(client, LC898122_WG_PANSTT2TMR1, 0x00);
	RegWriteA(client, LC898122_WG_PANSTT4TMR0, 0x02);
	RegWriteA(client, LC898122_WG_PANSTT4TMR1, 0x07);
	RegWriteA(client, LC898122_WG_PANSTTXXXTH, 0x00);

	/* NEW_PTST code not yet ported here */

	if (lc898122_dev->state.flags & LC898122_GAIN_CONT) {
		RamWrite32A(client, LC898122_gxlevlow, LC898122_TRI_LEVEL);
		RamWrite32A(client, LC898122_gylevlow, LC898122_TRI_LEVEL);
		RamWrite32A(client, LC898122_gxadjmin, LC898122_XMINGAIN);
		RamWrite32A(client, LC898122_gxadjmax, LC898122_XMAXGAIN);
		RamWrite32A(client, LC898122_gxadjdn, LC898122_XSTEPDN);
		RamWrite32A(client, LC898122_gxadjup, LC898122_XSTEPUP);
		RamWrite32A(client, LC898122_gyadjmin, LC898122_YMINGAIN);
		RamWrite32A(client, LC898122_gyadjmax, LC898122_YMAXGAIN);
		RamWrite32A(client, LC898122_gyadjdn, LC898122_YSTEPDN);
		RamWrite32A(client, LC898122_gyadjup, LC898122_YSTEPUP);

		RegWriteA(client, LC898122_WG_LEVADD, (u8) LC898122_MONADR);
		if (lc898122_dev->state.flags & LC898122_EXTCLK_ALL)
			RegWriteA(client, LC898122_WG_LEVTMR,
				LC898122_TIMEBSE_EXT);
		else
			RegWriteA(client, LC898122_WG_LEVTMR,
				LC898122_TIMEBSE);
		RegWriteA(client, LC898122_WG_LEVTMRLOW, LC898122_TIMELOW);
		RegWriteA(client, LC898122_WG_LEVTMRHGH, LC898122_TIMEHGH);
		RegWriteA(client, LC898122_WG_ADJGANADD, (u8) LC898122_GANADR);
		RegWriteA(client, LC898122_WG_ADJGANGO, 0x00);

		/* exe function */
		lc898122_autogaincontrol(lc898122_dev, OFF);
	}
}

static void lc898122_init_filters(struct lc898122_device *lc898122_dev)
{
	struct i2c_client *client = lc898122_dev->client;
	int index;
	struct STFILREG *pFilReg;
	struct STFILRAM	*pFilRam;

	if (lc898122_dev->state.UcModule == LC898122_MODULE_20M) {
		pFilReg = (struct STFILREG *)CsFilReg_20M;
		if (lc898122_dev->state.flags & LC898122_XY_SIMU_SET)
			pFilRam = (struct STFILRAM *)CsFilRam_20M_simul_set;
		else
			pFilRam = (struct STFILRAM *)CsFilRam_20M;
	} else {
		pFilReg = (struct STFILREG *)CsFilReg_13M;
		if (lc898122_dev->state.flags & LC898122_XY_SIMU_SET)
			pFilRam = (struct STFILRAM *)CsFilRam_13M_simul_set;
		else
			pFilRam = (struct STFILRAM *)CsFilRam_13M;
	}

	index = 0;
	while (pFilReg[index].UsRegAdd != 0xFFFF) {
		RegWriteA(client,
			pFilReg[index].UsRegAdd,
			pFilReg[index].UcRegDat);
		index++;
	}
	if (lc898122_dev->state.flags & LC898122_XY_SIMU_SET)
		RegWriteA(client, LC898122_WC_RAMACCXY, 0x01);

	index = 0;
	while (pFilRam[index].UsRamAdd != 0xFFFF) {
		RamWrite32A(client,
			pFilRam[index].UsRamAdd,
			pFilRam[index].UlRamDat);
		index++;
	}

	if (lc898122_dev->state.flags & LC898122_XY_SIMU_SET)
		RegWriteA(client, LC898122_WC_RAMACCXY, 0x00);
}

static void lc898122_init_adjust(struct lc898122_device *lc898122_dev)
{
	struct i2c_client *client = lc898122_dev->client;
	u8 BIAS_CUR_OIS;
	u8 AMP_GAIN_X;
	u8 AMP_GAIN_Y;

	if (lc898122_dev->state.UcModule == LC898122_MODULE_20M) {
		BIAS_CUR_OIS = LC898122_BIAS_CUR_OIS_20M;
		AMP_GAIN_X = LC898122_AMP_GAIN_X_20M;
		AMP_GAIN_Y = LC898122_AMP_GAIN_Y_20M;
	} else {
		BIAS_CUR_OIS = LC898122_BIAS_CUR_OIS_13M;
		AMP_GAIN_X = LC898122_AMP_GAIN_X_13M;
		AMP_GAIN_Y = LC898122_AMP_GAIN_Y_13M;
	}

	RegWriteA(client, LC898122_WC_RAMACCXY, 0x00);

	lc898122_IniPtAve(lc898122_dev);

	/* OIS */
	RegWriteA(client, LC898122_CMSDAC0, BIAS_CUR_OIS);
	RegWriteA(client, LC898122_OPGSEL0, AMP_GAIN_X);
	RegWriteA(client, LC898122_OPGSEL1, AMP_GAIN_Y);
	/* AF */
	RegWriteA(client, LC898122_CMSDAC1, LC898122_BIAS_CUR_AF);
	RegWriteA(client, LC898122_OPGSEL2, LC898122_AMP_GAIN_AF);

	RegWriteA(client, LC898122_OSCSET, LC898122_OSC_INI);

	/* adjusted value */
	RegWriteA(client, LC898122_IZAH, LC898122_DGYRO_OFST_XH);
	RegWriteA(client, LC898122_IZAL, LC898122_DGYRO_OFST_XL);
	RegWriteA(client, LC898122_IZBH, LC898122_DGYRO_OFST_YH);
	RegWriteA(client, LC898122_IZBL, LC898122_DGYRO_OFST_YL);

	/* Ram Access */
	lc898122_RamAccFixMod(lc898122_dev, ON);

	/* OIS adjusted parameter */
	RamWriteA(client, LC898122_DAXHLO, LC898122_DAHLXO_INI);
	RamWriteA(client, LC898122_DAXHLB, LC898122_DAHLXB_INI);
	RamWriteA(client, LC898122_DAYHLO, LC898122_DAHLYO_INI);
	RamWriteA(client, LC898122_DAYHLB, LC898122_DAHLYB_INI);
	RamWriteA(client, LC898122_OFF0Z, LC898122_HXOFF0Z_INI);
	RamWriteA(client, LC898122_OFF1Z, LC898122_HYOFF1Z_INI);
	RamWriteA(client, LC898122_sxg, LC898122_SXGAIN_INI);
	RamWriteA(client, LC898122_syg, LC898122_SYGAIN_INI);

	/* AF adjusted parameter */
	RamWriteA(client, LC898122_DAZHLO, LC898122_DAHLZO_INI);
	RamWriteA(client, LC898122_DAZHLB, LC898122_DAHLZB_INI);

	/* Ram Access */
	lc898122_RamAccFixMod(lc898122_dev, OFF);

	if (lc898122_dev->state.UcAfType == LC898122_BI_DIR)
		lc898122_SetDOFSTDAF(lc898122_dev, LC898122_AFDROF_INI);

	RamWrite32A(client, LC898122_gxzoom, LC898122_GXGAIN_INI);
	RamWrite32A(client, LC898122_gyzoom, LC898122_GYGAIN_INI);

	RamWrite32A(client, LC898122_sxq, LC898122_SXQ_INI);
	RamWrite32A(client, LC898122_syq, LC898122_SYQ_INI);

	if (LC898122_GXHY_GYHX) {
		RamWrite32A(client, LC898122_sxgx, 0x00000000);
		RamWrite32A(client, LC898122_sxgy, 0x3F800000);
		RamWrite32A(client, LC898122_sygy, 0x00000000);
		RamWrite32A(client, LC898122_sygx, 0x3F800000);
	}

	lc898122_SetZsp(lc898122_dev, 0);

	RegWriteA(client, LC898122_PWMA, 0xC0);

	RegWriteA(client, LC898122_STBB0, 0xDF);
	RegWriteA(client, LC898122_WC_EQSW, 0x02);
	RegWriteA(client, LC898122_WC_MESLOOP1, 0x02);
	RegWriteA(client, LC898122_WC_MESLOOP0, 0x00);
	RegWriteA(client, LC898122_WC_AMJLOOP1, 0x02);
	RegWriteA(client, LC898122_WC_AMJLOOP0, 0x00);

	lc898122_SetPanTiltMode(lc898122_dev, OFF);
	lc898122_SetGcf(lc898122_dev, 0);

	if (lc898122_dev->state.flags & LC898122_H1COEF_CHANGER)
		lc898122_SetH1cMod(lc898122_dev, LC898122_ACTMODE);

	lc898122_driversw(lc898122_dev, ON);
	RegWriteA(client, LC898122_WC_EQON, 0x01);
}

void lc898122_selectmodule(struct lc898122_device *lc898122_dev,
			    u8 uc_module)
{
	switch (uc_module) {
	case LC898122_MODULE_13M:
		lc898122_dev->state.UcAfType = LC898122_UNI_DIR;
		lc898122_dev->state.UcModule = LC898122_MODULE_13M;
		break;
	case LC898122_MODULE_20M:
		lc898122_dev->state.UcAfType = LC898122_BI_DIR;
		lc898122_dev->state.UcModule = LC898122_MODULE_20M;
		break;
	default:
		lc898122_dev->state.UcAfType = LC898122_UNI_DIR;
		lc898122_dev->state.UcModule = LC898122_MODULE_13M;
		break;
	}
}

void lc898122_initsettings(struct lc898122_device *lc898122_dev)
{
	/* Clock Setting */
	lc898122_init_clock(lc898122_dev);
	/* I/O Port Initial Setting */
	lc898122_init_iop(lc898122_dev);
	/* DigitalGyro Initial Setting */
	lc898122_init_dgyro(lc898122_dev);
	/* Monitor & Other Initial Setting */
	lc898122_init_monitor(lc898122_dev);
	/* Servo Initial Setting */
	lc898122_init_servo(lc898122_dev);
	/* Gyro Filter Initial Setting */
	lc898122_init_gyro(lc898122_dev);
	/* Gyro Filter Initial Setting */
	lc898122_init_filters(lc898122_dev);
	/* Adjust Fix Value Setting */
	lc898122_init_adjust(lc898122_dev);

}

void lc898122_initsettingsaf(struct lc898122_device *lc898122_dev)
{
	/* Clock Setting */
	lc898122_init_clock(lc898122_dev);
	/* AF Initial Setting */
	lc898122_afinitialsetting(lc898122_dev);

}

