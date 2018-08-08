/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2015 - 2018 Intel Corporation
 * Copyright (C) ON Semiconductor
 *
 */

#ifndef __LC898122_OISINIT_H__
#define  __LC898122_OISINIT_H__

#define	LC898122_FW_VER			0x0003

#define LC898122_SET_DEFAULTS       0x000001
#define LC898122_EXTCLK_ALL         0x000002
#define LC898122_EXTCLK_PWM         0x000004
#define LC898122_USE_3W_DGYRO       0x000008
#define LC898122_USE_STMICRO_L3G4IS 0x000010
#define LC898122_USE_INVENSENSE     0x000020
#define LC898122_USE_PANASONIC      0x000040
#define LC898122_PWM_BREAK          0x000080
#define LC898122_USE_VH_SYNC        0x000100
#define LC898122_GAIN_CONT          0x000200
#define LC898122_H1COEF_CHANGER     0x000400
#define LC898122_XY_SIMU_SET        0x000800
#define LC898122_AF_PWMMODE         0x001000
#define LC898122_GYROSTANDBY        0x002000
#define LC898122_MONITOR_OFF        0x004000
#define LC898122_NEUTRAL_CENTER     0x008000
#define LC898122_ACTREG_15P0OHM     0x010000
#define LC898122_ACTREG_9P2OHM      0x020000
#define LC898122_ACTREG_6P5OHM      0x040000
#define LC898122_ACCEPTANCE         0x080000
#define LC898122_USE_SINLPF         0x100000


#define LC898122_DEFCONFIG (LC898122_XY_SIMU_SET | LC898122_USE_INVENSENSE | \
			    LC898122_GAIN_CONT | LC898122_SET_DEFAULTS | \
			    LC898122_ACTREG_6P5OHM | LC898122_PWM_BREAK | \
			    LC898122_H1COEF_CHANGER | LC898122_USE_SINLPF)
struct lc898122_device;

struct lc898122_ois {
	u8 UcAfType;
	u8 UcModule;
	u8 UcOscAdjFlg;
	u8 UcPwmMod;
	u8 UcCvrCod;
	u32  flags;
	u32 UlH1Coefval;
	u8 UcH1LvlMod;
	u16	UsCntXof;
	u16	UsCntYof;
};

int lc898122_read_long(struct i2c_client *client, u16 reg, u32 *val);
int lc898122_write_long(struct i2c_client *client, u16 reg, u32 val);
int lc898122_read_word(struct i2c_client *client, u16 reg, u16 *val);
int lc898122_write_word(struct i2c_client *client, u16 reg, u16 val);
int lc898122_read_byte(struct i2c_client *client, u16 reg, u8 *val);
int lc898122_write_byte(struct i2c_client *client, u16 reg, u8 val);

static inline int RamRead32A(struct i2c_client *client, u16 addr, u32 *data)
{
	return lc898122_read_long(client, addr, data);
}

static inline int RamWrite32A(struct i2c_client *client, u16 addr, u32 data)
{
	return lc898122_write_long(client, addr, data);
}

static inline int RamReadA(struct i2c_client *client, u16 addr, u16 *data)
{
	return lc898122_read_word(client, addr, data);
}

static inline int RamWriteA(struct i2c_client *client, u16 addr, u16 data)
{
	return lc898122_write_word(client, addr, data);
}

static inline int RegReadA(struct i2c_client *client, u16 addr, u8 *data)
{
	return lc898122_read_byte(client, addr, data);
}

static inline int RegWriteA(struct i2c_client *client, u16 addr, u8 data)
{
	return lc898122_write_byte(client, addr, data);
}

#define LC898122_HALL_ADJ  0
#define LC898122_LOOPGAIN  1
#define LC898122_THROUGH   2
#define LC898122_NOISE     3

#define LC898122_INVENSENSE_FS_SEL		3
#define	LC898122_INVENSENSE_GYROX_INI		0x45
#define	LC898122_INVENSENSE_GYROY_INI		0x43

#define	LC898122_STMICRO_GYROX_INI		0x43
#define	LC898122_STMICRO_GYROY_INI		0x45

#define	LC898122_PANASONIC_GYROX_INI		0x7C
#define LC898122_PANASONIC_GYROY_INI		0x78

#define LC898122_ACT_CHK_LVL 0x3ecccccd
#define LC898122_ACT_THR     0x0400
#define LC898122_GEA_DIF_HIG 0x0010
#define LC898122_GEA_DIF_LOW 0x0001

