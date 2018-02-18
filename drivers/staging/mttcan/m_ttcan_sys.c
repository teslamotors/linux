/* Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
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

#include "hw/m_ttcan.h"

static ssize_t show_std_fltr(struct device *dev,
	struct device_attribute *devattr, char *buf)
{
	ssize_t ret, total = 0;
	int i = 0;
	struct mttcan_priv *priv = netdev_priv(to_net_dev(dev));
	int cur_filter_size = priv->ttcan->fltr_config.std_fltr_size;

	ret = sprintf(buf, "%s\n", "Standard Filters");
	total += ret;
	while (cur_filter_size--) {
		ret = sprintf(buf+total, "%d. 0x%x\n", i,
			ttcan_get_std_id_filter(priv->ttcan, i));
		total += ret;
		i++;
	}
	return total;
}

static ssize_t show_xtd_fltr(struct device *dev,
	struct device_attribute *devattr, char *buf)
{
	ssize_t ret, total = 0;
	int i = 0;
	struct mttcan_priv *priv = netdev_priv(to_net_dev(dev));
	int cur_filter_size = priv->ttcan->fltr_config.xtd_fltr_size;

	ret = sprintf(buf, "%s\n", "Extended Filters");
	total += ret;
	while (cur_filter_size--) {
		ret = sprintf(buf+total, "%d. 0x%llx\n", i,
			ttcan_get_xtd_id_filter(priv->ttcan, i));
		total += ret;
		i++;
	}
	return total;
}

static ssize_t show_gfc_fltr(struct device *dev,
	struct device_attribute *devattr, char *buf)
{
	struct mttcan_priv *priv = netdev_priv(to_net_dev(dev));
	return sprintf(buf, "%s\n0x%x\n", "Global filter",
			ttcan_get_gfc(priv->ttcan));
}

static ssize_t store_gfc_fltr(struct device *dev,
	struct device_attribute *devattr,
	const char *buf, size_t count)
{
	unsigned int anfs, anfe;
	unsigned int rrfs, rrfe;
	u32 gfc;
	int ret;

	struct net_device *ndev = to_net_dev(dev);
	struct mttcan_priv *priv = netdev_priv(ndev);

	if (ndev->flags & IFF_UP) {
		dev_err(dev, "GFC cannot be configured as device is running\n");
		return -EBUSY;
	}

	ret = sscanf(buf, "anfs=%u anfe=%u rrfs=%u rrfe=%u", &anfs,
			&anfe, &rrfs, &rrfe);
	if ((ret < 4) || ((anfs | anfe) > 3) || ((rrfs | rrfe) > 1)) {
		dev_err(dev, "Invalid Global filter\n");
		pr_err("usage:anfs=0..3 anfe=0..3 rrfs=0/1 rrfe=0/1\n");
		return -EINVAL;
	}

	gfc = 0;
	gfc = (anfs << MTT_GFC_ANFS_SHIFT) & MTT_GFC_ANFS_MASK;
	gfc |= (anfe << MTT_GFC_ANFE_SHIFT) & MTT_GFC_ANFE_MASK;
	gfc |= (rrfs << MTT_GFC_RRFS_SHIFT) & MTT_GFC_RRFS_MASK;
	gfc |= (rrfe << MTT_GFC_RRFE_SHIFT) & MTT_GFC_RRFE_SHIFT;

	priv->gfc_reg = gfc;
	ttcan_set_gfc(priv->ttcan, gfc);

	return count;
}

static ssize_t show_xidam(struct device *dev,
	struct device_attribute *devattr, char *buf)
{
	struct mttcan_priv *priv = netdev_priv(to_net_dev(dev));
	return sprintf(buf, "%s\n0x%x\n", "XIDAM",
			ttcan_get_xidam(priv->ttcan));
}

static ssize_t store_xidam(struct device *dev,
	struct device_attribute *devattr,
	const char *buf, size_t count)
{
	unsigned int xidam;

	struct net_device *ndev = to_net_dev(dev);
	struct mttcan_priv *priv = netdev_priv(ndev);

	if (ndev->flags & IFF_UP) {
		dev_err(dev, "XIDAM is protected, device is running\n");
		return -EBUSY;
	}
	/* usage: xidam=MASK */
	if (sscanf(buf, "xidam=%u", &xidam) != 1) {
		dev_err(dev, "Invalid XIDAM MASK\n");
		pr_err("usage: xidam=MASK\n");
		return -EINVAL;
	}

	priv->xidam_reg = xidam;
	ttcan_set_xidam(priv->ttcan, xidam);

	return count;
}

