/* =========================================================================
 * The Synopsys DWC ETHER QOS Software Driver and documentation (hereinafter
 * "Software") is an unsupported proprietary work of Synopsys, Inc. unless
 * otherwise expressly agreed to in writing between Synopsys and you.
 *
 * The Software IS NOT an item of Licensed Software or Licensed Product under
 * any End User Software License Agreement or Agreement for Licensed Product
 * with Synopsys or any supplement thereto.  Permission is hereby granted,
 * free of charge, to any person obtaining a copy of this software annotated
 * with this license and the Software, to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * =========================================================================
 */
/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */
/*!@file: eqos_eee.c
 * @brief: Driver functions.
 */
#include "yheader.h"

void eqos_enable_eee_mode(struct eqos_prv_data *pdata)
{
	struct tx_ring *ptx_ring = NULL;
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	int tx_idle = 0, qinx;

	DBGPR_EEE("-->eqos_enable_eee_mode\n");

	for (qinx = 0; qinx < pdata->tx_queue_cnt; qinx++) {
		ptx_ring = GET_TX_WRAPPER_DESC(qinx);

		if ((ptx_ring->dirty_tx == ptx_ring->cur_tx) &&
		    (!pdata->tx_path_in_lpi_mode)) {
			tx_idle = 1;
		} else {
			tx_idle = 0;
			break;
		}
	}

	if (tx_idle)
		hw_if->set_eee_mode();

	DBGPR_EEE("<--eqos_enable_eee_mode\n");
}

void eqos_disable_eee_mode(struct eqos_prv_data *pdata)
{
	struct hw_if_struct *hw_if = &(pdata->hw_if);

	DBGPR_EEE("-->eqos_disable_eee_mode\n");

	hw_if->reset_eee_mode();
	del_timer_sync(&pdata->eee_ctrl_timer);
	pdata->tx_path_in_lpi_mode = false;

	DBGPR_EEE("-->eqos_disable_eee_mode\n");
}


/*!
* \brief API to control EEE mode.
*
* \details This function will move the MAC transmitter in LPI mode
* if there is no data transfer and MAC is not already in LPI state.
*
* \param[in] data - data hook
*
* \return void
*/

static void eqos_eee_ctrl_timer(unsigned long data)
{
	struct eqos_prv_data *pdata =
		(struct eqos_prv_data *)data;

	DBGPR_EEE("-->eqos_eee_ctrl_timer\n");

	eqos_enable_eee_mode(pdata);

	DBGPR_EEE("<--eqos_eee_ctrl_timer\n");
}


static void eqos_mmd_phy_indirect(struct mii_bus *bus,
					 int reg_addr,
					 int dev_addr,
					 int phy_addr)
{
	/* Write the desired MMD dev_addr */
	bus->write(bus, phy_addr, MMD_CTRL_REG, dev_addr);

	/* Write the desired MMD reg_addr */
	bus->write(bus, phy_addr, MMD_ADDR_DATA_REG, reg_addr);

	/* Select the Function : DATA with no post increment */
	bus->write(bus, phy_addr, MMD_CTRL_REG,
		(dev_addr | MMD_CTRL_FUNC_DATA_NOINCR));
}


/*!
* \brief API to read data from the MMD registers.
*
* \details This function will read data from the MMD(clause 45) registers
* using clause 22 registers. The procedure to read MMD registers is,
* 1. Write the desired MMD device addr into reg 13
* 2. Write the desired MMD reg addr into reg 14
* 3. Select the desired Function - MMD data command by writing in reg 13
* 4. Read the content of the MMD's selected reg through reg 14
*
* \param[in] bus - the target MII bus
* \param[in] reg_addr - desired MMD reg addr to be read
* \param[in] dev_addr - desired MMD address
* \param[in] phy_addr - PHY addr/id on the MII bus
*
* \return integer
*/
static int eqos_phy_read_mmd_indirect(struct mii_bus *bus,
					     int reg_addr,
					     int dev_addr,
					     int phy_addr)
{
	u32 ret;

	DBGPR_EEE("-->eqos_phy_read_mmd_indirect\n");

	eqos_mmd_phy_indirect(bus, reg_addr, dev_addr, phy_addr);
	/* read the content of the MMD's selected register */
	ret = bus->read(bus, phy_addr, MMD_ADDR_DATA_REG);

	DBGPR_EEE("<--eqos_phy_read_mmd_indirect\n");

	return ret;
}


/*!
* \brief API to write data into the MMD registers.
*
* \details This function will write data into MMD(clause 45) registers
* using clause 22 registers. The procedure to write MMD registers is,
* 1. Write the desired MMD device addr into reg 13
* 2. Write the desired MMD reg addr into reg 14
* 3. Select the desired Function - MMD data command by writing in reg 13
* 4. Write the data into MMD's selected reg through reg 14
*
* \param[in] bus - the target MII bus
* \param[in] reg_addr - desired MMD reg addr to be written
* \param[in] dev_addr - desired MMD address
* \param[in] phy_addr - PHY addr/id on the MII bus
* \param[in] data - data to write into the MMD register
*
* \return void
*/
static void eqos_phy_write_mmd_indirect(struct mii_bus *bus,
					     int reg_addr,
					     int dev_addr,
					     int phy_addr,
					     u32 data)
{
	DBGPR_EEE("-->eqos_phy_write_mmd_indirect\n");

	eqos_mmd_phy_indirect(bus, reg_addr, dev_addr, phy_addr);
	/* Write the data into MMD's selected register */
	bus->write(bus, phy_addr, MMD_ADDR_DATA_REG, data);

	DBGPR_EEE("<--eqos_phy_write_mmd_indirect\n");
}

