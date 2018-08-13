/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2015 - 2018 Intel Corporation
 *
 * Author: Tommi Franttila <tommi.franttila@intel.com>
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
