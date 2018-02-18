/*
 * drivers/ata/ahci_tegra.c
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
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

#include "ahci_tegra.h"
#ifdef CONFIG_DEBUG_FS
#include "ahci_tegra_debug.h"
#endif

static int tegra_ahci_power_on(struct ahci_host_priv *hpriv);
static void tegra_ahci_power_off(struct ahci_host_priv *hpriv);
static int tegra_ahci_controller_init(struct ahci_host_priv *hpriv);
static void tegra_ahci_controller_deinit(struct ahci_host_priv *hpriv);
static int tegra_ahci_quirks(struct ahci_host_priv *hpriv);
static int tegra_ahci_disable_features(struct ahci_host_priv *hpriv);
static struct ahci_host_priv *
tegra_ahci_platform_get_resources(struct tegra_ahci_priv *);
#ifdef CONFIG_PM
static int tegra_ahci_runtime_suspend(struct device *dev);
static int tegra_ahci_runtime_resume(struct device *dev);
#endif
#ifdef CONFIG_PM_SLEEP
static int tegra_ahci_suspend(struct device *dev);
static int tegra_ahci_resume(struct device *dev);
#endif

static char * const tegra_rail_names[] = {};

static const struct tegra_ahci_soc_data tegra_ahci_data = {
	.sata_regulator_names = tegra_rail_names,
	.num_sata_regulators = ARRAY_SIZE(tegra_rail_names),
	.ops = {
		.tegra_ahci_power_on = tegra_ahci_power_on,
		.tegra_ahci_power_off = tegra_ahci_power_off,
		.tegra_ahci_quirks = tegra_ahci_quirks,
		.tegra_ahci_platform_get_resources =
			tegra_ahci_platform_get_resources,
	},
};

static const struct of_device_id tegra_ahci_of_match[] = {
	{ .compatible = "nvidia,tegra186-ahci-sata",
		.data = &tegra_ahci_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, of_ahci_tegra_match);

static void tegra_ahci_host_stop(struct ata_host *host)
{
	struct ahci_host_priv *hpriv = host->private_data;

	tegra_ahci_controller_deinit(hpriv);
}

static int tegra_ahci_port_suspend(struct ata_port *ap, pm_message_t mesg)
{
	struct ata_host *host = ap->host;
	struct ahci_host_priv *hpriv =  host->private_data;
	struct tegra_ahci_priv *tegra = hpriv->plat_data;
	struct ata_link *link;
	struct ata_device *dev;
	int ret = 0;
	u32 port_status = 0;
	enum tegra_ahci_port_runtime_status lpm_state;
	int i;

	lpm_state = TEGRA_AHCI_PORT_RUNTIME_ACTIVE;
	tegra->skip_rtpm = false;

	port_status = tegra_ahci_bar5_readl(hpriv, T_AHCI_PORT_PXSSTS);
	port_status = (port_status & T_AHCI_PORT_PXSSTS_IPM_MASK) >>
					T_AHCI_PORT_PXSSTS_IPM_SHIFT;

	ata_for_each_link(link, ap, PMP_FIRST) {
		if (link->flags & ATA_LFLAG_NO_LPM) {
			ata_link_info(link, "No LPM on this link\n");
			continue;
		}
		ata_for_each_dev(dev, link, ENABLED) {
			bool hipm = ata_id_has_hipm(dev->id);
			bool dipm = ata_id_has_dipm(dev->id) &&
				(!(link->ap->flags & ATA_FLAG_NO_DIPM));

			if (ap->target_lpm_policy == ATA_LPM_MIN_POWER) {
				if ((hpriv->cap2 & HOST_CAP2_SDS) &&
				(hpriv->cap2 & HOST_CAP2_SADM) &&
				(link->device->flags & ATA_DFLAG_DEVSLP))
					lpm_state =
						TEGRA_AHCI_PORT_RUNTIME_DEVSLP;
				else
					lpm_state =
						TEGRA_AHCI_PORT_RUNTIME_SLUMBER;
			} else if (ap->target_lpm_policy == ATA_LPM_MED_POWER) {
				lpm_state = TEGRA_AHCI_PORT_RUNTIME_PARTIAL;
			}

			if (hipm || dipm) {
				for (i = 0; i < TEGRA_AHCI_LPM_TIMEOUT; i++) {
					port_status = tegra_ahci_bar5_readl(
						hpriv, T_AHCI_PORT_PXSSTS);
					port_status =
						(port_status &
						T_AHCI_PORT_PXSSTS_IPM_MASK)
						>> T_AHCI_PORT_PXSSTS_IPM_SHIFT;
					if (port_status < lpm_state)
						mdelay(10);
					else
						break;
				}

				if (port_status < lpm_state) {
					ata_link_err(link,
						"Link didn't enter LPM\n");
					if (ap->pm_mesg.event & PM_EVENT_AUTO)
						ret = -EBUSY;
				} else {
					if (!(port_status ==
						TEGRA_AHCI_PORT_RUNTIME_ACTIVE))
						ata_link_info(link,
							"Link entered LPM\n");
				}
			} else {
				ata_dev_info(dev,
						"does not support HIPM/DIPM\n");
			}
		}
	}

	if (!ret && (lpm_state == TEGRA_AHCI_PORT_RUNTIME_ACTIVE ||
		port_status == TEGRA_AHCI_PORT_RUNTIME_ACTIVE)) {
		if (ap->pm_mesg.event & PM_EVENT_AUTO) {
			tegra->skip_rtpm = true;
			dev_info(&tegra->pdev->dev,
					"Skip powergating SATA Controller\n");
			return 0;
		}
	}

	if (!ret && !(ap->pflags & ATA_PFLAG_SUSPENDED)) {
		ret = ahci_ops.port_suspend(ap, mesg);
		if (ret == 0) {
			pm_runtime_mark_last_busy(&tegra->pdev->dev);
			pm_runtime_put_sync_autosuspend(&tegra->pdev->dev);
		}
	}

	return ret;
}

static int tegra_ahci_port_resume(struct ata_port *ap)
{
	struct ata_host *host = ap->host;
	struct ahci_host_priv *hpriv =  host->private_data;
	struct tegra_ahci_priv *tegra = hpriv->plat_data;
	struct ata_link *link = NULL;
#ifdef CONFIG_PM
	struct scsi_device *sdev = NULL;
#endif
	int ret = 0;

	if (tegra->skip_rtpm) {
		tegra->skip_rtpm = false;
		if (ap->pm_mesg.event & PM_EVENT_AUTO) {
			ata_for_each_link(link, ap, HOST_FIRST) {
				link->eh_info.action &= ~ATA_EH_RESET;
			}
			ata_eh_thaw_port(ap);
			return 0;
		}
	}

	ret = pm_runtime_get_sync(&tegra->pdev->dev);
	if (ret < 0) {
		dev_err(&tegra->pdev->dev,
				"%s(%d) Failed to resume the devcie err=%d\n",
				__func__, __LINE__, ret);
		return AC_ERR_SYSTEM;
	}

	if (ap->pm_mesg.event & PM_EVENT_RESUME) {
		if (ap->pm_mesg.event & PM_EVENT_AUTO)
			ata_for_each_link(link, ap, HOST_FIRST) {
				link->eh_info.action &= ~ATA_EH_RESET;
			}
#ifdef CONFIG_PM
		else
			shost_for_each_device(sdev, ap->scsi_host) {
				if (sdev->request_queue->rpm_status ==
						RPM_SUSPENDED)
					sdev->request_queue->rpm_status =
						RPM_ACTIVE;
			}
#endif
	}

	ret = ahci_ops.port_resume(ap);

	if ((ap->pm_mesg.event & PM_EVENT_AUTO) &&
			(ap->pm_mesg.event & PM_EVENT_RESUME))
		ata_eh_thaw_port(ap);

	return ret;
}

static unsigned int tegra_ahci_qc_issue(struct ata_queued_cmd *qc)
{

	if (qc->tf.command == ATA_CMD_SET_FEATURES &&
			qc->tf.feature ==  SATA_FPDMA_OFFSET) {
		WARN(1, "SATA_FPDMA_OFFSET Feature is not supported");
		return AC_ERR_INVALID;
	} else if (qc->tf.command == ATA_CMD_READ_LOG_EXT &&
					qc->tf.lbal == ATA_LOG_SATA_NCQ) {
		u8 *buf =
		(u8 *) page_address((const struct page *)qc->sg->page_link);

		/*
		 * Since our SATA Controller does not support this command
		 * don't send this command to the drive instead complete
		 * the function here and indicate to the upper layer
		 * that there is no entries in the buffer.
		*/
		buf += qc->sg->offset;
		buf[0] = TEGRA_AHCI_READ_LOG_EXT_NOENTRY;
		qc->complete_fn(qc);

		return 0;
	}

	return ahci_ops.qc_issue(qc);
}

