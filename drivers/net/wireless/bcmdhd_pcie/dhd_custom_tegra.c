/*
 * drivers/net/wireless/bcmdhd_pcie/dhd_custom_tegra.c
 *
 * Copyright (C) 2014-2015 NVIDIA Corporation. All rights reserved.
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

#include <linux/types.h>
#include <mach/nct.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/file.h>

#include <bcmutils.h>
#include <linux_osl.h>
#include <dhd_dbg.h>
#include <dngl_stats.h>
#include <dhd.h>
#include <wldev_common.h>
#include <wl_cfg80211.h>

#define WIFI_MAC_ADDR_FILE "/mnt/factory/wifi/wifi_mac.txt"

extern void dhd_set_ampdu_rx_tid(struct net_device *dev, int ampdu_rx_tid);

#ifdef CONFIG_TEGRA_USE_NCT
static int wifi_get_mac_addr_nct(unsigned char *buf)
{
	int ret = -ENODATA;
	union nct_item_type *entry = NULL;
	entry = kmalloc(sizeof(union nct_item_type), GFP_KERNEL);
	if (entry) {
		if (!tegra_nct_read_item(NCT_ID_WIFI_MAC_ADDR, entry)) {
			memcpy(buf, entry->wifi_mac_addr.addr,
					sizeof(struct nct_mac_addr_type));
			ret = 0;
		}
		kfree(entry);
		if (!is_valid_ether_addr(buf)) {
			DHD_ERROR(("%s: invalid mac %02x:%02x:%02x:%02x:%02x:%02x\n",
				__FUNCTION__,
				buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]));
			ret = -EINVAL;
		}
	}

	if (ret)
		DHD_ERROR(("%s: Couldn't find MAC address from NCT\n", __FUNCTION__));

	return ret;
}
#endif

static int wifi_get_mac_addr_file(unsigned char *buf)
{
	struct file *fp;
	int rdlen;
	char str[32];
	int mac[6];
	int ret = 0;

	/* open wifi mac address file */
	fp = filp_open(WIFI_MAC_ADDR_FILE, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		DHD_ERROR(("%s: cannot open %s\n",
			__FUNCTION__, WIFI_MAC_ADDR_FILE));
		return -ENOENT;
	}

	/* read wifi mac address file */
	memset(str, 0, sizeof(str));
	rdlen = kernel_read(fp, fp->f_pos, str, 17);
	if (rdlen > 0)
		fp->f_pos += rdlen;
	if (rdlen != 17) {
		DHD_ERROR(("%s: bad mac address file - len %d != 17",
						__FUNCTION__, rdlen));
		ret = -ENOENT;
	} else if (sscanf(str, "%x:%x:%x:%x:%x:%x",
		&mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) != 6) {
		DHD_ERROR(("%s: bad mac address file"
			" - must contain xx:xx:xx:xx:xx:xx\n",
			__FUNCTION__));
		ret = -ENOENT;
	} else {
		DHD_ERROR(("%s: using wifi mac %02x:%02x:%02x:%02x:%02x:%02x\n",
			__FUNCTION__,
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]));
		buf[0] = (unsigned char) mac[0];
		buf[1] = (unsigned char) mac[1];
		buf[2] = (unsigned char) mac[2];
		buf[3] = (unsigned char) mac[3];
		buf[4] = (unsigned char) mac[4];
		buf[5] = (unsigned char) mac[5];
	}

	if (!is_valid_ether_addr(buf)) {
		DHD_ERROR(("%s: invalid mac %02x:%02x:%02x:%02x:%02x:%02x\n",
			__FUNCTION__,
			buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]));
		ret = -EINVAL;
	}

	/* close wifi mac address file */
	filp_close(fp, NULL);

	return ret;
}

