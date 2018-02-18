/*
 * OF helpers for network devices.
 *
 * This file is released under the GPLv2
 *
 * Initially copied out of arch/powerpc/kernel/prom_parse.c
 */
#include <linux/etherdevice.h>
#include <linux/kernel.h>
#include <linux/of_net.h>
#include <linux/phy.h>
#include <linux/export.h>

/**
 * of_get_phy_mode - Get phy mode for given device_node
 * @np:	Pointer to the given device_node
 *
 * The function gets phy interface string from property 'phy-mode' or
 * 'phy-connection-type', and return its index in phy_modes table, or errno in
 * error case.
 */
int of_get_phy_mode(struct device_node *np)
{
	const char *pm;
	int err, i;

	err = of_property_read_string(np, "phy-mode", &pm);
	if (err < 0)
		err = of_property_read_string(np, "phy-connection-type", &pm);
	if (err < 0)
		return err;

	for (i = 0; i < PHY_INTERFACE_MODE_MAX; i++)
		if (!strcasecmp(pm, phy_modes(i)))
			return i;

	return -ENODEV;
}
EXPORT_SYMBOL_GPL(of_get_phy_mode);

static int _of_get_mac_addr_file(const char *filename, unsigned char *buf)
{
	struct file *fp;
	int rdlen;
	char str[32];
	int mac[6];
	int ret = 0;

	/* open mac address file */
	fp = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		pr_debug("%s: cannot open %s\n",
			__FUNCTION__, filename);
		return -ENOENT;
	}

	/* read mac address file */
	memset(str, 0, sizeof(str));
	rdlen = kernel_read(fp, fp->f_pos, str, 17);
	if (rdlen > 0)
		fp->f_pos += rdlen;
	if (rdlen != 17) {
		pr_debug("%s: bad mac address file"
			" - len %d < 17",
			__FUNCTION__, rdlen);
		ret = -ENOENT;
	} else if (sscanf(str, "%x:%x:%x:%x:%x:%x",
		&mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) != 6) {
		pr_debug("%s: bad mac address file"
			" - must contain xx:xx:xx:xx:xx:xx\n",
			__FUNCTION__);
		ret = -ENOENT;
	} else {
		pr_debug("%s: using mac %02x:%02x:%02x:%02x:%02x:%02x\n",
			__FUNCTION__,
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		buf[0] = (unsigned char) mac[0];
		buf[1] = (unsigned char) mac[1];
		buf[2] = (unsigned char) mac[2];
		buf[3] = (unsigned char) mac[3];
		buf[4] = (unsigned char) mac[4];
		buf[5] = (unsigned char) mac[5];
		if (!is_valid_ether_addr(buf)) {
			pr_debug("%s: ignoring invalid mac\n",
				__FUNCTION__);
			ret = -EINVAL;
		}
	}

	/* close mac address file */
	filp_close(fp, NULL);

	return ret;
}

/* Get MAC address from the specified DTB path */
static int _of_get_mac_address_dtb(const char *node_name,
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
	pr_err("%s: bad mac address at %s/%s: %s.\n",
		__func__, node_name, property_name,
		(mac_str) ? mac_str : "null");

	of_node_put(np);

	return ret;
}

static struct property *_of_set_mac_address(struct device_node *np,
	unsigned char *mac_addr)
{
	struct property *prop;

	/* allocate mac address property */
	prop = kzalloc(sizeof(struct property) + 12 + 6, GFP_KERNEL);
	if (!prop)
		return NULL;
	prop->name = ((char *) prop) + sizeof(struct property);
	sprintf(prop->name, "mac-address");
	prop->value = prop->name + strlen(prop->name) + 1;
	memcpy(prop->value, mac_addr, 6);
	prop->length = 6;
	prop->next = NULL;

	/* add mac address property */
	if (of_add_property(np, prop)) {
		pr_err("%s: failed to add property %s\n",
			__FUNCTION__, prop->name);
		kfree(prop);
		return NULL;
	}

	return prop;
}

/**
 * Search the device tree for the best MAC address to use.  'mac-address' is
 * checked first, because that is supposed to contain to "most recent" MAC
 * address. If that isn't set, then 'local-mac-address' is checked next,
 * because that is the default address.  If that isn't set, then the obsolete
 * 'address' is checked, just in case we're using an old device tree.
 *
 * Note that the 'address' property is supposed to contain a virtual address of
 * the register set, but some DTS files have redefined that property to be the
 * MAC address.
 *
 * All-zero MAC addresses are rejected, because those could be properties that
 * exist in the device tree, but were not set by U-Boot.  For example, the
 * DTS could define 'mac-address' and 'local-mac-address', with zero MAC
 * addresses.  Some older U-Boots only initialized 'local-mac-address'.  In
 * this case, the real MAC is in 'local-mac-address', and 'mac-address' exists
 * but is all zeros.
*/
const void *of_get_mac_address(struct device_node *np)
{
	struct property *pp;
	int err;
	const char *s;
	unsigned char mac_addr[6];

	pp = of_find_property(np, "mac-address", NULL);
	if (pp && (pp->length == 6) && is_valid_ether_addr(pp->value))
		return pp->value;

	pp = of_find_property(np, "local-mac-address", NULL);
	if (pp && (pp->length == 6) && is_valid_ether_addr(pp->value))
		return pp->value;

	pp = of_find_property(np, "address", NULL);
	if (pp && (pp->length == 6) && is_valid_ether_addr(pp->value))
		return pp->value;

	err = of_property_read_string(np, "mac-address-chosen-dtb-node", &s);
	if (err == 0) {
		pr_info("%s: checking /chosen dtb node for"
			" mac address property %s\n",
			__func__, s);
		err = _of_get_mac_address_dtb("/chosen", s, mac_addr);
		if (err == 0) {
			pp = _of_set_mac_address(np, mac_addr);
			if (pp && (pp->length == 6) &&
				is_valid_ether_addr(pp->value))
				return pp->value;
		}
	}

	err = of_property_read_string(np, "mac-address-file", &s);
	if (err == 0) {
		pr_info("%s: checking file %s for mac address\n",
			__func__, s);
		err = _of_get_mac_addr_file(s, mac_addr);
		if (err == 0) {
			pp = _of_set_mac_address(np, mac_addr);
			if (pp && (pp->length == 6) &&
				is_valid_ether_addr(pp->value))
				return pp->value;
		}
	}

	return NULL;
}
EXPORT_SYMBOL(of_get_mac_address);
