/*
 * Copyright (C) 2016 Tesla Motors, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>

#define PINCTRL_MAX_STATE_LEN	16

struct pinctrl_pseudo {
	struct pinctrl *pinctrl;
	char cur_state[PINCTRL_MAX_STATE_LEN+1];
};

static ssize_t pinctrl_pseudo_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct pinctrl_pseudo *pps = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n", pps->cur_state);
}

static ssize_t pinctrl_pseudo_state_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct pinctrl_pseudo *pps = dev_get_drvdata(dev);
	struct pinctrl_state *ps;
	char state_name[PINCTRL_MAX_STATE_LEN];
	int ret;

	if (sscanf(buf, "%" __stringify(PINCTRL_MAX_STATE_LEN) "s",
			state_name) != 1)
		return -EINVAL;

	ps = pinctrl_lookup_state(pps->pinctrl, state_name);
	if (IS_ERR(ps)) {
		dev_err(dev, "invalid state: '%s'\n", buf);
		return PTR_ERR(ps);
	}

	ret = pinctrl_select_state(pps->pinctrl, ps);
	if (ret < 0) {
		dev_err(dev, "failed to set state '%s': %d\n", buf, ret);
		return -EINVAL;
	}

	/* make a note of our new state */
	strcpy(pps->cur_state, state_name);

	return count;
}

static DEVICE_ATTR(state, S_IRUGO|S_IWUSR, pinctrl_pseudo_state_show,
			pinctrl_pseudo_state_store);

static ssize_t available_states_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct device_node *node = dev_of_node(dev);
	const char *statename;
	int count, i, len = 0, ret;

	/*
	 * The consumer interface for pinctrl is lacking the needed
	 * query functions, so we get these from the devicetree node
	 * directly.
	 */
	of_node_get(node);
	count = of_property_count_strings(node, "pinctrl-names");
	if (count < 1) {
		len = -ENODEV;
		goto out;
	}

	for (i = 0; i < count; i++) {
		ret = of_property_read_string_index(node, "pinctrl-names",
							i, &statename);
		if (ret)
			continue;

		len += snprintf(buf + len, PAGE_SIZE - len, "%s%s",
				(len == 0) ? "" : " ", statename);
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

out:
	of_node_put(node);

	return len;
}

static DEVICE_ATTR_RO(available_states);

static struct attribute *pinctrl_pseudo_dev_attrs[] = {
	&dev_attr_state.attr,
	&dev_attr_available_states.attr,
	NULL
};

static const struct attribute_group pinctrl_pseudo_attr_group = {
	.attrs = pinctrl_pseudo_dev_attrs,
};

static int pinctrl_pseudo_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pinctrl_pseudo *pps;
	int ret;

	pps = devm_kzalloc(dev, sizeof(*pps), GFP_KERNEL);
	if (!pps)
		return -ENOMEM;

	/*
	 * We should start in PINCTRL_STATE_DEFAULT, unless someone defined
	 * an "init" state for this device.  For simplicity's sake assume
	 * we were smart enough not to do that.
	 */
	strcpy(pps->cur_state, PINCTRL_STATE_DEFAULT);
	platform_set_drvdata(pdev, pps);

	pps->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pps->pinctrl)) {
		dev_err(dev, "unable to get pinctrl: %d\n",
			(int) PTR_ERR(pps->pinctrl));
		return PTR_ERR(pps->pinctrl);
	}

	ret = sysfs_create_group(&dev->kobj, &pinctrl_pseudo_attr_group);
	if (ret) {
		dev_err(dev, "unable to register attr group: %d\n", ret);
		return ret;
	}

	return 0;
}

static int pinctrl_pseudo_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &pinctrl_pseudo_attr_group);
	return 0;
}

static const struct of_device_id pinctrl_pseudo_of_match[] = {
	{ .compatible = "pinctrl-state-pseudo", },
	{ },
};

MODULE_DEVICE_TABLE(of, pinctrl_pseudo_of_match);

static struct platform_driver pinctrl_pseudo_driver = {
	.driver = {
		.name = "pinctrl-state-pseudo",
		.of_match_table = pinctrl_pseudo_of_match,
	},
	.probe = pinctrl_pseudo_probe,
	.remove = pinctrl_pseudo_remove,
};

module_platform_driver(pinctrl_pseudo_driver);

MODULE_AUTHOR("Joshua Neal <josneal@tesla.com>");
MODULE_DESCRIPTION("Pinctrl state switching pseudo-device");
MODULE_LICENSE("GPL v2");
