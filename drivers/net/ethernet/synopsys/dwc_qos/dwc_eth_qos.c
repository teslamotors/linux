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

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/regmap.h>
#include <linux/netdevice.h>
#include <linux/mfd/syscon.h>
#include <linux/etherdevice.h>
#include <linux/platform_device.h>
#include "dwc_eth_qos_yheader.h"

#define DRIVER_NAME "eth"
//#define EMULATOR_PHY_DISABLE
#define MAX_ETH_IDX 2
static unsigned char dev_addr[6] = {0, 0x55, 0x7b, 0xb5, 0x7d, 0xf7};

void __iomem *dwc_eqos_base_addr;

void dwc_eqos_init_all_fptrs(struct dwc_eqos_prv_data *pdata)
{
	dwc_eqos_init_function_ptrs_dev(&pdata->hw_if);
	dwc_eqos_init_function_ptrs_desc(&pdata->desc_if);
}

static int dwc_eqos_rxclk_mux_init(struct dwc_eqos_prv_data *pdata)
{
	struct platform_device *pdev = pdata->pdev;
	struct device_node *np = pdev->dev.of_node;

	pdata->rxclk_mux = of_clk_get_by_name(np, "eqos_rxclk_mux");
	if (IS_ERR(pdata->rxclk_mux)) {
		pdata->rxclk_mux = NULL;
		dev_dbg(&pdev->dev, "eqos_rxclk_mux not supported");
		return 0;
	}

	pdata->rxclk_external = of_clk_get_by_name(np, "eqos_phyrxclk");
	if (IS_ERR(pdata->rxclk_external)) {
		dev_dbg(&pdev->dev, "failed to get eqos_phyrxclk");
		return PTR_ERR(pdata->rxclk_external);
	}

	pdata->rxclk_internal = of_clk_get_by_name(np, "dout_peric_rgmii_clk");
	if (IS_ERR(pdata->rxclk_internal)) {
		dev_dbg(&pdev->dev, "failed to get dout_peric_rgmii_clk");
		return PTR_ERR(pdata->rxclk_internal);
	}

	return 0;
}

static int dwc_eqos_probe(struct platform_device *pdev)
{
	struct dwc_eqos_prv_data *pdata = NULL;
	struct resource *r_mem = NULL;
	struct net_device *ndev = NULL;
	//struct clk *pclk = NULL;
	int eth_idx = 0;
	int i, ret = 0;
	struct hw_if_struct *hw_if = NULL;
	struct desc_if_struct *desc_if = NULL;
	unsigned char tx_q_count = 0, rx_q_count = 0;

	DBGPR("--> dwc_eqos_probe\n");

	if(pdev->dev.of_node) {
		eth_idx = of_alias_get_id(pdev->dev.of_node, "eth");
		if (eth_idx < 0 || eth_idx >= MAX_ETH_IDX)
		{
			printk(KERN_ALERT "\nerror eth_idx = %d\n", eth_idx);
			return -EINVAL;
		}
	}

	r_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r_mem) {
		dev_err(&pdev->dev, "no IO resource defined.\n");
		return -ENXIO;
	}

	dwc_eqos_base_addr = devm_ioremap_resource(&pdev->dev, r_mem);
	if (IS_ERR(dwc_eqos_base_addr)) {
		dev_err(&pdev->dev, "failed to map baseaddress.\n");
		ret = PTR_ERR(dwc_eqos_base_addr);
		return ret;
	}

	/* Restrict the number of tx descriptor per packet to 1.
	 * Driver at present have bugs handling multiple tx descriptors,
	 * which needs to be fixed before we can enable this capability.
	 * For jumbo frames, this would mean we use up both the
	 * buffers in the descriptor by forcing the buffer address
	 * to 32-bit address.
	 */
	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (ret) {
		dev_err(&pdev->dev, "could not set DMA mask\n");
		return ret;
	}

	DBGPR("dwc_eqos_base_addr = %#lx\n", dwc_eqos_base_addr);

	#if 0
	pclk = devm_clk_get(&pdev->dev, "apb_pclk");
	if (IS_ERR(pclk)) {
		dev_err(&pdev->dev, "apb_pclk clock not found.\n");
		ret = PTR_ERR(pclk);
		return ret;
	}

	ret = clk_prepare_enable(pclk);
	if (ret) {
		dev_err(&pdev->dev, "Unable to enable APER clock.\n");
		return ret;
	}
	#endif

	/* queue count */
	tx_q_count = get_tx_queue_count(dwc_eqos_base_addr);
	rx_q_count = get_rx_queue_count(dwc_eqos_base_addr);

	/*
	 * For now, read the tx and rx queue from the dt file for A0 and B0 chip.
	 */
	if (pdev->dev.of_node) {
		u32 limit_txq, limit_rxq;

		if (!of_property_read_u32(pdev->dev.of_node, "num-txq", &limit_txq) &&
			limit_txq < tx_q_count)
			tx_q_count = limit_txq;

		if (!of_property_read_u32(pdev->dev.of_node, "num-rxq", &limit_rxq) &&
			limit_rxq < rx_q_count)
			rx_q_count = limit_rxq;
	}

	pr_info("txq = %d rxq = %d\n", tx_q_count, rx_q_count);

	ndev = alloc_etherdev_mqs(
			sizeof(struct dwc_eqos_prv_data), tx_q_count, rx_q_count);
	if (!ndev) {
		pr_alert("%s:Unable to alloc new net device\n", DEV_NAME);
		ret = -ENOMEM;
		#if 0
		goto err_out_clk_dis_aper;
		#endif
	}
	ndev->dev_addr[0] = dev_addr[0];
	ndev->dev_addr[1] = dev_addr[1];
	ndev->dev_addr[2] = dev_addr[2];
	ndev->dev_addr[3] = dev_addr[3];
	ndev->dev_addr[4] = dev_addr[4];
	ndev->dev_addr[5] = dev_addr[5] + eth_idx;

	if (pdev->dev.of_node) {
		const unsigned char *addr;
		int len;

		addr = of_get_property(pdev->dev.of_node, "local-mac-address", &len);

		if (addr && len == ETH_ALEN)
			memcpy(ndev->dev_addr, addr, ETH_ALEN);
	}

	ndev->base_addr = (unsigned long)dwc_eqos_base_addr;
	SET_NETDEV_DEV(ndev, &pdev->dev);
	pdata = netdev_priv(ndev);
	dwc_eqos_init_all_fptrs(pdata);
	hw_if = &pdata->hw_if;
	desc_if = &pdata->desc_if;

	pdata->pdev = pdev;
	pdata->ndev = ndev;
	pdata->base_addr = dwc_eqos_base_addr;
	pdata->tx_queue_cnt = tx_q_count;
	pdata->rx_queue_cnt = rx_q_count;
	pdata->if_idx = eth_idx;

	DBGPR("pdata = %p\n", pdata);
	DBGPR("pdata base_addr = %#lx\n", pdata->base_addr);

	/* issue software reset to device */
	hw_if->exit(pdata);
	ndev->irq = platform_get_irq(pdev, 0);

	dwc_eqos_get_all_hw_features(pdata);
	dwc_eqos_print_all_hw_features(pdata);

	ret = desc_if->alloc_queue_struct(pdata);
	if (ret < 0) {
		pr_alert("ERROR: Unable to alloc Tx/Rx queue\n");
		goto err_out_free_netdev;
	}

	ndev->netdev_ops = dwc_eqos_get_netdev_ops();

	pdata->interface = dwc_eqos_get_phy_interface(pdata);