static ssize_t store_std_fltr(struct device *dev,
	struct device_attribute *devattr,
	const char *buf, size_t count)
{
	unsigned int sft, sfec;
	unsigned int sfid1, sfid2;
	int idx = -1, cur_filter_size;
	int ret;

	struct mttcan_priv *priv = netdev_priv(to_net_dev(dev));
	struct net_device *ndev = to_net_dev(dev);

	if (ndev->flags & IFF_UP) {
		dev_err(dev, "device is running\n");
		return -EBUSY;
	}
	/* usage: sft="0/1/2/3" sfec=1...7 sfid1="ID1" sfid2="ID2" idx=%u
	*/
	ret = sscanf(buf, "sft=%u sfec=%u sfid1=%X sfid2=%X idx=%u", &sft,
			&sfec, &sfid1, &sfid2, &idx);
	if (ret < 4) {
		/* Not passing index is allowed */
		dev_err(dev, "Invalid std filter\n");
		pr_err("usage:sft=0..3 sfec=0..7 sfid1=ID1h sfid2=ID2h idx=i\n");
		return -EINVAL;
	}

	cur_filter_size = priv->ttcan->fltr_config.std_fltr_size;

	if ((idx > cur_filter_size) || (idx == -1))
		if (cur_filter_size >= priv->ttcan->mram_cfg[MRAM_SIDF].num) {
			dev_err(dev, "Max Invalid std filter Index\n");
			return -ENOSPC;
		}

	if (ret == 5) {
		if (idx > cur_filter_size) {
			dev_err(dev, "Invalid std filter Index\n");
			return -EINVAL;
		}
		ttcan_set_std_id_filter(priv->ttcan, priv->std_shadow,
			idx, (u8)sft, (u8)sfec, sfid1, sfid2);
		if (idx == cur_filter_size)
			priv->ttcan->fltr_config.std_fltr_size++;
	} else {
		ttcan_set_std_id_filter(priv->ttcan, priv->std_shadow,
			cur_filter_size, (u8) sft, (u8)sfec, sfid1, sfid2);
			priv->ttcan->fltr_config.std_fltr_size++;
	}
	return count;
}

static ssize_t store_xtd_fltr(struct device *dev,
	struct device_attribute *devattr,
	const char *buf, size_t count)
{
	unsigned int eft, efec;
	unsigned int efid1, efid2;
	int idx = -1, cur_filter_size;
	int ret;

	struct mttcan_priv *priv = netdev_priv(to_net_dev(dev));
	struct net_device *ndev = to_net_dev(dev);

	if (ndev->flags & IFF_UP) {
		dev_err(dev, "device is running\n");
		return -EBUSY;
	}
	/* usage: eft="0/1/2/3" efec=1...7 efid1="ID1h" efid2="ID2h" idx=%u
	*/
	ret = sscanf(buf, "eft=%u efec=%u efid1=%X efid2=%X idx=%u", &eft,
			&efec, &efid1, &efid2, &idx);
	if (ret < 4) {
		/* Not passing index is allowed */
		dev_err(dev, "Invalid std filter\n");
		pr_err("usage:eft=0..3 efec=0..7 efid1=ID1h efid2=ID2h idx=i\n");
		return -EINVAL;
	}

	cur_filter_size = priv->ttcan->fltr_config.xtd_fltr_size;

	if ((idx > cur_filter_size) || (idx == -1))
		if (cur_filter_size >= priv->ttcan->mram_cfg[MRAM_XIDF].num) {
			dev_err(dev, "Max Invalid std filter Index\n");
			return -ENOSPC;
		}

	if (ret == 5) {
		if (idx > cur_filter_size) {
			dev_err(dev, "Invalid std filter Index\n");
			return -EINVAL;
		}
		ttcan_set_xtd_id_filter(priv->ttcan, priv->xtd_shadow,
			idx, (u8) eft, (u8) efec, efid1, efid2);
		if (idx == cur_filter_size)
			priv->ttcan->fltr_config.xtd_fltr_size++;
	} else {
		ttcan_set_xtd_id_filter(priv->ttcan, priv->xtd_shadow,
			cur_filter_size, (u8) eft, (u8) efec, efid1, efid2);
		priv->ttcan->fltr_config.xtd_fltr_size++;
	}
	return count;
}

