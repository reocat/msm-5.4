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
#define RTC_SUPPORT_30BITS_PASSED_SECONDS	1
#else
#define RTC_SUPPORT_30BITS_PASSED_SECONDS	0
#endif

#if (CHIP_REV == I1) || (CHIP_REV == A7L) || (CHIP_REV == S2) || \
    (CHIP_REV == A8)
#define RTC_POWER_LOST_DETECT			1
#else
#define RTC_POWER_LOST_DETECT			0
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
#define RTC_PWC_ALAT_OFFSET		0x2C
#define RTC_PWC_CURT_OFFSET		0x30
#define RTC_CURT_OFFSET			0x34
#define RTC_ALAT_OFFSET			0x38
#define RTC_STATUS_OFFSET		0x3C
#define RTC_RESET_OFFSET		0x40
#define RTC_WO_OFFSET			0x7C

#define RTC_PWC_DNALERT_OFFSET		0xA8
#define RTC_PWC_LBAT_OFFSET		0xAC
#define RTC_PWC_REG_WKUPC_OFFSET	0xB0
#define RTC_PWC_REG_STA_OFFSET		0xB4
#define RTC_PWC_SET_STATUS_OFFSET	0xC0
#define RTC_PWC_CLR_REG_WKUPC_OFFSET	0xC4
#define RTC_PWC_WKENC_OFFSET		0xC8
#define RTC_POS3_OFFSET			0xD0

#define RTC_PWC_ENP1C_OFFSET		0xD4
#define RTC_PWC_ENP2C_OFFSET		0xD8
#define RTC_PWC_ENP3C_OFFSET		0xDC
#define RTC_PWC_ENP1_OFFSET		0xE0
#define RTC_PWC_ENP2_OFFSET		0xE4
#define RTC_PWC_ENP3_OFFSET		0xE8
#define RTC_PWC_DISP1C_OFFSET		0xEC
#define RTC_PWC_DISP2C_OFFSET		0xF0
#define RTC_PWC_DISP3C_OFFSET		0xF4
#define RTC_PWC_DISP1_OFFSET		0xF8
#define RTC_PWC_DISP2_OFFSET		0xB8
#define RTC_PWC_DISP3_OFFSET		0xBC

/* ==========================================================================*/
/* RTC_STATUS_REG */
#define RTC_STATUS_WKUP			0x8
#define RTC_STATUS_ALA_WK		0x4
#define RTC_STATUS_PC_RST		0x2
#define RTC_STATUS_RTC_CLK		0x1

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

