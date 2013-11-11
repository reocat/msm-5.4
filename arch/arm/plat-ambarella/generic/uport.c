/*
 * arch/arm/plat-ambarella/generic/uport.c
 *
 * Author: Cao Rongrong <rrcao@ambarella.com>
 *
 * Copyright (C) 2010-2016, Ambarella, Inc.
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
#include <linux/io.h>
#include <mach/hardware.h>
#include <mach/board.h>
#include <plat/uport.h>
#include <plat/rct.h>

/* S2 share the usb port between UHC and UDC
 * I1 and S2L share the usb port1 between UHC and UDC */
#if (CHIP_REV == I1) || (CHIP_REV == S2) || (CHIP_REV == S2L)
static u32 ambarella_usb_port_owner = 0;
#endif

void ambarella_enable_usb_port(int owner)
{
#if (CHIP_REV == A5S)
	amba_setbitsl(ANA_PWR_REG, 0x4);	/* always on */
#elif (CHIP_REV == I1) || (CHIP_REV == S2L)
	ambarella_usb_port_owner |= owner;
	/* We must enable usb port1 first. Note: no matter usb port1 is
	 * configured as Host or Slave, we always enable it. */
	amba_setbitsl(ANA_PWR_REG, 0x2);	/* on */
	amba_setbitsl(ANA_PWR_REG, 0x1000);	/* on */
#elif (CHIP_REV == S2)
	ambarella_usb_port_owner |= owner;
	amba_setbitsl(ANA_PWR_REG, 0x4);	/* always on */
#endif
}

void ambarella_disable_usb_port(int owner)
{
#if (CHIP_REV == A5S)
	amba_clrbitsl(ANA_PWR_REG, 0x6);
#elif (CHIP_REV == I1) || (CHIP_REV == S2L)
	amba_clrbitsl(ANA_PWR_REG, 0x3000);
	/* We disable usb port when neither UHC nor UDC use it */
	ambarella_usb_port_owner &= (~owner);
	if (ambarella_usb_port_owner == 0)
		amba_clrbitsl(ANA_PWR_REG, 0x6);
#elif (CHIP_REV == S2)
	/* We disable usb port when neither UHC nor UDC use it */
	ambarella_usb_port_owner &= (~owner);
	if (ambarella_usb_port_owner == 0)
		amba_clrbitsl(ANA_PWR_REG, 0x6);
#endif
}

