/*
 * arch/arm/plat-ambarella/generic/idc.c
 *
 * Author: Anthony Ginger <hfjiang@ambarella.com>
 *
 * Copyright (C) 2004-2010, Ambarella, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/i2c.h>
#include <linux/i2c-mux-ambarella.h>
#include <linux/moduleparam.h>
#include <linux/clk.h>
#include <mach/hardware.h>
#include <plat/idc.h>

#if (IDC_SUPPORT_INTERNAL_MUX == 1)
static struct ambarella_i2cmux_platform_data ambarella_i2cmux_info = {
	.parent		= 0,
	.number		= 2,
	.gpio		= IDC3_BUS_MUX,
	.select_function	= GPIO_FUNC_HW,
	.deselect_function	= GPIO_FUNC_SW_INPUT,
};

struct platform_device ambarella_idc0_mux = {
	.name		= "ambarella-i2cmux",
	.id		= 0,
	.dev		= {
		.platform_data		= &ambarella_i2cmux_info,
	}
};
#endif



