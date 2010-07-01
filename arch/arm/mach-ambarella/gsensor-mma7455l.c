/*
 * arch/arm/mach-ambarella/gsensor-mma7455l.c
 *
 * Author: Zhangkai Wang <zkwang@ambarella.com>
 *
 * Copyright (C) 2004-2010, Ambarella, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/i2c.h>

#include <linux/i2c/mma7455l.h>

struct mma7455l_platform_data ambarella_mma7455l_platform_data = {
	.g_select=4,
	.mode=1
};



/* ==========================================================================*/
struct i2c_board_info ambarella_mma7455l_board_info = {
	.type = "mma7455l",
	.addr = 0x1d,
	.platform_data = &ambarella_mma7455l_platform_data,
};