static int tegra_ahci_hardreset(struct ata_link *link, unsigned int *class,
			  unsigned long deadline)
{
	struct ata_host *host = link->ap->host;
	struct ahci_host_priv *hpriv =  host->private_data;
	int ret;
	u32 val;
	u32 mask;

	ret = ahci_ops.hardreset(link, class, deadline);
	if (ret < 0) {
		mask = val = T_SATA0_CFG_LINK_0_USE_POSEDGE_SCTL_DET;
		tegra_ahci_scfg_update(hpriv, val, mask, T_SATA0_CFG_LINK_0);
	}
	return ret;
}

static int tegra_ahci_softreset(struct ata_link *link, unsigned int *class,
			  unsigned long deadline)
{
	struct ata_host *host = link->ap->host;
	struct ahci_host_priv *hpriv =  host->private_data;
	int ret;
	u32 val;
	u32 mask;

	ret = ahci_ops.softreset(link, class, deadline);
	if (ret < 0) {
		mask = val = T_SATA0_CFG_LINK_0_USE_POSEDGE_SCTL_DET;
		tegra_ahci_scfg_update(hpriv, val, mask, T_SATA0_CFG_LINK_0);
	}
	return ret;

}

static struct ata_port_operations ahci_tegra_port_ops = {
	.inherits	= &ahci_ops,
	.qc_issue	= tegra_ahci_qc_issue,
	.host_stop	= tegra_ahci_host_stop,
	.port_suspend	= tegra_ahci_port_suspend,
	.port_resume	= tegra_ahci_port_resume,
	.hardreset	= tegra_ahci_hardreset,
	.softreset	= tegra_ahci_softreset,
};

static struct ata_port_info ahci_tegra_port_info = {
	.flags		= AHCI_FLAG_COMMON,
	.pio_mask	= ATA_PIO4,
	.udma_mask	= ATA_UDMA6,
	.port_ops	= &ahci_tegra_port_ops,
};

#if defined(CONFIG_PM_SLEEP) || defined(CONFIG_PM)
static void tegra_ahci_pg_save_registers(struct ata_host *host)
{
	struct ahci_host_priv *hpriv =  host->private_data;
	struct tegra_ahci_priv *tegra = hpriv->plat_data;
	u32 *pg_save;
	u32 regs;
	int i;

	pg_save = tegra->pg_save;

	/* save BAR5 registers */
	regs = ARRAY_SIZE(pg_save_bar5_registers);
	tegra_ahci_save_regs(&pg_save, tegra->base_list[TEGRA_SATA_AHCI],
			pg_save_bar5_registers, regs);

	/* save BAR5 port_registers */
	regs = ARRAY_SIZE(pg_save_bar5_port_registers);
	for (i = 0; i < hpriv->nports; ++i)
		tegra_ahci_save_regs(&pg_save,
				tegra->base_list[TEGRA_SATA_AHCI] + (0x80*i),
				pg_save_bar5_port_registers, regs);

	/* save bkdr registers */
	regs = ARRAY_SIZE(pg_save_bar5_bkdr_registers);
	tegra_ahci_save_regs(&pg_save, tegra->base_list[TEGRA_SATA_AHCI],
			pg_save_bar5_bkdr_registers, regs);

	/* and save bkdr per_port registers */
	for (i = 0; i < hpriv->nports; ++i) {
		tegra_ahci_scfg_writel(hpriv, (1 << i), T_SATA0_INDEX);
		regs = ARRAY_SIZE(pg_save_bar5_bkdr_port_registers);
		tegra_ahci_save_regs(&pg_save,
				tegra->base_list[TEGRA_SATA_AHCI] + (0x80*i),
				pg_save_bar5_bkdr_port_registers,
				regs);
	}
	tegra_ahci_scfg_writel(hpriv, T_SATA0_INDEX_NONE_SELECTED,
								T_SATA0_INDEX);
}

static void tegra_ahci_pg_restore_registers(struct ata_host *host)
{
	struct ahci_host_priv *hpriv =  host->private_data;
	struct tegra_ahci_priv *tegra = hpriv->plat_data;
	void *pg_save;
	u32 regs;
	int i;

	pg_save = tegra->pg_save;

	tegra_ahci_controller_init(hpriv);

	/* restore BAR5 registers */
	regs = ARRAY_SIZE(pg_save_bar5_registers);
	tegra_ahci_restore_regs(&pg_save, tegra->base_list[TEGRA_SATA_AHCI],
			pg_save_bar5_registers, regs);

	/* restore BAR5 port_registers */
	regs = ARRAY_SIZE(pg_save_bar5_port_registers);
	for (i = 0; i < hpriv->nports; ++i)
		tegra_ahci_restore_regs(&pg_save,
				tegra->base_list[TEGRA_SATA_AHCI]+(0x80*i),
				pg_save_bar5_port_registers, regs);

	/* restore bkdr registers */
	regs = ARRAY_SIZE(pg_restore_bar5_bkdr_registers);
	tegra_ahci_restore_regs(&pg_save, tegra->base_list[TEGRA_SATA_CONFIG],
			pg_restore_bar5_bkdr_registers, regs);

	/* and restore BAR5 bkdr per_port registers */
	for (i = 0; i < hpriv->nports; ++i) {
		tegra_ahci_scfg_writel(hpriv, (1 << i), T_SATA0_INDEX);
		regs = ARRAY_SIZE(pg_restore_bar5_bkdr_port_registers);
		tegra_ahci_restore_regs(&pg_save,
				tegra->base_list[TEGRA_SATA_CONFIG],
				pg_restore_bar5_bkdr_port_registers,
				regs);
	}
	tegra_ahci_scfg_writel(hpriv, T_SATA0_INDEX_NONE_SELECTED,
								T_SATA0_INDEX);
}

