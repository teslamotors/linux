/*
 * Copyright (c) 2010 NVIDIA Corporation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of the NVIDIA Corporation nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/delay.h>

#include <mach/iomap.h>

#include "nvcommon.h"
#include "nvrm_power.h"
#include "../../../../clock.h"

#define is_vcp(_mod) (NVRM_MODULE_ID_MODULE(_mod)==NvRmModuleID_Vcp)

#define is_bsea(_mod) (NVRM_MODULE_ID_MODULE(_mod)==NvRmModuleID_BseA)

#define is_vde(_mod) (NVRM_MODULE_ID_MODULE(_mod)==NvRmModuleID_Vde)

#define is_hdmi(_mod) (NVRM_MODULE_ID_MODULE(_mod)==NvRmModuleID_Hdmi)

#define CLK_VI_CORE_EXTERNAL (1<<24)
#define CLK_VI_PAD_INTERNAL (1<<25)

// Fixed HDMI frequencies
#define NVRM_HDMI_480_FIXED_FREQ_KHZ (27000)
#define NVRM_HDMI_720P_1080I_FIXED_FREQ_KHZ (74250)
#define NVRM_HDMI_720P_1080P_FIXED_FREQ_KHZ (148500)
#define NVRM_PLLD_FREQ_KHZ (594000)

NvError NvRmPowerModuleClockConfig(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmModuleID ModuleId,
    NvU32 ClientId,
    NvRmFreqKHz MinFreq,
    NvRmFreqKHz MaxFreq,
    const NvRmFreqKHz *PrefFreqList,
    NvU32 PrefFreqListCount,
    NvRmFreqKHz *CurrentFreq,
    NvU32 flags)
{
    NvRmFreqKHz freq;
    struct clk *module_clk;
    struct clk *pll_d_out0;
    char module_clk_name[20];
    unsigned long rate;

    if (!PrefFreqList || !PrefFreqListCount)
        return NvError_BadParameter;

    switch (ModuleId) {
    case NVRM_MODULE_ID(NvRmModuleID_Hdmi, 0):
        rate = PrefFreqList[0];

        /* Check for supported HDMI frequencies */
        if (rate != NVRM_HDMI_480_FIXED_FREQ_KHZ &&
            rate != NVRM_HDMI_720P_1080I_FIXED_FREQ_KHZ &&
            rate != NVRM_HDMI_720P_1080P_FIXED_FREQ_KHZ) {
            return NvError_ResourceError;
        }

        pll_d_out0 = clk_get_sys(NULL, "pll_d_out0");
        if (IS_ERR(pll_d_out0)){
            pr_err("%s: unable to get struct clk for %s\n", __func__, "pll_d_out0");
            return NvError_ResourceError;
        }

        snprintf(module_clk_name, sizeof(module_clk_name), "hdmi");
        module_clk = clk_get_sys(module_clk_name, NULL);
        if (IS_ERR(module_clk)) {
            pr_err("%s: unable to get struct clk for %s\n", __func__, "hdmi");
            return NvError_ResourceError;
        }

        if (clk_get_parent(module_clk) != pll_d_out0) {
            if (clk_set_parent(module_clk, pll_d_out0)) {
                pr_err("%s: unable to set parent clk for %s\n", __func__, "hdmi");
                return NvError_ResourceError;
            }
        }

        if (clk_set_rate(module_clk, rate * 1000)) {
            pr_err("%s: unable to set clk rate (%lu) for %s\n", __func__, rate, "hdmi");
            return NvError_ResourceError;
        }

        freq = clk_get_rate(module_clk) / 1000;
        break;
    case NVRM_MODULE_ID(NvRmModuleID_Display, 0):
    case NVRM_MODULE_ID(NvRmModuleID_Display, 1):

        snprintf(module_clk_name, sizeof(module_clk_name), "tegradc.%lu",
            NVRM_MODULE_ID_INSTANCE(ModuleId));
        module_clk = clk_get_sys(module_clk_name, NULL);
        if (IS_ERR(module_clk)) {
            pr_err("%s: unable to get struct clk for %s\n", __func__, module_clk_name);
            return NvError_ResourceError;
        }

        /* CRT case */
        if (flags & NvRmClockConfig_SubConfig) {
            struct clk *pll_c;

            /*
             * To support strict clock tolerances of certain displays,
             * several preferred PLLC configurations have been computed
             * (VESA modes). These configs (1 per VESA mode) ensure that
             * the output rate matches the desired pclk (this is how a
             * config is matched to a pclk). Each config can be classified
             * as either exact or approximate. For an exact config, the
             * listed output rate will match the actual output rate. For
             * an approximate config, the actual output rate will be +/-
             * 0.5% of the listed output rate (CRTs can tolerate this).
             * Approximate configs are needed because of input rate and
             * M/N/P value limitations.
             *
             * DC Configuration
             * ===========================================================
             * The clock tree is as follows:
             * pll_c ---> dc ---> [/X] ---> pclk
             *
             * The dc contains a divider (called shift-clk-divider) to
             * scale the module clock to the desired pclk. The management
             * of the shift-clk-divider is handled by the userspace nvddk
             * disp layer.
             *
             * For VESA timings, pll_c is either 1x, 2x, or 4x the target
             * pclk. Scaling to the target pclk is done using the dc's shift-
             * clk-divider.
             *
             * For all other timings, pll_c is fixed to 297 MHz. The dc's
             * shift-clk-divider is used to achieve the target pclk.
             */
            rate = PrefFreqList[0] * 1000;
            pll_c = clk_get_sys(NULL, "pll_c");
            if (IS_ERR(pll_c)) {
                pr_err("%s: unable to get struct clk for %s\n", __func__, "pll_c");
                return NvError_ResourceError;
            }

            if (rate != clk_get_rate(pll_c)) {
                if (clk_set_rate(pll_c, rate)) {
                    rate = NVRM_PLLD_FREQ_KHZ / 2 * 1000;
                    if (clk_set_rate(pll_c, rate)) {
                        pr_err("%s: unable to set clk rate (%lu) for %s\n", __func__, rate, "pll_c");
                        return NvError_ResourceError;
                    }
                }
            }

            if (clk_get_parent(module_clk) != pll_c) {
                if (clk_set_parent(module_clk, pll_c)) {
                    pr_err("%s: unable to set parent clk for %s\n", __func__, module_clk_name);
                    return NvError_ResourceError;
                }
            }

            freq = clk_get_rate(module_clk) / 1000;

            /*
             * TVDAC Configuration (needed for CRT operation)
             * ===========================================================
             * The clock tree is as follows:
             * pll_c ---> [/X] ---> tvdac ---> pclk
             *
             * Unlike the dc, tvdac doesn't contain an internal divider.
             * The clock-and-reset block is configured (in the kernel)
             * to scale down (if necessary) the clock going to tvdac.
             *
             * For VESA timings, pll_c is either 1x, 2x, or 4x the target
             * pclk. Scaling to the target pclk is done by dividing tvdac's
             * input clock.
             *
             * For all other timings, pll_c is fixed to 297 MHz. The clock
             * to tvdac is divided to achieve the target pclk.
             */
            snprintf(module_clk_name, sizeof(module_clk_name), "tvdac");
            module_clk = clk_get_sys(module_clk_name, NULL);
            if (IS_ERR(module_clk)) {
                pr_err("%s: unable to get struct clk for %s\n", __func__, "tvdac");
                return NvError_ResourceError;
            }

            if (clk_get_parent(module_clk) != pll_c) {
                if (clk_set_parent(module_clk, pll_c)) {
                    pr_err("%s: unable to set parent clk for %s\n", __func__, "tvdac");
                    return NvError_ResourceError;
                }
            }

            rate = PrefFreqList[0] * 1000;
            if (clk_set_rate(module_clk, rate)) {
                pr_err("%s: unable to set clk rate (%lu) for %s\n", __func__, rate, "tvdac");
                return NvError_ResourceError;
            }
        } else {
            struct clk *pll_d;

            /*
             * Use PLLD (low-jitter) when the output is LCD or HDMI. In this
             * configuration, pll_d is fixed to 594 MHz. This allows the
             * standard HDMI timings to be supported and (hopefully) allows
             * enough flexibility to support the various custom LCD timings.
             *
             * DC Configuration
             * ==============================================================
             * The clock tree is as follows:
             * pll_d ---> [/2] ---> pll_d_out0 ---> dc ---> [/X] ---> pclk
             *
             * The dc contains a divider (called shift-clk-divider) to scale
             * the input clock to the desired pclk. The management of the
             * shift-clk-divider is handled by the userspace nvddk disp layer.
             *
             * The dc's shift-clk-divider is used to achieve the target pclk.
             */
            pll_d = clk_get_sys(NULL, "pll_d");
            pll_d_out0 = clk_get_sys(NULL, "pll_d_out0");

            if (IS_ERR(pll_d_out0) || IS_ERR(pll_d)) {
                pr_err("%s: unable to get struct clk for %s\n", __func__, "pll_d*");
                return NvError_ResourceError;
            }

            rate = NVRM_PLLD_FREQ_KHZ * 1000;
            if (rate != clk_get_rate(pll_d)) {
                if (clk_set_rate(pll_d, rate)) {
                    pr_err("%s: unable to set clk rate (%lu) for %s\n", __func__, rate, "pll_d");
                    return NvError_ResourceError;
                }
            }

            if (clk_get_parent(module_clk) != pll_d_out0) {
                if (clk_set_parent(module_clk, pll_d_out0)) {
                    pr_err("%s: unable to set parent clk for %s\n", __func__, "pll_d_out0");
                    return NvError_ResourceError;
                }
            }
            freq = clk_get_rate(module_clk) / 1000;
        }
        break;
    default:
        return NvError_BadParameter;
    }

    if (CurrentFreq)
        *CurrentFreq = freq;

    return NvSuccess;
}

