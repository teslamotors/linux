/*
 * SCRIPT GENERATED FILE - DO NOT EDIT
 *
 * es-a300-reg.h
 *
 * This file contains the addresses for the codec hardware fields.  Addresses
 * are automatically extracted by script from the codec register spreadsheets.
 *
 * This file generated on 5/22/2015 at 10:58:03 AM
 */

#ifndef __ES_A300_REG_H__
#define __ES_A300_REG_H__


#define ES_MIC0_GAIN_MAX                     0x14
#define ES_MIC1_GAIN_MAX                     0x14
#define ES_MIC2_GAIN_MAX                     0x14
#define ES_MICHS_GAIN_MAX                    0x14
#define ES_AUXL_GAIN_MAX                     0x14
#define ES_AUXR_GAIN_MAX                     0x14
#define ES_EP_GAIN_MAX                       0x0F
#define ES_HPL_GAIN_MAX                      0x0F
#define ES_HPR_GAIN_MAX                      0x0F
#define ES_SPKR_L_GAIN_MAX                   0x1F
#define ES_SPKR_R_GAIN_MAX                   0x1F
#define ES_LO_L_GAIN_MAX                     0x0F
#define ES_LO_R_GAIN_MAX                     0x0F

#define ES_CHIP_EN                           0x0000
#define ES_CHIP_EN_MASK                      0x01
#define ES_CHIP_EN_SHIFT                     0
#define ES_CHIP_EN_WIDTH                     1

#define ES_STANDBY                           0x0001
#define ES_STANDBY_MASK                      0x01
#define ES_STANDBY_SHIFT                     0
#define ES_STANDBY_WIDTH                     1

#define ES_MBIAS0_MODE                       0x0010
#define ES_MBIAS0_MODE_MASK                  0x03
#define ES_MBIAS0_MODE_SHIFT                 0
#define ES_MBIAS0_MODE_WIDTH                 2

#define ES_MBIAS1_MODE                       0x0011
#define ES_MBIAS1_MODE_MASK                  0x03
#define ES_MBIAS1_MODE_SHIFT                 0
#define ES_MBIAS1_MODE_WIDTH                 2

#define ES_MBIAS2_MODE                       0x0012
#define ES_MBIAS2_MODE_MASK                  0x03
#define ES_MBIAS2_MODE_SHIFT                 0
#define ES_MBIAS2_MODE_WIDTH                 2

#define ES_MIC0_ON                           0x0014
#define ES_MIC0_ON_MASK                      0x01
#define ES_MIC0_ON_SHIFT                     0
#define ES_MIC0_ON_WIDTH                     1

#define ES_MIC0_GAIN                         0x0015
#define ES_MIC0_GAIN_MASK                    0x1F
#define ES_MIC0_GAIN_SHIFT                   0
#define ES_MIC0_GAIN_WIDTH                   5

#define ES_MIC0_ZIN_MODE                     0x0017
#define ES_MIC0_ZIN_MODE_MASK                0x03
#define ES_MIC0_ZIN_MODE_SHIFT               0
#define ES_MIC0_ZIN_MODE_WIDTH               2

#define ES_MIC1_ZIN_MODE                     0x0018
#define ES_MIC1_ZIN_MODE_MASK                0x03
#define ES_MIC1_ZIN_MODE_SHIFT               0
#define ES_MIC1_ZIN_MODE_WIDTH               2

#define ES_MIC2_ZIN_MODE                     0x0019
#define ES_MIC2_ZIN_MODE_MASK                0x03
#define ES_MIC2_ZIN_MODE_SHIFT               0
#define ES_MIC2_ZIN_MODE_WIDTH               2

#define ES_MICHS_ZIN_MODE                    0x001A
#define ES_MICHS_ZIN_MODE_MASK               0x03
#define ES_MICHS_ZIN_MODE_SHIFT              0
#define ES_MICHS_ZIN_MODE_WIDTH              2

#define ES_MIC1_ON                           0x001B
#define ES_MIC1_ON_MASK                      0x01
#define ES_MIC1_ON_SHIFT                     0
#define ES_MIC1_ON_WIDTH                     1

#define ES_MIC1_GAIN                         0x001C
#define ES_MIC1_GAIN_MASK                    0x1F
#define ES_MIC1_GAIN_SHIFT                   0
#define ES_MIC1_GAIN_WIDTH                   5