static ssize_t show_tx_cancel(struct device *dev,
	struct device_attribute *devattr, char *buf)
{
	struct mttcan_priv *priv = netdev_priv(to_net_dev(dev));
	return sprintf(buf, "%s\n0x%x\n", "TXBCF",
			ttcan_read_tx_cancelled_reg(priv->ttcan));
}

static ssize_t store_tx_cancel(struct device *dev,
	struct device_attribute *devattr,
	const char *buf, size_t count)
{
	unsigned int txcbr;

	struct net_device *ndev = to_net_dev(dev);
	struct mttcan_priv *priv = netdev_priv(ndev);

	/* usage: txbcr=bit_mask for buffer */
	if (sscanf(buf, "txbcr=%X", &txcbr) != 1) {
		dev_err(dev, "Invalid TXBCR value\n");
		pr_err("%s usage: txbcr=bit_mask to cancel\n", buf);
		return -EINVAL;
	}

	ttcan_set_tx_cancel_request(priv->ttcan, txcbr);

	return count;
}

static ssize_t show_ttrmc(struct device *dev,
	struct device_attribute *devattr, char *buf)
{
	struct mttcan_priv *priv = netdev_priv(to_net_dev(dev));
	return sprintf(buf, "%s\n0x%x\n", "TTRMC",
			ttcan_get_ttrmc(priv->ttcan));
}

static ssize_t store_ttrmc(struct device *dev,
	struct device_attribute *devattr,
	const char *buf, size_t count)
{
	unsigned int rmps, xtd, rid;
	struct net_device *ndev = to_net_dev(dev);
	struct mttcan_priv *priv = netdev_priv(ndev);

	if (ndev->flags & IFF_UP) {
		dev_err(dev, "TTRMC is protected, device is running\n");
		return -EBUSY;
	}
	/* usage: rmps=0/1 xtd=0/1 rid=ReferenceID */
	if (sscanf(buf, "rmps=%u xtd=%u rid=%X", &rmps, &xtd, &rid) != 3) {
		dev_err(dev, "Invalid TTRMC\n");
		pr_err("usage: rmps=0/1 xtd=0/1 rid=ReferenceID in hex\n");
		return -EINVAL;
	}

	ttcan_set_ref_mesg(priv->ttcan, rid, rmps, xtd);

	return count;
}

static ssize_t show_ttocf(struct device *dev,
	struct device_attribute *devattr, char *buf)
{
	struct mttcan_priv *priv = netdev_priv(to_net_dev(dev));
	return sprintf(buf, "%s\n0x%x\n", "TTOCF",
			ttcan_get_ttocf(priv->ttcan));
}

