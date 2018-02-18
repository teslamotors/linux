/*
 * PMC interface for NVIDIA SoCs Tegra
 *
 * Copyright (c) 2013-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
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

#ifndef __LINUX_TEGRA_PMC_H__
#define __LINUX_TEGRA_PMC_H__

#include <linux/tegra-pm.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_device.h>

extern void tegra_pmc_enable_wake_det(bool enable);
extern void tegra_pmc_set_dpd_sample(void);
extern void tegra_pmc_clear_dpd_sample(void);
extern void tegra_pmc_remove_dpd_req(void);
extern void __iomem *tegra_pmc_base;

extern void tegra_pmc_register_update(int offset,
	unsigned long mask, unsigned long val);
extern void tegra_pmc_write_bootrom_command(u32 command_offset,
	unsigned long val);

extern void tegra_pmc_pwr_detect_update(unsigned long mask, unsigned long val);
extern unsigned long tegra_pmc_pwr_detect_get(unsigned long mask);

extern void tegra_pmc_io_dpd_clear(void);
extern int tegra_pmc_io_dpd_enable(int reg, int bit_pos);
extern int tegra_pmc_io_dpd_disable(int reg, int bit_pos);
extern int tegra_pmc_io_dpd_get_status(int reg, int bit_pos);

extern void tegra_pmc_iopower_enable(int reg, u32 bit_mask);
extern void tegra_pmc_iopower_disable(int reg, u32 bit_mask);
extern int tegra_pmc_iopower_get_status(int reg, u32 bit_mask);

extern void tegra_pmc_reset_system(void);

extern bool tegra_is_dpd_mode;

struct pmc_pm_data {
	u32 cpu_good_time;	/* CPU power good time in uS */
	u32 cpu_off_time;	/* CPU power off time in uS */
	u32 cpu_suspend_freq;	/* CPU suspend frequency in hz */
	u32 core_osc_time;	/* Core power good osc time in uS */
	u32 core_pmu_time;	/* Core power good pmu time in uS */
	u32 core_off_time;	/* Core power off time in uS */
	bool corereq_high;	/* Core power request active-high */
	bool sysclkreq_high;	/* System clock request active-high */
	bool combined_req;	/* Combined pwr req for CPU & Core */
	bool cpu_pwr_good_en;	/* CPU power good signal is enabled */
	u32 lp0_vec_phy_addr;	/* The phy addr of LP0 warm boot code */
	u32 lp0_vec_size;	/* The size of LP0 warm boot code */
	enum tegra_suspend_mode suspend_mode;
};

/**
 * struct tegra_thermtrip_pmic_data - PMIC shutdown command data
 * @poweroff_reg_data:	The data to write to turn the system off
 * @poweroff_reg_addr:	The PMU address of poweroff register
 * @reset_tegra:	Flag indicating whether or not the system
 *			will shutdown during a thermal trip.
 * @controller_type:	If this field is set to 0, the PMIC is
 *			connected via I2C. If it is set to 1,
 *			it is connected via SPI. If it is set to
 *			2, it is connected via GPIO.
 * @i2c_controller_id:	The i2c bus controller id
 * @pinmux:		An array index used to configure which pins
 *			on the chip are muxed to the I2C/SPI/GPIO
 *			controller that is in use. Contact NVIDIA
 *			for more information on what these index values
 *			mean for a given chip.
 * @pmu_16bit_ops:	If 0, sends three bytes from the PMC_SCRATCH54
 *			register to the PMIC to turn it off; if 1, sends
 *			four bytes from the PMC_SCRATCH54 register to the PMIC
 *			to turn it off, plus one other byte. Must be set to
 *			0 - the current code does not support 16 bit
 *			operations.
 * @pmu_i2c_addr:	The address of the PMIC on the I2C bus
 *
 * When the SoC temperature gets too high, the SOC_THERM hardware can
 * reset the SoC, and, by setting a bit in one of its registers, can
 * instruct the boot ROM to power off the Tegra SoC. This data
 * structure contains the information that the boot ROM needs to tell
 * the PMIC to shut down.
 *
 * @poweroff_reg_data and @poweroff_reg_addr are written to the PMC SCRATCH54
 * register.
 *
 * @reset_tegra, @controller_type, @i2c_controller_id, @pinmux, @pmu_16bit_ops
 * and @pmu_i2c_addr are written to the PMC SCRATCH55 register.
 */
