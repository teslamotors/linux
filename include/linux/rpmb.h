/* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0 */
/*
 * Copyright (C) 2015-2018 Intel Corp. All rights reserved
 */
#ifndef __RPMB_H__
#define __RPMB_H__

#include <linux/types.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <uapi/linux/rpmb.h>
#include <linux/kref.h>

#define RPMB_API_VERSION 0x80000001

extern struct class rpmb_class;

/**
 * struct rpmb_cmd: rpmb access command
 *
 * @flags:   command flags
 *      0 - read command
 *      1 - write command RPMB_F_WRITE
 *      2 - reliable write RPMB_F_REL_WRITE
 * @nframes: number of rpmb frames in the command
 * @frames:  list of rpmb frames
 */
struct rpmb_cmd {
	u32 flags;
	u32 nframes;
	void *frames;
};

/**
 * struct rpmb_ops - RPMB ops to be implemented by underlying block device
 *
 * @cmd_seq        : send RPMB command sequence to the RPBM partition
 *                   backed by the storage device to specific
 *                   region(UFS)/target(NVMe)
 * @get_capacity   : rpmb size in 128K units in for region/target.
 * @type           : block device type eMMC, UFS, NVMe.
 * @block_size     : block size in half sectors (1 == 256B)
 * @wr_cnt_max     : maximal number of blocks that can be
 *                   written in one access.
 * @rd_cnt_max     : maximal number of blocks that can be
 *                   read in one access.
 * @auth_method    : rpmb_auth_method
 * @dev_id         : unique device identifier
 * @dev_id_len     : unique device identifier length
 */
struct rpmb_ops {
	int (*cmd_seq)(struct device *dev, u8 target,
		       struct rpmb_cmd *cmds, u32 ncmds);
	int (*get_capacity)(struct device *dev, u8 target);
	u32 type;
	u16 block_size;
	u16 wr_cnt_max;
	u16 rd_cnt_max;
	u16 auth_method;
	const u8 *dev_id;
	size_t dev_id_len;
};

/**
 * struct rpmb_dev - device which can support RPMB partition
 *
 * @lock       : the device lock
 * @dev        : device
 * @id         : device id
 * @target     : RPMB target/region within the physical device
 * @cdev       : character dev
 * @status     : device status
 * @ops        : operation exported by block layer
 */
struct rpmb_dev {
	struct mutex lock; /* device serialization lock */
	struct device dev;
	int id;
	u8 target;
#ifdef CONFIG_RPMB_INTF_DEV
	struct cdev cdev;
	unsigned long status;
#endif /* CONFIG_RPMB_INTF_DEV */
	const struct rpmb_ops *ops;
};

#define to_rpmb_dev(x) container_of((x), struct rpmb_dev, dev)

#if IS_ENABLED(CONFIG_RPMB)
struct rpmb_dev *rpmb_dev_get(struct rpmb_dev *rdev);
void rpmb_dev_put(struct rpmb_dev *rdev);
struct rpmb_dev *rpmb_dev_find_by_device(struct device *parent, u8 target);
struct rpmb_dev *rpmb_dev_get_by_type(u32 type);
struct rpmb_dev *rpmb_dev_register(struct device *dev, u8 target,
				   const struct rpmb_ops *ops);
void *rpmb_dev_get_drvdata(const struct rpmb_dev *rdev);
void rpmb_dev_set_drvdata(struct rpmb_dev *rdev, void *data);
int rpmb_dev_unregister(struct rpmb_dev *rdev);
int rpmb_dev_unregister_by_device(struct device *dev, u8 target);
int rpmb_cmd_seq(struct rpmb_dev *rdev, struct rpmb_cmd *cmds, u32 ncmds);
int rpmb_get_capacity(struct rpmb_dev *rdev);

#else
static inline struct rpmb_dev *rpmb_dev_get(struct rpmb_dev *rdev)
{
	return NULL;
}

static inline void rpmb_dev_put(struct rpmb_dev *rdev) { }

static inline struct rpmb_dev *rpmb_dev_find_by_device(struct device *parent,
						       u8 target)
{
	return NULL;
}

static inline
struct rpmb_dev *rpmb_dev_get_by_type(enum rpmb_type type)
{
	return NULL;
}

static inline void *rpmb_dev_get_drvdata(const struct rpmb_dev *rdev)
{
	return NULL;
}

static inline void rpmb_dev_set_drvdata(struct rpmb_dev *rdev, void *data)
{
}

static inline struct rpmb_dev *
rpmb_dev_register(struct device *dev, u8 target, const struct rpmb_ops *ops)
{
	return NULL;
}

static inline int rpmb_dev_unregister(struct rpmb_dev *dev)
{
	return 0;
}

static inline int rpmb_dev_unregister_by_device(struct device *dev, u8 target)
{
	return 0;
}

static inline int rpmb_cmd_seq(struct rpmb_dev *rdev,
			       struct rpmb_cmd *cmds, u32 ncmds)
{
	return 0;
}

static inline int rpmb_get_capacity(struct rpmb_dev *rdev)
{
	return 0;
}

#endif /* CONFIG_RPMB */

#endif /* __RPMB_H__ */