/* Get MAC address from the specified DTB path */
static int wifi_get_mac_address_dtb(const char *node_name,
					const char *property_name,
					unsigned char *mac_addr)
{
	struct device_node *np = of_find_node_by_path(node_name);
	const char *mac_str = NULL;
	int values[6] = {0};
	unsigned char mac_temp[6] = {0};
	int i, ret = 0;

	if (!np)
		return -EADDRNOTAVAIL;

	/* If the property is present but contains an invalid value,
	 * then something is wrong. Log the error in that case.
	 */
	if (of_property_read_string(np, property_name, &mac_str)) {
		ret = -EADDRNOTAVAIL;
		goto err_out;
	}

	/* The DTB property is a string of the form xx:xx:xx:xx:xx:xx
	 * Convert to an array of bytes.
	 */
	if (sscanf(mac_str, "%x:%x:%x:%x:%x:%x",
		&values[0], &values[1], &values[2],
		&values[3], &values[4], &values[5]) != 6) {
		ret = -EINVAL;
		goto err_out;
	}

	for (i = 0; i < 6; ++i)
		mac_temp[i] = (unsigned char)values[i];

	if (!is_valid_ether_addr(mac_temp)) {
		ret = -EINVAL;
		goto err_out;
	}

	memcpy(mac_addr, mac_temp, 6);

	of_node_put(np);

	return ret;

err_out:
	DHD_ERROR(("%s: bad mac address at %s/%s: %s.\n",
		__func__, node_name, property_name,
		(mac_str) ? mac_str : "null"));

	of_node_put(np);

	return ret;
}

int wifi_get_mac_addr(unsigned char *buf)
{
	int ret = -ENODATA;

	/* The MAC address search order is:
	 * DTB (from NCT/EEPROM)
	 * NCT
	 * File (FCT/rootfs)
	*/
	ret = wifi_get_mac_address_dtb("/chosen", "nvidia,wifi-mac", buf);

#ifdef CONFIG_TEGRA_USE_NCT
	if (ret)
		ret = wifi_get_mac_addr_nct(buf);
#endif

	if (ret)
		ret = wifi_get_mac_addr_file(buf);

	return ret;
}

int wldev_miracast_tuning(
	struct net_device *dev, char *command, int total_len)
{
	int error = 0;
	int mode = 0;
	int ampdu_mpdu;
	int ampdu_rx_tid = -1;
	int disable_interference_mitigation = 0;
	int auto_interference_mitigation = -1;
#ifdef VSDB_BW_ALLOCATE_ENABLE
	int mchan_algo;
	int mchan_bw;
#endif /* VSDB_BW_ALLOCATE_ENABLE */

	if (sscanf(command, "%*s %d", &mode) != 1) {
		DHD_ERROR(("Failed to get mode\n"));
		return -1;
	}

set_mode:


	if (mode == 0) {
		/* Normal mode: restore everything to default */
#ifdef CUSTOM_AMPDU_MPDU
		ampdu_mpdu = CUSTOM_AMPDU_MPDU;
#else
		ampdu_mpdu = -1;	/* FW default */
#endif
#ifdef VSDB_BW_ALLOCATE_ENABLE
		mchan_algo = 0;	/* Default */
		mchan_bw = 50;	/* 50:50 */
#endif /* VSDB_BW_ALLOCATE_ENABLE */
	}
	else if (mode == 1) {
		/* Miracast source mode */
		ampdu_mpdu = 8;	/* for tx latency */
#ifdef VSDB_BW_ALLOCATE_ENABLE
		mchan_algo = 1;	/* BW based */
		mchan_bw = 25;	/* 25:75 */
#endif /* VSDB_BW_ALLOCATE_ENABLE */
	}
	else if (mode == 2) {
		/* Miracast sink/PC Gaming mode */
		ampdu_mpdu = 8;	/* FW default */
#ifdef VSDB_BW_ALLOCATE_ENABLE
		mchan_algo = 0;	/* Default */
		mchan_bw = 50;	/* 50:50 */
#endif /* VSDB_BW_ALLOCATE_ENABLE */
	} else if (mode == 3) {
		ampdu_rx_tid = 0;
		mode = 2;
		goto set_mode;
	} else if (mode == 4) {
		ampdu_rx_tid = 0x7f;
		mode = 0;
		goto set_mode;
	} else if (mode == 5) {
		/* Blake connected mode, disable interference mitigation */
		error = wldev_ioctl(dev, WLC_SET_INTERFERENCE_OVERRIDE_MODE,
			&disable_interference_mitigation, sizeof(int), true);
		if (error) {
			DHD_ERROR((
				"Failed to set interference_override: mode:%d, error:%d\n",
				mode, error));
			return -1;
		}
		return error;
	} else if (mode == 6) {
		/* No Blake connected, enable auto interference mitigation */
		error = wldev_ioctl(dev, WLC_SET_INTERFERENCE_OVERRIDE_MODE,
			&auto_interference_mitigation, sizeof(int), true);
		if (error) {
			DHD_ERROR((
				"Failed to set interference_override: mode:%d, error:%d\n",
				mode, error));
			return -1;
		}
		return error;
	} else {
		DHD_ERROR(("Unknown mode: %d\n", mode));
		return -1;
	}

