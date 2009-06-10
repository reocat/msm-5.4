/*
 * arch/arm/mach-ambarella/include/mach/adc.h
 *
 * History:
 *	2006/12/27 - [Charles Chiou] created file
 *	2008/02/19 - [Allen Wang] changed to use capabilities and chip ID
 *	2008/05/13 - [Allen Wang] added capabilities of A2S and A2M silicons 
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

#ifndef __ASM_ARCH_HARDWARE_H
#error "include <mach/hardware.h> instead"
#endif

#ifndef __ASM_ARCH_ADC_H
#define __ASM_ARCH_ADC_H

/****************************************************/
/* Capabilities based on chip revision              */
/****************************************************/

#if (CHIP_REV == A1)
#define	ADC_MAX_RESOLUTION	8
#elif (CHIP_REV == A2) || (CHIP_REV == A3) ||	\
      (CHIP_REV == A2S) || (CHIP_REV == A2M) || (CHIP_REV == A2Q) 
#define	ADC_MAX_RESOLUTION	10
#else
#define	ADC_MAX_RESOLUTION	16
#endif

#if (CHIP_REV == A5) || (CHIP_REV == A6)
#define ADC_MAX_INSTANCES	8
#else
#define ADC_MAX_INSTANCES	4
#endif

/****************************************************/
/* Controller registers definitions                 */
/****************************************************/

#define ADC_CONTROL_OFFSET		0x00
#define ADC_DATA0_OFFSET		0x04
#define ADC_DATA1_OFFSET		0x08
#define ADC_DATA2_OFFSET		0x0c
#define ADC_DATA3_OFFSET		0x10
#define ADC_RESET_OFFSET		0x14
#define ADC_ENABLE_OFFSET		0x18
#define ADC_CHAN0_INTR_OFFSET		0x44
#define ADC_CHAN1_INTR_OFFSET		0x48
#define ADC_CHAN2_INTR_OFFSET		0x4c
#define ADC_CHAN3_INTR_OFFSET		0x50
#define ADC_DATA4_OFFSET		0x54
#define ADC_DATA5_OFFSET		0x58
#define ADC_DATA6_OFFSET		0x5c
#define ADC_DATA7_OFFSET		0x60
#define ADC_CHAN4_INTR_OFFSET		0x64
#define ADC_CHAN5_INTR_OFFSET		0x68
#define ADC_CHAN6_INTR_OFFSET		0x6c
#define ADC_CHAN7_INTR_OFFSET		0x70

#define ADC_CONTROL_REG			ADC_REG(0x00)
#define ADC_DATA0_REG			ADC_REG(0x04)
#define ADC_DATA1_REG			ADC_REG(0x08)
#define ADC_DATA2_REG			ADC_REG(0x0c)
#define ADC_DATA3_REG			ADC_REG(0x10)
#define ADC_RESET_REG			ADC_REG(0x14)
#define ADC_ENABLE_REG			ADC_REG(0x18)
#define ADC_CHAN0_INTR_REG		ADC_REG(0x44)
#define ADC_CHAN1_INTR_REG		ADC_REG(0x48)
#define ADC_CHAN2_INTR_REG		ADC_REG(0x4c)
#define ADC_CHAN3_INTR_REG		ADC_REG(0x50)
#define ADC_DATA4_REG			ADC_REG(0x54)
#define ADC_DATA5_REG			ADC_REG(0x58)
#define ADC_DATA6_REG			ADC_REG(0x5c)
#define ADC_DATA7_REG			ADC_REG(0x60)
#define ADC_CHAN4_INTR_REG		ADC_REG(0x64)
#define ADC_CHAN5_INTR_REG		ADC_REG(0x68)
#define ADC_CHAN6_INTR_REG		ADC_REG(0x6c)
#define ADC_CHAN7_INTR_REG		ADC_REG(0x70)

/* ADC_CONTROL_REG */
#define ADC_CONTROL_MODE		0x04
#define ADC_CONTROL_START		0x02
#define ADC_CONTROL_STATUS		0x01

#ifndef __ASSEMBLER__
extern int ambarella_adc_get_instances(void);
extern void ambarella_adc_get_array(u32 *adc_data, u32 *array_size);
extern u32 ambarella_adc_get_channel(u32 channel_id);
extern void ambarella_adc_start(void);
extern void ambarella_adc_stop(void);
#endif /* __ASSEMBLER__ */

#endif