static int tegra_ahci_pg_save_restore_init(struct ahci_host_priv *hpriv)
{
	struct tegra_ahci_priv *tegra = hpriv->plat_data;
	struct platform_device *pdev = tegra->pdev;
	struct device *dev = &pdev->dev;
	u32 save_size;
	int ret = 0;

	/* Setup PG save/restore area: */

	/* calculate the size */
	save_size = ARRAY_SIZE(pg_save_bar5_registers) +
		ARRAY_SIZE(pg_save_bar5_bkdr_registers);

	/* and add save port_registers for all the ports */
	save_size += ARRAY_SIZE(pg_save_bar5_port_registers) +
		 ARRAY_SIZE(pg_save_bar5_bkdr_port_registers);

	/*
	 * save_size is number of registers times number of bytes per
	 * register to get total save size.
	 */
	save_size *= sizeof(u32);
	tegra->pg_save = devm_kzalloc(dev, save_size, GFP_KERNEL);
	if (!tegra->pg_save)
		ret = -ENOMEM;

	return 0;
}

static bool tegra_ahci_is_link_in_devslp(struct ahci_host_priv *hpriv)
{
	u32 rval = 0;

	rval = tegra_ahci_aux_readl(hpriv, SATA_AUX_RX_STAT_INT_0);
	if (rval & SATA_DEVSLP)
		return true;
	return false;
}

static void tegra_ahci_override_devslp(struct ahci_host_priv *hpriv,
		bool override)
{
	u32 val = 0;
	u32 mask = DEVSLP_OVERRIDE;

	if (override)
		val = DEVSLP_OVERRIDE;
	else
		val = 0xFFFFFF & ~DEVSLP_OVERRIDE;

	tegra_ahci_aux_update(hpriv, val, mask, SATA_AUX_MISC_CNTL_1_0);
}

static int tegra_ahci_elpg_enter(struct ata_host *host)
{
	struct ahci_host_priv *hpriv =  host->private_data;
	struct tegra_ahci_priv *tegra = hpriv->plat_data;
	int ret = 0;
	int i;

	/*
	 * 1. Program the UPHY_LANE registers to put UPHY to IDDQ
	 */
	for (i = 0; i < hpriv->nports; i++) {
		if (!hpriv->phys[i])
			continue;
		phy_power_off(hpriv->phys[i]);
	}

	/* 2. Program a register in the PMC to indicate to SATA that it is
	 *    entering power gating. This shall drive the pmc2sata_pg_info
	 *    signal
	 */
	tegra_pmc_sata_pwrgt_update(PMC_IMPL_SATA_PWRGT_0_PG_INFO,
						PMC_IMPL_SATA_PWRGT_0_PG_INFO);

	/*
	 * 3. Do the context save procedure for SATA
	 */
	tegra_ahci_pg_save_registers(host);

	/*
	 * 4. Check the assertion status of DEVSLP and
	 *    set the DEVSLP override with the following SATA AUX
	 *    registers accordingly.
	 */

	tegra->devslp_override = tegra_ahci_is_link_in_devslp(hpriv);
	if (tegra->devslp_override)
		tegra_ahci_override_devslp(hpriv, true);

	/* 5. Powergate */
	ret = tegra_powergate_partition_with_clk_off(TEGRA_POWERGATE_SATA);

	return ret;
}

static int tegra_ahci_elpg_exit(struct ata_host *host)
{
	struct ahci_host_priv *hpriv =  host->private_data;
	struct tegra_ahci_priv *tegra = hpriv->plat_data;
	int ret = 0;
	u32 val;
	u32 mask;
	int i;

	/* 1. unpowergate */
	ret = tegra_unpowergate_partition_with_clk_on(TEGRA_POWERGATE_SATA);
	if (ret)
		return ret;

	/* 2. Restore SATA Registers */
	tegra_ahci_pg_restore_registers(host);
	/*
	 * During the restoration of the registers, the driver would now need to
	 * restore the register T_SATA0_CFG_POWER_GATE_SSTS_RESTORED after the
	 * ssts_det, ssts_spd are restored. This register is used to tell the
	 * controller whether a drive existed earlier or not and move the PHY
	 * state machines into either HR_slumber or not.
	 */
	tegra_ahci_scfg_update(hpriv, T_SATA0_CFG_POWER_GATE_SSTS_RESTORED,
			T_SATA0_CFG_POWER_GATE_SSTS_RESTORED,
			T_SATA0_CFG_POWER_GATE);

	/* 3. If devslp asserted, de-assert devslp */
	if (tegra->devslp_override) {
		tegra_ahci_override_devslp(hpriv, false);

		val = mask = MDAT_TIMER_AFTER_PG_VALID;
		tegra_ahci_aux_update(hpriv, val, mask, SATA_AUX_SPARE_CFG0_0);
	}

	/* 4. Program a register in the PMC to indicate to SATA that it is
	 *    entering power gating. This shall drive the pmc2sata_pg_info
	 *    signal
	 */
	tegra_pmc_sata_pwrgt_update(PMC_IMPL_SATA_PWRGT_0_PG_INFO,
						~PMC_IMPL_SATA_PWRGT_0_PG_INFO);

	if (tegra->devslp_override) {
		tegra->devslp_override = false;
		val = mask = T_SATA0_CFG_POWER_GATE_POWER_UNGATE_COMP;
		tegra_ahci_scfg_update(hpriv, val, mask,
						T_SATA0_CFG_POWER_GATE);
	}
	/*
	 * 5. Program the UPHY_LANE registers to bring up UPHY from IDDQ
	 */
	for (i = 0; i < hpriv->nports; i++) {
		if (!hpriv->phys[i])
			continue;
		ret = phy_power_on(hpriv->phys[i]);
		if (ret) {
			phy_exit(hpriv->phys[i]);
			goto disable_phys;
		}
	}

	return 0;
disable_phys:
	while (--i >= 0) {
		phy_power_off(hpriv->phys[i]);
		phy_exit(hpriv->phys[i]);
	}
	return ret;
}
#endif

