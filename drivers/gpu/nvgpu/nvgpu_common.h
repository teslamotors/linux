/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NVGPU_COMMON_H
#define NVGPU_COMMON_H

struct gk20a;

int nvgpu_probe(struct gk20a *g,
		const char *debugfs_symlink,
		const char *interface_name,
		struct class *class);

#define NVGPU_REQUEST_FIRMWARE_NO_WARN		BIT(0)
#define NVGPU_REQUEST_FIRMWARE_NO_SOC		BIT(1)

const struct firmware *nvgpu_request_firmware(struct gk20a *g,
					      const char *fw_name,
					      int flags);

#endif