	/* Update ampdu_mpdu */
	error = wldev_iovar_setint(dev, "ampdu_mpdu", ampdu_mpdu);
	if (error) {
		DHD_ERROR(("Failed to set ampdu_mpdu: mode:%d, error:%d\n",
			mode, error));
		return -1;
	}

	if (ampdu_rx_tid != -1)
		dhd_set_ampdu_rx_tid(dev, ampdu_rx_tid);

#ifdef VSDB_BW_ALLOCATE_ENABLE
if (bcmdhd_vsdb_bw_allocate_enable) {
	error = wldev_iovar_setint(dev, "mchan_algo", mchan_algo);
	if (error) {
		DHD_ERROR(("Failed to set mchan_algo: mode:%d, error:%d\n",
			mode, error));
		return -1;
	}

	error = wldev_iovar_setint(dev, "mchan_bw", mchan_bw);
	if (error) {
		DHD_ERROR(("Failed to set mchan_bw: mode:%d, error:%d\n",
			mode, error));
		return -1;
	}
}
#endif /* VSDB_BW_ALLOCATE_ENABLE */

	return error;
}

int wl_android_ampdu_send_delba(struct net_device *dev, char *command)
{
	int error = 0;
	struct ampdu_ea_tid aet;
	char smbuf[WLC_IOCTL_SMLEN];

	DHD_INFO(("%s, %s\n", __FUNCTION__, command));

	/* get tid */
	aet.tid = bcm_strtoul(command, &command, 10);
	if (aet.tid > MAXPRIO) {
		DHD_ERROR(("%s: error: invalid tid %d\n", __FUNCTION__, aet.tid));
		return BCME_BADARG;
	}
	command++;

	/* get mac address, here 17 is strlen("xx:xx:xx:xx:xx:xx") */
	if ((strlen(command) < 17) || !bcm_ether_atoe(command, &aet.ea)) {
		DHD_ERROR(("%s: error: invalid MAC address %s\n", __FUNCTION__, command));
		return BCME_BADARG;
	}
	/* skip mac address */
	command += strlen("xx:xx:xx:xx:xx:xx") + 1;

	/* get initiator */
	aet.initiator = bcm_strtoul(command, &command, 10);
	if (aet.initiator > 1) {
		DHD_ERROR(("%s: error: inivalid initiator %d\n", __FUNCTION__, aet.initiator));
		return BCME_BADARG;
	}

	error = wldev_iovar_setbuf(dev, "ampdu_send_delba", &aet, sizeof(aet),
		smbuf, sizeof(smbuf), NULL);
	if (error) {
		DHD_ERROR(("%s: Failed to send delba, error = %d\n", __FUNCTION__, error));
	}

	return error;
}


