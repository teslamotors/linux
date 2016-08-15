/*
 * Copyright (c) 2015--2016 Intel Corporation.
 *
 * Author: Tommi Franttila <tommi.franttila@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __CRLMODULE_NVM_H_
#define __CRLMODULE_NVM_H_

#include "crlmodule.h"

#define CRL_NVM_ADDR_MODE_8BIT  0x00000001
#define CRL_NVM_ADDR_MODE_16BIT 0x00000002

#define CRL_NVM_ADDR_MODE_MASK (CRL_NVM_ADDR_MODE_8BIT | \
				CRL_NVM_ADDR_MODE_16BIT)


int crlmodule_nvm_init(struct crl_sensor *sensor);
void crlmodule_nvm_deinit(struct crl_sensor *sensor);

#endif /* __CRLMODULE_NVM_H_ */