#if 0

#define MDIO_EEE_100TX		0x0002	/* EEE is supported for 100BASE-TX */
#define MDIO_EEE_1000T		0x0004	/* EEE is supported for 1000BASE-T */
#define MDIO_EEE_10GT		0x0008	/* EEE is supported for 10GBASE-T */
#define MDIO_EEE_1000KX		0x0010	/* EEE is supported for 1000BASE-KX */
#define MDIO_EEE_10GKX4		0x0020	/* EEE is supported for 10GBASE-KX4 */
#define MDIO_EEE_10GKR		0x0040	/* EEE is supported for 10GBASE KR */

 /* A small helper function that translates MMD EEE Capability (3.20) bits
 * to ethtool supported settings.
 */
static u32 eqos_mmd_eee_cap_to_ethtool_sup_t(u16 eee_cap)
{
	u32 supported = 0;

	if (eee_cap & MDIO_EEE_100TX)
		supported |= SUPPORTED_100baseT_Full;
	if (eee_cap & MDIO_EEE_1000T)
		supported |= SUPPORTED_1000baseT_Full;
	if (eee_cap & MDIO_EEE_10GT)
		supported |= SUPPORTED_10000baseT_Full;
	if (eee_cap & MDIO_EEE_1000KX)
		supported |= SUPPORTED_1000baseKX_Full;
	if (eee_cap & MDIO_EEE_10GKX4)
		supported |= SUPPORTED_10000baseKX4_Full;
	if (eee_cap & MDIO_EEE_10GKR)
		supported |= SUPPORTED_10000baseKR_Full;

	return supported;
}

 /* A small helper function that translates the MMD EEE Advertisment (7.60)
  * and MMD EEE Link Partner Ability (7.61) bits to ethtool advertisement
  * settings.
  */
static inline u32 eqos_mmd_eee_adv_to_ethtool_adv_t(u16 eee_adv)
{
	u32 adv = 0;

	if (eee_adv & MDIO_EEE_100TX)
		adv |= ADVERTISED_100baseT_Full;
	if (eee_adv & MDIO_EEE_1000T)
		adv |= ADVERTISED_1000baseT_Full;
	if (eee_adv & MDIO_EEE_10GT)
		adv |= ADVERTISED_10000baseT_Full;
	if (eee_adv & MDIO_EEE_1000KX)
		adv |= ADVERTISED_1000baseKX_Full;
	if (eee_adv & MDIO_EEE_10GKX4)
		adv |= ADVERTISED_10000baseKX4_Full;
	if (eee_adv & MDIO_EEE_10GKR)
		adv |= ADVERTISED_10000baseKR_Full;

	return adv;
}
#endif

/*!
* \brief API to initialize and check EEE mode.
*
* \details This function checks if the EEE is supported by
* looking at the MMD registes and it also programs the MMD
* register 3.0 setting the "Clock stop enable" bit if required.
*
* \param[in] phydev - pointer to target phy_device structure
* \param[in] clk_stop_enable - PHY may stop the clock during LPI
*
* \return integer
*
* \retval zero if EEE is supported else return -ve number.
*/
static int eqos_phy_init_eee(struct phy_device *phydev,
		bool clk_stop_enable)
{
	int ret = -EPROTONOSUPPORT;

	DBGPR_EEE("-->eqos_phy_init_eee\n");

	/* According to 802.3az,the EEE is supported only in full duplex-mode.
	 * Also EEE feature is active when core is operating with MII, GMII,
	 * SGMII or RGMII.
	 */
	if ((phydev->duplex == DUPLEX_FULL) &&
	    ((phydev->interface == PHY_INTERFACE_MODE_MII) ||
	    (phydev->interface == PHY_INTERFACE_MODE_GMII) ||
	    (phydev->interface == PHY_INTERFACE_MODE_SGMII) ||
	    (phydev->interface == PHY_INTERFACE_MODE_RGMII))) {
		int eee_lp, eee_cap, eee_adv;
		int status;

		/* Read phy status to properly get the right settings */
		status = phy_read_status(phydev);
		if (status)
			return status;

		/* First check if the EEE ability is supported */
		eee_cap = eqos_phy_read_mmd_indirect(phydev->bus,
				CL45_PCS_EEE_ABLE, MDIO_MMD_PCS, phydev->addr);
		if (eee_cap < 0)
			return eee_cap;
/*
		cap = eqos_mmd_eee_cap_to_ethtool_sup_t(eee_cap);
		if (!cap)
			goto eee_exit;
*/
		/* check whether link Partner support EEE or not */
		eee_lp = eqos_phy_read_mmd_indirect(phydev->bus,
			CL45_AN_EEE_LPABLE_REG, MDIO_MMD_AN, phydev->addr);
		if (eee_lp < 0)
			return eee_lp;

		eee_adv = eqos_phy_read_mmd_indirect(phydev->bus,
				CL45_ADV_EEE_REG, MDIO_MMD_AN, phydev->addr);
		if (eee_adv < 0)
			return eee_adv;
/*
		//TODO:check this
		adv = eqos_mmd_eee_adv_to_ethtool_adv_t(eee_adv);
		lp = eqos_mmd_eee_adv_to_ethtool_adv_t(eee_lp);
		idx = phy_find_setting(phydev->speed, phydev->duplex);
		if ((lp & adv & settings[idx].setting))
			goto eee_exit;
*/
		if (clk_stop_enable) {
			/* Configure the PHY to stop receiving xMII
			 * clock while it is signaling LPI.
			 */
			int val = eqos_phy_read_mmd_indirect(phydev->bus,
					CL45_CLK_STOP_EN_REG, MDIO_MMD_PCS,
					phydev->addr);
			if (val < 0)
				return val;

			val |= CL45_CLK_STOP_EN;
			eqos_phy_write_mmd_indirect(phydev->bus,
					CL45_CLK_STOP_EN_REG, MDIO_MMD_PCS,
					phydev->addr, val);
		}

		ret = 0; /* EEE supported */
	}

	DBGPR_EEE("<--eqos_phy_init_eee\n");

/* eee_exit: */
	return ret;
}


