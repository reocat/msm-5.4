/*
 * arch/arm/plat-ambarella/include/plat/sd.h
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

#ifndef __PLAT_AMBARELLA_SD_H
#define __PLAT_AMBARELLA_SD_H

/* ==========================================================================*/
#define SD_MAX_SLOT_NUM				(8)

/* ==========================================================================*/
#ifndef __ASSEMBLER__

struct ambarella_sd_slot {
	int					(*check_owner)(void);
	void					(*request)(void);
	void					(*release)(void);
	struct ambarella_gpio_io_info		ext_power;
	struct ambarella_gpio_io_info		ext_reset;
	u32					max_blk_sz;
	u32					use_bounce_buffer;
	struct ambarella_gpio_irq_info		gpio_cd;
	int					cd_delay;	//jiffies
	struct ambarella_gpio_io_info		gpio_wp;
};

struct ambarella_sd_controller {
	u32					num_slots;
	struct ambarella_sd_slot		slot[SD_MAX_SLOT_NUM];
	u32					clk_limit;
	u32					wait_tmo;

	void					(*set_pll)(u32);
	u32					(*get_pll)(void);
	u32					support_pll_scaler;
	u32					max_clk;
};
#define AMBA_SD_PARAM_CALL(controller_id, slot_id, arg, perm) \
	module_param_call(sd##controller_id##_slot##slot_id##_use_bounce_buffer, param_set_int, param_get_int, &(arg.slot[slot_id].use_bounce_buffer), perm); \
	module_param_call(sd##controller_id##_slot##slot_id##_max_blk_sz, param_set_int, param_get_int, &(arg.slot[slot_id].max_blk_sz), perm); \
	module_param_call(sd##controller_id##_slot##slot_id##_cd_delay, param_set_int, param_get_int, &(arg.slot[slot_id].cd_delay), perm); \
	AMBA_GPIO_IO_MODULE_PARAM_CALL(sd##controller_id##_slot##slot_id##_power_, arg.slot[slot_id].ext_power, perm); \
	AMBA_GPIO_IO_MODULE_PARAM_CALL(sd##controller_id##_slot##slot_id##_reset_, arg.slot[slot_id].ext_reset, perm); \
	AMBA_GPIO_IRQ_MODULE_PARAM_CALL(sd##controller_id##_slot##slot_id##_cd_, arg.slot[slot_id].gpio_cd, perm); \
	AMBA_GPIO_IO_MODULE_PARAM_CALL(sd##controller_id##_slot##slot_id##_wp_, arg.slot[slot_id].gpio_wp, perm)

/* ==========================================================================*/
extern struct platform_device			ambarella_sd0;
extern struct platform_device			ambarella_sd1;

#endif /* __ASSEMBLER__ */
/* ==========================================================================*/

#endif

