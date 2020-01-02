/*
 * DWC Ether QOS controller driver for Samsung TRAV SoCs
 *
 * Copyright (C) Synopsys India Pvt. Ltd.
 * Copyright (C) 2017 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Pankaj Dubey <pankaj.dubey@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "dwc_eth_qos_yheader.h"

/* Read MII registers through the API read_phy_reg where the
 * related MAC registers can be configured.
 * Return 0 if successful.
 */

int dwc_eqos_mdio_read_direct(struct dwc_eqos_prv_data *pdata,
			      int phyaddr, int phyreg, int *phydata)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;
	int phy_reg_read_status;

	DBGPR_MDIO("--> dwc_eqos_mdio_read_direct\n");

	if (hw_if->read_phy_regs) {
		phy_reg_read_status =
			hw_if->read_phy_regs(pdata, phyaddr, phyreg, phydata);
	} else {
		phy_reg_read_status = 1;
		netdev_alert(pdata->ndev, "hw_if->read_phy_regs not defined\n");
	}

	DBGPR_MDIO("<-- dwc_eqos_mdio_read_direct\n");

	return phy_reg_read_status;
}

/* Writes MII registers through the API write_phy_reg where the
 * related MAC registers can be configured.
 * Return 0 if succcessful.
 */

int dwc_eqos_mdio_write_direct(struct dwc_eqos_prv_data *pdata,
			       int phyaddr, int phyreg, int phydata)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;
	int phy_reg_write_status;

	DBGPR_MDIO("--> dwc_eqos_mdio_write_direct\n");

	if (hw_if->write_phy_regs) {
		phy_reg_write_status =
			hw_if->write_phy_regs(pdata, phyaddr, phyreg, phydata);
	} else {
		phy_reg_write_status = 1;
		netdev_alert(pdata->ndev, "write_phy_regs not defined\n");
	}

	DBGPR_MDIO("<-- dwc_eqos_mdio_write_direct\n");

	return phy_reg_write_status;
}

/* Read MII registers through the API read_phy_reg where the
 * related MAC registers can be configured.
 * Return  value read from given phy register.
 */

static int dwc_eqos_mdio_read(struct mii_bus *bus, int phyaddr, int phyreg)
{
	struct net_device *dev = bus->priv;
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	int phydata;

	DBGPR_MDIO("--> dwc_eqos_mdio_read: phyaddr = %d, phyreg = %d\n",
		   phyaddr, phyreg);

	if (hw_if->read_phy_regs)
		hw_if->read_phy_regs(pdata, phyaddr, phyreg, &phydata);
	else
		netdev_alert(dev, "read_phy_regs not defined\n");

	DBGPR_MDIO("<-- dwc_eqos_mdio_read: phydata = %#x\n", phydata);

	return phydata;
}

/* This API is expected to write MII registers with the value being
 * passed as the last argument which is done in write_phy_regs API
 * called by this function.
 *
 * Return 0 on success and -ve number on failure.
 */

static int dwc_eqos_mdio_write(struct mii_bus *bus, int phyaddr, int phyreg,
			       u16 phydata)
{
	struct net_device *dev = bus->priv;
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	int ret = Y_SUCCESS;

	DBGPR_MDIO("--> dwc_eqos_mdio_write\n");

	if (hw_if->write_phy_regs) {
		hw_if->write_phy_regs(pdata, phyaddr, phyreg, phydata);
	} else {
		ret = -1;
		netdev_alert(dev, "hw_if->write_phy_regs not defined\n");
	}

	DBGPR_MDIO("<-- dwc_eqos_mdio_write\n");

	return ret;
}

/* This API is issue soft reset to PHY core and waits
 * until soft reset completes.
 *
 * Return 0 on success and -ve number on failure.
 */

