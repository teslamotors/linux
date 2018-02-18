/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Laxman Dewangan<ldewangan@nvidia.com>
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

#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/of.h>
#include <linux/of_fdt.h>

enum plugin_manager_match_type {
	PLUGIN_MANAGER_MATCH_EXACT,
	PLUGIN_MANAGER_MATCH_PARTIAL,
	PLUGIN_MANAGER_MATCH_GE,
};

static struct property *__of_copy_property(const struct property *prop,
		gfp_t flags)
{
	struct property *propn;

	propn = kzalloc(sizeof(*prop), flags);
	if (propn == NULL)
		return NULL;

	propn->name = kstrdup(prop->name, flags);
	if (propn->name == NULL)
		goto err_fail_name;

	if (prop->length > 0) {
		propn->value = kmalloc(prop->length, flags);
		if (propn->value == NULL)
			goto err_fail_value;
		memcpy(propn->value, prop->value, prop->length);
		propn->length = prop->length;
	}
	return propn;

err_fail_value:
	kfree(propn->name);
err_fail_name:
	kfree(propn);
	return NULL;
}

static struct property *__of_string_append(struct device_node *target,
					   struct property *prop)
{
	struct property *new_prop, *tprop;
	const char *tprop_name, *curr_str;
	int slen, tlen, lenp;

	tprop_name = of_prop_next_string(prop, NULL);
	if (!tprop_name)
		return NULL;

	new_prop = kzalloc(sizeof(*new_prop), GFP_KERNEL);
	if (!new_prop)
		return NULL;

	new_prop->name = kstrdup(tprop_name, GFP_KERNEL);
	if (!new_prop->name)
		goto err_fail_name;

	curr_str = of_prop_next_string(prop, tprop_name);
	for (slen = 0; curr_str; curr_str = of_prop_next_string(prop, curr_str))
		slen += strlen(curr_str);

	tprop = of_find_property(target, tprop_name, &lenp);
	tlen = (tprop) ? tprop->length : 0;

	new_prop->value = kmalloc(slen + tlen, GFP_KERNEL);
	if (!new_prop->value)
		goto err_fail_value;

	if (tlen)
		memcpy(new_prop->value, tprop->value, tlen);

	if (slen) {
		curr_str = of_prop_next_string(prop, tprop_name);
		memcpy(new_prop->value + tlen, curr_str, slen);
	}

	new_prop->length = slen + tlen;

	return new_prop;

err_fail_value:
	kfree(new_prop->name);
err_fail_name:
	kfree(new_prop);

	return NULL;
}

struct device_node *of_get_child_by_addressed_name(
		const struct device_node *node, const char *name)
{
	struct device_node *child;
	const char *address_node_name;

	for_each_child_of_node(node, child) {
		address_node_name =  strrchr(child->full_name, '/');
		if (!strcmp(name, address_node_name))
			return child;
	}

	return NULL;
}

static int __init update_target_node_from_overlay(
		struct device_node *target, struct device_node *overlay)
{
	struct property *prop;
	struct property *tprop;
	struct property *new_prop;
	const char *pval;
	int lenp = 0;
	int ret;

	pr_debug("Update properties from %s to %s\n", overlay->full_name,
		 target->full_name);

	for_each_property_of_node(overlay, prop) {
		/* Skip those we do not want to proceed */
		if (!strcmp(prop->name, "name") ||
			!strcmp(prop->name, "phandle") ||
			!strcmp(prop->name, "linux,phandle"))
				continue;
		if (!strcmp(prop->name, "delete-target-property")) {
			if (prop->length <= 0)
				continue;
			pval = (const char *)prop->value;
			pr_info("Removing Prop %s from target %s\n",
				pval, target->full_name);
			tprop = of_find_property(target, pval, &lenp);
			if (tprop)
				of_remove_property(target, tprop);
			continue;
		}

		if (!strcmp(prop->name, "append-string-property")) {
			if (prop->length <= 0)
				continue;

			new_prop = __of_string_append(target, prop);
			if (!new_prop) {
				pr_err("Prop %s can not be appended\n",
					of_prop_next_string(prop, NULL));
				return -EINVAL;
			}
			goto add_prop;
		}

		new_prop = __of_copy_property(prop, GFP_KERNEL);
		if (!new_prop) {
			pr_err("Prop %s can not be duplicated\n",
				prop->name);
			return -EINVAL;
		}

add_prop:
		tprop = of_find_property(target, new_prop->name, &lenp);
		if (!tprop) {
			ret = of_add_property(target, new_prop);
			if (ret < 0) {
				pr_err("Prop %s can not be added on node %s\n",
					new_prop->name, target->full_name);
				return ret;
			}
		} else {
			ret = of_update_property(target, new_prop);
			if (ret < 0) {
				pr_err("Prop %s can not be updated on node %s\n",
					new_prop->name, target->full_name);
				return ret;
			}
		}
	}
	return 0;
}