#ifdef EMULATOR_PHY_DISABLE
	/* Bypass PHYLIB for TBI, RTBI and SGMII interface */
	pdata->hw_feat.sma_sel = 0;
#endif

	/* no phy ? */
	if (pdev->dev.of_node &&
	    of_property_match_string(pdev->dev.of_node, "use-phy", "NONE_PHY") >= 0) {
		pdata->hw_feat.sma_sel = 0;
		ret = dwc_eqos_fixed_phy_register(ndev);
		if (ret) {
			netdev_err(ndev, "Failed to register fixed PHY device\n");
			return ret;
		}
	}

	if (pdata->hw_feat.sma_sel == 1) {
		ret = dwc_eqos_mdio_register(ndev);
		if (ret < 0) {
			pr_alert(
				"MDIO bus (id %d) registration failed\n",
				pdata->bus_id);
			goto err_out_mdio_reg;
		}
	} else {
		netdev_alert(ndev, "MDIO is not present\n");
	}

	/* enabling and registration of irq with magic wakeup */
	if (pdata->hw_feat.mgk_sel == 1) {
		device_set_wakeup_capable(&pdev->dev, 1);
		pdata->wolopts = WAKE_MAGIC;
		enable_irq_wake(ndev->irq);
	}

	for (i = 0; i < pdata->rx_queue_cnt; i++) {
		struct dwc_eqos_rx_queue *rx_queue = &pdata->rx_queue[(i)];
		struct dwc_eqos_tx_queue *tx_queue = &pdata->tx_queue[(i)];

		netif_napi_add(ndev, &rx_queue->napi, dwc_eqos_poll_rxq,
					   DWC_EQOS_NAPI_WEIGHT);
		netif_tx_napi_add(ndev, &tx_queue->napi, dwc_eqos_poll_txq,
						  DWC_EQOS_NAPI_WEIGHT);
	}

	ndev->ethtool_ops = dwc_eqos_get_ethtool_ops();

	if (pdata->hw_feat.tx_coe_sel) {
		ndev->hw_features = NETIF_F_IP_CSUM;
		ndev->hw_features |= NETIF_F_IPV6_CSUM;
		pr_info("Supports TX COE\n");
	}

	if (pdata->hw_feat.rx_coe_sel) {
		ndev->hw_features |= NETIF_F_RXCSUM;
		ndev->hw_features |= NETIF_F_LRO;
		pr_info("Supports RX COE and LRO\n");
	}

	if (IS_ENABLED(CONFIG_VLAN_8021Q)) {
		ndev->vlan_features |= ndev->hw_features;
		ndev->hw_features |= NETIF_F_HW_VLAN_CTAG_RX;
		if (pdata->hw_feat.sa_vlan_ins) {
			ndev->hw_features |= NETIF_F_HW_VLAN_CTAG_TX;
			pr_info("VLAN Feature enabled\n");
		}
		if (pdata->hw_feat.vlan_hash_en) {
			ndev->hw_features |=
				NETIF_F_HW_VLAN_CTAG_FILTER;
			pr_info("VLAN HASH Filtering enabled\n");
		}
	}

	ndev->features |= ndev->hw_features;
	pdata->dev_state |= ndev->features;

	dwc_eqos_init_rx_coalesce(pdata);
	dwc_eqos_init_tx_coalesce(pdata);

	if (IS_ENABLED(CONFIG_DWC_QOS_PTPSUPPORT))
		dwc_eqos_ptp_init(pdata);

	spin_lock_init(&pdata->lock);
	spin_lock_init(&pdata->tx_lock);

	if (dwc_eqos_rxclk_mux_init(pdata)) {
		pr_alert("ERROR: Unable to init rxclock mux\n");
		goto err_out_pg_failed;
	}

	/* Set max MTU to the maximum size that DWC_EQOS can support */
	ndev->max_mtu = DWC_EQOS_MAX_SUPPORTED_MTU;

	ret = register_netdev(ndev);
	if (ret) {
		pr_alert("%s: Net device registration failed\n", DEV_NAME);
		goto err_out_remove_ptp;
	}

	if (IS_ENABLED(CONFIG_DWC_QOS_DEBUGFS)) {
		dwc_eqos_get_pdata(pdata);
		create_debug_files();
		/* to give prv data to debugfs */
	}

	platform_set_drvdata(pdev, ndev);

	DBGPR("<-- dwc_eqos_probe\n");

	return 0;