int wldev_get_rx_rate_stats (struct net_device *dev, char *command, int total_len)
{
	wl_scb_rx_rate_stats_t *rstats;
	struct ether_addr ea;
	char smbuf[WLC_IOCTL_SMLEN];
	char eabuf[18] = {0, };
	int bytes_written = 0;
	int error;

	memcpy(eabuf, command+strlen("RXRATESTATS")+1, 17);

	if (!bcm_ether_atoe(eabuf, &ea)) {
		DHD_ERROR(("Invalid MAC Address\n"));
		return -1;
	}

	error = wldev_iovar_getbuf(dev, "rx_rate_stats",
		&ea, ETHER_ADDR_LEN, smbuf, sizeof(smbuf), NULL);
	if (error < 0) {
		DHD_ERROR(("get rx_rate_stats failed = %d\n", error));
		return -1;
	}

	rstats = (wl_scb_rx_rate_stats_t *)smbuf;
	bytes_written = sprintf(command, "1/%d/%d,",
		dtoh32(rstats->rx1mbps[0]), dtoh32(rstats->rx1mbps[1]));
	bytes_written += sprintf(command+bytes_written, "2/%d/%d,",
		dtoh32(rstats->rx2mbps[0]), dtoh32(rstats->rx2mbps[1]));
	bytes_written += sprintf(command+bytes_written, "5.5/%d/%d,",
		dtoh32(rstats->rx5mbps5[0]), dtoh32(rstats->rx5mbps5[1]));
	bytes_written += sprintf(command+bytes_written, "6/%d/%d,",
		dtoh32(rstats->rx6mbps[0]), dtoh32(rstats->rx6mbps[1]));
	bytes_written += sprintf(command+bytes_written, "9/%d/%d,",
		dtoh32(rstats->rx9mbps[0]), dtoh32(rstats->rx9mbps[1]));
	bytes_written += sprintf(command+bytes_written, "11/%d/%d,",
		dtoh32(rstats->rx11mbps[0]), dtoh32(rstats->rx11mbps[1]));
	bytes_written += sprintf(command+bytes_written, "12/%d/%d,",
		dtoh32(rstats->rx12mbps[0]), dtoh32(rstats->rx12mbps[1]));
	bytes_written += sprintf(command+bytes_written, "18/%d/%d,",
		dtoh32(rstats->rx18mbps[0]), dtoh32(rstats->rx18mbps[1]));
	bytes_written += sprintf(command+bytes_written, "24/%d/%d,",
		dtoh32(rstats->rx24mbps[0]), dtoh32(rstats->rx24mbps[1]));
	bytes_written += sprintf(command+bytes_written, "36/%d/%d,",
		dtoh32(rstats->rx36mbps[0]), dtoh32(rstats->rx36mbps[1]));
	bytes_written += sprintf(command+bytes_written, "48/%d/%d,",
		dtoh32(rstats->rx48mbps[0]), dtoh32(rstats->rx48mbps[1]));
	bytes_written += sprintf(command+bytes_written, "54/%d/%d",
		dtoh32(rstats->rx54mbps[0]), dtoh32(rstats->rx54mbps[1]));

	return bytes_written;
}

int wldev_get_assoc_resp_ie(
	struct net_device *dev, char *command, int total_len)
{
	wl_assoc_info_t *assoc_info;
	char smbuf[WLC_IOCTL_SMLEN];
	char bssid[6], null_bssid[6];
	int resp_ies_len = 0;
	int bytes_written = 0;
	int error, i;

	bzero(bssid, 6);
	bzero(null_bssid, 6);

	/* Check Association */
	error = wldev_ioctl(dev, WLC_GET_BSSID, &bssid, sizeof(bssid), 0);
	if (error == BCME_NOTASSOCIATED) {
		/* Not associated */
		bytes_written += snprintf(&command[bytes_written], total_len, "NA");
		goto done;
	}
	else if (error < 0) {
		DHD_ERROR(("WLC_GET_BSSID failed = %d\n", error));
		return -1;
	}
	else if (memcmp(bssid, null_bssid, ETHER_ADDR_LEN) == 0) {
		/*  Zero BSSID: Not associated */
		bytes_written += snprintf(&command[bytes_written], total_len, "NA");
		goto done;
	}