static int plugin_manager_get_fabid(const char *id_str)
{
	int fabid = 0;
	int i;

	if (strlen(id_str) < 13)
		return -EINVAL;

	for (i = 0; i < 3; ++i) {
		if ((id_str[10 + i] >= '0') && (id_str[10 + i] <= '9'))
			fabid = fabid * 10 + id_str[10 + i] - '0';
		else
			return -EINVAL;
	}

	return fabid;
}

static bool plugin_manager_match_id(struct device_node *np, const char *id_name)
{
	struct property *prop;
	const char *in_str = id_name;
	int match_type = PLUGIN_MANAGER_MATCH_EXACT;
	int valid_str_len = strlen(id_name);
	int fabid = 0, prop_fabid;
	int i;

	if ((valid_str_len > 2) && (in_str[0] == '>') && (in_str[1] == '=')) {
		in_str += 2;
		valid_str_len -= 2;
		match_type = PLUGIN_MANAGER_MATCH_GE;
		goto match_type_done;
	}

	if ((valid_str_len > 1) && (in_str[0] == '^')) {
		in_str += 1;
		valid_str_len -= 1;
		match_type = PLUGIN_MANAGER_MATCH_PARTIAL;
		goto match_type_done;
	}

	for (i = 0; i < valid_str_len; ++i) {
		if (in_str[i] == '*') {
			valid_str_len = i;
			match_type = PLUGIN_MANAGER_MATCH_PARTIAL;
			break;
		}
	}

match_type_done:
	if (match_type == PLUGIN_MANAGER_MATCH_GE) {
		fabid = plugin_manager_get_fabid(in_str);
		if (fabid < 0)
			return false;
	}

	for_each_property_of_node(np, prop) {
		/* Skip those we do not want to proceed */
		if (!strcmp(prop->name, "name") ||
			!strcmp(prop->name, "phandle") ||
			!strcmp(prop->name, "linux,phandle"))
				continue;
		switch (match_type) {
		case PLUGIN_MANAGER_MATCH_EXACT:
			if (strlen(prop->name) != valid_str_len)
				break;
			if (!memcmp(in_str, prop->name, valid_str_len))
				return true;
			break;

		case PLUGIN_MANAGER_MATCH_PARTIAL:
			if (strlen(prop->name) < valid_str_len)
				break;
			if (!memcmp(in_str, prop->name, valid_str_len))
				return true;
			break;

		case PLUGIN_MANAGER_MATCH_GE:
			if (strlen(prop->name) < 13)
				break;
			if (memcmp(in_str, prop->name, 10))
				break;
			prop_fabid = plugin_manager_get_fabid(prop->name);
			if (prop_fabid < 0)
				break;
			if (prop_fabid >= fabid)
				return true;
			break;
		}
	}

	return false;
}

static int __init update_target_node(struct device_node *target,
	struct device_node *overlay)
{
	struct device_node *tchild, *ochild;
	const char *address_name;
	int ret;

	ret = update_target_node_from_overlay(target, overlay);
	if (ret < 0) {
		pr_err("Target %s update with overlay %s failed: %d\n",
			target->name, overlay->name, ret);
		return ret;
	}

	for_each_child_of_node(overlay, ochild) {
		address_name = strrchr(ochild->full_name, '/');
		tchild = of_get_child_by_addressed_name(target, address_name);
		if (!tchild) {
			pr_err("Overlay node %s not found in target node %s\n",
				ochild->full_name, target->full_name);
			continue;
		}
		ret = update_target_node(tchild, ochild);
		if (ret < 0) {
			pr_err("Target %s update with overlay %s failed: %d\n",
				tchild->name, ochild->name, ret);
			return ret;
		}
	}
	return 0;
}