/* OIS Calibration Parameter */
#define LC898122_DAHLXO_INI	0x0000
#define LC898122_DAHLXB_INI	0xC000
#define	LC898122_DAHLYO_INI	0x0000
#define	LC898122_DAHLYB_INI	0xC000
#define	LC898122_HXOFF0Z_INI	0x0000
#define	LC898122_HYOFF1Z_INI	0x0000
#define	LC898122_SXGAIN_INI	0x2000
#define	LC898122_SYGAIN_INI	0x2000
#define	LC898122_DGYRO_OFST_XH	0x00
#define	LC898122_DGYRO_OFST_XL	0x00
#define	LC898122_DGYRO_OFST_YH	0x00
#define	LC898122_DGYRO_OFST_YL	0x00
#define	LC898122_GXGAIN_INI	0x3F333333
#define	LC898122_GYGAIN_INI	0xBF333333
#define	LC898122_OSC_INI	0x2C

/* Actuator Select */
/* Hall parameter */
#define	LC898122_BIAS_CUR_OIS_20M	0x44
#define	LC898122_AMP_GAIN_X_20M		0x03
#define	LC898122_AMP_GAIN_Y_20M		0x03
#define	LC898122_BIAS_CUR_OIS_13M	0x33
#define	LC898122_AMP_GAIN_X_13M		0x05
#define	LC898122_AMP_GAIN_Y_13M		0x05

/* AF Open parameter */
#define	LC898122_RWEXD1_L_AF_20M	0x7FFF
#define	LC898122_RWEXD2_L_AF_20M	0x113E
#define	LC898122_RWEXD3_L_AF_20M	0x7211
#define	LC898122_FSTCTIME_AF_20M	0xA9
#define	LC898122_FSTMODE_AF_20M		0x00
#define	LC898122_RWEXD1_L_AF_13M	0x7FFF
#define	LC898122_RWEXD2_L_AF_13M	0x4a02
#define	LC898122_RWEXD3_L_AF_13M	0x7d62
#define	LC898122_FSTCTIME_AF_13M	0xF9
#define	LC898122_FSTMODE_AF_13M		0x02

/* (0.3750114X^3+0.5937681X)*(0.3750114X^3+0.5937681X) 6.5ohm*/
#define ACTREG_6P5OHM_A3_IEXP3          0x3EC0017F
#define ACTREG_6P5OHM_A1_IEXP1          0x3F180130

/* (0.3750114X^3+0.55X)*(0.3750114X^3+0.55X) 9.2ohm*/
#define ACTREG_9P2OHM_A3_IEXP3          0x3EC0017F
#define ACTREG_9P2OHM_A1_IEXP1          0x3F0CCCCD

/* (0.4531388X^3+0.4531388X)*(0.4531388X^3+0.4531388X) 15ohm*/
#define ACTREG_15P0OHM_A3_IEXP3         0x3EE801CF
#define ACTREG_15P0OHM_A1_IEXP1         0x3EE801CF

/* AF adjust parameter */
#define	LC898122_DAHLZB_INI		0x9000
#define	LC898122_DAHLZO_INI		0x0000
#define	LC898122_BIAS_CUR_AF		0x00
#define	LC898122_AMP_GAIN_AF		0x00

#define LC898122_AFDROF_INI		0x10

/*** Hall, Gyro Parameter Setting ***/
/* Hall Parameter */
#define LC898122_SXGAIN_LOP		0x3000
#define	LC898122_SYGAIN_LOP		0x3000
#define LC898122_SXQ_INI		0x3F800000
#define	LC898122_SYQ_INI		0xBF800000

/* Gyro Parameter */
#define	LC898122_GXHY_GYHX		0
#define	LC898122_TCODEH_ADJ		0x0000

#define	LC898122_GYRLMT1H_20M		0x3DCCCCC0 /* 0.1F */
#define	LC898122_GYRLMT3_S1_20M		0x3EE66666 /* 0.45F */
#define	LC898122_GYRLMT3_S2_20M		0x3EE66666 /* 0.45F */

#define	LC898122_GYRLMT4_S1_20M		0x40400000 /* 3.0F */
#define	LC898122_GYRLMT4_S2_20M		0x40400000 /* 3.0F */

