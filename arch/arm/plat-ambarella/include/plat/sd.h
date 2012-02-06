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
	struct mmc_host				*pmmc_host;

	u32					use_bb;
	u32					max_blk_sz;
	u32					caps;

	struct ambarella_gpio_io_info		ext_power;
	struct ambarella_gpio_io_info		ext_reset;
	int					fixed_cd;
	struct ambarella_gpio_irq_info		gpio_cd;
	u32					cd_delay;	//jiffies
	int					fixed_wp;
	struct ambarella_gpio_io_info		gpio_wp;

	u32					dump_rw_access;

	int					(*check_owner)(void);
	void					(*request)(void);
	void					(*release)(void);
	void					(*set_int)(u32 mask, u32 on);
	int					(*set_vdd)(u32 vdd);
};

struct ambarella_sd_controller {
	u32					num_slots;
	struct ambarella_sd_slot		slot[SD_MAX_SLOT_NUM];
	void					(*set_pll)(u32);
	u32					(*get_pll)(void);

	u32					max_clk;
	u32					clk_limit;
	u32					active_clk;
	u32					support_pll_scaler;

	u32					wait_tmo;
};
#define AMBA_SD_PARAM_CALL(controller_id, slot_id, arg, cdpos, perm) \
	module_param_cb(sd##controller_id##_slot##slot_id##_use_bounce_buffer, &param_ops_uint, &(arg.slot[slot_id].use_bb), perm); \
	module_param_cb(sd##controller_id##_slot##slot_id##_dump_rw_access, &param_ops_uint, &(arg.slot[slot_id].dump_rw_access), perm); \
	module_param_cb(sd##controller_id##_slot##slot_id##_max_blk_sz, &param_ops_uint, &(arg.slot[slot_id].max_blk_sz), perm); \
	module_param_cb(sd##controller_id##_slot##slot_id##_cd_delay, &param_ops_uint, &(arg.slot[slot_id].cd_delay), perm); \
	module_param_cb(sd##controller_id##_slot##slot_id##_fixed_cd, cdpos, &(arg.slot[slot_id].fixed_cd), perm); \
	module_param_cb(sd##controller_id##_slot##slot_id##_fixed_wp, &param_ops_int, &(arg.slot[slot_id].fixed_wp), perm); \
	AMBA_GPIO_IO_MODULE_PARAM_CALL(sd##controller_id##_slot##slot_id##_power_, arg.slot[slot_id].ext_power, perm); \
	AMBA_GPIO_IO_MODULE_PARAM_CALL(sd##controller_id##_slot##slot_id##_reset_, arg.slot[slot_id].ext_reset, perm); \
	AMBA_GPIO_IRQ_MODULE_PARAM_CALL(sd##controller_id##_slot##slot_id##_cd_, arg.slot[slot_id].gpio_cd, perm); \
	AMBA_GPIO_IO_MODULE_PARAM_CALL(sd##controller_id##_slot##slot_id##_wp_, arg.slot[slot_id].gpio_wp, perm)

/* ==========================================================================*/
extern struct platform_device			ambarella_sd0;
extern struct platform_device			ambarella_sd1;

extern int ambarella_init_sd(void);
extern void ambarella_detect_sd_slot(int bus, int slot, int fixed_cd);

#endif /* __ASSEMBLER__ */
/* ==========================================================================*/

#endif