#ifdef CONFIG_PM
static int tegra_ahci_runtime_suspend(struct device *dev)
{
	struct ata_host *host = dev_get_drvdata(dev);
	int ret = 0;

	ret = tegra_ahci_elpg_enter(host);
	return ret;
}

static int tegra_ahci_runtime_resume(struct device *dev)
{
	struct ata_host *host = dev_get_drvdata(dev);
	int ret = 0;

	ret = tegra_ahci_elpg_exit(host);
	return ret;
}
#endif

#ifdef CONFIG_PM_SLEEP
static int tegra_ahci_suspend(struct device *dev)
{
	struct ata_host *host = dev_get_drvdata(dev);
	struct ahci_host_priv *hpriv = host->private_data;
	struct tegra_ahci_priv *tegra = hpriv->plat_data;
	int ret;
	int i;

	if (!pm_runtime_suspended(dev)) {
		ret = ahci_platform_suspend_host(dev);
		if (ret)
			return ret;

		ret = tegra_ahci_elpg_enter(host);
		if (ret)
			return ret;
	}

	/*
	 * If DEVSLP is asserted, PAD driver should enable the pull-up
	 * of the GPIO pin by programming the following register before
	 * entering LP0.
	 */

	if (tegra->devslp_override) {
		/* Yet to add the code - http://nvbugs/200132422 */
		tegra->devslp_pinmux_override = true;
	}

	/* Place uphy to reset */
	for (i = 0; i < hpriv->nports; i++) {
		if (!hpriv->phys[i])
			continue;
		phy_exit(hpriv->phys[i]);
	}

	return 0;
}

static int tegra_ahci_resume(struct device *dev)
{
	struct ata_host *host = dev_get_drvdata(dev);
	struct ahci_host_priv *hpriv = host->private_data;
	struct tegra_ahci_priv *tegra = hpriv->plat_data;
	int ret;
	int i;

	for (i = 0; i < hpriv->nports; i++) {
		if (!hpriv->phys[i])
			continue;
		ret = phy_init(hpriv->phys[i]);
		if (ret)
			goto disable_phys;
	}

	ret = tegra_ahci_elpg_exit(host);
	if (ret)
		goto disable_phys;

	/*
	 * If DEVSLP is supported and GPIO pin is assigned to SATA,
	 * PAD driver should disable the pull-up of the GPIO pin
	 * by programming the following register after exiting LP0
	 * and the DEVSLP override in SATA AUX register has been set.
	 */

	if (tegra->devslp_pinmux_override) {
		/* Yet to add the code - http://nvbugs/200132422 */

		tegra->devslp_pinmux_override = false;
	}

	ret = ahci_platform_resume_host(dev);
	if (ret)
		goto elpg_entry;

	/* We resumed so update PM runtime state */
	pm_runtime_disable(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	return 0;
elpg_entry:
	tegra_ahci_elpg_enter(host);
disable_phys:
	while (--i >= 0)
		phy_exit(hpriv->phys[i]);
	return ret;
}
#endif

static const struct dev_pm_ops tegra_ahci_dev_rt_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(tegra_ahci_suspend, tegra_ahci_resume)
	SET_RUNTIME_PM_OPS(tegra_ahci_runtime_suspend,
			tegra_ahci_runtime_resume, NULL)
};

static int tegra_ahci_power_on(struct ahci_host_priv *hpriv)
{
	struct tegra_ahci_priv *tegra = hpriv->plat_data;
	int ret;

	ret = regulator_bulk_enable(tegra->soc_data->num_sata_regulators,
			tegra->supplies);
	if (ret)
		return ret;

	reset_control_assert(tegra->sata_rst);
	reset_control_assert(tegra->sata_cold_rst);

	/* set SATA clk and SATA_OOB clk source */
	ret = clk_set_parent(tegra->sata_clk, tegra->pllp_uphy_clk);
	if (ret)
		goto disable_regulators;
	ret = clk_set_parent(tegra->sata_oob_clk, tegra->pllp_clk);
	if (ret)
		goto disable_regulators;
	ret = clk_set_rate(tegra->sata_clk, TEGRA_SATA_CORE_CLOCK_FREQ_HZ);
	if (ret)
		goto disable_regulators;
	ret = clk_set_rate(tegra->sata_oob_clk, TEGRA_SATA_OOB_CLOCK_FREQ_HZ);
	if (ret)
		goto disable_regulators;

	ret = clk_prepare_enable(tegra->sata_clk);
	if (ret)
		goto disable_regulators;

	ret = clk_prepare_enable(tegra->sata_oob_clk);
	if (ret)
		goto disable_sata_clk;

	ret = tegra_unpowergate_partition(TEGRA_POWERGATE_SATA);
	if (ret)
		goto disable_sata_oob_clk;

	ret = ahci_platform_enable_resources(hpriv);
	if (ret)
		goto disable_power;

	reset_control_deassert(tegra->sata_rst);
	reset_control_deassert(tegra->sata_cold_rst);

	return 0;

disable_power:
	tegra_powergate_partition(TEGRA_POWERGATE_SATA);
disable_sata_oob_clk:
	clk_disable_unprepare(tegra->sata_oob_clk);
disable_sata_clk:
	clk_disable_unprepare(tegra->sata_clk);
disable_regulators:
	regulator_bulk_disable(tegra->soc_data->num_sata_regulators,
			tegra->supplies);
	return ret;
}

static void tegra_ahci_power_off(struct ahci_host_priv *hpriv)
{
	struct tegra_ahci_priv *tegra = hpriv->plat_data;

	ahci_platform_disable_resources(hpriv);

	reset_control_assert(tegra->sata_rst);
	reset_control_assert(tegra->sata_cold_rst);

	clk_disable_unprepare(tegra->sata_clk);
	clk_disable_unprepare(tegra->sata_oob_clk);
	tegra_powergate_partition(TEGRA_POWERGATE_SATA);

	regulator_bulk_disable(tegra->soc_data->num_sata_regulators,
			tegra->supplies);

	if (tegra && tegra->prod_list) {
		tegra_prod_release(&tegra->prod_list);
		tegra->prod_list = NULL;
	}
}

