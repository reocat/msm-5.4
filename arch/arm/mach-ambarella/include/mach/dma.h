/*
 * arch/arm/mach-ambarella/include/mach/dma.h
 *
 * History:
 *	2006/12/27 - [Charles Chiou] created file
 *	2009/01/12 - [Anthony Ginger] Port to 2.6.28
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

#ifndef __ASM_ARCH_DMA_H
#define __ASM_ARCH_DMA_H

#include <mach/hardware.h>

#if (DMA_SUPPORT_DMA_FIOS == 1)
/* Max number of channel */
#define NUM_DMA_CHANNELS 	5
#define NUM_DMA_FIOS_CHANNELS 	1
#else
/* Max number of channel */
#define NUM_DMA_CHANNELS 	5
#endif

/* Max number of DMA channel IRQ handlers */
#define MAX_DMA_CHANNEL_IRQ_HANDLERS	4

/*
 * This is an aggregate data structure that is used in arguments passed into
 * all the DMA interface library functions.
 */
typedef struct ambarella_dma_req_s {
	u32 src;		/*  Source address */
	u32 dst;		/* Destination address */
	struct ambarella_dma_req_s *next; /* Pointing to next descriptor */
	u32 rpt;		/* The physical address to store DMA hardware
					reporting status */
	u32 xfr_count;		/* Transfer byte count , max value = 2^22 */
	u32 attr;		/* Descriptor 's attribute */
} ambarella_dma_req_t;

/**
 * This is a data structure that is used to describe the
 * dma descriptor for FIOS/DMA in the chain.
 */
typedef struct ambarella_dmadesc_s {
	u32	src_addr;	/**< Source address */
	u32	dst_addr;	/**< Destination address */
	u32	next;		/**< Next descriptor */
	u32	rpt_addr;	/**< Report address */
	u32	xfrcnt;		/**< Transfer count */
	u32	ctrl;		/**< Control */
	u32	rsv0;		/**< Reserved */
	u32	rsv1;		/**< Reserved */
	u32	rpt;		/**< Report */
	u32	rsv2;		/**< Reserved */
	u32	rsv3;		/**< Reserved */
	u32	rsv4;		/**< Reserved */
} ambarella_dmadesc_t;

typedef void (*ambarella_dma_handler)(void *dev_id, u32 status);

/*
 * Struct for DMA manager.
 */
struct dma_s {
	struct dma_chan_s {
		u32 status;	/**< The status of the current transaction */
		int use_flag;
		int irq_count;
		struct {
			int enabled;
			ambarella_dma_handler handler;
			void *harg;
		} irq[MAX_DMA_CHANNEL_IRQ_HANDLERS];
	} chan[NUM_DMA_CHANNELS];
};

/*
 * Request the DMA channel IRQ.
 * 
 * @param chan - Channel number
 * @param handler - A callback function that will be run in the 
 * 		  ISR context of the DMA.
 * @param harg - Argument pass-back for the handler.
 * @returns - 0 success, < 0 failure
 */
extern int ambarella_dma_request_irq(int chan,
	ambarella_dma_handler handler, void *harg);

/*
 * Free a DMA channel IRQ.
 * 
 * @param chan - Channel number
 * @param handler - The callback function handler that was requested.
 * @returns - 0 success, < 0 failure
 */
extern void ambarella_dma_free_irq(int chan, ambarella_dma_handler handler);

/*
 * Enable the DMA channel IRQ that was requested
 * 
 * @param chan - Channel number
 * @param handler - The callback function handler that was requested.
 */
extern int ambarella_dma_enable_irq(int chan, ambarella_dma_handler handler);

/*
 * Disable the DMA channel IRQ that was requested
 * 
 * @param chan - Channel number
 * @param handler - The callback function handler that was requested.
 */
extern int ambarella_dma_disable_irq(int chan, ambarella_dma_handler handler);

/*
 * Submit a DMA FIOS request and forget about it.
 *
 * @param req - DMA Request
 * @param chan - Channel number
 * @returns - 0 success, < 0 failure
 * @see wait_dma 
 */
extern int ambarella_dma_xfr(ambarella_dma_req_t *req, int chan);

/*
 * Submit a DMA (with descriptors) request.
 *
 * @param desc - addr to the first descriptor memory
 * @param chan - Channel number
 * @returns - 0 success, < 0 failure
 */
extern int ambarella_dma_desc_xfr(dma_addr_t desc_addr, int chan);

#endif

