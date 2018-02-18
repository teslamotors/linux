/*
 * es755.h  --  ES755 Soc Audio driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ES755_H
#define _ES755_H

#include <linux/cdev.h>
#include <linux/mutex.h>
#include <sound/soc.h>
#include <linux/time.h>
#include "escore.h"
#if defined(CONFIG_SND_SOC_ES854)
#include "es-a350-reg.h"
#elif defined(CONFIG_SND_SOC_ES857)
#include "es-a375-reg.h"
#else
#include "es-a300-reg.h"
#endif

#define ES_INVALID_REG_VALUE		-1
#define ES_SYNC_CMD			0x8000
#define ES_SYNC_POLLING		0x0000
#define ES_SYNC_ACK			0x80000000

#define ES_GET_POWER_STATE		0x800f
#define ES_SET_POWER_STATE		0x9010
#define ES_SET_POWER_STATE_SLEEP	0x0001
#define ES_SET_POWER_STATE_MP_SLEEP	0x0002
#define ES_SET_POWER_STATE_MP_CMD	0x0003
#define ES_SET_POWER_STATE_NORMAL	0x0004
#define ES_SET_POWER_STATE_VS_OVERLAY	0x0005
#define ES_SET_POWER_STATE_VS_LOWPWR	0x0006
#define ES_SET_POWER_STATE_DEEP_SLEEP	0x0007

#define ES_DHWPT_CMD			0x9052
/*
 * Device parameter command codes
 */
#define ES_DEV_PARAM_OFFSET		0x2000
#define ES_GET_DEV_PARAM		0x800b
#define ES_SET_DEV_PARAM_ID		0x900c
#define ES_SET_DEV_PARAM		0x900d

/*
 * Audio sample rates and formats
 */
#define ES_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
			SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |\
			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |\
			SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000 |\
			SNDRV_PCM_RATE_44100)
#define ES_SLIMBUS_RATES (SNDRV_PCM_RATE_48000)

#define ES_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S16_BE |\
			SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S20_3BE |\
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S24_BE |\
			SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_S32_BE)
#define ES_SLIMBUS_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			SNDRV_PCM_FMTBIT_S16_BE)

#define ES_SET_MUX_CMD			0xB05A
#define ES_SET_PATH_CMD			0xB05A
#define ES_SET_PATH_ID_CMD		0xB05B
#define ES_SET_RATE_CMD			0xB063
#define ES_CONNECT_CMD			0xB064
#define ES_SET_GROUP_CMD		0xB068
#define ES_SET_ALGO_PROC_CMD		0xB01C


#define ES_API_WORD(upper, lower) ((upper << 16) | lower)

/*
 * Algoithm command codes
 */
#define ES_START_ALGORITHM		0x8061
#define ES_STOP_ALGORITHM		0x8062
#define ES_STOP_ROUTE			0x906B
#define ES_VP				0x0001
#define ES_MM				0x0002
#define ES_VP_MM			0x0003
#define ES_AF				0x0004
#define ES_PASSTHRU			0x0008
#define ES_PASSTHRU_VP		0x0009
#define ES_PASSTHRU_MM		0x000A
#define ES_PASSTHRU_VP_MM	0x000B
#define ES_PASSTHRU_AF		0x000C
#define ES_VOICESENSE		0x0010

/*
 * Algoithm parameter command codes
 */
#define ES_ALGO_PARAM_OFFSET		0x0000
#define ES_GET_ALGO_PARAM		0x8016
#define ES_SET_ALGO_PARAM_ID		0x9017
#define ES_SET_ALGO_PARAM		0x9018
#define ES_GET_EVENT			0x806D
#define ES_SET_EVENT_RESP		0x901A
#define ES_GET_FACTORY_INT		0x8059
#define ES_ACCDET_CONFIG_CMD		0x9056
#define ES_SMOOTH_RATE			0x804E
#define ES755_FW_DOWNLOAD_MAX_RETRY	5
#define ES_EVENT_STATUS_RETRY_COUNT	2

/*
 * Audio path commands codes
 */
#define ES_SELECT_ENDPOINT		0x8069
#define ES_SET_DIGITAL_GAIN		0x8015
#define ES_GET_DIGITAL_GAIN		0x801D
#define ES_DIGITAL_GAIN_MASK		0xFF
#define ES_SELECT_HPF_MODE              0x806E

/*
 * Codec Control Register Access
 * */
#define ES_GET_CODEC_VAL               0x807B
#define ES_CODEC_VAL_MASK              0xFF

#if defined(CONFIG_SND_SOC_ES854) || defined(CONFIG_SND_SOC_ES857)
#define ES_MAX_CODEC_CONTROL_REG       0x77
#else
#define ES_MAX_CODEC_CONTROL_REG       0x55
#endif