static int __init parse_fragment(struct device_node *np)
{
	struct device_node *board_np, *nct_np, *odm_np, *overlay, *target, *cnp;
	struct device_node *config_np, *chip_np;
	const char *bname;
	struct property *prop;
	int board_count;
	int odm_count, nct_count, chip_id_count;
	int cname_count, cval_count;
	int nchild;
	bool found = false;
	bool override_on_all_match;
	int ret;


	override_on_all_match = of_property_read_bool(np,
					"enable-override-on-all-matches");

	cname_count = of_property_count_strings(np, "config-names");
	cval_count = of_property_count_u32(np, "configs");
	if (cname_count != cval_count) {
		pr_err("Node %s does not have config-names and configs\n",
			np->name);
		return -EINVAL;
	}

	board_count = of_property_count_strings(np, "ids");
	odm_count = of_property_count_strings(np, "odm-data");
	nct_count = of_property_count_strings(np, "nct-data");
	chip_id_count = of_property_count_strings(np, "chip-id");
	if ((board_count <= 0) && (odm_count <= 0) && (cname_count <= 0) &&
	    (nct_count <= 0) && (chip_id_count <= 0)) {
		pr_err("Node %s does not have property ids, nct and odm data\n",
			np->name);
		return -EINVAL;
	}

	nchild = of_get_child_count(np);
	if (!nchild) {
		pr_err("Node %s does not have Overlay child\n", np->name);
		return -EINVAL;
	}

	/* Match the IDs or odm data */
	board_np = of_find_node_by_path("/chosen/plugin-manager/ids");
	odm_np = of_find_node_by_path("/chosen/plugin-manager/odm-data");
	nct_np = of_find_node_by_path("/chosen/plugin-manager/nct-data");
	chip_np = of_find_node_by_path("/chosen/plugin-manager/chip-id");
	config_np = of_find_node_by_path("/chosen/plugin-manager/configs");
	if (!board_np && !odm_np && !config_np && !nct_np && !chip_np) {
		pr_err("chosen/plugin-manager does'nt have ids, nct and odm-data\n");
		return -EINVAL;
	}

	if ((board_count > 0) && board_np) {
		of_property_for_each_string(np, "ids", prop, bname) {
			found = plugin_manager_match_id(board_np, bname);
			if (found) {
				pr_info("node %s match with board %s\n",
					np->full_name, bname);
				if (override_on_all_match)
					break;
				else
					goto search_done;
			}
		}

		if (override_on_all_match && !found)
			return 0;
	}

	if ((odm_count > 0) && odm_np) {
		of_property_for_each_string(np, "odm-data", prop, bname) {
			found = of_property_read_bool(odm_np, bname);
			if (found) {
				pr_info("node %s match with odm-data %s\n",
					np->full_name, bname);
				if (override_on_all_match)
					break;
				else
					goto search_done;
			}
		}

		if (override_on_all_match && !found)
			return 0;
	}

	if ((nct_count > 0) && nct_np) {
		of_property_for_each_string(np, "nct-data", prop, bname) {
			found = of_property_read_bool(nct_np, bname);
			if (found) {
				pr_info("node %s match with nct-data %s\n",
					np->full_name, bname);
				if (override_on_all_match)
					break;
				else
					goto search_done;
			}
		}

		if (override_on_all_match && !found)
			return 0;
	}

	if ((chip_id_count > 0) && chip_np) {
		of_property_for_each_string(np, "chip-id", prop, bname) {
			found = of_property_read_bool(chip_np, bname);
			if (found) {
				pr_info("node %s match with chip-id %s\n",
					np->full_name, bname);
				if (override_on_all_match)
					break;
				else
					goto search_done;
			}
		}

		if (override_on_all_match && !found)
			return 0;
	}

	if ((cname_count > 0) && config_np) {
		int index = 0;
		u32 pval = 0, pmv = 0, mask, value;

		of_property_for_each_string(np, "config-names", prop, bname) {
			ret = of_property_read_u32_index(np, "configs",
					index, &pmv);
			if (ret < 0) {
				pr_info("node %s do not have proper configs\n",
					np->name);
				return ret;
			}
			index++;
			ret = of_property_read_u32(config_np, bname, &pval);
			if (ret < 0)
				continue;

			mask = (pmv >> 8) & 0xFF;
			value = pmv & 0xFF;
			pval &= 0xFF;
			found = ((pval & mask) == value);
			if (found) {
				pr_info("node %s match with config %s\n",
					np->full_name, bname);
				if (override_on_all_match)
					break;
				else
					goto search_done;
			}
		}

		if (override_on_all_match && !found)
			return 0;
	}

search_done:
	if (!found)
		return 0;

	for_each_child_of_node(np, cnp) {
		target = of_parse_phandle(cnp, "target", 0);
		if (!target) {
			pr_err("Node %s does not have targer node\n",
				cnp->name);
			continue;
		}

		overlay = of_get_child_by_name(cnp, "_overlay_");
		if (!overlay) {
			pr_err("Node %s does not have Overlay\n", cnp->name);
			continue;
		}

		ret = update_target_node(target, overlay);
		if (ret < 0) {
			pr_err("Target %s update with overlay %s failed: %d\n",
				target->name, overlay->name, ret);
			continue;
		}
	}
	return 0;
}

static int __init plugin_manager_init(void)
{
	struct device_node *pm_node;
	struct device_node *child;
	int ret;

	pr_info("Initializing plugin-manager\n");

	pm_node = of_find_node_by_path("/plugin-manager");
	if (!pm_node) {
		pr_info("Plugin-manager not available\n");
		return 0;
	}

	if (!of_device_is_available(pm_node)) {
		pr_info("Plugin-manager status disabled\n");
		return 0;
	}

	for_each_child_of_node(pm_node, child) {
		if (!of_device_is_available(child)) {
			pr_info("Plugin-manager child %s status disabled\n",
				child->name);
			continue;
		}
		ret = parse_fragment(child);
		if (ret < 0)
			pr_err("Error in parsing node %s: %d\n",
				child->full_name, ret);
	}
	return 0;
}
core_initcall(plugin_manager_init);