static int tegra_ahci_controller_init(struct ahci_host_priv *hpriv)
{
	struct tegra_ahci_priv *tegra = hpriv->plat_data;
	unsigned int val;
	unsigned int mask;
	int ret = 0;

	/* Program the following  SATA IPFS registers
	 * to allow SW accesses to SATA.s MMIO Register */
	mask = FPCI_BAR5_START_MASK | FPCI_BAR5_ACCESS_TYPE;
	val = FPCI_BAR5_START | FPCI_BAR5_ACCESS_TYPE;
	tegra_ahci_sata_update(hpriv, val, mask, SATA_FPCI_BAR5_0);

	/* Program the following SATA IPFS register to enable the SATA */
	val = SATA_CONFIGURATION_0_EN_FPCI;
	tegra_ahci_sata_update(hpriv, val, val, SATA_CONFIGURATION_0);

	/* Electrical settings for better link stability */

	tegra_ahci_scfg_writel(hpriv, T_SATA_CHX_PHY_CTRL17_RX_EQ_CTRL_L_GEN1,
			T_SATA_CHX_PHY_CTRL17);
	tegra_ahci_scfg_writel(hpriv, T_SATA_CHX_PHY_CTRL18_RX_EQ_CTRL_L_GEN2,
			T_SATA_CHX_PHY_CTRL18);
	tegra_ahci_scfg_writel(hpriv, T_SATA_CHX_PHY_CTRL20_RX_EQ_CTRL_H_GEN1,
			T_SATA_CHX_PHY_CTRL20);
	tegra_ahci_scfg_writel(hpriv, T_SATA_CHX_PHY_CTRL21_RX_EQ_CTRL_H_GEN2,
			T_SATA_CHX_PHY_CTRL21);

	/* tegra ahci quirks */
	ret = tegra->soc_data->ops.tegra_ahci_quirks(hpriv);
	if (ret)
		return ret;

	/* Configure SATA Configuration registers*/

	/* Program the following SATA configuration registers
	 * to initialize SATA */
	val = (T_SATA_CFG_1_IO_SPACE | T_SATA_CFG_1_MEMORY_SPACE |
			T_SATA_CFG_1_BUS_MASTER | T_SATA_CFG_1_SERR);
	tegra_ahci_scfg_update(hpriv, val, val, T_SATA_CFG_1);
	tegra_ahci_scfg_writel(hpriv, T_SATA_CFG_9_BASE_ADDRESS, T_SATA_CFG_9);

	/* Program Class Code and Programming interface for SATA */
	val = T_SATA_CFG_SATA_BACKDOOR_PROG_IF_EN;
	tegra_ahci_scfg_update(hpriv, val, val, T_SATA_CFG_SATA);

	mask = T_SATA_BKDOOR_CC_CLASS_CODE_MASK | T_SATA_BKDOOR_CC_PROG_IF_MASK;
	val = T_SATA_BKDOOR_CC_CLASS_CODE | T_SATA_BKDOOR_CC_PROG_IF;
	tegra_ahci_scfg_update(hpriv, val, mask, T_SATA_BKDOOR_CC);

	mask = T_SATA_CFG_SATA_BACKDOOR_PROG_IF_EN;
	val = (u32) ~T_SATA_CFG_SATA_BACKDOOR_PROG_IF_EN;
	tegra_ahci_scfg_update(hpriv, val, mask, T_SATA_CFG_SATA);

	/* Enabling LPM capabilities through Backdoor Programming */

	val = (T_SATA0_AHCI_HBA_CAP_BKDR_PARTIAL_ST_CAP |
			T_SATA0_AHCI_HBA_CAP_BKDR_SLUMBER_ST_CAP |
			T_SATA0_AHCI_HBA_CAP_BKDR_SALP |
			T_SATA0_AHCI_HBA_CAP_BKDR_SUPP_PM);
	tegra_ahci_scfg_update(hpriv, val, val, T_SATA0_AHCI_HBA_CAP_BKDR);

	/* SATA Second Level Clock Gating configuration */
	/* Enabling Gating of Tx/Rx clocks and driving Pad IDDQ and Lane
	 * IDDQ Signals */
	mask = T_SATA0_CFG_35_IDP_INDEX_MASK;
	val = T_SATA0_CFG_35_IDP_INDEX;
	tegra_ahci_scfg_update(hpriv, val, mask, T_SATA0_CFG_35);
	tegra_ahci_scfg_writel(hpriv, T_SATA0_AHCI_IDP1_DATA,
			T_SATA0_AHCI_IDP1);
	val = (T_SATA0_CFG_PHY_1_PADS_IDDQ_EN |
			T_SATA0_CFG_PHY_1_PAD_PLL_IDDQ_EN);
	tegra_ahci_scfg_update(hpriv, val, val, T_SATA0_CFG_PHY_1);

	/*
	 *  Indicate Sata only has the capability to enter DevSleep
	 * from slumber link.
	 */
	tegra_ahci_aux_update(hpriv, DESO_SUPPORT, DESO_SUPPORT,
						SATA_AUX_MISC_CNTL_1_0);
	/* Enabling IPFS Clock Gating */
	mask = SATA_CONFIGURATION_CLK_OVERRIDE;
	val = (u32) ~SATA_CONFIGURATION_CLK_OVERRIDE;
	tegra_ahci_sata_update(hpriv, val, mask, SATA_CONFIGURATION_0);

	val = IP_INT_MASK;
	tegra_ahci_sata_update(hpriv, val, val, SATA_INTR_MASK_0);

	ret = tegra_ahci_disable_features(hpriv);

	return ret;
}

static void tegra_ahci_controller_deinit(struct ahci_host_priv *hpriv)
{
	struct tegra_ahci_priv *tegra = hpriv->plat_data;
	struct platform_device *pdev = tegra->pdev;
	struct device *dev = &pdev->dev;

	if (tegra_platform_is_silicon())
		tegra_ahci_runtime_suspend(dev);
	pm_runtime_disable(dev);
}

static void tegra_ahci_disable_devslp(struct ahci_host_priv *hpriv)
{
	u32 val = 0;
	u32 mask = SDS_SUPPORT;

	val = 0xFFFFFFFF & ~SDS_SUPPORT;
	tegra_ahci_aux_update(hpriv, val, mask, SATA_AUX_MISC_CNTL_1_0);
}

static void tegra_ahci_disable_hipm(struct ahci_host_priv *hpriv)
{
	u32 val = 0;
	u32 mask = T_SATA0_AHCI_HBA_CAP_BKDR_SALP;

	val = 0xFFFFFFFF & ~T_SATA0_AHCI_HBA_CAP_BKDR_SALP;
	tegra_ahci_scfg_update(hpriv, val, mask, T_SATA0_AHCI_HBA_CAP_BKDR);
}

static void tegra_ahci_disable_partial(struct ahci_host_priv *hpriv)
{
	u32 val = 0;
	u32 mask = T_SATA0_AHCI_HBA_CAP_BKDR_PARTIAL_ST_CAP;

	val = 0xFFFFFFFF & ~T_SATA0_AHCI_HBA_CAP_BKDR_PARTIAL_ST_CAP;
	tegra_ahci_scfg_update(hpriv, val, mask, T_SATA0_AHCI_HBA_CAP_BKDR);
}

static void tegra_ahci_disable_slumber(struct ahci_host_priv *hpriv)
{
	u32 val = 0;
	u32 mask = T_SATA0_AHCI_HBA_CAP_BKDR_SLUMBER_ST_CAP;

	val = 0xFFFFFFFF & ~T_SATA0_AHCI_HBA_CAP_BKDR_SLUMBER_ST_CAP;
	tegra_ahci_scfg_update(hpriv, val, mask, T_SATA0_AHCI_HBA_CAP_BKDR);
}