/*
 * Stereo widening presets for headphone playback and MM audio algo rate 48K
 */
#define ES_STEREO_WIDE_ON		0x028B
#define ES_STEREO_WIDE_OFF		0x02BB

#define ES_FULL_NARROWING		0xFFF6
#define ES_FULL_WIDENING		0x000A
#define ES_DEF_ST_WIDE_SETTING		0x8000

#define ES_MAX_AF_MODE			2
#define ES_MAX_AEC_MODE			7
/* Impedance level for MIC detection */
#define MIC_IMPEDANCE_LEVEL		0x6

/* Ambient Noise Query API */
#define ES_QUERY_AMBIENT_NOISE		0x80420002
#define ES_MIN_NOISE_DB			(-96)

/* Jack detection mask */
#if defined(CONFIG_SND_SOC_ES854) || defined(CONFIG_SND_SOC_ES857)
/* BTN_3 is mapped with IMPD_LEVEL 1 to support
 * Google Voice Search trigger functionality */
#define JACK_DET_MASK	(SND_JACK_HEADSET | SND_JACK_BTN_0 | SND_JACK_BTN_1 |\
		SND_JACK_BTN_2 | SND_JACK_BTN_3)
#else
#define JACK_DET_MASK	(SND_JACK_HEADSET | SND_JACK_BTN_0 | SND_JACK_BTN_1 |\
		SND_JACK_BTN_2)
#endif

#define MAX_RETRY_TO_SWITCH_TO_LOW_POWER_MODE	5

#define ES755_DMIC0_VS_ROUTE_PRESET		0x056A
#define ES755_MIC0_VS_ROUTE_PRESET		0x0568
#define ES755_MICHS_VS_ROUTE_PRESET		0x056B
#define ES755_DMIC0_CVS_PRESET			0x1B59
#define ES755_MIC0_CVS_PRESET			0x1B8A
#define ES755_MICHS_CVS_PRESET			0x1B8B

#if defined(CONFIG_SND_SOC_ES854) || defined(CONFIG_SND_SOC_ES857)
#define ES755_WDB_MAX_SIZE	4096
#else
#define ES755_WDB_MAX_SIZE	512
#endif

/* PCM Port IDs */
#define ES755_PCM_PORT_A		0xA
#define ES755_PCM_PORT_B		0xB
#define ES755_PCM_PORT_C		0xC

/* Unidirectional Digital Hardware Passthrough Mapping for PCM ports */
#define PORT_A_TO_D	0x01CC
#define PORT_B_TO_D	0x01DD
#define PORT_C_TO_D	0x01EE

/* Minimum delay for redetecting the HS IN ms*/
#define MIN_HS_REDETECTION_DELAY 50
#define DEFAULT_HS_REDETECTION_DELAY 250

/* Delay values required for different stages */
#define ES755_DELAY_WAKEUP_TO_NORMAL	20 /* ms */
#define ES755_DELAY_WAKEUP_TO_VS	20 /* ms */
#define ES755_DELAY_VS_TO_NORMAL	90 /* ms */
#define ES755_DELAY_MPSLEEP_TO_NORMAL	30 /* ms */
#define ES755_DELAY_VS_STREAMING_TO_VS	10 /* ms */

/* Value to disable Master clock */
#define ES755_DISABLE_MASTER_CLK	8

/* Delay value to put chip into MP Sleep mode */
#define ES755_MP_SLEEP_DELAY	2000 /* 2 sec */

enum {
	ES755_MIC0,
	ES755_DMIC0,
	ES755_MICHS,
};

union es755_accdet_status_reg {
	u16 value;
	struct {
		u16 plug_det_fsm1:1;
		u16 plug_det_fsm2:1;
		u16 hp_l_det:1;
		u16 plugdet:1;
		u16 unsupp_mic:1;
		u16 michs_bias:1;
		u16 hp_out:1;
		u16 thermal_shutdown:1;
		u16 impd_level:3;
		u16 res:2;
		u16 mg_select:1;
		u16 i2c_ctl_ready:1;
		u16 glb_enable_status:1;
	} fields;
};

/*
 * addresses
 * Ensure digital addresses do not overlap with analog
 */
