/**
* Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2015, Intel Corporation.
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

#ifndef IRQ_SUPPORT_INCLUDED_H_
#define IRQ_SUPPORT_INCLUDED_H_

#if defined(_MSC_VER) || defined(__KERNEL__)
#define vied_subsystem_register_interrupt_handler(handler)
#else
#define vied_subsystem_register_interrupt_handler(handler) \
  hrt_register_interrupt_handler(handler)
#endif

#endif /*IRQ_SUPPORT_INCLUDED_H_*/
