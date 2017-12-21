/*
 * Copyright © 2014 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "i915_params.h"
#include "i915_drv.h"

#define i915_param_named(name, T, perm, desc) \
	module_param_named(name, i915_modparams.name, T, perm); \
	MODULE_PARM_DESC(name, desc)
#define i915_param_named_unsafe(name, T, perm, desc) \
	module_param_named_unsafe(name, i915_modparams.name, T, perm); \
	MODULE_PARM_DESC(name, desc)

struct i915_params i915_modparams __read_mostly = {
	.modeset = -1,
	.panel_ignore_lid = 1,
	.semaphores = -1,
	.lvds_channel_mode = 0,
	.panel_use_ssc = -1,
	.vbt_sdvo_panel_type = -1,
	.enable_rc6 = -1,
	.enable_dc = -1,
	.enable_fbc = -1,
	.enable_execlists = -1,
	.enable_hangcheck = true,
	.enable_ppgtt = -1,
	.enable_psr = -1,
	.alpha_support = IS_ENABLED(CONFIG_DRM_I915_ALPHA_SUPPORT),
	.disable_power_well = -1,
	.enable_ips = 1,
	.fastboot = 0,
	.prefault_disable = 0,
	.load_detect_test = 0,
	.force_reset_modeset_test = 0,
	.reset = 2,
	.error_capture = true,
	.invert_brightness = 0,
	.disable_display = 0,
	.enable_cmd_parser = true,
	.use_mmio_flip = 0,
	.mmio_debug = 0,
	.verbose_state_checks = 1,
	.nuclear_pageflip = 0,
	.edp_vswing = 0,
	.enable_guc_loading = 1,
	.enable_guc_submission = 1,
	.guc_log_level = -1,
	.guc_firmware_path = NULL,
	.huc_firmware_path = NULL,
	.enable_dp_mst = true,
	.inject_load_failure = 0,
	.enable_dpcd_backlight = false,
	.enable_gvt = false,
	.enable_pvmmio = 1,
	.enable_gvt_oos = 1,
	.enable_conformance_check = true,
	.disable_gvt_fw_loading = true,
	.gvt_workload_priority = 0,
	.enable_initial_modeset = false,
	.splash = NULL,
	.avail_planes_per_pipe = 0,
	.domain_plane_owners = 0,
	.bg_color = 0x00000000,
	.gvt_emulate_hdmi = true,
	.domain_scaler_owner = 0x21100,
	.memtrack_debug = 1,
};

i915_param_named(modeset, int, 0400,
	"Use kernel modesetting [KMS] (0=disable, "
	"1=on, -1=force vga console preference [default])");

i915_param_named_unsafe(panel_ignore_lid, int, 0600,
	"Override lid status (0=autodetect, 1=autodetect disabled [default], "
	"-1=force lid closed, -2=force lid open)");

i915_param_named_unsafe(semaphores, int, 0400,
	"Use semaphores for inter-ring sync "
	"(default: -1 (use per-chip defaults))");

i915_param_named_unsafe(enable_rc6, int, 0400,
	"Enable power-saving render C-state 6. "
	"Different stages can be selected via bitmask values "
	"(0 = disable; 1 = enable rc6; 2 = enable deep rc6; 4 = enable deepest rc6). "
	"For example, 3 would enable rc6 and deep rc6, and 7 would enable everything. "
	"default: -1 (use per-chip default)");

i915_param_named_unsafe(enable_dc, int, 0400,
	"Enable power-saving display C-states. "
	"(-1=auto [default]; 0=disable; 1=up to DC5; 2=up to DC6)");

i915_param_named_unsafe(enable_fbc, int, 0600,
	"Enable frame buffer compression for power savings "
	"(default: -1 (use per-chip default))");

i915_param_named_unsafe(lvds_channel_mode, int, 0400,
	 "Specify LVDS channel mode "
	 "(0=probe BIOS [default], 1=single-channel, 2=dual-channel)");

i915_param_named_unsafe(panel_use_ssc, int, 0600,
	"Use Spread Spectrum Clock with panels [LVDS/eDP] "
	"(default: auto from VBT)");

i915_param_named_unsafe(vbt_sdvo_panel_type, int, 0400,
	"Override/Ignore selection of SDVO panel mode in the VBT "
	"(-2=ignore, -1=auto [default], index in VBT BIOS table)");

i915_param_named_unsafe(reset, int, 0600,
	"Attempt GPU resets (0=disabled, 1=full gpu reset, 2=engine reset [default])");

i915_param_named_unsafe(vbt_firmware, charp, 0400,
	"Load VBT from specified file under /lib/firmware");

#if IS_ENABLED(CONFIG_DRM_I915_CAPTURE_ERROR)
i915_param_named(error_capture, bool, 0600,
	"Record the GPU state following a hang. "
	"This information in /sys/class/drm/card<N>/error is vital for "
	"triaging and debugging hangs.");
#endif

i915_param_named_unsafe(enable_hangcheck, bool, 0644,
	"Periodically check GPU activity for detecting hangs. "
	"WARNING: Disabling this can cause system wide hangs. "
	"(default: true)");

i915_param_named_unsafe(enable_ppgtt, int, 0400,
	"Override PPGTT usage. "
	"(-1=auto [default], 0=disabled, 1=aliasing, 2=full, 3=full with extended address space)");

i915_param_named_unsafe(enable_execlists, int, 0400,
	"Override execlists usage. "
	"(-1=auto [default], 0=disabled, 1=enabled)");

i915_param_named_unsafe(enable_psr, int, 0600,
	"Enable PSR "
	"(0=disabled, 1=enabled - link mode chosen per-platform, 2=force link-standby mode, 3=force link-off mode) "
	"Default: -1 (use per-chip default)");

i915_param_named_unsafe(alpha_support, bool, 0400,
	"Enable alpha quality driver support for latest hardware. "
	"See also CONFIG_DRM_I915_ALPHA_SUPPORT.");

i915_param_named_unsafe(disable_power_well, int, 0400,
	"Disable display power wells when possible "
	"(-1=auto [default], 0=power wells always on, 1=power wells disabled when possible)");

i915_param_named_unsafe(enable_ips, int, 0600, "Enable IPS (default: true)");

i915_param_named(fastboot, bool, 0600,
	"Try to skip unnecessary mode sets at boot time (default: false)");

i915_param_named_unsafe(prefault_disable, bool, 0600,
	"Disable page prefaulting for pread/pwrite/reloc (default:false). "
	"For developers only.");

i915_param_named_unsafe(load_detect_test, bool, 0600,
	"Force-enable the VGA load detect code for testing (default:false). "
	"For developers only.");

i915_param_named_unsafe(force_reset_modeset_test, bool, 0600,
	"Force a modeset during gpu reset for testing (default:false). "
	"For developers only.");

i915_param_named_unsafe(invert_brightness, int, 0600,
	"Invert backlight brightness "
	"(-1 force normal, 0 machine defaults, 1 force inversion), please "
	"report PCI device ID, subsystem vendor and subsystem device ID "
	"to dri-devel@lists.freedesktop.org, if your machine needs it. "
	"It will then be included in an upcoming module version.");

i915_param_named(disable_display, bool, 0400,
	"Disable display (default: false)");

i915_param_named_unsafe(enable_cmd_parser, bool, 0400,
	"Enable command parsing (true=enabled [default], false=disabled)");

i915_param_named_unsafe(use_mmio_flip, int, 0600,
	"use MMIO flips (-1=never, 0=driver discretion [default], 1=always)");

i915_param_named(mmio_debug, int, 0600,
	"Enable the MMIO debug code for the first N failures (default: off). "
	"This may negatively affect performance.");

i915_param_named(verbose_state_checks, bool, 0600,
	"Enable verbose logs (ie. WARN_ON()) in case of unexpected hw state conditions.");

i915_param_named_unsafe(nuclear_pageflip, bool, 0400,
	"Force enable atomic functionality on platforms that don't have full support yet.");

/* WA to get away with the default setting in VBT for early platforms.Will be removed */
i915_param_named_unsafe(edp_vswing, int, 0400,
	"Ignore/Override vswing pre-emph table selection from VBT "
	"(0=use value from vbt [default], 1=low power swing(200mV),"
	"2=default swing(400mV))");

