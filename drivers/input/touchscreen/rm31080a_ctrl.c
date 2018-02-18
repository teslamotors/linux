/*
 * Raydium RM31080 touchscreen driver
 *
 * Copyright (C) 2012-2014, Raydium Semiconductor Corporation.
 * All Rights Reserved.
 * Copyright (C) 2012-2014, NVIDIA Corporation.  All Rights Reserved.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
/*=============================================================================
	INCLUDED FILES
=============================================================================*/
#include <linux/device.h>
#include <linux/uaccess.h>	/* copy_to_user() */
#include <linux/delay.h>
#include <linux/module.h>	/* Module definition */

#include <linux/spi/rm31080a_ts.h>
#include <linux/spi/rm31080a_ctrl.h>

/*=============================================================================
	GLOBAL VARIABLES DECLARATION
=============================================================================*/
struct rm_tch_ctrl_para g_st_ctrl;

/*=============================================================================
	FUNCTION DECLARATION
=============================================================================*/
/*=============================================================================
	Description:

	Input:
			N/A
	Output:
			N/A
=============================================================================*/
void rm_tch_ctrl_init(void)
{
	memset(&g_st_ctrl, 0, sizeof(struct rm_tch_ctrl_para));
}

/*=============================================================================
	Description: To transfer the value to HAL layer

	Input:
			N/A
	Output:
			N/A
=============================================================================*/
unsigned char rm_tch_ctrl_get_idle_mode(u8 *p)
{
	u32 u32Ret;
	u32Ret = copy_to_user(p,
		&g_st_ctrl.u8_idle_mode_check, 1);
	if (u32Ret)
		return RETURN_FAIL;
	return RETURN_OK;
}

/*=============================================================================
	Description:

	Input:
			N/A
	Output:
			N/A
=============================================================================*/
void rm_tch_ctrl_set_parameter(void *arg)
{
	ssize_t missing;
	missing = copy_from_user(&g_st_ctrl,
		arg, sizeof(struct rm_tch_ctrl_para));
	if (missing)
		return;
}

/*===========================================================================*/
MODULE_AUTHOR("xxxxxxxxxx <xxxxxxxx@rad-ic.com>");
MODULE_DESCRIPTION("Raydium touchscreen control functions");
MODULE_LICENSE("GPL");