struct tegra_thermtrip_pmic_data {
	u8 poweroff_reg_data;
	u8 poweroff_reg_addr;
	u8 reset_tegra;
	u8 controller_type;
	u8 i2c_controller_id;
	u8 pinmux;
	u8 pmu_16bit_ops;
	u8 pmu_i2c_addr;
};

#if defined(CONFIG_OF) && defined(CONFIG_COMMON_CLK) && defined(CONFIG_PM_SLEEP)
void set_power_timers(unsigned long us_on, unsigned long us_off);
#endif

bool tegra_pmc_cpu_is_powered(int cpuid);
int tegra_pmc_cpu_power_on(int cpuid);
int tegra_pmc_cpu_remove_clamping(int cpuid);
struct pmc_pm_data *tegra_get_pm_data(void);

extern void tegra_pmc_config_thermal_trip(struct tegra_thermtrip_pmic_data *data);

extern void tegra_pmc_enable_thermal_trip(void);
extern void tegra_pmc_lock_thermal_shutdown(void);

#ifdef CONFIG_PADCTRL_TEGRA210_PMC
extern int tegra210_pmc_padctrl_init(struct device *dev,
		struct device_node *np);
#endif

#ifdef CONFIG_PADCTRL_TEGRA186_PMC
extern int tegra186_pmc_padctrl_init(struct device *dev,
		struct device_node *np);
#endif

static inline int tegra_pmc_padctrl_init(struct device *dev,
                struct device_node *np)
{
#ifdef CONFIG_PADCTRL_TEGRA210_PMC
	return tegra210_pmc_padctrl_init(dev, np);
#endif
#ifdef CONFIG_PADCTRL_TEGRA186_PMC
	return tegra186_pmc_padctrl_init(dev, np);
#endif
	return 0;
}

#ifdef CONFIG_TEGRA210_BOOTROM_PMC
extern int tegra210_boorom_pmc_init(struct device *dev);
#endif

static inline int tegra_boorom_pmc_init(struct device *dev)
{
#ifdef CONFIG_TEGRA210_BOOTROM_PMC
	return tegra210_boorom_pmc_init(dev);
#else
	return 0;
#endif
}

extern void __iomem *tegra186_pmc_base;
extern void tegra186_pmc_register_update(int offset,
	unsigned long mask, unsigned long val);
extern unsigned long tegra_pmc_register_get(u32 offset);

extern int tegra186_pmc_io_dpd_enable(int reg, int bit_pos);
extern int tegra186_pmc_io_dpd_disable(int reg, int bit_pos);
extern int tegra186_pmc_io_dpd_get_status(int reg, int bit_pos);

extern void tegra_pmc_pad_voltage_update(unsigned int reg,
		unsigned long mask, unsigned long val);
extern unsigned long tegra_pmc_pad_voltage_get(unsigned int reg);

extern void tegra_pmc_ufs_pwrcntrl_update(unsigned long mask,
		unsigned long val);
extern unsigned long tegra_pmc_ufs_pwrcntrl_get(void);
extern void tegra_pmc_nvcsi_ab_brick_update(unsigned long mask,
		unsigned long val);
extern unsigned long tegra_pmc_nvcsi_ab_brick_getstatus(void);
extern void tegra_pmc_nvcsi_cdef_brick_update(unsigned long mask,
		unsigned long val);
extern unsigned long tegra_pmc_nvcsi_cdef_brick_getstatus(void);
extern void tegra186_pmc_disable_nvcsi_brick_dpd(void);
extern void tegra186_pmc_enable_nvcsi_brick_dpd(void);

#ifdef CONFIG_TEGRA186_PMC
extern void tegra_pmc_sata_pwrgt_update(unsigned long mask,
		unsigned long val);
extern unsigned long tegra_pmc_sata_pwrgt_get(void);
#else
static inline void tegra_pmc_sata_pwrgt_update(unsigned long mask,
		unsigned long val)
{
}

static inline unsigned long tegra_pmc_sata_pwrgt_get(void)
{
	return -EINVAL;
}
#endif

extern void tegra_pmc_fuse_control_ps18_latch_set(void);
extern void tegra_pmc_fuse_control_ps18_latch_clear(void);

#ifdef CONFIG_TEGRA186_PMC
extern bool tegra_pmc_is_halt_in_fiq(void);
#else
static inline bool tegra_pmc_is_halt_in_fiq(void)
{
	return false;
}
#endif

#endif	/* __LINUX_TEGRA_PMC_H__ */
