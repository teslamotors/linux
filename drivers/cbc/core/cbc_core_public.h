/*
 * carrier boards communicatons core.
 * demultiplexes the cbc protocol.
 *
 * Copryright (C) 2014 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

/* -----------------------------------------------------------------
   - Include Protection                                            -
   ----------------------------------------------------------------- */
#ifndef CBC_CORE_PUBLIC_H
#define CBC_CORE_PUBLIC_H

#include "cbc_types.h"


/* -----------------------------------------------------------------
   - Global Definitions                                            -
   ----------------------------------------------------------------- */
/*! \brief version id for CBC version    */
#define CBC_VERSION_ID                     1U

/* brief the following function needs to be implemented on CM/IOC */
enum cbc_error target_specific_send_cbc_uart_data(u16 length
					, const u8 *raw_buffer);



#endif /* CBC_CORE_PUBLIC_H */

/* -----------------------------------------------------------------
   - end of file                                                   -
   ----------------------------------------------------------------- */