NvError NvRmPowerModuleClockControl(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmModuleID ModuleId,
    NvU32 ClientId,
    NvBool Enable)
{
    const char *vcp_names[] = { "vcp", NULL };
    const char *bsea_names[] = { "bsea", NULL };
    const char *vde_names[] = { "vde", NULL };
    const char *hdmi_names[] = { "hdmi", NULL };
    const char **names = NULL;

    if (is_vcp(ModuleId))
        names = vcp_names;
    else if (is_bsea(ModuleId))
        names = bsea_names;
    else if (is_vde(ModuleId))
        names = vde_names;
    else if (is_hdmi(ModuleId))
        names = hdmi_names;

    if (!names) {
        pr_err("%s: MOD[%lu] INST[%lu] not supported\n", __func__,
               NVRM_MODULE_ID_MODULE(ModuleId),
               NVRM_MODULE_ID_INSTANCE(ModuleId));
        return NvError_BadParameter;
    }

    for ( ; *names ; names++) {
        struct clk *clk = clk_get_sys(*names, NULL);

        if (IS_ERR_OR_NULL(clk)) {
            pr_err("%s: unable to get struct clk for %s\n", __func__, *names);
            continue;
        }

        if (Enable)
            clk_enable(clk);
        else
            clk_disable(clk);
    }

    return NvSuccess;
}

NvError NvRmPowerVoltageControl( 
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmModuleID ModuleId,
    NvU32 ClientId,
    NvRmMilliVolts MinVolts,
    NvRmMilliVolts MaxVolts,
    const NvRmMilliVolts * PrefVoltageList,
    NvU32 PrefVoltageListCount,
    NvRmMilliVolts * CurrentVolts)
{
    return NvSuccess;
}
