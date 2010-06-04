/*
 * arch/arm/plat-ambarella/include/plat/idc.h
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

#ifndef __PLAT_AMBARELLA_IDC_H
#define __PLAT_AMBARELLA_IDC_H

/* ==========================================================================*/

/* ==========================================================================*/
#ifndef __ASSEMBLER__

struct ambarella_idc_platform_info {
	int					clk_limit;	//Hz
	int					bulk_write_num;
	unsigned int				i2c_class;
	void					(*set_pin_muxing)(u32 on);
	u32					(*get_clock)(void);
};
#define AMBA_IDC_PARAM_CALL(id, arg, perm) \
	module_param_call(idc##id##_clk_limit, param_set_int, param_get_int, &(arg.clk_limit), perm); \
	module_param_call(idc##id##_bulk_write_num, param_set_int, param_get_int, &(arg.bulk_write_num), perm)

/* ==========================================================================*/
extern struct platform_device			ambarella_idc0;
extern struct platform_device			ambarella_idc1;

#endif /* __ASSEMBLER__ */
/* ==========================================================================*/

#endif