static void tegra_ahci_disable_ncq(struct ahci_host_priv *hpriv)
{
	u32 val = 0;
	u32 mask = T_SATA0_AHCI_HBA_CAP_BKDR_SNCQ;

	val = 0xFFFFFFFF & ~T_SATA0_AHCI_HBA_CAP_BKDR_SNCQ;
	tegra_ahci_scfg_update(hpriv, val, mask, T_SATA0_AHCI_HBA_CAP_BKDR);
}

static int tegra_ahci_disable_features(struct ahci_host_priv *hpriv)
{
	struct tegra_ahci_priv *tegra = hpriv->plat_data;
	struct platform_device *pdev = tegra->pdev;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct property *prop;
	const char *feature;
	bool devslp_enabled = true;
	int ret = 0;

	if (of_property_count_strings(np, "nvidia,disable-features") <= 0)
		return 0;

	of_property_for_each_string(np, "nvidia,disable-features", prop,
						feature) {
		if (!strcmp(feature, "devslp")) {
			tegra_ahci_disable_devslp(hpriv);
			devslp_enabled = false;
		} else if (!strcmp(feature, "hipm")) {
			tegra_ahci_disable_hipm(hpriv);
		} else if (!strcmp(feature, "ncq")) {
			tegra_ahci_disable_ncq(hpriv);
		} else if (!strcmp(feature, "dipm")) {
			ahci_tegra_port_info.flags |= ATA_FLAG_NO_DIPM;
		} else if (!strcmp(feature, "partial")) {
			tegra_ahci_disable_partial(hpriv);
		} else if (!strcmp(feature, "slumber")) {
			tegra_ahci_disable_slumber(hpriv);
		}
	}

	if (devslp_enabled) {
		if (gpio_is_valid(tegra->devslp_gpio)) {
			ret = gpio_request(tegra->devslp_gpio,
					"ahci-tegra");
			if (ret != 0) {
				dev_err(&pdev->dev,
						"gpio request failed\n");
				return ret;
			}
			gpio_direction_output(tegra->devslp_gpio, 1);
		}

		if (tegra->devslp_pin) {
			ret = pinctrl_select_state(tegra->devslp_pin,
					tegra->devslp_active);
			if (ret < 0) {
				dev_err(&pdev->dev,
						"setting devslp pin state failed\n");
				return ret;
			}
		}
	}
	return ret;
}

static int tegra_ahci_quirks(struct ahci_host_priv *hpriv)
{
	struct tegra_ahci_priv *tegra = hpriv->plat_data;
	struct platform_device *pdev = tegra->pdev;
	struct device *dev = &pdev->dev;
	unsigned int val;
	unsigned int mask;
	int ret = 0;

	/* SATA WARS */
	/* For SQUELCH Filter & Gen3 drive getting detected as Gen1 drive */

	mask = T_SATA_CFG_PHY_0_MASK_SQUELCH |
		T_SATA_CFG_PHY_0_USE_7BIT_ALIGN_DET_FOR_SPD;
	val = T_SATA_CFG_PHY_0_MASK_SQUELCH;
	val &= ~T_SATA_CFG_PHY_0_USE_7BIT_ALIGN_DET_FOR_SPD;
	tegra_ahci_scfg_update(hpriv, val, mask, T_SATA_CFG_PHY_0);

	mask = (T_SATA0_NVOOB_COMMA_CNT_MASK |
		T_SATA0_NVOOB_SQUELCH_FILTER_LENGTH_MASK |
		T_SATA0_NVOOB_SQUELCH_FILTER_MODE_MASK);
	val = (T_SATA0_NVOOB_COMMA_CNT | T_SATA0_NVOOB_SQUELCH_FILTER_LENGTH |
			T_SATA0_NVOOB_SQUELCH_FILTER_MODE);
	tegra_ahci_scfg_update(hpriv, val, mask, T_SATA0_NVOOB);

	/* Change CFG2NVOOB_2_COMWAKE_IDLE_CNT_LOW from 83.3 ns to 58.8ns */
	mask = T_SATA0_CFG2NVOOB_2_COMWAKE_IDLE_CNT_LOW_MASK;
	val = T_SATA0_CFG2NVOOB_2_COMWAKE_IDLE_CNT_LOW;
	tegra_ahci_scfg_update(hpriv, val, mask, T_SATA0_CFG2NVOOB_2);

	if (tegra && tegra->prod_list) {
		ret = tegra_prod_set_by_name(tegra->base_list, "prod",
							tegra->prod_list);
		if (ret)
			dev_err(dev, "Prod setting from DT failed\n");
	}

	return ret;
}

static int
tegra_ahci_platform_get_clks_resets(struct tegra_ahci_priv *tegra)
{
	struct platform_device *pdev = tegra->pdev;
	int ret = 0;

	tegra->sata_clk = devm_clk_get(&pdev->dev, "sata");
	if (IS_ERR(tegra->sata_clk)) {
		dev_err(&pdev->dev, "Failed to get sata clock\n");
		ret = PTR_ERR(tegra->sata_clk);
		goto err_out;
	}

	tegra->sata_oob_clk = devm_clk_get(&pdev->dev, "sata_oob");
	if (IS_ERR(tegra->sata_oob_clk)) {
		dev_err(&pdev->dev, "Failed to get sata_oob clock\n");
		ret = PTR_ERR(tegra->sata_oob_clk);
		goto err_out;
	}

	tegra->pllp_clk = devm_clk_get(&pdev->dev, "pllp");
	if (IS_ERR(tegra->pllp_clk)) {
		dev_err(&pdev->dev, "Failed to get pllp clock\n");
		ret = PTR_ERR(tegra->pllp_clk);
		goto err_out;
	}

	tegra->pllp_uphy_clk = devm_clk_get(&pdev->dev, "pllp_uphy");
	if (IS_ERR(tegra->pllp_uphy_clk)) {
		dev_err(&pdev->dev, "Failed to get pllp_uphy clock\n");
		ret = PTR_ERR(tegra->pllp_uphy_clk);
		goto err_out;
	}

	tegra->sata_rst = devm_reset_control_get(&pdev->dev, "sata");
	if (IS_ERR(tegra->sata_rst)) {
		dev_err(&pdev->dev, "Failed to get sata reset\n");
		ret = PTR_ERR(tegra->sata_rst);
		goto err_out;
	}

	tegra->sata_cold_rst = devm_reset_control_get(&pdev->dev, "satacold");
	if (IS_ERR(tegra->sata_cold_rst)) {
		dev_err(&pdev->dev, "Failed to get sata-cold reset\n");
		ret = PTR_ERR(tegra->sata_cold_rst);
		goto err_out;
	}

	return 0;
err_out:
	return ret;
}