#define ES_AUX_ZIN_MODE                      0x001E
#define ES_AUX_ZIN_MODE_MASK                 0x03
#define ES_AUX_ZIN_MODE_SHIFT                0
#define ES_AUX_ZIN_MODE_WIDTH                2

#define ES_REC_MUTE                          0x001F
#define ES_REC_MUTE_MASK                     0x01
#define ES_REC_MUTE_SHIFT                    0
#define ES_REC_MUTE_WIDTH                    1

#define ES_AUXL_ON                           0x0024
#define ES_AUXL_ON_MASK                      0x01
#define ES_AUXL_ON_SHIFT                     0
#define ES_AUXL_ON_WIDTH                     1

#define ES_AUXL_GAIN                         0x0025
#define ES_AUXL_GAIN_MASK                    0x1F
#define ES_AUXL_GAIN_SHIFT                   0
#define ES_AUXL_GAIN_WIDTH                   5

#define ES_MIC2_ON                           0x0028
#define ES_MIC2_ON_MASK                      0x01
#define ES_MIC2_ON_SHIFT                     0
#define ES_MIC2_ON_WIDTH                     1

#define ES_MIC2_GAIN                         0x0029
#define ES_MIC2_GAIN_MASK                    0x1F
#define ES_MIC2_GAIN_SHIFT                   0
#define ES_MIC2_GAIN_WIDTH                   5

#define ES_AUXR_ON                           0x002C
#define ES_AUXR_ON_MASK                      0x01
#define ES_AUXR_ON_SHIFT                     0
#define ES_AUXR_ON_WIDTH                     1

#define ES_AUXR_GAIN                         0x002D
#define ES_AUXR_GAIN_MASK                    0x1F
#define ES_AUXR_GAIN_SHIFT                   0
#define ES_AUXR_GAIN_WIDTH                   5

#define ES_ADC0_ON                           0x0030
#define ES_ADC0_ON_MASK                      0x01
#define ES_ADC0_ON_SHIFT                     0
#define ES_ADC0_ON_WIDTH                     1

#define ES_ADC1_ON                           0x0031
#define ES_ADC1_ON_MASK                      0x01
#define ES_ADC1_ON_SHIFT                     0
#define ES_ADC1_ON_WIDTH                     1

#define ES_ADC2_ON                           0x0032
#define ES_ADC2_ON_MASK                      0x01
#define ES_ADC2_ON_SHIFT                     0
#define ES_ADC2_ON_WIDTH                     1

#define ES_ADC3_ON                           0x0033
#define ES_ADC3_ON_MASK                      0x01
#define ES_ADC3_ON_SHIFT                     0
#define ES_ADC3_ON_WIDTH                     1

#define ES_ADC1_IN_SEL                       0x0034
#define ES_ADC1_IN_SEL_MASK                  0x01
#define ES_ADC1_IN_SEL_SHIFT                 0
#define ES_ADC1_IN_SEL_WIDTH                 1

#define ES_ADC2_IN_SEL                       0x0035
#define ES_ADC2_IN_SEL_MASK                  0x01
#define ES_ADC2_IN_SEL_SHIFT                 0
#define ES_ADC2_IN_SEL_WIDTH                 1

#define ES_ADC_MUTE                          0x0036
#define ES_ADC_MUTE_MASK                     0x01
#define ES_ADC_MUTE_SHIFT                    0
#define ES_ADC_MUTE_WIDTH                    1

#define ES_MICHS_ON                          0x0042
#define ES_MICHS_ON_MASK                     0x01
#define ES_MICHS_ON_SHIFT                    0
#define ES_MICHS_ON_WIDTH                    1

#define ES_MICHS_GAIN                        0x0043
#define ES_MICHS_GAIN_MASK                   0x1F
#define ES_MICHS_GAIN_SHIFT                  0
#define ES_MICHS_GAIN_WIDTH                  5

#define ES_MICHS_IN_SEL                      0x0045
#define ES_MICHS_IN_SEL_MASK                 0x01
#define ES_MICHS_IN_SEL_SHIFT                0
#define ES_MICHS_IN_SEL_WIDTH                1

#define ES_EP_GAIN                           0x004B
#define ES_EP_GAIN_MASK                      0x0F
#define ES_EP_GAIN_SHIFT                     0
#define ES_EP_GAIN_WIDTH                     4

#define ES_EP_MUTE                           0x004E
#define ES_EP_MUTE_MASK                      0x01
#define ES_EP_MUTE_SHIFT                     0
#define ES_EP_MUTE_WIDTH                     1

