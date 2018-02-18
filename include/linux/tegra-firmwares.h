/*
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */
#ifndef __TEGRA_FIRMWARES_H
#define __TEGRA_FIRMWARES_H

/*
 * max size of version string
 */
static const size_t TFW_VERSION_MAX_SIZE = 256;

#if IS_ENABLED(CONFIG_TEGRA_FIRMWARES_CLASS)

struct device *tegrafw_register(const char *name,
			const u32 flags,
			ssize_t (*reader)(struct device *dev, char *, size_t),
			const char *string);
void tegrafw_unregister(struct device *fwdev);
struct device *devm_tegrafw_register(struct device *dev,
			const char *name,
			const u32 flags,
			ssize_t (*reader)(struct device *dev, char *, size_t),
			const char *string);
void devm_tegrafw_unregister(struct device *dev, struct device *fwdev);
void tegrafw_invalidate(struct device *fwdev);
struct device *devm_tegrafw_register_dt_string(struct device *dev,
						const char *name,
						const char *path,
						const char *property);
#else

static inline struct device *tegrafw_register(const char *name,
			const u32 flags,
			ssize_t (*reader)(struct device *, char *, size_t),
			const char *string)
{
	return ERR_PTR(-ENOTSUPP);
}

static inline void tegrafw_unregister(struct device *fwdev)
{
}

static inline void tegrafw_invalidate(struct device *fwdev)
{
}

static inline struct device *devm_tegrafw_register(struct device *dev,
			const char *name,
			const u32 flags,
			ssize_t (*reader)(struct device *dev, char *, size_t),
			const char *string)
{
	return ERR_PTR(-ENOTSUPP);
}

static inline void devm_tegrafw_unregister(struct device *dev,
			struct device *fwdev)
{
}

static inline
struct device *devm_tegrafw_register_dt_string(struct device *dev,
						const char *name,
						const char *path,
						const char *property)
{
	return ERR_PTR(-ENOTSUPP);
}

#endif /* IS_ENABLED(...) */

enum {
	TFW_NORMAL	= 0x0000,
	TFW_DONT_CACHE	= 0x0001,
	TFW_MAX		= 0xFFFF,
};

static inline struct device *tegrafw_register_string(const char *name,
						     const char *string)
{
	return tegrafw_register(name, TFW_NORMAL, NULL, string);
}

static inline struct device *devm_tegrafw_register_string(struct device *dev,
						     const char *name,
						     const char *string)
{
	return devm_tegrafw_register(dev, name, TFW_NORMAL, NULL, string);
}

static inline struct device *tegrafw_register_dt_string(const char *name,
							const char *path,
							const char *property)
{
	return devm_tegrafw_register_dt_string(NULL, name, path, property);
}

#endif /* __TEGRA_FIRMWARES_CLASS */