	/* Get assoc_info */
	bzero(smbuf, sizeof(smbuf));
	error = wldev_iovar_getbuf(dev, "assoc_info", NULL, 0, smbuf, sizeof(smbuf), NULL);
	if (error < 0) {
		DHD_ERROR(("get assoc_info failed = %d\n", error));
		return -1;
	}

	assoc_info = (wl_assoc_info_t *)smbuf;
	resp_ies_len = dtoh32(assoc_info->resp_len) - sizeof(struct dot11_assoc_resp);

	/* Retrieve assoc resp IEs */
	if (resp_ies_len) {
		error = wldev_iovar_getbuf(dev, "assoc_resp_ies", NULL, 0, smbuf, sizeof(smbuf),
			NULL);
		if (error < 0) {
			DHD_ERROR(("get assoc_resp_ies failed = %d\n", error));
			return -1;
		}

		/* Length */
		bytes_written += snprintf(&command[bytes_written], total_len, "%d,", resp_ies_len);

		/* IEs */
		if ((total_len - bytes_written) > resp_ies_len) {
			for (i = 0; i < resp_ies_len; i++) {
				bytes_written += sprintf(&command[bytes_written], "%02x", smbuf[i]);
			}
		} else {
			DHD_ERROR(("Not enough buffer\n"));
			return -1;
		}
	} else {
		DHD_ERROR(("Zero Length assoc resp ies = %d\n", resp_ies_len));
		return -1;
	}

done:

	return bytes_written;
}

int wldev_get_max_linkspeed(
	struct net_device *dev, char *command, int total_len)
{
	wl_assoc_info_t *assoc_info;
	char smbuf[WLC_IOCTL_SMLEN];
	char bssid[6], null_bssid[6];
	int resp_ies_len = 0;
	int bytes_written = 0;
	int error, i;

	bzero(bssid, 6);
	bzero(null_bssid, 6);

	/* Check Association */
	error = wldev_ioctl(dev, WLC_GET_BSSID, &bssid, sizeof(bssid), 0);
	if (error == BCME_NOTASSOCIATED) {
		/* Not associated */
		bytes_written += snprintf(&command[bytes_written],
					total_len, "-1");
		goto done;
	} else if (error < 0) {
		DHD_ERROR(("WLC_GET_BSSID failed = %d\n", error));
		return -1;
	} else if (memcmp(bssid, null_bssid, ETHER_ADDR_LEN) == 0) {
		/*  Zero BSSID: Not associated */
		bytes_written += snprintf(&command[bytes_written],
					total_len, "-1");
		goto done;
	}
	/* Get assoc_info */
	bzero(smbuf, sizeof(smbuf));
	error = wldev_iovar_getbuf(dev, "assoc_info", NULL, 0, smbuf,
				sizeof(smbuf), NULL);
	if (error < 0) {
		DHD_ERROR(("get assoc_info failed = %d\n", error));
		return -1;
	}

	assoc_info = (wl_assoc_info_t *)smbuf;
	resp_ies_len = dtoh32(assoc_info->resp_len) -
				sizeof(struct dot11_assoc_resp);