static int dwc_eqos_mdio_reset(struct mii_bus *bus)
{
	struct net_device *dev = bus->priv;
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	int phydata;

	DBGPR_MDIO("-->dwc_eqos_mdio_reset: phyaddr : %d\n", pdata->phyaddr);

#if 0 //def DWC_EQOS_CONFIG_PGTEST
	pr_alert("PHY Programming for Autoneg disable\n");
	hw_if->read_phy_regs(pdata->phyaddr, MII_BMCR, &phydata);
	phydata &= ~(1 << 12);
	hw_if->write_phy_regs(pdata->phyaddr, MII_BMCR, phydata);
#endif

	hw_if->read_phy_regs(pdata, pdata->phyaddr, MII_BMCR, &phydata);

	if (phydata < 0)
		return 0;

	/* issue soft reset to PHY */
	phydata |= BMCR_RESET;
	hw_if->write_phy_regs(pdata, pdata->phyaddr, MII_BMCR, phydata);

	/* wait until software reset completes */
	do {
		hw_if->read_phy_regs(pdata, pdata->phyaddr, MII_BMCR, &phydata);
	} while ((phydata >= 0) && (phydata & BMCR_RESET));

#if 0 //def DWC_EQOS_CONFIG_PGTEST
	pr_alert("PHY Programming for Loopback\n");
	hw_if->read_phy_regs(pdata->phyaddr, MII_BMCR, &phydata);
	phydata |= (1 << 14);
	hw_if->write_phy_regs(pdata->phyaddr, MII_BMCR, phydata);
#endif

	DBGPR_MDIO("<--dwc_eqos_mdio_reset\n");

	return 0;
}

/* This function is invoked by other functions to get the PHY register
 * dump. This function is used during development phase for debug purpose.
 *
 */

void dump_phy_registers(struct dwc_eqos_prv_data *pdata)
{
	int phydata = 0;

	pr_alert("\n************* PHY Reg dump *************************\n");
	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_BMCR, &phydata);
	pr_alert("Phy Control Reg(Basic Mode Control Reg) (%#x) = %#x\n",
		 MII_BMCR, phydata);

	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_BMSR, &phydata);
	pr_alert("Phy Status Reg(Basic Mode Status Reg) (%#x) = %#x\n",
		 MII_BMSR, phydata);

	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_PHYSID1,
				  &phydata);
	pr_alert("Phy Id (PHYS ID 1) (%#x)= %#x\n", MII_PHYSID1,
		 phydata);

	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_PHYSID2,
				  &phydata);
	pr_alert("Phy Id (PHYS ID 2) (%#x)= %#x\n", MII_PHYSID2,
		 phydata);

	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_ADVERTISE,
				  &phydata);
	pr_alert("Auto-nego Adv (Advertisement Control Reg) (%#x) = %#x\n",
		 MII_ADVERTISE, phydata);

	/* read Phy Control Reg */
	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_LPA,
				  &phydata);
	pr_alert("Auto-nego Lap (Link Partner Ability Reg) (%#x)= %#x\n",
		 MII_LPA, phydata);

	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_EXPANSION,
				  &phydata);
	pr_alert("Auto-nego Exp (Extension Reg) (%#x) = %#x\n",
		 MII_EXPANSION, phydata);

	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr,
				  DWC_EQOS_AUTO_NEGO_NP, &phydata);
	pr_alert("Auto-nego Np (%#x) = %#x\n",
		 DWC_EQOS_AUTO_NEGO_NP, phydata);

	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_ESTATUS,
				  &phydata);
	pr_alert("Extended Status Reg (%#x) = %#x\n", MII_ESTATUS,
		 phydata);

	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_CTRL1000,
				  &phydata);
	pr_alert("1000 Ctl Reg (1000BASE-T Control Reg) (%#x) = %#x\n",
		 MII_CTRL1000, phydata);

	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_STAT1000,
				  &phydata);
	pr_alert("1000 Sts Reg (1000BASE-T Status)(%#x) = %#x\n",
		 MII_STAT1000, phydata);

	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, DWC_EQOS_PHY_CTL,
				  &phydata);
	pr_alert("PHY Ctl Reg (%#x) = %#x\n", DWC_EQOS_PHY_CTL, phydata);

	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr,
				  DWC_EQOS_PHY_STS, &phydata);
	pr_alert("PHY Sts Reg (%#x) = %#x\n", DWC_EQOS_PHY_STS, phydata);

	pr_alert("\n****************************************************\n");
}