/*!
* \brief API to initialize EEE mode.
*
* \details This function enables the LPI state and start the timer
* to verify whether the tx path can enter in LPI state if
* a. GMAC supports EEE mode &
* b. phy can also manage EEE.
*
* \param[in] pdata - pointer to private data structure
*
* \return bool
*
* \retval true on success & false on failure.
*/
bool eqos_eee_init(struct eqos_prv_data *pdata)
{
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	bool ret = false;

	DBGPR_EEE("-->eqos_eee_init\n");

	/* HW supports the EEE feature */
	if (pdata->hw_feat.eee_sel) {
		/* check if the PHY supports EEE */
		if (eqos_phy_init_eee(pdata->phydev, 1))
			goto phy_eee_failed;

		if (!pdata->eee_active) {
			pdata->eee_active = 1;
			init_timer(&pdata->eee_ctrl_timer);
			pdata->eee_ctrl_timer.function = eqos_eee_ctrl_timer;
			pdata->eee_ctrl_timer.data = (unsigned long)pdata;
			pdata->eee_ctrl_timer.expires =
				EQOS_LPI_TIMER(EQOS_DEFAULT_LPI_TIMER);
			add_timer(&pdata->eee_ctrl_timer);

			hw_if->set_eee_timer(EQOS_DEFAULT_LPI_LS_TIMER,
				EQOS_DEFAULT_LPI_TWT_TIMER);
			if (pdata->use_lpi_tx_automate)
				hw_if->set_lpi_tx_automate();
		} else {
			/* When EEE has been already initialized we have to
			 * modify the PLS bit in MAC_LPI_Control_Status reg
			 * according to PHY link status.
			 */
			hw_if->set_eee_pls(pdata->phydev->link);
		}

		DBGPR_EEE("EEE initialized\n");

		ret = true;
	}

	DBGPR_EEE("<--eqos_eee_init\n");

 phy_eee_failed:

	return ret;
}

#define MAC_LPS_TLPIEN 0x00000001
#define MAC_LPS_TLPIEX 0x00000002
#define MAC_LPS_RLPIEN 0x00000004
#define MAC_LPS_RLPIEX 0x00000008
void eqos_handle_eee_interrupt(struct eqos_prv_data *pdata)
{
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	u32 lpi_status;

	DBGPR_EEE("-->eqos_handle_eee_interrupt\n");

	lpi_status = hw_if->get_lpi_status();
	DBGPR_EEE("MAC_LPI_Control_Status = %#x\n", lpi_status);

	if (lpi_status & MAC_LPS_TLPIEN) {
		pdata->tx_path_in_lpi_mode = 1;
		pdata->xstats.tx_path_in_lpi_mode_irq_n++;
		DBGPR_EEE("MAC Transmitter has entered the LPI state\n");
	}

	if (lpi_status & MAC_LPS_TLPIEX) {
		pdata->tx_path_in_lpi_mode = 0;
		pdata->xstats.tx_path_exit_lpi_mode_irq_n++;
		DBGPR_EEE("MAC Transmitter has exited the LPI state\n");
	}

	if (lpi_status & MAC_LPS_RLPIEN) {
		pdata->xstats.rx_path_in_lpi_mode_irq_n++;
		DBGPR_EEE("MAC Receiver has entered the LPI state\n");
	}

	if (lpi_status & MAC_LPS_RLPIEX) {
		pdata->xstats.rx_path_exit_lpi_mode_irq_n++;
		DBGPR_EEE("MAC Receiver has exited the LPI state\n");
	}

	DBGPR_EEE("<--eqos_handle_eee_interrupt\n");
}