err_out_remove_ptp:
	if (IS_ENABLED(CONFIG_DWC_QOS_PTPSUPPORT))
		dwc_eqos_ptp_remove(pdata);
err_out_pg_failed:
	if (pdata->hw_feat.sma_sel == 1)
		dwc_eqos_mdio_unregister(ndev);
err_out_mdio_reg:
	desc_if->free_queue_struct(pdata);

err_out_free_netdev:
	free_netdev(ndev);
#if 0
err_out_clk_dis_aper:
	clk_disable_unprepare(pclk);
#endif
	return ret;
}

static int dwc_eqos_remove(struct platform_device *pdev)
{
	struct net_device *dev = platform_get_drvdata(pdev);
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	struct desc_if_struct *desc_if = &pdata->desc_if;

	DBGPR("--> dwc_eqos_remove\n");

	if (pdata->irq_number != 0) {
		free_irq(pdata->irq_number, pdata);
		pdata->irq_number = 0;
	}

	if (pdata->hw_feat.sma_sel == 1)
		dwc_eqos_mdio_unregister(dev);

	if (IS_ENABLED(CONFIG_DWC_QOS_PTPSUPPORT))
		dwc_eqos_ptp_remove(pdata);

	unregister_netdev(dev);

	desc_if->free_queue_struct(pdata);

	free_netdev(dev);

	if (IS_ENABLED(CONFIG_DWC_QOS_DEBUGFS))
		remove_debug_files();

	DBGPR("<-- dwc_eqos_remove\n");

	return 0;
}

#ifdef CONFIG_PM
static int dwc_eqos_resume(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	int ret = Y_SUCCESS;

	DBGPR("<-- dwc_eqos_resume\n");

	if (!ndev || !netif_running(ndev))
		return 0;

	ret = ndev->netdev_ops->ndo_open(ndev);
	if (ret  == Y_SUCCESS)
		netif_device_attach(ndev);

	return ret;
}

static int dwc_eqos_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct net_device *ndev = platform_get_drvdata(pdev);

	DBGPR("<-- dwc_eqos_suspend\n");

	if (!ndev || !netif_running(ndev))
		return 0;

	/* suspend only on prior resume success */
	if (!netif_device_present(ndev))
		return 0;

	netif_device_detach(ndev);

	return ndev->netdev_ops->ndo_stop(ndev);
}
#endif

static const struct of_device_id dwc_eqos_of_match[] = {
	{ .compatible = "snps,dwc-qos-ethernet-4.21", },
	{}
};
MODULE_DEVICE_TABLE(of, dwc_eqos_of_match);

static struct platform_driver dwc_eqos_driver = {
	.probe   = dwc_eqos_probe,
	.remove  = dwc_eqos_remove,
#ifdef CONFIG_PM
	.resume  = dwc_eqos_resume,
	.suspend = dwc_eqos_suspend,
#endif
	.driver  = {
		.name  = DRIVER_NAME,
		.of_match_table = dwc_eqos_of_match,
	},
};

module_platform_driver(dwc_eqos_driver);

MODULE_AUTHOR("Pankaj Dubey <pankaj.dubey@samsung.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Designware Ethernet QoS Controller Driver");