#define ES_EP_ON                             0x004F
#define ES_EP_ON_MASK                        0x01
#define ES_EP_ON_SHIFT                       0
#define ES_EP_ON_WIDTH                       1

#define ES_DAC0L_TO_EP                       0x0050
#define ES_DAC0L_TO_EP_MASK                  0x01
#define ES_DAC0L_TO_EP_SHIFT                 0
#define ES_DAC0L_TO_EP_WIDTH                 1

#define ES_HPL_GAIN                          0x0055
#define ES_HPL_GAIN_MASK                     0x0F
#define ES_HPL_GAIN_SHIFT                    0
#define ES_HPL_GAIN_WIDTH                    4

#define ES_HPL_MUTE                          0x0058
#define ES_HPL_MUTE_MASK                     0x01
#define ES_HPL_MUTE_SHIFT                    0
#define ES_HPL_MUTE_WIDTH                    1

#define ES_HPL_ON                            0x0059
#define ES_HPL_ON_MASK                       0x01
#define ES_HPL_ON_SHIFT                      0
#define ES_HPL_ON_WIDTH                      1

#define ES_DAC0L_TO_HPL                      0x005A
#define ES_DAC0L_TO_HPL_MASK                 0x01
#define ES_DAC0L_TO_HPL_SHIFT                0
#define ES_DAC0L_TO_HPL_WIDTH                1

#define ES_HPR_GAIN                          0x0060
#define ES_HPR_GAIN_MASK                     0x0F
#define ES_HPR_GAIN_SHIFT                    0
#define ES_HPR_GAIN_WIDTH                    4

#define ES_HPR_MUTE                          0x0063
#define ES_HPR_MUTE_MASK                     0x01
#define ES_HPR_MUTE_SHIFT                    0
#define ES_HPR_MUTE_WIDTH                    1

#define ES_HPR_ON                            0x0064
#define ES_HPR_ON_MASK                       0x01
#define ES_HPR_ON_SHIFT                      0
#define ES_HPR_ON_WIDTH                      1

#define ES_DAC0R_TO_HPR                      0x0065
#define ES_DAC0R_TO_HPR_MASK                 0x01
#define ES_DAC0R_TO_HPR_SHIFT                0
#define ES_DAC0R_TO_HPR_WIDTH                1

#define ES_SPKRL_GAIN                        0x006B
#define ES_SPKRL_GAIN_MASK                   0x1F
#define ES_SPKRL_GAIN_SHIFT                  0
#define ES_SPKRL_GAIN_WIDTH                  5

#define ES_SPKRL_MUTE                        0x006E
#define ES_SPKRL_MUTE_MASK                   0x01
#define ES_SPKRL_MUTE_SHIFT                  0
#define ES_SPKRL_MUTE_WIDTH                  1

#define ES_SPKRL_ON                          0x006F
#define ES_SPKRL_ON_MASK                     0x01
#define ES_SPKRL_ON_SHIFT                    0
#define ES_SPKRL_ON_WIDTH                    1

#define ES_DAC0L_TO_SPKRL                    0x0070
#define ES_DAC0L_TO_SPKRL_MASK               0x01
#define ES_DAC0L_TO_SPKRL_SHIFT              0
#define ES_DAC0L_TO_SPKRL_WIDTH              1

#define ES_DAC1L_TO_SPKRL                    0x0071
#define ES_DAC1L_TO_SPKRL_MASK               0x01
#define ES_DAC1L_TO_SPKRL_SHIFT              0
#define ES_DAC1L_TO_SPKRL_WIDTH              1

#define ES_SPKRR_GAIN                        0x0075
#define ES_SPKRR_GAIN_MASK                   0x1F
#define ES_SPKRR_GAIN_SHIFT                  0
#define ES_SPKRR_GAIN_WIDTH                  5

#define ES_SPKRR_MUTE                        0x0077
#define ES_SPKRR_MUTE_MASK                   0x01
#define ES_SPKRR_MUTE_SHIFT                  0
#define ES_SPKRR_MUTE_WIDTH                  1

#define ES_SPKRR_ON                          0x0078
#define ES_SPKRR_ON_MASK                     0x01
#define ES_SPKRR_ON_SHIFT                    0
#define ES_SPKRR_ON_WIDTH                    1

#define ES_DAC0R_TO_SPKRR                    0x0079
#define ES_DAC0R_TO_SPKRR_MASK               0x01
#define ES_DAC0R_TO_SPKRR_SHIFT              0
#define ES_DAC0R_TO_SPKRR_WIDTH              1

