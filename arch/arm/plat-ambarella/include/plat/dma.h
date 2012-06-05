/*
 * arch/arm/plat-ambarella/include/plat/dma.h
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

#ifndef __PLAT_AMBARELLA_DMA_H
#define __PLAT_AMBARELLA_DMA_H

#include <linux/init.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/dmaengine.h>

#include <asm/irq.h>
#include <mach/hardware.h>

/* ==========================================================================*/
#define MAX_DMA_CHANNEL_IRQ_HANDLERS	4
/* ==========================================================================*/

#ifndef __ASSEMBLER__
#if (DMA_SUPPORT_DMA_FIOS == 0)
#define FIOS_DMA_DEV_ID	0
#endif

extern struct platform_device		ambarella_dma;
#if (DMA_SUPPORT_DMA_FIOS == 1)
#define FIOS_DMA_DEV_ID	1
extern struct platform_device		ambarella_dma_fios;
#endif
#endif /* __ASSEMBLER__ */

/* ==========================================================================*/

#endif