#define	LC898122_GYRA12_HGH_20M		0x40000000 /* 2.00F */
#define	LC898122_GYRA12_MID_20M		0x3F800000 /* 1.0F */
#define	LC898122_GYRA34_HGH_20M		0x3F000000 /* 0.5F */
#define	LC898122_GYRA34_MID_20M		0x3DCCCCCD /* 0.1F */

#define	LC898122_GYRB12_HGH_20M		0x3E4CCCCD /* 0.20F */
#define	LC898122_GYRB12_MID_20M		0x3CA3D70A /* 0.02F */
#define	LC898122_GYRB34_HGH_20M		0x3CA3D70A /* 0.02F */
#define	LC898122_GYRB34_MID_20M		0x3C23D70A /* 0.001F */

#define	LC898122_GYRLMT1H_13M		0x3DCCCCC0 /* 0.1F */
#define	LC898122_GYRLMT3_S1_13M		0x3F0F5C29 /* 0.56F */
#define	LC898122_GYRLMT3_S2_13M		0x3F0F5C29 /* 0.56F */

#define	LC898122_GYRLMT4_S1_13M		0x40333333 /* 2.8F */
#define	LC898122_GYRLMT4_S2_13M		0x40333333 /* 2.8F */

#define	LC898122_GYRA12_HGH_13M		0x401CCCCD /* 2.45F */
#define	LC898122_GYRA12_MID_13M		0x3FB33333 /* 1.4F */
#define	LC898122_GYRA34_HGH_13M		0x3F000000 /* 0.5F */
#define	LC898122_GYRA34_MID_13M		0x3DCCCCCD /* 0.1F */

#define	LC898122_GYRB12_HGH_13M		0x3E4CCCCD /* 0.20F */
#define	LC898122_GYRB12_MID_13M		0x3CA3D70A /* 0.02F */
#define	LC898122_GYRB34_HGH_13M		0x3CA3D70A /* 0.02F */
#define	LC898122_GYRB34_MID_13M		0x3C23D70A /* 0.01F */

/* Command Status */
#define	LC898122_EXE_END   0x02  /* Execute End (Adjust OK) */
#define	LC898122_EXE_HXADJ 0x06  /* Adjust NG : X Hall NG (Gain or Offset) */
#define	LC898122_EXE_HYADJ 0x0A  /* Adjust NG : Y Hall NG (Gain or Offset) */
#define	LC898122_EXE_LXADJ 0x12  /* Adjust NG : X Loop NG (Gain) */
#define	LC898122_EXE_LYADJ 0x22  /* Adjust NG : Y Loop NG (Gain) */
#define	LC898122_EXE_GXADJ 0x42  /* Adjust NG : X Gyro NG (offset) */
#define	LC898122_EXE_GYADJ 0x82  /* Adjust NG : Y Gyro NG (offset) */
#define	LC898122_EXE_OCADJ 0x402 /* Adjust NG : OSC Clock NG */
#define	LC898122_EXE_AFOFF 0x802 /* Adjust NG : AF Offset */
#define	LC898122_EXE_ERR   0x99  /* Execute Error End */

#define LC898122_EXE_HXMVER 0x06 /* X Err */
#define LC898122_EXE_HYMVER 0x0A /* Y Err */

/* Gyro Examination of Acceptance */
#define LC898122_EXE_GXABOVE 0x06 /* X Above */
#define LC898122_EXE_GXBELOW 0x0A /* X Below */
#define LC898122_EXE_GYABOVE 0x12 /* Y Above */
#define LC898122_EXE_GYBELOW 0x22 /* Y Below */


#define	LC898122_SUCCESS 0x00
#define	LC898122_FAILURE 0x01

#ifndef ON
#define	ON 0x01
#define	OFF 0x00
#endif
#define	SPC 0x02

#define	LC898122_X_DIR	0x00
#define	LC898122_Y_DIR	0x01
#define	LC898122_X2_DIR 0x10
#define	LC898122_Y2_DIR	0x11

/* Standby mode */
#define	LC898122_STB1_ON	0x00
#define	LC898122_STB1_OFF	0x01
#define	LC898122_STB2_ON	0x02
#define	LC898122_STB2_OFF	0x03
#define	LC898122_STB3_ON	0x04
#define	LC898122_STB3_OFF	0x05
#define	LC898122_STB4_ON	0x06
#define	LC898122_STB4_OFF	0x07
#define	LC898122_STB2_OISON	0x08
#define	LC898122_STB2_OISOFF	0x09
#define	LC898122_STB2_AFON	0x0A
#define	LC898122_STB2_AFOFF	0x0B