	/* Retrieve assoc resp IEs */
	if (resp_ies_len) {
		error = wldev_iovar_getbuf(dev, "assoc_resp_ies", NULL, 0,
					smbuf, sizeof(smbuf), NULL);
		if (error < 0) {
			DHD_ERROR(("get assoc_resp_ies failed = %d\n",
				error));
			return -1;
		}

		{
			int maxRate = 0;
			struct dot11IE {
				unsigned char ie;
				unsigned char len;
				unsigned char data[0];
			} *dot11IE = (struct dot11IE *)smbuf;
			int remaining = resp_ies_len;

			while (1) {
				if (remaining < 2)
					break;
				if (remaining < dot11IE->len + 2)
					break;
				switch (dot11IE->ie) {
				case 0x01: /* supported rates */
				case 0x32: /* extended supported rates */
					for (i = 0; i < dot11IE->len; i++) {
						int rate = ((dot11IE->data[i] &
								0x7f) / 2);
						if (rate > maxRate)
							maxRate = rate;
					}
					break;
				case 0x2d: /* HT capabilities */
				case 0x3d: /* HT operation */
					/* 11n supported */
					maxRate = 150; /* Just return an 11n
					rate for now. Could implement detailed
					parser later. */
					break;
				default:
					break;
				}

				/* next IE */
				dot11IE = (struct dot11IE *)
				((unsigned char *)dot11IE + dot11IE->len + 2);
				remaining -= (dot11IE->len + 2);
			}
			bytes_written += snprintf(&command[bytes_written],
						total_len, "MaxLinkSpeed %d",
						maxRate);
			goto done;
			}
	} else {
		DHD_ERROR(("Zero Length assoc resp ies = %d\n",
			resp_ies_len));
		return -1;
	}

done:

	return bytes_written;

}

int wl_android_get_iovar(struct net_device *dev, char *command,	int total_len)
{
	int skip = strlen(CMD_GETIOVAR) + 1;
	char iovbuf[WLC_IOCTL_SMLEN];
	s32 param = -1;
	int bytes_written = 0;

	if (strlen(command) < skip ) {
		DHD_ERROR(("%s: Invalid command length", __func__));
		return BCME_BADARG;
	}

	DHD_INFO(("%s: command buffer %s command length:%zd\n",
			__func__, (command + skip), strlen(command + skip)));

	/* Initiate get_iovar command */
	memset(iovbuf, 0, sizeof(iovbuf));
	bytes_written = wldev_iovar_getbuf(dev, (command + skip), &param,
				sizeof(param), iovbuf, sizeof(iovbuf), NULL);

	/* Check for errors, log the error and reset bytes written value */
	if (bytes_written < 0) {
		DHD_ERROR(("%s: get iovar failed (error=%d)\n",
				__func__, bytes_written));
		bytes_written = 0;
	} else {
		snprintf(command, total_len, iovbuf);
		bytes_written = snprintf(command, total_len,
					"%d:%s", dtoh32(param), iovbuf);
		DHD_INFO(("%s: param:%d iovbuf:%s strlen(iovbuf):%zd"
				" bytes_written:%d\n", __func__, param, iovbuf,
				strlen(iovbuf), bytes_written));
	}

	return bytes_written;
}

int wl_android_set_iovar(struct net_device *dev, char *command, int total_len)
{
	int skip = strlen(CMD_GETIOVAR) + 1;
	char iovbuf[WLC_IOCTL_SMLEN];
	char iovar[WLC_IOCTL_SMLEN];
	s32 param = -1;
	int bytes_written = 0;
	int ret = -1;

	if (strlen(command) < skip ) {
		DHD_ERROR(("Short command length"));
		return BCME_BADARG;
	}

	DHD_INFO(("%s: command buffer:%s command length:%zd\n",
			__func__, (command + skip), strlen(command + skip)));

	/* Parse command and get iovbuf and param values */
	memset(iovbuf, 0, sizeof(iovbuf));
	memset(iovar, 0, sizeof(iovbuf));
	ret = sscanf((command + skip), "%s %d:%s", iovar, &param, iovbuf);
	if (ret < 3) {
		DHD_ERROR(("%s: Failed to get Parameter %d\n", __func__, ret));
		return BCME_BADARG;
	}
	DHD_INFO(("%s: iovar:%s param:%d iovbuf:%s strlen(iovbuf):%zd\n", __func__,
			iovar, param, iovbuf, strlen(iovbuf)));

	bytes_written = wldev_iovar_setbuf(dev, iovar, &param,
				sizeof(param), iovbuf, sizeof(iovbuf), NULL);
	if (bytes_written < 0)
		DHD_ERROR(("%s: set iovar failed (error=%d)\n",
				__func__, bytes_written));

	return bytes_written;
}