/* This function will be called by PAL to inform the driver
 * about various link parameters like duplex and speed. This function
 * will configure the MAC based on link parameters.
 *
 */

static void dwc_eqos_adjust_link(struct net_device *dev)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct phy_device *phydev = pdata->phydev;
	unsigned long flags;
	int new_state = 0;

	if (!phydev)
		return;

	DBGPR_MDIO("-->dwc_eqos_adjust_link. link %d\n", phydev->link);

	spin_lock_irqsave(&pdata->lock, flags);

	if (phydev->link) {
		/* Now we make sure that we can be in full duplex mode.
		 * If not, we operate in half-duplex mode
		 */
		if (phydev->duplex != pdata->oldduplex) {
			new_state = 1;
			if (phydev->duplex)
				hw_if->set_full_duplex(dev);
			else
				hw_if->set_half_duplex(dev);

			pdata->oldduplex = phydev->duplex;
		}

		/* FLOW ctrl operation */
		if (phydev->pause || phydev->asym_pause) {
			if (pdata->flow_ctrl != pdata->oldflow_ctrl)
				dwc_eqos_configure_flow_ctrl(pdata);
		}

		if (phydev->speed != pdata->speed) {
			new_state = 1;
			switch (phydev->speed) {
			case SPEED_1000:
				hw_if->set_gmii_speed(dev);
				break;
			case SPEED_100:
				hw_if->set_mii_speed_100(dev);
				break;
			case SPEED_10:
				hw_if->set_mii_speed_10(dev);
				break;
			}
			pdata->speed = phydev->speed;
		}

		if (!pdata->oldlink) {
			new_state = 1;
			pdata->oldlink = 1;
		}
	} else if (pdata->oldlink) {
		new_state = 1;
		pdata->oldlink = 0;
		pdata->speed = 0;
		pdata->oldduplex = -1;
	}

	if (new_state)
		phy_print_status(phydev);

	spin_unlock_irqrestore(&pdata->lock, flags);

	DBGPR_MDIO("<--dwc_eqos_adjust_link\n");
}

/* This function will initializes the driver's PHY state and attaches
 * the PHY to the MAC driver.
 *
 * Return 0 on success & negative number on failure.
 */

static int dwc_eqos_init_phy(struct net_device *dev)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	struct phy_device *phydev = NULL;
	char phy_id_fmt[MII_BUS_ID_SIZE + 3];
	char bus_id[MII_BUS_ID_SIZE];

	DBGPR_MDIO("-->dwc_eqos_init_phy\n");

	pdata->oldlink = 0;
	pdata->speed = 0;
	pdata->oldduplex = -1;

	snprintf(bus_id, MII_BUS_ID_SIZE, "dwc_phy-%x", pdata->if_idx);

	snprintf(phy_id_fmt, MII_BUS_ID_SIZE + 3, PHY_ID_FMT, bus_id,
		 pdata->phyaddr);

	DBGPR_MDIO("trying to attach to %s\n", phy_id_fmt);

	phydev = phy_connect(dev, phy_id_fmt, &dwc_eqos_adjust_link,
			     pdata->interface);
	if (IS_ERR(phydev)) {
		pr_alert("%s: Could not attach to PHY\n", dev->name);
		return PTR_ERR(phydev);
	}

	if (phydev->phy_id == 0) {
		phy_disconnect(phydev);
		return -ENODEV;
	}

	if (pdata->interface == PHY_INTERFACE_MODE_GMII)
		phydev->supported = PHY_GBIT_FEATURES;
	else if ((pdata->interface == PHY_INTERFACE_MODE_MII) ||
		 (pdata->interface == PHY_INTERFACE_MODE_RMII))
		phydev->supported = PHY_BASIC_FEATURES;


	phydev->supported |= (SUPPORTED_Pause | SUPPORTED_Asym_Pause);

	phydev->advertising = phydev->supported;

	DBGPR_MDIO("%s: attached to PHY (UID 0x%x) Link = %d\n", dev->name,
		   phydev->phy_id, phydev->link);

	pdata->phydev = phydev;
	phy_start(pdata->phydev);

	DBGPR_MDIO("<--dwc_eqos_init_phy\n");

	return 0;
}

