/*
 * arch/arm/plat-ambarella/include/plat/rtc.h
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

#ifndef __PLAT_AMBARELLA_RTC_H__
#define __PLAT_AMBARELLA_RTC_H__

#include <linux/timer.h>

/* ==========================================================================*/
#if (CHIP_REV == A5S)
#define RTC_SUPPORT_EXTRA_WAKEUP_PINS		1
#define RTC_SUPPORT_30BITS_PASSED_SECONDS	1
#elif (CHIP_REV == I1) || (CHIP_REV == A7L) || (CHIP_REV == S2) || \
      (CHIP_REV == A8)
#define RTC_SUPPORT_EXTRA_WAKEUP_PINS		1
#define RTC_SUPPORT_30BITS_PASSED_SECONDS	0
#else
#define RTC_SUPPORT_EXTRA_WAKEUP_PINS		0
#define RTC_SUPPORT_30BITS_PASSED_SECONDS	0
#endif

#if (CHIP_REV == I1) || (CHIP_REV == A7L) || (CHIP_REV == S2) || \
    (CHIP_REV == A8)
#define RTC_SUPPORT_GPIO_PAD_PULL_CTRL		1
#define RTC_POWER_LOST_DETECT			1
#define RTC_SW_POWER_LOST_DETECT		0
#else
#define RTC_SUPPORT_GPIO_PAD_PULL_CTRL		0
#define RTC_POWER_LOST_DETECT			0
#define RTC_SW_POWER_LOST_DETECT		0
#endif

#if (CHIP_REV == I1) || (CHIP_REV == A7L) || (CHIP_REV == S2) || \
    (CHIP_REV == A8)
#define RTC_SEQ_LEVEL_CONFIGURABLE		1
#define RTC_WAKEUP_CTRL				1
#define RTC_LOW_BATTERY_DETECT			1
#else
#define RTC_SEQ_LEVEL_CONFIGURABLE		0
#define RTC_WAKEUP_CTRL				0
#define RTC_LOW_BATTERY_DETECT			0
#endif

#if (CHIP_REV == I1)
#define RTC_PWR_LOSS_DETECT_BIT			5
#define RTC_PWC_LOSS_MASK			0x20
#elif (CHIP_REV == A7L) || (CHIP_REV == S2) || (CHIP_REV == A8) /* FIXME */
#define RTC_PWR_LOSS_DETECT_BIT			6
#define RTC_PWC_LOSS_MASK			0x40
#else
#define RTC_PWR_LOSS_DETECT_BIT			1
#define RTC_PWC_LOSS_MASK			0x2
#endif

/* ==========================================================================*/
#define RTC_OFFSET			0xD000
#define RTC_BASE			(APB_BASE + RTC_OFFSET)
#define RTC_REG(x)			(RTC_BASE + (x))

/* ==========================================================================*/
#define RTC_POS0_OFFSET			0x20
#define RTC_POS1_OFFSET			0x24
#define RTC_POS2_OFFSET			0x28
#define RTC_PWC_ALAT_OFFSET		0x2c
#define RTC_PWC_CURT_OFFSET		0x30
#define RTC_CURT_OFFSET			0x34
#define RTC_ALAT_OFFSET			0x38
#define RTC_STATUS_OFFSET		0x3c
#define RTC_RESET_OFFSET		0x40

#if (RTC_SUPPORT_EXTRA_WAKEUP_PINS == 1)
#define RTC_WO_OFFSET			0x7c
#endif

#if (RTC_SUPPORT_GPIO_PAD_PULL_CTRL == 1)
#define RTC_GPIO_PULL_EN_0_OFFSET	0x80
#define RTC_GPIO_PULL_EN_1_OFFSET	0x84
#define RTC_GPIO_PULL_EN_2_OFFSET	0x88
#define RTC_GPIO_PULL_EN_3_OFFSET	0x8c
#define RTC_GPIO_PULL_EN_4_OFFSET	0x90
#define RTC_GPIO_PULL_EN_5_OFFSET	0x54
#define RTC_GPIO_PULL_DIR_0_OFFSET	0x94
#define RTC_GPIO_PULL_DIR_1_OFFSET	0x98
#define RTC_GPIO_PULL_DIR_2_OFFSET	0x9c
#define RTC_GPIO_PULL_DIR_3_OFFSET	0xa0
#define RTC_GPIO_PULL_DIR_4_OFFSET	0xa4
#define RTC_GPIO_PULL_DIR_5_OFFSET	0x58