int wl_android_mkeep_alive(struct net_device *dev, char *command, int total_len)
{
	char *pcmd = command;
	char *str = NULL;
	wl_mkeep_alive_pkt_t *mkeep_alive_pkt = NULL;
	char *ioctl_buf = NULL;
	u16 kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
	s32 err = BCME_OK;
	uint32 len;
	char *endptr;
	int i = 0;

	len = sizeof(wl_mkeep_alive_pkt_t);
	mkeep_alive_pkt = (wl_mkeep_alive_pkt_t *)kzalloc(len, kflags);
	if (!mkeep_alive_pkt) {
		WL_ERR(("%s: mkeep_alive pkt alloc failed\n", __func__));
		return -ENOMEM;
	}

	ioctl_buf = kzalloc(WLC_IOCTL_MEDLEN, GFP_KERNEL);
	if (!ioctl_buf) {
		WL_ERR(("ioctl memory alloc failed\n"));
		if (mkeep_alive_pkt) {
			kfree(mkeep_alive_pkt);
		}
		return -ENOMEM;
	}
	memset(ioctl_buf, 0, WLC_IOCTL_MEDLEN);

	/* drop command */
	str = bcmstrtok(&pcmd, " ", NULL);
	if (!str) {
		WL_ERR(("Invalid drop command %s\n", str));
		err = -EINVAL;
		goto exit;
	}

	/* get index */
	str = bcmstrtok(&pcmd, " ", NULL);
	if (!str) {
		WL_ERR(("Invalid index parameter %s\n", str));
		err = -EINVAL;
		goto exit;
	}
	mkeep_alive_pkt->keep_alive_id = bcm_strtoul(str, &endptr, 0);
	if (*endptr != '\0') {
		WL_ERR(("Invalid index parameter %s\n", str));
		err = -EINVAL;
		goto exit;
	}

	/* get period */
	str = bcmstrtok(&pcmd, " ", NULL);
	if (!str) {
		WL_ERR(("Invalid period parameter %s\n", str));
		err = -EINVAL;
		goto exit;
	}
	mkeep_alive_pkt->period_msec = bcm_strtoul(str, &endptr, 0);
	if (*endptr != '\0') {
		WL_ERR(("Invalid period parameter %s\n", str));
		err = -EINVAL;
		goto exit;
	}

	mkeep_alive_pkt->version = htod16(WL_MKEEP_ALIVE_VERSION);
	mkeep_alive_pkt->length = htod16(WL_MKEEP_ALIVE_FIXED_LEN);

	/*get packet*/
	str = bcmstrtok(&pcmd, " ", NULL);
	if (str) {
		if (strncmp(str, "0x", 2) != 0 &&
				strncmp(str, "0X", 2) != 0) {
			WL_ERR(("Packet invalid format. Needs to start with 0x\n"));
			err = -EINVAL;
			goto exit;
		}
		str = str + 2; /* Skip past 0x */
		if (strlen(str) % 2 != 0) {
			WL_ERR(("Packet invalid format. Needs to be of even length\n"));
			err = -EINVAL;
			goto exit;
		}
		for (i = 0; *str != '\0'; i++) {
			char num[3];
			strncpy(num, str, 2);
			num[2] = '\0';
			mkeep_alive_pkt->data[i] = (uint8)bcm_strtoul(num, NULL, 16);
			str += 2;
		}
		mkeep_alive_pkt->len_bytes = i;
	}

	err = wldev_iovar_setbuf(dev, "mkeep_alive", mkeep_alive_pkt,
		len + i, ioctl_buf, WLC_IOCTL_MEDLEN, NULL);
	if (err != BCME_OK) {
		WL_ERR(("%s: Fail to set iovar %d\n", __func__, err));
		err = -EINVAL;
	}

exit:
	if (mkeep_alive_pkt)
		kfree(mkeep_alive_pkt);

	if (ioctl_buf)
		kfree(ioctl_buf);

	return err;
}