i915_param_named_unsafe(enable_guc_loading, int, 0400,
	"Enable GuC firmware loading "
	"(-1=auto, 0=never, 1=if available [default], 2=required)");

i915_param_named_unsafe(enable_guc_submission, int, 0400,
	"Enable GuC submission "
	"(-1=auto, 0=never, 1=if available [default], 2=required)");

i915_param_named(guc_log_level, int, 0400,
	"GuC firmware logging level (-1:disabled (default), 0-3:enabled)");

i915_param_named_unsafe(guc_firmware_path, charp, 0400,
	"GuC firmware path to use instead of the default one");

i915_param_named_unsafe(huc_firmware_path, charp, 0400,
	"HuC firmware path to use instead of the default one");

i915_param_named_unsafe(enable_dp_mst, bool, 0600,
	"Enable multi-stream transport (MST) for new DisplayPort sinks. (default: true)");

i915_param_named_unsafe(inject_load_failure, uint, 0400,
	"Force an error after a number of failure check points (0:disabled (default), N:force failure at the Nth failure check point)");

i915_param_named(enable_dpcd_backlight, bool, 0600,
	"Enable support for DPCD backlight control (default:false)");

i915_param_named(enable_gvt, bool, 0400,
	"Enable support for Intel GVT-g graphics virtualization host support(default:false)");

