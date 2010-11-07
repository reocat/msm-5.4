/*
 * arch/arm/plat-ambarella/include/plat/eth.h
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

#ifndef __PLAT_AMBARELLA_ETH_H
#define __PLAT_AMBARELLA_ETH_H

/* ==========================================================================*/
#define AMBETH_MAC_SIZE				(6)

/* ==========================================================================*/
#ifndef __ASSEMBLER__

struct ambarella_eth_platform_info {
	u8					mac_addr[AMBETH_MAC_SIZE];
	int					napi_weight;
	int					max_work_count;
	int					mii_id;
	u32					phy_id;
	struct ambarella_gpio_io_info		mii_power;
	struct ambarella_gpio_io_info		mii_reset;
	int					default_clk;
	int					default_1000_clk;
	int					default_100_clk;
	int					default_10_clk;

	int					(*is_enabled)(void);
	int					(*is_supportclk)(void);
	void					(*setclk)(u32 freq);
	u32					(*getclk)(void);
	u32					(*get_supported)(void);
};
#define AMBA_ETH_PARAM_CALL(id, arg, perm) \
	module_param_cb(eth##id##_napi_weight, &param_ops_int, &(arg.napi_weight), perm); \
	module_param_cb(eth##id##_max_work_count, &param_ops_int, &(arg.max_work_count), perm); \
	module_param_cb(eth##id##_mii_id, &param_ops_int, &(arg.mii_id), perm); \
	module_param_cb(eth##id##_phy_id, &param_ops_uint, &(arg.phy_id), perm); \
	module_param_cb(eth##id##_default_clk, &param_ops_uint, &(arg.default_clk), perm); \
	AMBA_GPIO_IO_MODULE_PARAM_CALL(eth##id##_mii_power_, arg.mii_power, perm); \
	AMBA_GPIO_RESET_MODULE_PARAM_CALL(eth##id##_mii_reset_, arg.mii_reset, perm)

/* ==========================================================================*/
extern struct platform_device			ambarella_eth0;

/* ==========================================================================*/
extern int ambarella_init_eth0(unsigned int high, unsigned int low);

#endif /* __ASSEMBLER__ */
/* ==========================================================================*/

#endif