static ssize_t store_ttocf(struct device *dev,
	struct device_attribute *devattr,
	const char *buf, size_t count)
{
	unsigned int evtp, ecc, egtf, awl, eecs, irto, ldsdl, tm, gen, om;
	struct net_device *ndev = to_net_dev(dev);
	struct mttcan_priv *priv = netdev_priv(ndev);

	if (ndev->flags & IFF_UP) {
		dev_err(dev, "TTCOF is protected, device is running\n");
		return -EBUSY;
	}
	/* usage: evtp=0/1 ecc=0/1 egtf=0/1 awl=0..255 eecs=0 irto=0..127
	 * ldsdl=0..7 tm=0/1 gen=0/1 om=0..4 */
	if (sscanf(buf,
		"evtp=%u ecc=%u egtf=%u awl=%u eecs=%u irto=%u ldsdl=%u tm=%u gen=%u om=%u",
		&evtp, &ecc, &egtf, &awl, &eecs, &irto, &ldsdl,
		&tm, &gen, &om) != 10) {
		dev_err(dev, "Invalid TTOCF\n");
		pr_err("usage: evtp=0/1 ecc=0/1 egtf=0/1 awl=0..255 eecs=0 irto=0..127 ldsdl=0..7 tm=0/1 gen=0/1 om=0..4\n");
		return -EINVAL;
	}

	ttcan_set_tt_config(priv->ttcan, evtp, ecc, egtf,
		awl, eecs, irto, ldsdl, tm, gen, om);

	return count;
}

static ssize_t show_ttmlm(struct device *dev,
	struct device_attribute *devattr, char *buf)
{
	struct mttcan_priv *priv = netdev_priv(to_net_dev(dev));
	return sprintf(buf, "%s\n0x%x\n", "TTMLM",
			ttcan_get_ttmlm(priv->ttcan));
}

static ssize_t store_ttmlm(struct device *dev,
	struct device_attribute *devattr,
	const char *buf, size_t count)
{
	unsigned int entt, txew, css, ccm;
	struct net_device *ndev = to_net_dev(dev);
	struct mttcan_priv *priv = netdev_priv(ndev);

	if (ndev->flags & IFF_UP) {
		dev_err(dev, "TTMLM is protected, device is running\n");
		return -EBUSY;
	}
	/* usage: entt=0...4095 txew=0..15 css=0..2 ccm=0..63(2*pow(n)-1) */
	if (sscanf(buf, "entt=%u txew=%u css=%u ccm=%u",
			&entt, &txew, &css, &ccm) != 4) {
		dev_err(dev, "Invalid TTMLM\n");
		pr_err("usage: entt=0...4095 txew=0..15 css=0..2 ccm=0..63(2*pow(n)-1)\n");
		return -EINVAL;
	}

	ttcan_set_matrix_limits(priv->ttcan, entt, txew, css, ccm);

	return count;
}

static ssize_t show_tttmc(struct device *dev,
	struct device_attribute *devattr, char *buf)
{
	struct mttcan_priv *priv = netdev_priv(to_net_dev(dev));
	return sprintf(buf, "%s\n0x%x\n", "TTTMC",
			ttcan_get_tttmc(priv->ttcan));
}

static ssize_t store_tttmc(struct device *dev,
	struct device_attribute *devattr,
	const char *buf, size_t count)
{
	u32 tme, tttmc;
	struct net_device *ndev = to_net_dev(dev);
	struct mttcan_priv *priv = netdev_priv(ndev);

	if (ndev->flags & IFF_UP) {
		dev_err(dev, "TTTMC is protected, device is running\n");
		return -EBUSY;
	}
	/* usage: tme=number of elements */
	if (sscanf(buf, "tme=%u", &tme) != 1) {
		dev_err(dev, "Invalid TTTMC\n");
		pr_err("usage: tme=0..64 (Num Elements)\n");
		return -EINVAL;
	}

	if (tme > 64)
		tme = 64;

	tttmc = ttcan_get_tttmc(priv->ttcan);
	tttmc &= ~MTT_TTTMC_TME_MASK;
	tttmc |= (tme << MTT_TTTMC_TME_SHIFT) &
		MTT_TTTMC_TME_MASK;

	ttcan_set_tttmc(priv->ttcan, tttmc);

	return count;
}

