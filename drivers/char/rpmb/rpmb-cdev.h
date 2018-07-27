/* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0 */
/*
 * Copyright (C) 2015-2018 Intel Corp. All rights reserved
 */
#ifdef CONFIG_RPMB_INTF_DEV
int __init rpmb_cdev_init(void);
void __exit rpmb_cdev_exit(void);
void rpmb_cdev_prepare(struct rpmb_dev *rdev);
void rpmb_cdev_add(struct rpmb_dev *rdev);
void rpmb_cdev_del(struct rpmb_dev *rdev);
#else
static inline int __init rpmb_cdev_init(void) { return 0; }
static inline void __exit rpmb_cdev_exit(void) {}
static inline void rpmb_cdev_prepare(struct rpmb_dev *rdev) {}
static inline void rpmb_cdev_add(struct rpmb_dev *rdev) {}
static inline void rpmb_cdev_del(struct rpmb_dev *rdev) {}
#endif /* CONFIG_RPMB_INTF_DEV */