#if (CHIP_REV == I1)
#define RTC_RTC_CTL_OFFSET		0xff
#else
#define RTC_RTC_CTL_OFFSET		0xfc
#endif

#endif

#define RTC_PWC_DNALERT_OFFSET		0xa8
#define RTC_PWC_LBAT_OFFSET		0xac
#define RTC_PWC_REG_WKUPC_OFFSET	0xb0
#define RTC_PWC_REG_STA_OFFSET		0xb4
#define RTC_PWC_SET_STATUS_OFFSET	0xc0
#define RTC_PWC_CLR_REG_WKUPC_OFFSET	0xc4
#define RTC_PWC_WKENC_OFFSET		0xc8

#define RTC_POS3_OFFSET			0xd0

#define RTC_PWC_ENP1C_OFFSET		0xd4
#define RTC_PWC_ENP2C_OFFSET		0xd8
#define RTC_PWC_ENP3C_OFFSET		0xdc
#define RTC_PWC_ENP1_OFFSET		0xe0
#define RTC_PWC_ENP2_OFFSET		0xe4
#define RTC_PWC_ENP3_OFFSET		0xe8

#define RTC_PWC_DISP1C_OFFSET		0xec
#define RTC_PWC_DISP2C_OFFSET		0xf0
#define RTC_PWC_DISP3C_OFFSET		0xf4
#define RTC_PWC_DISP1_OFFSET		0xf8
#define RTC_PWC_DISP2_OFFSET		0xb8
#define RTC_PWC_DISP3_OFFSET		0xbc

/* Registers */

#define RTC_POS0_REG			RTC_REG(RTC_POS0_OFFSET)
#define RTC_POS1_REG			RTC_REG(RTC_POS1_OFFSET)
#define RTC_POS2_REG			RTC_REG(RTC_POS2_OFFSET)
#define RTC_POS3_REG			RTC_REG(RTC_POS3_OFFSET)
#define RTC_PWC_ALAT_REG		RTC_REG(RTC_PWC_ALAT_OFFSET)
#define RTC_PWC_CURT_REG		RTC_REG(RTC_PWC_CURT_OFFSET)
#define RTC_CURT_REG			RTC_REG(RTC_CURT_OFFSET)
#define RTC_ALAT_REG			RTC_REG(RTC_ALAT_OFFSET)
#define RTC_STATUS_REG			RTC_REG(RTC_STATUS_OFFSET)
#define RTC_RESET_REG			RTC_REG(RTC_RESET_OFFSET)

#if (RTC_SUPPORT_EXTRA_WAKEUP_PINS == 1)
#define RTC_WO_REG			RTC_REG(RTC_WO_OFFSET)
#endif

#if (RTC_SUPPORT_GPIO_PAD_PULL_CTRL == 1)
#define RTC_GPIO_PULL_EN_0_REG		RTC_REG(RTC_GPIO_PULL_EN_0_OFFSET)
#define RTC_GPIO_PULL_EN_1_REG		RTC_REG(RTC_GPIO_PULL_EN_1_OFFSET)
#define RTC_GPIO_PULL_EN_2_REG		RTC_REG(RTC_GPIO_PULL_EN_2_OFFSET)
#define RTC_GPIO_PULL_EN_3_REG		RTC_REG(RTC_GPIO_PULL_EN_3_OFFSET)
#define RTC_GPIO_PULL_EN_4_REG		RTC_REG(RTC_GPIO_PULL_EN_4_OFFSET)
#define RTC_GPIO_PULL_EN_5_REG		RTC_REG(RTC_GPIO_PULL_EN_5_OFFSET)
#define RTC_GPIO_PULL_DIR_0_REG		RTC_REG(RTC_GPIO_PULL_DIR_0_OFFSET)
#define RTC_GPIO_PULL_DIR_1_REG		RTC_REG(RTC_GPIO_PULL_DIR_1_OFFSET)
#define RTC_GPIO_PULL_DIR_2_REG		RTC_REG(RTC_GPIO_PULL_DIR_2_OFFSET)
#define RTC_GPIO_PULL_DIR_3_REG		RTC_REG(RTC_GPIO_PULL_DIR_3_OFFSET)
#define RTC_GPIO_PULL_DIR_4_REG		RTC_REG(RTC_GPIO_PULL_DIR_4_OFFSET)
#define RTC_GPIO_PULL_DIR_5_REG		RTC_REG(RTC_GPIO_PULL_DIR_5_OFFSET)
#endif

