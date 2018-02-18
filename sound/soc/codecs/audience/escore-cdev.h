/*
 * escore-slim.h  --  Audience eS705 character device interface.
 *
 * Copyright 2013 Audience, Inc.
 *
 * Author: Marc Butler <mbutler@audience.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _ESCORE_CDEV_H
#define _ESCORE_CDEV_H

/* This interface is used to support development and deployment
 * tasks. It does not replace ALSA as a controll interface.
 */

int escore_cdev_init(struct escore_priv *escore);
void escore_cdev_cleanup(struct escore_priv *escore);

#endif