/* Optical Center & Gyro Gain for Mode */
#define	LC898122_VAL_SET 0x00
#define	LC898122_VAL_FIX 0x01
#define	LC898122_VAL_SPC 0x02

struct STFILREG {
	u16	UsRegAdd;
	u8	UcRegDat;
};

struct STFILRAM {
	u16	UsRamAdd;
	u32	UlRamDat;
};

#define	LC898122_MEASSTR 0x01
#define	LC898122_MEASCNT 0x08
#define	LC898122_MEASFIX 0x80

#define LC898122_PWMMOD_CVL 0x00
#define LC898122_PWMMOD_PWM 0x01

#define	LC898122_INIT_PWMMODE LC898122_PWMMOD_CVL

#define	LC898122_CVER122 0x93

#define	LC898122_MODULE_13M 0x01
#define	LC898122_MODULE_20M 0x02

#define	LC898122_UNI_DIR 0x01
#define	LC898122_BI_DIR  0x02

#define LC898122_CLR_FRAM0   0x01
#define LC898122_CLR_FRAM1   0x02
#define LC898122_CLR_ALL_RAM 0x03

#define LC898122_DIFIL_S2_20M 0x3F7FFD00
#define LC898122_DIFIL_S2_13M 0x3F7FFE00

#define LC898122_SINEWAVE 0
#define	LC898122_XHALWAVE 1
#define	LC898122_YHALWAVE 2
#define LC898122_XACTTEST 10
#define LC898122_YACTTEST 10
#define	LC898122_CIRCWAVE 255

#define LC898122_Mlnp 0
#define LC898122_Mpwm 1

#define LC898122_S2MODE  0x40
#define LC898122_ACTMODE 0x80
#define LC898122_MOVMODE 0xFF

#define LC898122_HALL_H_VAL 0x3F800000
#define LC898122_PTP_BEFORE 0
#define LC898122_PTP_AFTER  1

void lc898122_initsettingsaf(struct lc898122_device *lc898122_dev);
void lc898122_initsettings(struct lc898122_device *lc898122_dev);
void lc898122_selectmodule(struct lc898122_device *lc898122_dev,
			   u8 uc_module);
void lc898122_settregaf(struct lc898122_device *lc898122_dev,
			u16 UsTregAf);
void lc898122_autogaincontrol(struct lc898122_device *lc898122_dev,
			      u8 UcModeSw);
void lc898122_RamAccFixMod(struct lc898122_device *lc898122_dev,
			   u8 UcAccMod);
void lc898122_cleargyro(struct lc898122_device *lc898122_dev,
			u16 UsClrFil, u8 UcClrMod);
void lc898122_SetDOFSTDAF(struct lc898122_device *lc898122_dev,
			  u8 ucSetDat);

u8 lc898122_RtnCen(struct lc898122_device *lc898122_dev,
			      u8 UcCmdPar);
void lc898122_RemOff(struct lc898122_device *lc898122_dev,
		      u8 UcMod);
void lc898122_BsyWit(struct lc898122_device *lc898122_dev,
		      u16 UsTrgAdr, u8 UcTrgDat);
void lc898122_OisEna(struct lc898122_device *lc898122_dev);

void lc898122_SetGcf(struct lc898122_device *lc898122_dev,
		     u8 UcSetNum);
void lc898122_SetH1cMod(struct lc898122_device *lc898122_dev,
			u8 UcSetNum);
void lc898122_SetZsp(struct lc898122_device *lc898122_dev,
		     u8 UcZoomStepDat);
void lc898122_SetPanTiltMode(struct lc898122_device *lc898122_dev,
			     u8 UcPnTmod);
void lc898122_IniPtMovMod(struct lc898122_device *lc898122_dev, u8 UcPtMod);
void lc898122_driversw(struct lc898122_device *lc898122_dev, u8 UcDrvSw);
void lc898122_afdriversw(struct lc898122_device *lc898122_dev, u8 UcDrvSw);
void lc898122_selectgyrosleep(struct lc898122_device *lc898122_dev,
		u8 UcSelMode);
void lc898122_GyOutSignalCont(struct lc898122_device *lc898122_dev);
void lc898122_SrvCon(struct lc898122_device *lc898122_dev, u8 UcDirSel,
		u8 UcSwcCon);

#endif
