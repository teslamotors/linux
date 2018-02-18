/*
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __LINUX_PADCTRL_H
#define __LINUX_PADCTRL_H

#include <linux/device.h>
#include <linux/of.h>

struct padctrl;
struct padctrl_dev;

/*
 * padctrl_ops: Callback API to implement the padcontrol specific ops.
 */
struct padctrl_ops {
	int (*set_voltage)(struct padctrl_dev *pad_dev,
			int pad_id, u32 voltage);
	int (*get_voltage)(struct padctrl_dev *pad_dev,
			int pad_id, u32 *voltage);
	int (*power_enable)(struct padctrl_dev *pad_dev, int pad_id);
	int (*power_disable)(struct padctrl_dev *pad_dev, int pad_id);
};

/*
 * padctrl_desc: Padcontrol description describe the HW behavior.
 */
struct padctrl_desc {
	const char *name;
	struct padctrl_ops *ops;
};

/*
 * padctrl_confif: Padcontrol configuration.
 */
struct padctrl_config {
	const char *name;
	struct device_node *of_node;
};

#ifdef CONFIG_PADCTRL
extern struct padctrl *padctrl_get(struct device *dev, const char *name);
extern void padctrl_put(struct padctrl *pctrl);

extern struct padctrl *devm_padctrl_get(struct device *dev, const char *name);
extern struct padctrl *devm_padctrl_get_from_node(struct device *dev,
						  struct device_node *np,
						  const char *name);

extern int padctrl_set_voltage(struct padctrl *pctrl, u32 voltage);
extern int padctrl_get_voltage(struct padctrl *pctrl, u32 *voltage);

extern int padctrl_power_enable(struct padctrl *pctrl);
extern int padctrl_power_disable(struct padctrl *pctrl);

extern struct padctrl_dev *padctrl_register(struct device *dev,
	struct padctrl_desc *desc, struct padctrl_config *config);
extern void padctrl_unregister(struct padctrl_dev *pad_dev);

extern struct padctrl_dev *devm_padctrl_register(struct device *dev,
		struct padctrl_desc *desc, struct padctrl_config *config);
extern void devm_padctrl_unregister(struct device *dev,
		struct padctrl_dev *pad_dev);

extern void padctrl_set_drvdata(struct padctrl_dev *pad_dev, void *drv_data);
extern void *padctrl_get_drvdata(struct padctrl_dev *pad_dev);

#else

static inline struct padctrl *padctrl_get(struct device *dev,
		const char *name)
{
	return ERR_PTR(-ENODEV);
}

static inline void *padctrl_put(struct padctrl *pctrl)
{
	return ERR_PTR(-ENODEV);
}

static inline struct padctrl *devm_padctrl_get(struct device *dev,
	const char *name)
{
	return ERR_PTR(-ENODEV);
}

static inline struct padctrl *devm_padctrl_get_from_node(struct device *dev,
		struct device_node *np, const char *name)
{
	return ERR_PTR(-ENODEV);
}

static inline int padctrl_set_voltage(struct padctrl *pctrl, u32 voltage)
{
	return -EINVAL;
}

static inline int padctrl_get_voltage(struct padctrl *ctrl, u32 *voltage)
{
	return -EINVAL;
}

static inline int padctrl_power_enable(struct padctrl *pctrl)
{
	return -EINVAL;
}

static inline int padctrl_power_disable(struct padctrl *pctrl)
{
	return -EINVAL;
}

static inline struct padctrl_dev *padctrl_register(struct device *dev,
	struct padctrl_desc *desc, struct padctrl_config *config)
{
	return ERR_PTR(-ENODEV);
}
static inline void padctrl_unregister(struct padctrl_dev *pad_dev)
{
}

static inline struct padctrl_dev *devm_padctrl_register(struct device *dev,
		struct padctrl_desc *desc, struct padctrl_config *config)
{
	return ERR_PTR(-ENODEV);
}
static inline void devm_padctrl_unregister(struct device *dev,
		struct padctrl_dev *pad_dev)
{
}
static inline void padctrl_set_drvdata(struct padctrl_dev *pad_dev,
			void *drv_data)
{
}

static inline void *padctrl_get_drvdata(struct padctrl_dev *pad_dev)
{
	return NULL;
}
#endif

#endif
