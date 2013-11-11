/*
 * arch/arm/plat-ambarella/include/plat/uhc.h
 *
 * Author: Cao Rongrong <rrcao@ambarella.com>
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

#ifndef __PLAT_AMBARELLA_UHC_H
#define __PLAT_AMBARELLA_UHC_H

/* ==========================================================================*/
#if (CHIP_REV == S2) || (CHIP_REV == S2L)
#define USB_HOST_CTRL_EHCI_OFFSET	0x18000
#define USB_HOST_CTRL_OHCI_OFFSET	0x19000
#elif (CHIP_REV == I1)
#define USB_HOST_CTRL_EHCI_OFFSET	0x17000
#define USB_HOST_CTRL_OHCI_OFFSET	0x18000
#endif
#define USB_HOST_CTRL_EHCI_BASE		(AHB_BASE + USB_HOST_CTRL_EHCI_OFFSET)
#define USB_HOST_CTRL_OHCI_BASE		(AHB_BASE + USB_HOST_CTRL_OHCI_OFFSET)
#define USB_HOST_CTRL_EHCI_REG(x)	(USB_HOST_CTRL_EHCI_BASE + (x))
#define USB_HOST_CTRL_OHCI_REG(x)	(USB_HOST_CTRL_OHCI_BASE + (x))

/* ==========================================================================*/
#ifndef __ASSEMBLER__

struct ambarella_uhc_controller {
	void		(*enable_host)(struct ambarella_uhc_controller *pdata);
	void		(*disable_host)(struct ambarella_uhc_controller *pdata);
	int		usb1_is_host;
	unsigned long	irqflags;
};

/* ==========================================================================*/
extern struct platform_device			ambarella_ehci0;
extern struct platform_device			ambarella_ohci0;

#endif /* __ASSEMBLER__ */
/* ==========================================================================*/

#endif