enum {
	ES_CHANGE_STATUS = ES_MAX_REGISTER,
	ES_FW_FIRST_CHAR,
	ES_FW_NEXT_CHAR,
	ES_GET_SYS_INTERRUPT,
	ES_ACCDET_CONFIG,
	ES_GET_ACCDET_STATUS,
	ES_BUTTON_DETECTION_ENABLE,
	ES_BUTTON_SERIAL_CONFIG,
	ES_BUTTON_PARALLEL_CONFIG,
	ES_BUTTON_DETECTION_RATE,
	ES_BUTTON_PRESS_SETTLING_TIME,
	ES_BUTTON_BOUNCE_TIME,
	ES_BUTTON_DETECTION_LONG_PRESS_TIME,
	ES_ALGO_SAMPLE_RATE,
	ES_ALGORITHM_VP,
	ES_ALGORITHM_MM,
	ES_ALGORITHM_PT,
	ES_ALGORITHM_PT_VP,
	ES_ALGORITHM_AF,
	ES_ALGORITHM_BRG,
	ES_ALGORITHM_KALAOK,
	ES_ALGORITHM_DHWPT,

	/* PCM Port Parameters*/
	ES_PCM_PORT,
	ES_PORT_WORD_LEN,
	ES_PORT_TDM_SLOTS_PER_FRAME,
	ES_PORT_TX_DELAY_FROM_FS,
	ES_PORT_RX_DELAY_FROM_FS,
	ES_PORT_LATCH_EDGE,
	ES_PORT_ENDIAN,
	ES_PORT_TRISTATE,
	ES_PORT_AUDIO_MODE,
	ES_PORT_TDM_ENABLED,
	ES_PORT_CLOCK_CONTROL,
	ES_PORT_DATA_JUSTIFICATION,
	ES_PORT_FS_DURATION,

	ES_BUTTON_CTRL1,

	ES_PRIMARY_MUX,
	ES_SECONDARY_MUX,
	ES_TERTIARY_MUX,
	ES_AECREF1_MUX,
	ES_FEIN_MUX,
	ES_UITONE1_MUX,
	ES_UITONE2_MUX,
	ES_UITONE3_MUX,
	ES_UITONE4_MUX,

	ES_AUDIN1_MUX,
	ES_AUDIN2_MUX,
	ES_MMUITONE1_MUX,
	ES_MMUITONE2_MUX,

	ES_AF_PRI_MUX,
	ES_AF_SEC_MUX,
	ES_AF_TER_MUX,
	ES_AF_AI1_MUX,

	ES_MM_PASSIN1_MUX,
	ES_MM_PASSIN2_MUX,
	ES_PASSIN1_MUX,
	ES_PASSIN2_MUX,
	ES_PASSIN3_MUX,
	ES_PASSIN4_MUX,

	ES_DAC0_0_MUX,
	ES_DAC0_1_MUX,
	ES_DAC1_0_MUX,
	ES_DAC1_1_MUX,

	ES_PCM0_0_MUX,
	ES_PCM0_1_MUX,
	ES_PCM0_2_MUX,
	ES_PCM0_3_MUX,
	ES_PCM1_0_MUX,
	ES_PCM1_1_MUX,
	ES_PCM1_2_MUX,
	ES_PCM1_3_MUX,
	ES_PCM2_0_MUX,
	ES_PCM2_1_MUX,
	ES_PCM2_2_MUX,
	ES_PCM2_3_MUX,

	ES_SBUSTX0_MUX,
	ES_SBUSTX1_MUX,
	ES_SBUSTX2_MUX,
	ES_SBUSTX3_MUX,
	ES_SBUSTX4_MUX,
	ES_SBUSTX5_MUX,

	ES_CODEC_VALUE,
	ES_CODEC_ADDR,
	ES_POWER_STATE,
	ES_RUNTIME_PM,
	ES_VS_INT_OSC_MEASURE_START,
	ES_VS_INT_OSC_MEASURE_STATUS,
	ES_EVENT_RESPONSE,
	ES_VOICE_SENSE_SET_KEYWORD,
	ES_VOICE_SENSE_EVENT,
	ES_VOICE_SENSE_TRAINING_MODE,
	ES_VOICE_SENSE_DETECTION_SENSITIVITY,
	ES_VOICE_ACTIVITY_DETECTION_SENSITIVITY,
	ES_VS_STORED_KEYWORD,
	ES_VOICE_SENSE_TRAINING_RECORD,
	ES_VOICE_SENSE_TRAINING_STATUS,
	ES_VOICE_SENSE_TRAINING_MODEL_LENGTH,
	ES_CVS_PRESET,
	ES_PRESET,
	ES_SET_NS,
	ES_STEREO_WIDTH,
	ES_MIC_CONFIG,
	ES_AEC_MODE,
	ES_AF_MODE,
	ES_VP_METERS,
	ES_CMD_COMPL_MODE,
	ES_FE_STREAMING,
	ES_FEIN2_MUX,
	ES_CODEC_OUTPUT_RATE,
	ES_API_ADDR_MAX,
};

