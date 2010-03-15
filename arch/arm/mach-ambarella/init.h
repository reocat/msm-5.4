/*
 * arch/arm/mach-ambarella/init.h
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

#ifndef __ARCH_AMBARELLA_INIT_H
#define __ARCH_AMBARELLA_INIT_H

/* ==========================================================================*/
#ifdef CONFIG_VMSPLIT_3G
#define NOLINUX_MEM_V_START	(0xE0000000)
#else
#define NOLINUX_MEM_V_START	(0xC0000000)
#endif

#define DEFAULT_BSB_START	(0x00000000)
#define DEFAULT_BSB_BASE	(0x00000000)
#define DEFAULT_BSB_SIZE	(0x00000000)

#define DEFAULT_DSP_START	(0x00000000)
#define DEFAULT_DSP_BASE	(0x00000000)
#define DEFAULT_DSP_SIZE	(0x00000000)

#define DEFAULT_DEBUG_START	(0xC00F8000)
#ifdef CONFIG_VMSPLIT_3G
#define DEFAULT_DEBUG_BASE	(0xE00F8000)
#else
#define DEFAULT_DEBUG_BASE	(0xC00F8000)
#endif
#define DEFAULT_DEBUG_SIZE	(0x00008000)

#define DEFAULT_HAL_START	(0xC00A0000)
#define DEFAULT_HAL_BASE	(0xFEE00000)
#define DEFAULT_HAL_SIZE	(0x00030000)
#if	(CHIP_REV == A5S) || (CHIP_REV == A7) 
#define SYSTEM_SUPPORT_HAL	(1)
#include <mach/hal/ambhal.h>
#include <mach/hal/ambhalmini.h>
#include <mach/hal/header.h>
#else
#define SYSTEM_SUPPORT_HAL	(0)
#endif

#define DEFAULT_TOSS_START	(0x00000000)
#define DEFAULT_TOSS_BASE	(0x00000000)
#define DEFAULT_TOSS_SIZE	(0x00000000)

#if (SYSTEM_SUPPORT_HAL == 1)
#define HAL_BASE_VP		(get_ambarella_hal_vp())
extern void *get_ambarella_hal_vp(void);
#endif

#define DEFAULT_SSS_START	(0xC00F7000)
#define DEFAULT_SSS_MAGIC0	(0x19790110)
#define DEFAULT_SSS_MAGIC1	(0x19450107)
#define DEFAULT_SSS_MAGIC2	(0x19531110)

/* ==========================================================================*/
extern struct sys_timer ambarella_timer;

/* ==========================================================================*/
extern struct platform_device ambarella_sd0;
#if (SD_INSTANCES >= 2)
extern struct platform_device ambarella_sd1;
#endif
extern struct platform_device ambarella_idc0;
#if (IDC_INSTANCES >= 2)
extern struct platform_device ambarella_idc1;
#endif
extern struct platform_device ambarella_spi0;
#if (SPI_INSTANCES >= 2)
extern struct platform_device ambarella_spi1;
#endif
extern struct platform_device ambarella_i2s0;
extern struct platform_device ambarella_rtc0;
extern struct platform_device ambarella_wdt0;
extern struct platform_device ambarella_eth0;
extern struct platform_device ambarella_udc0;
#if (CONFIG_AMBARELLA_FB_NUM >= 1)
extern struct platform_device ambarella_fb0;
#endif
#if (CONFIG_AMBARELLA_FB_NUM >= 2)
extern struct platform_device ambarella_fb1;
#endif
extern struct platform_device ambarella_ir0;
extern struct platform_device ambarella_uart;
#if (UART_INSTANCES >= 2)
extern struct platform_device ambarella_uart1;
#endif
extern struct platform_device ambarella_nand;
extern struct platform_device ambarella_nor;
extern struct platform_device ambarella_power_supply;

#ifdef CONFIG_ARCH_AMBARELLA_A5S
extern struct platform_device ambarella_crypto;
#endif
extern struct platform_device ambarella_adc0;

/* ==========================================================================*/
extern struct ambarella_gpio_power_info system_power_gpio_info;
extern struct ambarella_gpio_power_info sound_input_gpio_info;
extern struct ambarella_gpio_power_info sound_output_gpio_info;

/* ==========================================================================*/
extern void ambarella_map_io(void);

extern int ambarella_create_proc_dir(void);

extern int ambarella_init_toss(void);
extern int ambarella_init_gpio(void);
extern int ambarella_init_fio(void);
extern int ambarella_init_dma(void);
extern int ambarella_init_eth0(unsigned int high, unsigned int low);
extern int ambarella_init_adc(void);
extern int ambarella_init_pwm(void);
extern int ambarella_init_pll(void);
extern int ambarella_init_fb(void);
extern int ambarella_init_pm(void);
extern int ambarella_init_tm(void);
extern int ambarella_init_audio(void);

extern void ambarella_init_nand_hotboot(
	struct ambarella_nand_timing *hot_nand_timing);

extern void ambarella_gpio_set_valid(unsigned pin, int valid);
extern void ambarella_register_spi_device(void);
extern void ambarella_register_i2c_device(void);

extern void ambarella_init_irq(void);
extern void ambarella_gpio_ack_irq(unsigned int irq);

extern u32 get_ambarella_sss_virt(void);
extern u32 get_ambarella_sss_entry_virt(void);
extern void set_ambarella_hal_invalid(void);

extern u32 ambarella_adc_suspend(u32 level);
extern u32 ambarella_adc_resume(u32 level);
extern u32 ambarella_gpio_suspend(u32 level);
extern u32 ambarella_gpio_resume(u32 level);
extern u32 ambarella_pll_suspend(u32 level);
extern u32 ambarella_pll_resume(u32 level);
extern u32 ambarella_timer_suspend(u32 level);
extern u32 ambarella_timer_resume(u32 level);
extern u32 ambarella_irq_suspend(u32 level);
extern u32 ambarella_irq_resume(u32 level);

/* ==========================================================================*/

#endif