static int
tegra_ahci_platform_get_memory_resources(struct tegra_ahci_priv *tegra)
{
	struct platform_device *pdev = tegra->pdev;
	struct resource *res;
	int ret = 0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	tegra->base_list[TEGRA_SATA_AHCI] = devm_ioremap_resource(&pdev->dev,
			res);
	if (IS_ERR(tegra->base_list[TEGRA_SATA_AHCI])) {
		ret = PTR_ERR(tegra->base_list[TEGRA_SATA_AHCI]);
		goto err_out;
	}
	tegra->res[TEGRA_SATA_AHCI] = res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	tegra->base_list[TEGRA_SATA_CONFIG] =
		devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(tegra->base_list[TEGRA_SATA_CONFIG])) {
		ret = PTR_ERR(tegra->base_list[TEGRA_SATA_CONFIG]);
		goto err_out;
	}
	tegra->res[TEGRA_SATA_CONFIG] = res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	tegra->base_list[TEGRA_SATA_IPFS] = devm_ioremap_resource(&pdev->dev,
			res);
	if (IS_ERR(tegra->base_list[TEGRA_SATA_IPFS])) {
		ret = PTR_ERR(tegra->base_list[TEGRA_SATA_IPFS]);
		goto err_out;
	}
	tegra->res[TEGRA_SATA_IPFS] = res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	tegra->base_list[TEGRA_SATA_AUX] = devm_ioremap_resource(&pdev->dev,
			res);
	if (IS_ERR(tegra->base_list[TEGRA_SATA_AUX])) {
		ret = PTR_ERR(tegra->base_list[TEGRA_SATA_AUX]);
		goto err_out;
	}
	tegra->res[TEGRA_SATA_AUX] = res;

	return 0;

err_out:
	return ret;
}

static int tegra_ahci_set_lpm(struct ahci_host_priv *hpriv)
{
	struct tegra_ahci_priv *tegra = hpriv->plat_data;
	struct platform_device *pdev = tegra->pdev;
	struct device *dev = &pdev->dev;
	struct ata_host *host = dev_get_drvdata(dev);
	struct device_node *np = dev->of_node;
	struct property *prop;
	const char *feature;
	enum ata_lpm_policy policy = ATA_LPM_MAX_POWER;
	int ret = 0;
	int i;

	if (of_property_count_strings(np, "nvidia,link-flags") <= 0)
		return 0;

	of_property_for_each_string(np, "nvidia,link-flags", prop,
			feature) {
		if (!strcmp(feature, "min_power"))
			policy = ATA_LPM_MIN_POWER;
		else if (!strcmp(feature, "max_power"))
			policy = ATA_LPM_MAX_POWER;
		else if (!strcmp(feature, "med_power"))
			policy = ATA_LPM_MED_POWER;
	}

	if (policy > ATA_LPM_MAX_POWER) {
		for (i = 0; i < host->n_ports; i++) {
			struct ata_port *ap = host->ports[i];

			ap->target_lpm_policy = policy;
			ata_port_schedule_eh(ap);
		}

	}
	return ret;
}


static	struct ahci_host_priv *
tegra_ahci_platform_get_resources(struct tegra_ahci_priv *tegra)
{
	struct ahci_host_priv *hpriv;
	struct platform_device *pdev = tegra->pdev;
	struct device *dev = &pdev->dev;
	struct phy *phy = NULL;
	int ret = -ENOMEM;
	int i;

	hpriv = devm_kzalloc(dev, sizeof(*hpriv), GFP_KERNEL);
	if (!hpriv) {
		ret = PTR_ERR(hpriv);
		goto err_out;
	}

	hpriv->plat_data = tegra;

	tegra->devslp_pin = devm_pinctrl_get(dev);
	 if (IS_ERR(tegra->devslp_pin)) {
		dev_warn(dev, "Missing devslp pinctrl\n");
		tegra->devslp_pin = NULL;
	}

	if (tegra->devslp_pin) {
		tegra->devslp_active = pinctrl_lookup_state(tegra->devslp_pin,
						"devslp_active");
		if (IS_ERR(tegra->devslp_active)) {
			dev_err(dev, "Missing devslp-active state\n");
			ret = PTR_ERR(tegra->devslp_active);
			goto err_out;
		}
		tegra->devslp_pullup = pinctrl_lookup_state(tegra->devslp_pin,
						"devslp_pullup");
		if (IS_ERR(tegra->devslp_pullup)) {
			dev_err(dev, "Missing devslp-pullup state\n");
			ret = PTR_ERR(tegra->devslp_pullup);
			goto err_out;
		}

		tegra->devslp_gpio =
				of_get_named_gpio(dev->of_node, "gpios", 0);
	}

	ret = tegra_ahci_platform_get_memory_resources(tegra);
	if (ret)
		goto err_out;
	else
		hpriv->mmio = tegra->base_list[TEGRA_SATA_AHCI];

	hpriv->target_pwr = devm_regulator_get_optional(dev, "target-3v3");
	if (IS_ERR(hpriv->target_pwr)) {
		ret = PTR_ERR(hpriv->target_pwr);
		if (ret == -EPROBE_DEFER)
			goto err_out;
		hpriv->target_pwr = NULL;
	}

	ret = tegra_ahci_platform_get_clks_resets(tegra);
	if (ret)
		goto err_out;

	if (tegra_platform_is_silicon()) {
		phy = devm_phy_optional_get(dev, "sata-phy");
		if (!IS_ERR(phy)) {
			hpriv->phys = devm_kzalloc(dev, sizeof(*hpriv->phys),
					GFP_KERNEL);
			if (!hpriv->phys) {
				ret = -ENOMEM;
				goto err_out;
			}
			hpriv->phys[0] = phy;
			hpriv->nports = 1;
		} else {
			ret = PTR_ERR(phy);
			goto err_out;
		}
	} else {
		hpriv->nports = 1;
	}

	tegra->supplies = devm_kzalloc(dev,
			sizeof(*tegra->supplies) *
			tegra->soc_data->num_sata_regulators,
			GFP_KERNEL);
	if (IS_ERR(tegra->supplies)) {
		dev_err(dev, "memory allocation failed for tegra supplies\n");
		ret = PTR_ERR(tegra->supplies);
		goto err_out;
	}

	for (i = 0; i < tegra->soc_data->num_sata_regulators; i++)
		tegra->supplies[i].supply =
			tegra->soc_data->sata_regulator_names[i];

	ret = devm_regulator_bulk_get(dev, tegra->soc_data->num_sata_regulators,
			tegra->supplies);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get regulators\n");
		goto err_out;
	}

	tegra->prod_list = tegra_prod_init(dev->of_node);
	if (IS_ERR(tegra->prod_list)) {
		dev_err(dev, "Prod Init failed\n");
		tegra->prod_list = NULL;
	}

#ifdef CONFIG_PM
	ret = tegra_ahci_pg_save_restore_init(hpriv);
	if (ret) {
		dev_err(&pdev->dev,
		"Failed to allocate memory for save and restore\n");
		goto err_out;
	}