module_param_named(enable_pvmmio, i915_modparams.enable_pvmmio, bool, 0400);
MODULE_PARM_DESC(enable_pvmmio,
	"Enable pv mmio feature, default TRUE. This parameter "
	"could only set from host, guest value is set through vgt_if");

module_param_named(enable_gvt_oos, i915_modparams.enable_gvt_oos, bool, 0400);
MODULE_PARM_DESC(enable_gvt_oos, "To toggle the gvt ppgtt page table OOS (Out of Sync) feature.");

module_param_named(enable_conformance_check, i915_modparams.enable_conformance_check, bool, 0400);
MODULE_PARM_DESC(enable_conformance_check, "To toggle the GVT guest conformance feature.");

module_param_named(disable_gvt_fw_loading, i915_modparams.disable_gvt_fw_loading, bool, 0400);
MODULE_PARM_DESC(disable_gvt_fw_loading, "Disable GVT-g fw loading.");

module_param_named(gvt_workload_priority, i915_modparams.gvt_workload_priority, int, 0600);
MODULE_PARM_DESC(gvt_workload_priority,
	"Set GVT-g workload priority, (range: (-1023, 1023), default: 0, "
	"more positive value means higher priority).");
module_param_named_unsafe(enable_initial_modeset, i915_modparams.enable_initial_modeset, bool, 0400);
MODULE_PARM_DESC(enable_initial_modeset,
		 "Do initial modeset (default : false)");

i915_param_named_unsafe(splash, charp, 0400,
	"Load a splash screen binary image for a specific display splash=<connector>:<image>:w,h,pitch,crtc_x,crtc_y,crtc_w,crtc_h");

module_param_named_unsafe(avail_planes_per_pipe, i915_modparams.avail_planes_per_pipe, uint, 0400);
/* pipeA = BITS 0-3, pipeB = BITS 8-11, pipeC = BITS 16-18
 * +----------+-------+---------+--------+--------+--------+--------+
 * |unused    |unused |  Pipe C | unused | Pipe B | unused | Pipe A |
 * +----------+-------+---------+--------+--------+--------+--------+
 * 31         23      18        15       11       7        3        0
 *
 *
 * BITS 0,1,2,3 - needs to be set planes assigned for pipes A and B
 * and BITs 0,1,2 - for pipe C
 * eg: avail_planes_per_pipe = 0x3 - pipe A=2(planes 1 and 2) , pipeB=0 and pipeC=0 planes
 * eg: avail_planes_per_pipe = 0x5 - pipe A=2(planes 1 and 3) , pipeB=0 and pipeC=0 planes
 * avail_planes_per_pipe = 0x030701 - pipe A =1(plane 1, pipeB=3(planes 1,2 and 3), pipeC=2( planes 1 and 2)
 *
 */