#define ES_DAC1R_TO_SPKRR                    0x007A
#define ES_DAC1R_TO_SPKRR_MASK               0x01
#define ES_DAC1R_TO_SPKRR_SHIFT              0
#define ES_DAC1R_TO_SPKRR_WIDTH              1

#define ES_LO_L_GAIN                         0x007E
#define ES_LO_L_GAIN_MASK                    0x0F
#define ES_LO_L_GAIN_SHIFT                   0
#define ES_LO_L_GAIN_WIDTH                   4

#define ES_LO_L_MUTE                         0x0081
#define ES_LO_L_MUTE_MASK                    0x01
#define ES_LO_L_MUTE_SHIFT                   0
#define ES_LO_L_MUTE_WIDTH                   1

#define ES_LO_L_ON                           0x0082
#define ES_LO_L_ON_MASK                      0x01
#define ES_LO_L_ON_SHIFT                     0
#define ES_LO_L_ON_WIDTH                     1

#define ES_DAC1L_TO_LO_L                     0x0083
#define ES_DAC1L_TO_LO_L_MASK                0x01
#define ES_DAC1L_TO_LO_L_SHIFT               0
#define ES_DAC1L_TO_LO_L_WIDTH               1

#define ES_LO_R_GAIN                         0x008A
#define ES_LO_R_GAIN_MASK                    0x0F
#define ES_LO_R_GAIN_SHIFT                   0
#define ES_LO_R_GAIN_WIDTH                   4

#define ES_LO_R_MUTE                         0x008D
#define ES_LO_R_MUTE_MASK                    0x01
#define ES_LO_R_MUTE_SHIFT                   0
#define ES_LO_R_MUTE_WIDTH                   1

#define ES_LO_R_ON                           0x008E
#define ES_LO_R_ON_MASK                      0x01
#define ES_LO_R_ON_SHIFT                     0
#define ES_LO_R_ON_WIDTH                     1

#define ES_DAC1R_TO_LO_R                     0x008F
#define ES_DAC1R_TO_LO_R_MASK                0x01
#define ES_DAC1R_TO_LO_R_SHIFT               0
#define ES_DAC1R_TO_LO_R_WIDTH               1

#define ES_VC_ADC_ON                         0x00ED
#define ES_VC_ADC_ON_MASK                    0x01
#define ES_VC_ADC_ON_SHIFT                   0
#define ES_VC_ADC_ON_WIDTH                   1

#define ES_MB2_TRIM                          0x0100
#define ES_MB2_TRIM_MASK                     0x07
#define ES_MB2_TRIM_SHIFT                    0
#define ES_MB2_TRIM_WIDTH                    3

#define ES_MBHS_TRIM                         0x0101
#define ES_MBHS_TRIM_MASK                    0x07
#define ES_MBHS_TRIM_SHIFT                   0
#define ES_MBHS_TRIM_WIDTH                   3

#define ES_LD02_TRIM                         0x0102
#define ES_LD02_TRIM_MASK                    0x03
#define ES_LD02_TRIM_SHIFT                   0
#define ES_LD02_TRIM_WIDTH                   2

#define ES_MB0_TRIM                          0x0103
#define ES_MB0_TRIM_MASK                     0x07
#define ES_MB0_TRIM_SHIFT                    0
#define ES_MB0_TRIM_WIDTH                    3

#define ES_MB1_TRIM                          0x0104
#define ES_MB1_TRIM_MASK                     0x07
#define ES_MB1_TRIM_SHIFT                    0
#define ES_MB1_TRIM_WIDTH                    3

#define ES_SQUELCH_THRESHOLD                 0x014C
#define ES_SQUELCH_THRESHOLD_MASK            0x07
#define ES_SQUELCH_THRESHOLD_SHIFT           0
#define ES_SQUELCH_THRESHOLD_WIDTH           3

#define ES_SQUELCH_TERM_CNT                  0x014D
#define ES_SQUELCH_TERM_CNT_MASK             0x0F
#define ES_SQUELCH_TERM_CNT_SHIFT            0
#define ES_SQUELCH_TERM_CNT_WIDTH            4



#define ES_MAX_REGISTER             0x014F


int es_analog_add_snd_soc_controls(struct snd_soc_codec *codec);
int es_analog_add_snd_soc_dapm_controls(struct snd_soc_codec *codec);
int es_analog_add_snd_soc_route_map(struct snd_soc_codec *codec);

#endif
