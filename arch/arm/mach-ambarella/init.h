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

extern void ambarella_map_io(void);
extern void ambarella_init_irq(void);

extern struct sys_timer ambarella_timer;

extern struct platform_device ambarella_sd0;
extern struct platform_device ambarella_sd1;
extern struct platform_device ambarella_idc0;
extern struct platform_device ambarella_spi0;
extern struct platform_device ambarella_spi1;
extern struct platform_device ambarella_i2s0;
extern struct platform_device ambarella_rtc0;
extern struct platform_device ambarella_wdt0;
extern struct platform_device ambarella_eth0;
extern struct platform_device ambarella_udc0;
#ifdef CONFIG_AMBARELLA_FB0
extern struct platform_device ambarella_fb0;
#endif
#ifdef CONFIG_AMBARELLA_FB1
extern struct platform_device ambarella_fb1;
#endif
extern struct platform_device ambarella_ir0;
extern struct platform_device ambarella_ide0;
extern struct platform_device ambarella_uart;
extern struct platform_device ambarella_nand;
extern struct platform_device ambarella_power_supply;

extern struct ambarella_gpio_power_info system_power_gpio_info;
extern struct ambarella_gpio_power_info sound_input_gpio_info;
extern struct ambarella_gpio_power_info sound_output_gpio_info;

extern int ambarella_create_proc_dir(void);

extern int ambarella_init_nand(void *);
extern int ambarella_init_gpio(void);
extern int ambarella_init_fio(void);
extern int ambarella_init_dma(void);
extern int ambarella_init_eth0(unsigned int high, unsigned int low);
extern int ambarella_init_adc(void);
extern int ambarella_init_pwm(void);
extern int ambarella_init_fb(void);
extern int ambarella_init_pm(void);
extern int ambarella_init_tm(void);

extern void ambarella_gpio_set_valid(unsigned pin, int valid);
extern void ambarella_register_spi_device(void);

#endif