static ssize_t show_cccr_txbar(struct device *dev,
	struct device_attribute *devattr, char *buf)
{
	struct mttcan_priv *priv = netdev_priv(to_net_dev(dev));
	int init = ttcan_get_cccr(priv->ttcan) & 0x1;
	return sprintf(buf, "CCCR.INIT %s\n", init ? "set" : "reset");
}

static ssize_t store_cccr_txbar(struct device *dev,
	struct device_attribute *devattr,
	const char *buf, size_t count)
{
	struct mttcan_priv *priv = netdev_priv(to_net_dev(dev));
	char str[32];
	int txbar = 0;

	if ((sscanf(buf, "%s txbar=%d", str, &txbar) != 2) || (txbar > 32)) {
		dev_err(dev, "Invalid String or txbar\n");
		pr_err("usage: reset/set txbar=0..32 (Num Elements)\n");
		return -EINVAL;
	}

	/* usage: set/reset */
	if (strcmp("set", str) == 0)
		ttcan_set_config_change_enable(priv->ttcan);
	else if (strcmp("reset", str) == 0)
		ttcan_reset_config_change_enable(priv->ttcan);
	else {
		dev_err(dev, "Invalid String\n");
		pr_err("valid strings: set/reset\n");
		return -EINVAL;
	}

	txbar = (1UL << txbar) - 1;
	ttcan_set_txbar(priv->ttcan, txbar);

	return count;
}

static ssize_t show_txbar(struct device *dev,
	struct device_attribute *devattr, char *buf)
{
	return sprintf(buf, "Not implemented\n");
}

static ssize_t store_txbar(struct device *dev,
	struct device_attribute *devattr,
	const char *buf, size_t count)
{
	unsigned int txbar = 0;
	struct mttcan_priv *priv = netdev_priv(to_net_dev(dev));

	/* usage: txbar=1...32*/
	if ((sscanf(buf, "txbar=%u", &txbar) != 1) || (txbar > 32)) {
		dev_err(dev, "Invalid TXBAR\n");
		pr_err("usage: txbar=1..32\n");
		return -EINVAL;
	}

	txbar = (1UL << txbar) - 1;
	ttcan_set_txbar(priv->ttcan, txbar);

	return count;
}

static ssize_t show_trigger_mem(struct device *dev,
	struct device_attribute *devattr, char *buf)
{
	ssize_t ret, total = 0;
	int i = 0;
	struct mttcan_priv *priv = netdev_priv(to_net_dev(dev));
	int cur = priv->ttcan->tt_mem_elements;

	ret = sprintf(buf, "%s\n", "Trigger Memory Elements");
	total += ret;
	while (cur--) {
		ret = sprintf(buf+total, "%d. 0x%llx\n", i,
			ttcan_get_trigger_mem(priv->ttcan, i));
		total += ret;
		i++;
	}
	return total;
}

