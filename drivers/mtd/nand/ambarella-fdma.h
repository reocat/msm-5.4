/*
 * driver/mtd/nand/ambarella-dma.h
 *
 * History:
 *	2012/07/03 - Ken He <jianhe@ambarella.com> created file
 *
 * Copyright (C) 2008-2012, Ambarella, Inc.
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

#ifndef __AMBARELLA_FDMA_H
#define __AMBARELLA_FDMA_H

#define MAX_FDMA_CHANNEL_IRQ_HANDLERS 4

/**
* struct ambarella_fdma_desc
* - ambarella fdma engine descriptor fields -
* @src: Source starting address
* @dst: Destination starting address
* @next_desc: Next Descriptor address
* @rpt: Report address
* @xfr_count: Transfer count
* @ctrl_info: Control Information
**/
typedef struct ambarella_fdma_desc {
	dma_addr_t src;
	dma_addr_t dst;
	struct ambarella_fdma_desc *next_desc;
	u32 rpt;
	u32 xfr_count;
	u32 ctrl_info;
} ambarella_fdma_desc_t;

/**
 * FDMA spare descriptor.
 */
#if (NAND_DUAL_SPACE_MODE == 1)
typedef struct ambarella_fdma_spr_desc
{
	dma_addr_t src;
	dma_addr_t dst;
	struct ambarella_fdma_spr_desc *next_desc;
	u32	rpt;
	u32	xfr_count;
} ambarella_fdma_spr_desc_t;
#endif

typedef void (*ambarella_fdma_handler)(void *dev_id, u32 status);

typedef struct ambarella_fdma_chan_s {
	u32 status;
	u32 spare_status;
	ambarella_fdma_desc_t *desc;
	int use_flag;
	int irq_count;
	struct {
		int enabled;
		ambarella_fdma_handler handler;
		void *harg;
	}irq[MAX_FDMA_CHANNEL_IRQ_HANDLERS];
} ambarella_fdma_chan_t;

/* ==========================================================================*/
extern int ambarella_fdma_enable_irq(ambarella_fdma_handler handler);
extern int ambarella_fdma_disable_irq(ambarella_fdma_handler handler);
extern int ambarella_fdma_request_irq(ambarella_fdma_handler handler, void *harg);
extern void ambarella_fdma_free_irq(ambarella_fdma_handler handler);
extern int ambarella_fdma_xfr(ambarella_fdma_desc_t *req, int chan);
extern int ambarella_fdma_desc_xfr(dma_addr_t desc_addr, int chan);
extern int ambarella_fdma_init(void);
/* ==========================================================================*/

#endif