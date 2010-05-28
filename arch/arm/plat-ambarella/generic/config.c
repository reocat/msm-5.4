/*
 * arch/arm/plat-ambarella/generic/config.c
 *
 * Author: Anthony Ginger <hfjiang@ambarella.com>
 *
 * Copyright (C) 2004-2009, Ambarella, Inc.
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

#include <mach/hardware.h>
#include <mach/board.h>
#include <mach/init.h>
#include <plat/debug.h>

AMBA_ETH_PARAM_CALL(0, ambarella_eth0_platform_info, 0644);

AMBA_IDC_PARAM_CALL(0, ambarella_idc0_platform_info, 0644);
#if (IDC_INSTANCES >= 2)
AMBA_IDC_PARAM_CALL(1, ambarella_idc1_platform_info, 0644);
#endif

AMBA_RTC_PARAM_CALL(0, ambarella_platform_rtc_controller0, 0644, rtc_set_pos);

module_param_call(sd0_clk_limit, param_set_int, param_get_int,
	&(ambarella_platform_sd_controller0.clk_limit), 0644);
module_param_call(sd0_wait_timoout, param_set_int, param_get_int,
	&(ambarella_platform_sd_controller0.wait_tmo), 0644);
AMBA_SD_PARAM_CALL(0, 0, ambarella_platform_sd_controller0, 0644);
#if (SD_HAS_INTERNAL_MUXER == 1)
AMBA_SD_PARAM_CALL(0, 1, ambarella_platform_sd_controller0, 0644);
#endif
#if (SD_INSTANCES >= 2)
module_param_call(sd1_clk_limit, param_set_int, param_get_int,
	&(ambarella_platform_sd_controller1.clk_limit), 0644);
module_param_call(sd1_wait_timoout, param_set_int, param_get_int,
	&(ambarella_platform_sd_controller1.wait_tmo), 0644);
AMBA_SD_PARAM_CALL(1, 0, ambarella_platform_sd_controller1, 0644);
#endif

AMBA_SPI_PARAM_CALL(0, ambarella_spi0_cs_pins, 0644);
#if (SPI_INSTANCES >= 2 )
AMBA_SPI_PARAM_CALL(1, ambarella_spi1_cs_pins, 0644);
#endif

