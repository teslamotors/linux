/* SPDX-License-Identifier: GPL-2.0 */
/*
 * CBC line discipline kernel module.
 * Handles Carrier Board Communications (CBC) protocol.
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef CBC_CORE_PUBLIC_H
#define CBC_CORE_PUBLIC_H

#include "cbc_types.h"

/* CBC version.	*/
#define CBC_VERSION_ID		1

/* The following function needs to be implemented on CM/IOC. */
enum cbc_error target_specific_send_cbc_uart_data(u16 length,
						const u8 *raw_buffer);


#endif /* CBC_CORE_PUBLIC_H */