#define ES_SLIM_1_PB_MAX_CHANS	2
#define ES_SLIM_1_CAP_MAX_CHANS	2
#define ES_SLIM_2_PB_MAX_CHANS	2
#define ES_SLIM_2_CAP_MAX_CHANS	2
#define ES_SLIM_3_PB_MAX_CHANS	2
#define ES_SLIM_3_CAP_MAX_CHANS	2

#define ES_SLIM_1_PB_OFFSET	0
#define ES_SLIM_2_PB_OFFSET	2
#define ES_SLIM_3_PB_OFFSET	4
#define ES_SLIM_1_CAP_OFFSET	0
#define ES_SLIM_2_CAP_OFFSET	2
#define ES_SLIM_3_CAP_OFFSET	4


#define ES_SLIM_CH_RX_OFFSET		152
#define ES_SLIM_CH_TX_OFFSET		156
/* #define ES_SLIM_RX_PORTS		10 */
#define ES_SLIM_RX_PORTS		6
#define ES_SLIM_TX_PORTS		6

#define ES_I2S_PORTA		7
#define ES_I2S_PORTB		8
#define ES_I2S_PORTC		9

#define ES_SLIM_RX_PORTS		6
#define ES_SLIM_TX_PORTS		6

#if defined(CONFIG_SND_SOC_ES_SLIM)
#define ES_NUM_CODEC_SLIM_DAIS       6
#define ES_NUM_CODEC_I2S_DAIS	0
#else
#define ES_NUM_CODEC_SLIM_DAIS       0
#define ES_NUM_CODEC_I2S_DAIS	3
#endif

#define ES_NUM_CODEC_DAIS	(ES_NUM_CODEC_SLIM_DAIS + \
		ES_NUM_CODEC_I2S_DAIS)

/*
 * Codec Events
 */
#define ES_EP_SHORT_CIRCUIT_EVENT(value)		((1<<13) & value)
#define ES_HP_SHORT_CIRCUIT_EVENT(value)		((1<<12) & value)
#define ES_SPKR_RIGHT_SHORT_CIRCUIT_EVENT(value)	((1<<11) & value)
#define ES_SPKR_LEFT_SHORT_CIRCUIT_EVENT(value)		((1<<10) & value)
#define ES_LINEOUT_SHORT_CIRCUIT_EVENT(value)		((1<<9) & value)
#define ES_THERMAL_SHUTDOWN_EVENT(value)		((1<<8) & value)

#define ES_PLUG_EVENT(value)		((1<<6) & value)
#define ES_UNPLUG_EVENT(value)		((1<<5) & value)
#define ES_MICDET_EVENT(value)		((1<<4) & value)
#define ES_BUTTON_RELEASE_EVENT(value)	((1<<3) & value)
#define ES_BUTTON_RELEASE_EVENT_MASK		(1<<3)

#define ES_BUTTON_PRESS_EVENT(value)	((1<<2) & value)
#define ES_BUTTON_PRESS_EVENT_MASK		(1<<2)

#define ES_OPEN_MICDET_EVENT(value)	((1<<0) & value)
#define ES_FACTORY_INTR_EVENT(value)	((1<<1) & value)
enum {
	ES_SLIM_1_PB = ES_DAI_ID_BASE,
	ES_SLIM_1_CAP,
	ES_SLIM_2_PB,
	ES_SLIM_2_CAP,
	ES_SLIM_3_PB,
	ES_SLIM_3_CAP,
};

/* Base name used by character devices. */
#define ES_CDEV_NAME "adnc"

extern struct snd_soc_codec_driver soc_codec_dev_es755;
extern struct snd_soc_dai_driver es755_dai[];

extern int es755_core_probe(struct device *dev);

extern int es755_bootup(struct escore_priv *es755);

#define es755_resp(obj) ((obj)->last_response)
int es755_cmd(struct escore_priv *es755, u32 cmd);
int es755_bus_init(struct escore_priv *es755);
int es755_boot_finish(struct escore_priv *es755);
int es755_set_streaming(struct escore_priv *escore, int value);
int es755_set_datalogging(struct escore_priv *escore, int value);
int es755_i2c_sleep(struct escore_priv *escore);
int es755_i2c_wakeup(struct escore_priv *escore);
int es755_slim_sleep(struct escore_priv *escore);
int es755_slim_wakeup(struct escore_priv *escore);
void es755_slim_setup(struct escore_priv *escore_priv);
int es755_detect(struct snd_soc_codec *codec, struct snd_soc_jack *jack);
int es755_power_transition(int next_power_state,
				unsigned int set_power_state_cmd);
int es755_enter_mp_sleep(void);
int es755_exit_mp_sleep(void);

#endif /* _ES_H */