#endif

	return hpriv;
err_out:
	return ERR_PTR(ret);
}

static void tegra_ahci_shutdown(struct platform_device *pdev)
{
	struct ata_host *host = platform_get_drvdata(pdev);
	struct ahci_host_priv *hpriv = host->private_data;
	struct tegra_ahci_priv *tegra = hpriv->plat_data;

	if (!pm_runtime_suspended(&pdev->dev))
		ata_platform_remove_one(pdev);

	if (tegra && tegra->prod_list) {
		tegra_prod_release(&tegra->prod_list);
		tegra->prod_list = NULL;
	}
}

/*
       This function is similar to ahci_enable_ahci which is
       defined in libahci.c.
       Since tegra_ahci_enable needs to be called before a
       host is allocated we will not be able to call ahci_enable_ahci
*/

static void tegra_ahci_enable(struct ahci_host_priv *hpriv)
{
	int i;
	u32 tmp;
	u32 val;
	u32 mask;

	/* turn on AHCI_EN */
	tmp = tegra_ahci_bar5_readl(hpriv, T_AHCI_HBA_GHC);
	if (tmp & HOST_AHCI_EN)
		return;

	/* Some controllers need AHCI_EN to be written multiple times.
	 * Try a few times before giving up.
	 */
	for (i = 0; i < 5; i++) {
		val = mask = T_AHCI_HBA_GHC_AE;
		tegra_ahci_bar5_update(hpriv, val, mask, T_AHCI_HBA_GHC);
		tmp = tegra_ahci_bar5_readl(hpriv, T_AHCI_HBA_GHC);
		if (tmp & T_AHCI_HBA_GHC_AE)
			return;
		msleep(10);
	}

	WARN_ON(1);
}

/*
       This function is similar to ahci_reset_controller which is
       defined in libahci.c.
       Since tegra_ahci_reset_controller needs to be called before a
       host is allocated we will not be able to call ahci_reset_controller
*/
static int tegra_ahci_reset_controller(struct ahci_host_priv *hpriv)
{
	struct tegra_ahci_priv *tegra = hpriv->plat_data;
	void __iomem *mmio = tegra->base_list[TEGRA_SATA_AHCI];
	struct platform_device *pdev = tegra->pdev;
	struct device *dev = &pdev->dev;
	u32 tmp;
	u32 val;
	u32 mask;

	/* we must be in AHCI mode, before using anything
	 * AHCI-specific, such as HOST_RESET.
	 */
	tegra_ahci_enable(hpriv);

	/* global controller reset */
	tmp = tegra_ahci_bar5_readl(hpriv, T_AHCI_HBA_GHC);
	if ((tmp & T_AHCI_HBA_GHC_HR) == 0) {
		val = mask = T_AHCI_HBA_GHC_HR;
		tegra_ahci_bar5_update(hpriv, val, mask, T_AHCI_HBA_GHC);

		tmp = tegra_ahci_bar5_readl(hpriv, T_AHCI_HBA_GHC);
	}

	/*
	 * to perform host reset, OS should set HOST_RESET
	 * and poll until this bit is read to be "0".
	 * reset must complete within 1 second, or
	 * the hardware should be considered fried.
	 */
	tmp = ata_wait_register(NULL, mmio + HOST_CTL, HOST_RESET,
			HOST_RESET, 10, 1000);

	if (tmp & T_AHCI_HBA_GHC_HR) {
		dev_err(dev, "controller reset failed (0x%x)\n",
				tmp);
		return -EIO;
	}
	mdelay(20);

	return 0;
}

static int tegra_ahci_probe(struct platform_device *pdev)
{
	struct ahci_host_priv *hpriv;
	struct tegra_ahci_priv *tegra;
	const struct of_device_id *match = NULL;
	int ret;

	tegra = devm_kzalloc(&pdev->dev, sizeof(*tegra), GFP_KERNEL);
	if (!tegra)
		return -ENOMEM;

	tegra->pdev = pdev;

	match = of_match_device(of_match_ptr(tegra_ahci_of_match),
			&pdev->dev);
	if (!match)
		return -ENODEV;

	tegra->soc_data =
		(struct tegra_ahci_soc_data *)match->data;

	hpriv = tegra->soc_data->ops.tegra_ahci_platform_get_resources(tegra);
	if (IS_ERR(hpriv))
		return PTR_ERR(hpriv);

	if (tegra_platform_is_silicon()) {
		ret = tegra->soc_data->ops.tegra_ahci_power_on(hpriv);
		if (ret)
			return ret;
	}

	ret = tegra_ahci_controller_init(hpriv);
	if (ret)
		goto poweroff_controller;

	ret = tegra_ahci_reset_controller(hpriv);
	if (ret)
		goto poweroff_controller;

	if (!tegra_ahci_bar5_readl(hpriv, T_AHCI_PORT_PXSSTS)) {
		struct platform_driver *drv =
			to_platform_driver(pdev->dev.driver);

		dev_info(&pdev->dev, "Drive not present\n");
		drv->driver.pm = NULL;
		ret = -ENODEV;
		goto poweroff_controller;
	}


	ret = ahci_platform_init_host(pdev, hpriv, &ahci_tegra_port_info);
	if (ret)
		goto poweroff_controller;

	ret = tegra_ahci_set_lpm(hpriv);
	if (ret) {
		dev_err(&pdev->dev,
		"Failed to set lpm\n");
		goto poweroff_controller;
	}

	ret = pm_runtime_set_active(&pdev->dev);
	if (ret) {
		dev_dbg(&pdev->dev, "unable to set runtime pm active err=%d\n",
								ret);
	} else {
		pm_runtime_set_autosuspend_delay(&pdev->dev,
				TEGRA_AHCI_DEFAULT_IDLE_TIME);
		pm_runtime_use_autosuspend(&pdev->dev);
		pm_suspend_ignore_children(&pdev->dev, true);
		pm_runtime_get_noresume(&pdev->dev);
		pm_runtime_enable(&pdev->dev);
	}

#ifdef CONFIG_DEBUG_FS
	tegra_ahci_dump_debuginit(hpriv);
#endif
	return 0;
poweroff_controller:
	if (tegra_platform_is_silicon())
		tegra->soc_data->ops.tegra_ahci_power_off(hpriv);

	return ret;
};

static struct platform_driver tegra_ahci_driver = {
	.probe = tegra_ahci_probe,
	.remove = ata_platform_remove_one,
	.shutdown = tegra_ahci_shutdown,
	.driver = {
		.name = "tegra-ahci",
		.of_match_table = tegra_ahci_of_match,
#ifdef CONFIG_PM
		.pm      = &tegra_ahci_dev_rt_ops,
#endif
	},
};
module_platform_driver(tegra_ahci_driver);

MODULE_DESCRIPTION("Tegra AHCI SATA driver");
MODULE_LICENSE("GPL v2");