static ssize_t store_trigger_mem(struct device *dev,
	struct device_attribute *devattr,
	const char *buf, size_t count)
{
	unsigned int tm, cc, tmin, tmex, type;
	unsigned int ftype, mnr;
	int idx = -1, cur;
	int ret;

	struct net_device *ndev = to_net_dev(dev);
	struct mttcan_priv *priv = netdev_priv(ndev);

	if (ndev->flags & IFF_UP) {
		dev_err(dev, "Trigger Mem is protected, device is running\n");
		return -EBUSY;
	}
	/* usage: tm=0..FFFF cc=0..127 tmin=0/1 tmex=0/1 type=0..10
			ftype=0/1 mnr=0..31 idx=%u */
	ret = sscanf(buf,
		"tm=%X cc=%u tmin=%u tmex=%u type=%u ftype=%u mnr=%u idx=%u",
		&tm, &cc, &tmin, &tmex, &type, &ftype, &mnr, &idx);
	if (ret < 7) {
		/* Not passing index is allowed */
		dev_err(dev, "Invalid Trigger Element\n");
		pr_err("tm=0..0xFFFF cc=0..127 tmin=0/1 tmex=0/1 type=0..10 ftype=0/1 mnr=0..31 idx=i\n");
		return -EINVAL;
	}

	cur = priv->ttcan->tt_mem_elements;

	if ((idx > cur) || (idx == -1))
		if (cur >= priv->ttcan->mram_cfg[MRAM_TMC].num) {
			dev_err(dev, "Max Invalid Trigger mem Index\n");
			return -ENOSPC;
		}

	if (ret == 8) {
		if (idx > cur) {
			dev_err(dev, "Invalid Trigger Mem Index\n");
			return -EINVAL;
		}
		ttcan_set_trigger_mem(priv->ttcan, priv->tmc_shadow, idx, tm,
			cc, tmin, tmex, type, ftype, mnr);

		if (idx == cur)
			priv->ttcan->tt_mem_elements++;
	} else {
		ttcan_set_trigger_mem(priv->ttcan,  priv->tmc_shadow, cur, tm,
			cc, tmin, tmex, type, ftype, mnr);
		priv->ttcan->tt_mem_elements++;
	}
	return count;
}

static DEVICE_ATTR(std_filter, S_IRUGO | S_IWUSR, show_std_fltr,
	store_std_fltr);
static DEVICE_ATTR(xtd_filter, S_IRUGO | S_IWUSR, show_xtd_fltr,
	store_xtd_fltr);
static DEVICE_ATTR(gfc_filter, S_IRUGO | S_IWUSR, show_gfc_fltr,
	store_gfc_fltr);
static DEVICE_ATTR(xidam, S_IRUGO | S_IWUSR, show_xidam, store_xidam);
static DEVICE_ATTR(tx_cancel, S_IRUGO | S_IWUSR, show_tx_cancel,
	store_tx_cancel);
static DEVICE_ATTR(ttrmc, S_IRUGO | S_IWUSR, show_ttrmc, store_ttrmc);
static DEVICE_ATTR(ttocf, S_IRUGO | S_IWUSR, show_ttocf, store_ttocf);
static DEVICE_ATTR(ttmlm, S_IRUGO | S_IWUSR, show_ttmlm, store_ttmlm);
static DEVICE_ATTR(tttmc, S_IRUGO | S_IWUSR, show_tttmc, store_tttmc);
static DEVICE_ATTR(txbar, S_IRUGO | S_IWUSR, show_txbar, store_txbar);
static DEVICE_ATTR(cccr_init_txbar, S_IRUGO | S_IWUSR, show_cccr_txbar,
	store_cccr_txbar);
static DEVICE_ATTR(trigger_mem, S_IRUGO | S_IWUSR, show_trigger_mem,
		store_trigger_mem);

static struct attribute *mttcan_attr[] = {
	&dev_attr_std_filter.attr,
	&dev_attr_xtd_filter.attr,
	&dev_attr_gfc_filter.attr,
	&dev_attr_xidam.attr,
	&dev_attr_tx_cancel.attr,
	&dev_attr_ttrmc.attr,
	&dev_attr_ttocf.attr,
	&dev_attr_ttmlm.attr,
	&dev_attr_tttmc.attr,
	&dev_attr_txbar.attr,
	&dev_attr_cccr_init_txbar.attr,
	&dev_attr_trigger_mem.attr,
	NULL
};

static const struct attribute_group mttcan_attr_group = {
	.attrs = mttcan_attr,
};

int mttcan_create_sys_files(struct device *dev)
{
	return sysfs_create_group(&dev->kobj, &mttcan_attr_group);
}

void mttcan_delete_sys_files(struct device *dev)
{
	sysfs_remove_group(&dev->kobj, &mttcan_attr_group);
}