int dwc_eqos_fixed_phy_register(struct net_device *dev)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	struct device_node *np = pdata->pdev->dev.of_node;
	struct device_node *phy_node;
	struct phy_device *phydev;

	if (!np || (of_phy_register_fixed_link(np) < 0))
		return -ENODEV;

	phy_node = of_node_get(np);

	phydev = of_phy_connect(pdata->ndev, phy_node, &dwc_eqos_adjust_link, 0,
							pdata->interface);
	if (!phydev)
		return -ENODEV;

	pdata->phydev = phydev;

	return 0;
}

/* This function will allocate mdio bus and register it
 * phy layer.
 *
 * Return 0 on success and -ve on failure.
 */

int dwc_eqos_mdio_register(struct net_device *dev)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	struct mii_bus *new_bus = NULL;
	int phyaddr = 0;
	unsigned short phy_detected = 0;
	int ret = Y_SUCCESS;

	DBGPR_MDIO("-->dwc_eqos_mdio_register\n");

	/* find the phy ID or phy address which is connected to our MAC */
	for (phyaddr = 0; phyaddr < 32; phyaddr++) {
		int phy_reg_read_status, mii_status;

		phy_reg_read_status =
			dwc_eqos_mdio_read_direct(pdata, phyaddr, MII_BMSR,
						  &mii_status);
		if (phy_reg_read_status == 0) {
			if (mii_status != 0x0000 && mii_status != 0xffff) {
				netdev_alert(dev, " Phy detected at ID/ADDR %d\n",
					     phyaddr);
				phy_detected = 1;
				break;
			}
		} else if (phy_reg_read_status < 0) {
			netdev_alert(dev, "MII_BMSR for phy ID/ADDR %d\n",
				     phyaddr);
		}
	}
	if (!phy_detected) {
		pr_alert("%s: No phy could be detected\n", DEV_NAME);
		return -ENOLINK;
	}
	pdata->phyaddr = phyaddr;
	pdata->bus_id = 0x1;

	DBGPHY_REGS(pdata);

	new_bus = mdiobus_alloc();
	if (!new_bus) {
		pr_alert("Unable to allocate mdio bus\n");
		return -ENOMEM;
	}

	new_bus->name = "dwc_phy";
	new_bus->read = dwc_eqos_mdio_read;
	new_bus->write = dwc_eqos_mdio_write;
	new_bus->reset = dwc_eqos_mdio_reset;
	snprintf(new_bus->id, MII_BUS_ID_SIZE, "%s-%x",
		 new_bus->name, pdata->if_idx);
	new_bus->priv = dev;
	new_bus->phy_mask = 0;
	new_bus->parent = pdata->ndev->dev.parent;
	ret = mdiobus_register(new_bus);
	if (ret != 0) {
		pr_alert("%s: Cannot register as MDIO bus\n", new_bus->name);
		mdiobus_free(new_bus);
		return ret;
	}
	pdata->mii = new_bus;

	ret = dwc_eqos_init_phy(dev);
	if (unlikely(ret)) {
		pr_alert("Cannot attach to PHY (error: %d)\n", ret);
		goto err_out_phy_connect;
	}

	DBGPR_MDIO("<--dwc_eqos_mdio_register\n");

	return ret;

err_out_phy_connect:
	dwc_eqos_mdio_unregister(dev);
	return ret;
}

/* This function will unregister mdio bus and free's the memory
 * allocated to it.
 */

void dwc_eqos_mdio_unregister(struct net_device *dev)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);

	DBGPR_MDIO("-->dwc_eqos_mdio_unregister\n");

	if (pdata->phydev) {
		phy_stop(pdata->phydev);
		phy_disconnect(pdata->phydev);
		pdata->phydev = NULL;
	}

	mdiobus_unregister(pdata->mii);
	pdata->mii->priv = NULL;
	mdiobus_free(pdata->mii);
	pdata->mii = NULL;

	DBGPR_MDIO("<--dwc_eqos_mdio_unregister\n");
}
