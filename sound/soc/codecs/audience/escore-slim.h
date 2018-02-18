/*
 * escore-slim.h  --  Slimbus interface for Audience earSmart chips
 *
 * Copyright 2011 Audience, Inc.
 *
 * Author: Greg Clemson <gclemson@audience.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ESCORE_SLIM_H
#define _ESCORE_SLIM_H

#define ES_SLIM_BOOT_CMD			0x0001
#define ES_SLIM_BOOT_ACK			0x01010101

enum escore_slim_device_class {
	ESCORE_DEVICE_NONE = 0, /* for prev verison compatibility */
	ESCORE_INTERFACE_DEVICE = 1,
	ESCORE_GENERIC_DEVICE = 2,

	ESCORE_MAX_DEVICE_CLASS,
};


extern int escore_slimbus_init(void);
extern void es515_slim_setup(struct escore_priv *escore_priv);
#ifdef CONFIG_ARCH_MSM
extern const struct slim_device_id escore_slim_id[];
#endif
extern struct slim_driver escore_slim_driver;
extern struct snd_soc_dai_ops escore_slim_port_dai_ops;
int escore_cfg_slim_rx(struct slim_device *sbdev, unsigned int *ch_num,
			     unsigned int ch_cnt, unsigned int rate);
int escore_close_slim_rx(struct slim_device *sbdev, unsigned int *ch_num,
			       unsigned int ch_cnt);
int escore_cfg_slim_tx(struct slim_device *sbdev, unsigned int *ch_num,
			     unsigned int ch_cnt, unsigned int rate);
int escore_close_slim_tx(struct slim_device *sbdev, unsigned int *ch_num,
			       unsigned int ch_cnt);
void escore_slim_get_logical_addr(struct escore_priv *escore);

#endif