MODULE_PARM_DESC(avail_planes_per_pipe, "plane mask for each	pipe: \
		set BITS 0-3:pipeA 8-11:pipeB 16-18:pipeC to specify the planes that \
		are available eg: 0x030701 : planes 1:pipeA 1,2,3:pipeB \
		1,2:pipeC (0x0 - default value)");
module_param_named_unsafe(domain_plane_owners, i915_modparams.domain_plane_owners, ullong, 0400);
/* pipeA = BITS 0-15 pipeB = 16-31, pipeC = 32-47
 *
 * +----------+------------+-------------+------------+
 * |unused    |  Pipe C    |   Pipe B    |   Pipe A   |
 * +----------+------------+-------------+------------+
 * 63         47           31            15           0
 *
 * Each nibble represents domain id. 0 for Dom0, 1,2,3...0xF for DomUs
 * eg: domain_plane_owners = 0x022111000010 // 0x0221|1100|0010
 * plane		 domain
 * plane_owner1A -0
 * plane_owner2A -1
 * plane_owner3A -0
 * plane_owner4A -0
 * plane_owner1B -0
 * plane_owner2B -0
 * plane_owner3B -1
 * plane_owner4B -1
 * plane_owner1C -1
 * plane_owner2C -2
 * plane_owner3C -2
 *
 *
 */
MODULE_PARM_DESC(domain_plane_owners, "plane owners for each domain and for each pipe \
		ids can be from 0-F,  eg: domain_plane_owners = 0x022111000010 \
		  planes owner: 3C:2 2C:2 1C:1 4B:1 3B:1 2B:1 1B:0 4A:0 3A:0 2A:1 1A:0 \
		  (0x0 - default value)");

module_param_named_unsafe(bg_color, i915_modparams.bg_color, uint, 0400);
MODULE_PARM_DESC(bg_color, "Set the background (canvas) color");

module_param_named(gvt_emulate_hdmi, i915_modparams.gvt_emulate_hdmi, bool, 0400);
MODULE_PARM_DESC(gvt_emulate_hdmi, "GVT-g emulate HDMI or DP port for Guest OS.");

module_param_named_unsafe(domain_scaler_owner,
			  i915_modparams.domain_scaler_owner, int, 0400);
/* pipeA Scaler = BITS 0-7 pipeB scaler = 8-15, pipeC = 16-19
 *
 * +----------+------------+-------------+------------+
 * |unused    |  Pipe C    |   Pipe B    |   Pipe A   |
 * +----------+------------+-------------+------------+
 * 31       20 19        16 15           8 7           0
 *
 * Each nibble represents domain id. 0 for Dom0, 1,2,3...0xF for DomUs
 * eg: domain_plane_owners = 0x00030210 // 0x000|3|02|10
 * plane                 domain
 * scaler_owner1A -0
 * scaler_owner2A -1
 * scaler_owner3A -2
 * scaler_owner4A -0
 * scaler_owner1B -3
 * scaler_owner2B -0
 *
 */
MODULE_PARM_DESC(domain_scaler_owner, "scaler owners for each domain and for\n"
	"each pipe ids can be from 0-F, eg domain_scaler_owners = 0x00030210\n"
	"scaler owner: 1C:3 2B:0 1B:2 2A:1 1A:0 (0x0 - default value)\n");

i915_param_named(fpreempt_timeout, uint, 0600,
	"Wait time in msecs before forcing a preemption with reset (0:never force [default])");
module_param_named(memtrack_debug, i915_modparams.memtrack_debug, int, 0600);
MODULE_PARM_DESC(memtrack_debug,
		"use Memtrack debug capability (0=never, 1=always)");
