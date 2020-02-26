/*
 * Analogix ANX1122 (e)DP/LVDS bridge driver
 *
 * Copyright (C) 2017 Codethink Ltd.
 * Author:
 * 	Thomas Preston <thomas.preston@codethink.co.uk>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _ANX1122_H_
#define _ANX1122_H_

#include <drm/drm_crtc.h>

/**
 * anx1122_bridge_init() - Initialise the ANX1122 bridge.
 * @encoder:	The encoder this bridge will be attached to
 */
int anx1122_bridge_init(struct drm_encoder *encoder);

#endif /* _ANX1122_H_ */