#define RTC_RTC_CTL_REG			RTC_REG(RTC_RTC_CTL_OFFSET)

#define RTC_PWC_DNALERT_REG		RTC_REG(RTC_PWC_DNALERT_OFFSET)
#define RTC_PWC_LBAT_REG		RTC_REG(RTC_PWC_LBAT_OFFSET)
#define RTC_PWC_REG_WKUPC_REG		RTC_REG(RTC_PWC_REG_WKUPC_OFFSET)
#define RTC_PWC_REG_STA_REG		RTC_REG(RTC_PWC_REG_STA_OFFSET)
#define RTC_PWC_SET_STATUS_REG		RTC_REG(RTC_PWC_SET_STATUS_OFFSET)
#define RTC_PWC_CLR_REG_WKUPC_REG	RTC_REG(RTC_PWC_CLR_REG_WKUPC_OFFSET)
#define RTC_PWC_WKENC_REG		RTC_REG(RTC_PWC_WKENC_OFFSET)

/* Ties pseq* low / high */
#define RTC_PWC_ENP1C_REG		RTC_REG(RTC_PWC_ENP1C_OFFSET)
#define RTC_PWC_ENP2C_REG		RTC_REG(RTC_PWC_ENP2C_OFFSET)
#define RTC_PWC_ENP3C_REG		RTC_REG(RTC_PWC_ENP3C_OFFSET)
#define RTC_PWC_ENP1_REG		RTC_REG(RTC_PWC_ENP1_OFFSET)
#define RTC_PWC_ENP2_REG		RTC_REG(RTC_PWC_ENP2_OFFSET)
#define RTC_PWC_ENP3_REG		RTC_REG(RTC_PWC_ENP3_OFFSET)

#define RTC_PWC_DISP1C_REG		RTC_REG(RTC_PWC_DISP1C_OFFSET)
#define RTC_PWC_DISP2C_REG		RTC_REG(RTC_PWC_DISP2C_OFFSET)
#define RTC_PWC_DISP3C_REG		RTC_REG(RTC_PWC_DISP3C_OFFSET)
#define RTC_PWC_DISP1_REG		RTC_REG(RTC_PWC_DISP1_OFFSET)
#define RTC_PWC_DISP2_REG		RTC_REG(RTC_PWC_DISP2_OFFSET)
#define RTC_PWC_DISP3_REG		RTC_REG(RTC_PWC_DISP3_OFFSET)


/* RTC_STATUS_REG */
#define RTC_STATUS_WKUP			0x8
#define RTC_STATUS_ALA_WK		0x4
#define RTC_STATUS_PC_RST		0x2
#define RTC_STATUS_RTC_CLK		0x1

/* RTC_SEQ_NUM */
#define PWC_SEQ1			1
#define PWC_SEQ2			2
#define PWC_SEQ3			3
#define PWC_SEQ1C			4
#define PWC_SEQ2C			5
#define PWC_SEQ3C			6

/* RTC_WAKEUP_CTRL */
#define RTC_WAKEUP_CTRL_USB		1
#define RTC_WAKEUP_CTRL_SDCARD		2
#define RTC_WAKEUP_CTRL_ETHNET		4

/* ==========================================================================*/
#ifndef __ASSEMBLER__

struct ambarella_rtc_controller {
	u8					pos0;
	u8					pos1;
	u8					pos2;
	int					(*check_capacity)(u32);
	u32					(*check_status)(void);
	void					(*set_curt_time)(u32);
	u32					(*get_curt_time)(void);
	void					(*set_alat_time)(u32);
	u32					(*get_alat_time)(void);
	u32					reset_delay;
	struct  timer_list          alarm_polling_timer;
};
#define AMBA_RTC_PARAM_CALL(id, arg, perm, param_ops_rtcpos) \
	module_param_cb(rtc##id##_pos0, &param_ops_byte, &(arg.pos0), perm); \
	module_param_cb(rtc##id##_pos1, &param_ops_byte, &(arg.pos1), perm); \
	module_param_cb(rtc##id##_pos2, &param_ops_byte, &(arg.pos2), perm); \
	module_param_cb(rtc##id##_setpos, &param_ops_rtcpos, NULL, 0200)

/* ==========================================================================*/
extern struct platform_device ambarella_rtc0;

#endif /* __ASSEMBLER__ */
/* ==========================================================================*/

#endif /* __PLAT_AMBARELLA_RTC_H__ */

