/*
* linux/drivers/usb/gadget/ambarella_udc.c
* driver for High/Full speed USB device controller on Ambarella processors
*
* History:
*	2008/06/12 - [Cao Rongrong] created file
*	2009/03/16 - [Cao Rongrong] Change DMA descriptor allocate mode
*
* Copyright (C) 2008 by Ambarella, Inc.
* http://www.ambarella.com
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
* along with this program; if not, write to the
* Free Software Foundation, Inc., 59 Temple Place - Suite 330,
* Boston, MA  02111-1307, USA.
*/

#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/usb.h>
#include <linux/usb/gadget.h>
#include <linux/proc_fs.h>

#include "ambarella_udc.h"

#define DRIVER_DESC	"Ambarella USB Device Controller Gadget"
#define DRIVER_VERSION	"12 June 2008"
#define DRIVER_AUTHOR	"Cao Rongrong <rrcao@ambarella.com>"

#define	DMA_ADDR_INVALID	(~(dma_addr_t)0)

static const char		gadget_name[] = "ambarella_udc";
static const char		driver_desc[] = DRIVER_DESC;
static const char 	ep0name [] = "ep0";

static struct ambarella_udc	*the_controller;

static const char *amb_ep_string[] = {
	ep0name,
	"ep1in-bulk", "ep2in-bulk", "ep3in-bulk", "ep4in-int",
	"ep5in-bulk", "ep6in-bulk", "ep7in-bulk", "ep8in-bulk",
	"ep9in-bulk", "ep10in-bulk", "ep11in-bulk", "ep12in-bulk",
	"ep13in-bulk", "ep14in-bulk", "ep15in-bulk",

	"ep0-out", "ep1out-bulk", "ep2out-bulk", "ep3out-bulk",
	"ep4out-bulk", "ep5out-bulk", "ep6out-bulk", "ep7out-bulk",
	"ep8out-bulk", "ep9out-bulk", "ep10out-bulk", "ep11out-bulk",
	"ep12out-bulk", "ep13out-bulk", "ep14out-bulk", "ep15out-bulk"
};


/*************************** DEBUG FUNCTION ***************************/
#define DEBUG_ERR		0
#define DEBUG_NORMAL		1
#define DEBUG_VERBOSE	2

#define DEBUG_BULK_OUT	3
#define DEBUG_BULK_IN	3
#define DEBUG_INTR_IN		3

#define AMBARELLA_USB_DEBUG 1
#if AMBARELLA_USB_DEBUG

#define dprintk(level,format,args...) \
	do {	\
		if(level < 1)	\
			printk(KERN_INFO "\n %s: " format,	\
			__FUNCTION__,## args);	\
	} while(0)

#else
#define dprintk(args...) do { } while(0)
#endif


static inline struct ambarella_ep *to_ambarella_ep(struct usb_ep *ep)
{
	return container_of(ep, struct ambarella_ep, ep);
}

static inline struct ambarella_udc *to_ambarella_udc(struct usb_gadget *gadget)
{
	return container_of(gadget, struct ambarella_udc, gadget);
}

static inline struct ambarella_request *to_ambarella_req(struct usb_request *req)
{
	return container_of(req, struct ambarella_request, req);
}


/**************  PROC FILESYSTEM BEGIN *****************/

#ifdef CONFIG_USB_GADGET_DEBUG_FILES

static const char proc_node_name[] = "driver/udc";

static int
ambarella_proc_udc_read(char *page, char **start, off_t off, int count,
	      int *eof, void *_dev)
{
	char *buf = page;
	struct ambarella_udc *udc = _dev;
	char *next = buf;
	unsigned size = count;
	int t;
	struct usb_ctrlrequest *crq = (struct usb_ctrlrequest *)&udc->setup[0];

	if (off != 0)
		return 0;

	/* basic device status */
	t = scnprintf(next, size,
		DRIVER_DESC "\n"
		"Name: %s\n"
		"Version: %s\n"
		"Author: %s\n\n"
		"Gadget driver: %s\n"
		"Host: %s\n\n",
		gadget_name,
		DRIVER_VERSION,
		DRIVER_AUTHOR,
		udc->driver ? udc->driver->driver.name : "(none)",
		ambarella_check_connected()?
			(udc->gadget.speed == USB_SPEED_HIGH ?"high speed" : "full speed") :
			"disconnected");
	size -= t;
	next += t;

	t = scnprintf(next, size, "the last setup packet is: \n"
		"bRequestType = 0x%02x, bRequest = 0x%02x,\n"
		"wValue = 0x%04x, wIndex = 0x%04x, wLength = 0x%04x\n\n",
		crq->bRequestType, crq->bRequest, crq->wValue, crq->wIndex,
		crq->wLength);
	size -= t;
	next += t;

/*
	t = scnprintf(next, size, "max_cmd_num = %d\tmax_ep0_cmd_num = %d\n\n",
		udc->max_cmd_num, udc->max_ep0_cmd_num);
	size -= t;
	next += t;
*/
	*eof = 1;
	return count - size;
}

#define create_proc_files() 	create_proc_read_entry(proc_node_name, 0, NULL, ambarella_proc_udc_read, udc)
#define remove_proc_files() 	remove_proc_entry(proc_node_name, NULL)

#else	/* !CONFIG_USB_GADGET_DEBUG_FILES */

#define create_proc_files() do {} while (0)
#define remove_proc_files() do {} while (0)

#endif	/* CONFIG_USB_GADGET_DEBUG_FILES */

/**************  PROC FILESYSTEM  END*****************/

static void ambarella_regaddr_map(struct ambarella_udc *udc)
{
	u32 i;

	for(i = 0; i < EP_NUM_MAX; i++) {
		struct ambarella_ep *ep;
		ep = &udc->ep[i];
		if(ep->dir == USB_DIR_IN){
			ep->ep_reg.ctrl_reg = UDC_EP_IN_CTRL_REG(ep->id);
			ep->ep_reg.sts_reg = UDC_EP_IN_STS_REG(ep->id);
			ep->ep_reg.buf_sz_reg = UDC_EP_IN_BUF_SZ_REG(ep->id);
			ep->ep_reg.max_pkt_sz_reg =
				UDC_EP_IN_MAX_PKT_SZ_REG(ep->id);
			ep->ep_reg.dat_desc_ptr_reg =
				UDC_EP_IN_DAT_DESC_PTR_REG(ep->id);
		} else {
			ep->ep_reg.ctrl_reg = UDC_EP_OUT_CTRL_REG(ep->id - 16);
			ep->ep_reg.sts_reg = UDC_EP_OUT_STS_REG(ep->id - 16);
			ep->ep_reg.buf_sz_reg =
				UDC_EP_OUT_PKT_FRM_NUM_REG(ep->id - 16);
			ep->ep_reg.max_pkt_sz_reg =
				UDC_EP_OUT_MAX_PKT_SZ_REG(ep->id - 16);
			ep->ep_reg.dat_desc_ptr_reg =
				UDC_EP_OUT_DAT_DESC_PTR_REG(ep->id - 16);
		}

		ep->ep_reg.setup_buf_ptr_reg = (i == CTRL_OUT) ?
			UDC_EP_OUT_SETUP_BUF_PTR_REG(ep->id - 16) : (u32)NULL;
	}
}


static void ambarella_disable_all_intr(void)
{
	/* device interrupt mask register */
	amba_writel(UDC_INTR_MSK_REG, 0x0000007f);
	amba_writel(UDC_EP_INTR_MSK_REG, 0xffffffff);
	amba_writel(UDC_INTR_REG, 0x0000007f);
	amba_writel(UDC_EP_INTR_REG, 0xffffffff);
}

static void ambarella_ep_fifo_flush(struct ambarella_ep *ep)
{
	int retry_count = 10000;
	volatile u32 dummy;

	if(ep->dir == USB_DIR_IN)  /* Empty Tx FIFO */
		amba_setbits(ep->ep_reg.ctrl_reg, UDC_EP_CTRL_FLUSH);
	else { 			  /* Empty RX FIFO */
		if (!(amba_readl(UDC_STS_REG) & UDC_STS_RXFIFO_EMPTY)){
			dprintk(DEBUG_NORMAL,"%s Rx FIFO not empty\n",ep->ep.name);
			/* Switch to slave mode */
			amba_clrbits(UDC_CTRL_REG, UDC_CTRL_DMA_MODE);

			while (!(amba_readl(UDC_STS_REG) & UDC_STS_RXFIFO_EMPTY)) {
				if (retry_count-- < 0) {
					printk (KERN_ERR "%s: failed", __func__);
					break;
				}
				dummy = amba_readl (USB_RXFIFO_BASE);
				udelay(3);
			}
			/* Switch to DMA mode */
			amba_setbits(UDC_CTRL_REG, UDC_CTRL_DMA_MODE);
		}
	}
}


/*
 * Name: ambarella_flush_fifo
 * Description:
 *	 Empty Tx/Rx FIFO of DMA
 */
static void ambarella_udc_fifo_flush(void)
{
	struct ambarella_udc *udc = the_controller;
	struct ambarella_ep *ep;
	u32 ep_id;

	for (ep_id = 0; ep_id < EP_NUM_MAX; ep_id++) {
		ep = &udc->ep[ep_id];
		if(ep->desc == NULL && !IS_EP0(ep))
			continue;
		ambarella_ep_fifo_flush(ep);
	}
}

static void ambarella_init_usb(void)
{
	u32 value;

	/* disable all interrupts */
	ambarella_disable_all_intr();
	/* disable Tx and Rx DMA */
	amba_clrbits(UDC_CTRL_REG, UDC_CTRL_RCV_DMA_EN | UDC_CTRL_TRN_DMA_EN);
	/* flush dma fifo, may used in AMboot */
	ambarella_udc_fifo_flush();

	/* device config register */
	value = UDC_CFG_SPD_HI |
		UDC_CFG_SELF_POWER |
		UDC_CFG_P1_8BIT |
		UDC_CFG_DIR_BI |
		UDC_CFG_HALT_ACK |
		UDC_CFG_SET_DESC_STALL |
		UDC_CFG_DDR |
		UDC_CFG_CSR_PRG |
		UDC_CFG_RWKP;

	amba_setbits(UDC_CFG_REG, value);

	/* device control register */
	value = 	UDC_CTRL_DESC_UPD_PYL |
		UDC_CTRL_LITTLE_ENDN |
		UDC_CTRL_DMA_MODE |
		UDC_CTRL_BURST_EN |
		UDC_CTRL_DEFAULT_BRT_LEN;

	amba_setbits(UDC_CTRL_REG, value);

	udelay(200); // FIXME: how long to wait is the best?
}


/*
 * Name: init_setup_descriptor
 * Description:
 *  Config the setup packet to specific endpoint register
 */
static void init_setup_descriptor(struct ambarella_udc *udc)
{
	struct ambarella_ep *ep = &udc->ep[CTRL_OUT];

	udc->setup_buf->status 	= USB_DMA_BUF_HOST_RDY;
	udc->setup_buf->reserved = 0xffffffff;
	udc->setup_buf->data0	= 0xffffffff;
	udc->setup_buf->data1	= 0xffffffff;

	amba_writel(ep->ep_reg.setup_buf_ptr_reg, udc->setup_addr);
}


static int init_null_pkt_desc(struct ambarella_udc *udc)
{
	udc->dummy_desc = dma_pool_alloc(udc->desc_dma_pool, GFP_KERNEL,
		&udc->dummy_desc_addr);
	if(udc->dummy_desc == NULL){
		printk(KERN_ERR "No memory to DMA\n");
		return -ENOMEM;
	}

	udc->dummy_desc->data_ptr = udc->dummy_desc_addr;
	udc->dummy_desc->reserved = 0xffffffff;
	udc->dummy_desc->next_desc_ptr = udc->dummy_desc_addr;
	udc->dummy_desc->status = USB_DMA_BUF_HOST_RDY | USB_DMA_LAST;

	return 0;
}


static void init_ep0(struct ambarella_udc *udc)
{
	struct ambarella_ep_reg *ep_reg;

	ep_reg = &udc->ep[CTRL_IN].ep_reg;
	amba_writel(ep_reg->ctrl_reg, UDC_EP_TYPE_CTRL);
	amba_writel(ep_reg->buf_sz_reg, USB_TXFIFO_DEPTH_CTRLIN);
	amba_writel(ep_reg->max_pkt_sz_reg, USB_EP_CTRLIN_MAX_PKT_SZ);
	udc->ep[CTRL_IN].ctrl_sts_phase = 0;

	ep_reg = &udc->ep[CTRL_OUT].ep_reg;
	amba_writel(ep_reg->ctrl_reg, UDC_EP_TYPE_CTRL);
	amba_writel(ep_reg->max_pkt_sz_reg, USB_EP_CTRL_MAX_PKT_SZ);
	init_setup_descriptor(udc);
	udc->ep[CTRL_OUT].ctrl_sts_phase = 0;

	/* This should be set after gadget->bind */
	udc->ep[CTRL_OUT].ep.driver_data = udc->ep[CTRL_IN].ep.driver_data;

	/* setup CSR */
	amba_clrbits(USB_UDC_REG(CTRL_IN), 0x7ff << 19);
	amba_setbits(USB_UDC_REG(CTRL_IN), USB_EP_CTRL_MAX_PKT_SZ << 19);
	amba_setbits(USB_UDC_REG(CTRL_IN), 0x1 << 4);

	amba_clrbits(USB_UDC_REG(CTRL_OUT - CTRL_OUT_UDC_IDX), 0x7ff << 19);
	amba_setbits(USB_UDC_REG(CTRL_OUT - CTRL_OUT_UDC_IDX),
		USB_EP_CTRL_MAX_PKT_SZ << 19);
	amba_clrbits(USB_UDC_REG(CTRL_OUT - CTRL_OUT_UDC_IDX), 0x1 << 4);
}


/*
 * Name: ambarella_check_connected
 * Description:
 *	Check if USB cable is connected to a host
 */
static int ambarella_check_connected(void)
{
	return !!(amba_readl(VIC_RAW_STA_REG) & 1);
}


/*
 * Name: ambarella_check_softdis
 * Description:
 *	Check if udc is soft disconnected
 */
static int ambarella_check_softdis(void)
{
	return !!(amba_readl(UDC_CTRL_REG) & UDC_CTRL_SOFTDISC);
}

static void ambarella_set_softdis(void)
{
	if(amba_readl(VIC_RAW_STA_REG) & 1) {
		/* note don't change the order */
		amba_setbits(UDC_CTRL_REG, UDC_CTRL_SOFTDISC);
		amba_setbits(UDC_CFG_REG, UDC_CFG_RWKP);
	}
}


static struct ambarella_data_desc *
ambarella_get_last_desc(struct ambarella_ep *ep)
{
	struct ambarella_data_desc *desc = ep->data_desc;
	int retry_count = 1000;

	while (desc && (desc->status & USB_DMA_LAST) == 0){
		if (retry_count-- < 0){
			printk(KERN_ERR "Can't find the last descriptor\n");
			break;
		}
		desc = desc->next_desc_virt;
	};

	return desc;
}


/*
 * Name: ambarella_check_error
 * Description:
 *	Check if transfer error occurred for specified endpoint.
 */
static u32 ambarella_check_error (struct ambarella_ep *ep)
{
	u32	ep_sts, retval = 0;
	u32	sts_tmp1, sts_tmp2;

	ep_sts = amba_readl(ep->ep_reg.sts_reg);

	if (ep_sts & UDC_EP_STS_BUF_NOT_AVAIL)	/* Error: Buffer Not Available */
	{
		printk(KERN_ERR "[USB]:BNA error in %s\n", ep->ep.name);
		amba_writel(ep->ep_reg.sts_reg, UDC_EP_STS_BUF_NOT_AVAIL);
		retval = 1;
	}
	if (ep_sts & UDC_EP_STS_HOST_ERR)	/* Error: Host Error */
	{
		printk(KERN_ERR "[USB]:HE error in %s\n", ep->ep.name);
		amba_writel(ep->ep_reg.sts_reg, UDC_EP_STS_HOST_ERR);
		retval = 1;
	}

	if(ep->last_data_desc){
		sts_tmp1 = ep->last_data_desc->status & USB_DMA_BUF_STS;
		sts_tmp2 = ep->last_data_desc->status & USB_DMA_RXTX_STS;
		if ((sts_tmp1 != USB_DMA_BUF_DMA_DONE) || (sts_tmp2 != USB_DMA_RXTX_SUCC)){
			printk(KERN_ERR "%s: DMA failed\n", ep->ep.name);
			retval = 1;
		}
	}


	return retval;
}


static void ambarella_free_descriptor(struct ambarella_ep *ep,
	struct ambarella_request *req)
{
	struct ambarella_data_desc *data_desc, *next_data_desc;
	struct dma_pool *desc_dma_pool = ep->udc->desc_dma_pool;
	int i;

	next_data_desc = req->data_desc;
	for(i = 0; i < req->desc_count; i++){
		data_desc = next_data_desc;
		next_data_desc = data_desc->next_desc_virt;
		dma_pool_free(desc_dma_pool, data_desc, data_desc->cur_desc_addr);
	}

	req->desc_count = 0;
	req->data_desc = NULL;
	req->data_desc_addr = 0;
}

static int ambarella_prepare_descriptor(struct ambarella_ep *ep,
	struct ambarella_request *req, gfp_t gfp)
{
	struct ambarella_udc *udc = ep->udc;
	struct ambarella_data_desc *data_desc = NULL;
	struct ambarella_data_desc *prev_data_desc = NULL;
	dma_addr_t desc_phys;
	u32 desc_count, data_transmit, rest_bytes, i;

	/* build data descriptor chain */
	if(req->desc_count > 0){
		ambarella_free_descriptor(ep, req);
	}

	desc_count = (req->req.length + ep->ep.maxpacket - 1) / ep->ep.maxpacket;
	if(req->req.zero && (req->req.length % ep->ep.maxpacket == 0))
		desc_count++;
	if(desc_count == 0)
		desc_count = 1;

	for(i = 0; i < desc_count; i++){
		rest_bytes = req->req.length - i * ep->ep.maxpacket;
		if(ep->dir == USB_DIR_IN)
			data_transmit = rest_bytes < ep->ep.maxpacket ?
				rest_bytes : ep->ep.maxpacket;
		else
			data_transmit = 0;

		data_desc = dma_pool_alloc(udc->desc_dma_pool, gfp, &desc_phys);
		if (!data_desc)
			return -ENOMEM;

		data_desc->data_ptr = req->req.dma + i * ep->ep.maxpacket;
		data_desc->reserved = 0xffffffff;
		data_desc->next_desc_ptr = 0;
		data_desc->status = USB_DMA_BUF_HOST_RDY | data_transmit;
		data_desc->cur_desc_addr = desc_phys;

		req->desc_count++;

		if(prev_data_desc){
			prev_data_desc->next_desc_ptr = desc_phys;
			prev_data_desc->next_desc_virt = data_desc;
		}

		prev_data_desc = data_desc;

		if(i == 0){
			req->data_desc = data_desc;
			req->data_desc_addr = desc_phys;
		}
	}

	/* Patch last one */
	data_desc->status |= USB_DMA_LAST;
	data_desc->next_desc_ptr = req->data_desc_addr;
	data_desc->next_desc_virt = req->data_desc;

	return 0;
}


static void ambarella_clr_ep_nak(struct ambarella_ep *ep)
{
	struct ambarella_ep_reg *ep_reg = &ep->ep_reg;

	amba_setbits(ep_reg->ctrl_reg, UDC_EP_CTRL_CNAK);
	if (amba_readl(ep_reg->ctrl_reg) & UDC_EP_CTRL_NAK) {
		/* can't clear NAK, let somebody clear it after Rx DMA is done. */
		ep->need_cnak = 1;
	}
}

static void ambarella_enable_rx_dma(void)
{
	amba_setbits(UDC_CTRL_REG, UDC_CTRL_RCV_DMA_EN);
}

/*
 * Name: ambarella_set_tx_dma
 * Description:
 *	Set data descriptor and Prepare Tx-FIFO for IN endpoint
 */
static void ambarella_set_tx_dma(struct ambarella_ep *ep,
	struct ambarella_request * req)
{
	struct ambarella_ep_reg *ep_reg = &ep->ep_reg;

	unsigned long flags;

	dprintk(DEBUG_NORMAL, "Enter. %s Tx DMA\n", ep->ep.name);

	/* disable usb interrupt to avoid flood-in interrupts of
	   IN packets due to completion interrupt of tx dma enabled */
	local_irq_save(flags);

	ep->data_desc = req->data_desc;
	amba_writel(ep_reg->dat_desc_ptr_reg, req->data_desc_addr);
	/* set Poll command to transfer data to Tx FIFO */
	amba_setbits(ep_reg->ctrl_reg, UDC_EP_CTRL_POLL);

	/* re-enable usb interrupt */
	local_irq_restore(flags);

	dprintk(DEBUG_NORMAL, "Exit\n");
}


static void ambarella_set_rx_dma(struct ambarella_ep *ep,
	struct ambarella_request * req)
{
	struct ambarella_ep_reg *ep_reg = &ep->ep_reg;
	struct ambarella_udc *udc = ep->udc;
	struct ambarella_data_desc *last_data_desc;

	u32 i, wait_timeout = 0xffffff;

	dprintk(DEBUG_NORMAL, "Enter. %s Rx DMA\n", ep->ep.name);

	if(req){
		ep->data_desc = req->data_desc;
		amba_writel(ep_reg->dat_desc_ptr_reg, req->data_desc_addr);
	} else {
		/* receive zero-length-packet */
		amba_writel(ep_reg->dat_desc_ptr_reg, udc->dummy_desc_addr);
	}

	/* enable dma completion interrupt for next RX data */
	amba_clrbits(UDC_EP_INTR_MSK_REG, 1 << ep->id);
	/* re-enable DMA read */
	amba_setbits (UDC_CTRL_REG, UDC_CTRL_RCV_DMA_EN);

	/* Wait until FIFO empty */
	while (!(amba_readl(UDC_STS_REG) & UDC_STS_RXFIFO_EMPTY)) {

		if((last_data_desc = ambarella_get_last_desc(ep)) == NULL)
			break;

		if ((last_data_desc->status & USB_DMA_BUF_STS) == USB_DMA_BUF_DMA_DONE){
			break;
		}

		wait_timeout--;
		if (wait_timeout == 0)
		{
			printk (KERN_INFO "%s: [usb] Empty RX FIFO\n", ep->ep.name);
			ambarella_ep_fifo_flush(ep);
			break;
		}
	}

	/* clear NAK for TX dma */
	for (i = 0; i < EP_NUM_MAX; i++) {
		struct ambarella_ep *_ep = &udc->ep[i];
		if (_ep->need_cnak == 1)
			ambarella_clr_ep_nak(_ep);
	}

	/* clear NAK */
	ambarella_clr_ep_nak(ep);

	dprintk(DEBUG_NORMAL, "Exit\n");
}


/*
 * Name: decode_request
 * Description:
 *	Request from host through the default control pipe
 *	Decode what's the request
 */
static void ambarella_handle_request_packet(struct ambarella_udc *udc)
{
	struct usb_ctrlrequest *crq;
	int ret;

	dprintk(DEBUG_NORMAL, "Enter\n");

	ambarella_ep_nuke(&udc->ep[CTRL_OUT], -EPROTO);  // Todo

	/* read out setup packet */
	udc->setup[0] = udc->setup_buf->data0;
	udc->setup[1] = udc->setup_buf->data1;
	/* reinitialize setup packet */
	init_setup_descriptor(udc);

	crq = (struct usb_ctrlrequest *) &udc->setup[0];

	dprintk(DEBUG_NORMAL, "bRequestType = 0x%02x, bRequest = 0x%02x,\n"
		"wValue = 0x%04x, wIndex = 0x%04x, wLength = 0x%04x\n",
		crq->bRequestType, crq->bRequest, crq->wValue, crq->wIndex,
		crq->wLength);

	if((crq->bRequestType & USB_TYPE_MASK) == USB_TYPE_STANDARD){
		switch(crq->bRequest)
		{
		case USB_REQ_GET_STATUS:
		case USB_REQ_SET_ADDRESS:
		case USB_REQ_CLEAR_FEATURE:
		case USB_REQ_SET_FEATURE:
			dprintk(DEBUG_ERR, "This bRequest is not implemented!\n"
				"\tbRequestType = 0x%02x, bRequest = 0x%02x,\n"
				"\twValue = 0x%04x, wIndex = 0x%04x, wLength = 0x%04x\n",
				crq->bRequestType, crq->bRequest, crq->wValue, crq->wIndex,
				crq->wLength);
		}
	}

	if (crq->bRequestType & USB_DIR_IN)
		udc->gadget.ep0 = &udc->ep[CTRL_IN].ep;
	
	else
		udc->gadget.ep0 = &udc->ep[CTRL_OUT].ep;
	

	ret = udc->driver->setup(&udc->gadget, crq);

	dprintk(DEBUG_NORMAL, "Exit\n");

	return;

}


/*
 * Name: ambarella_handle_data_in
 * Description:
 *	Data IN transfer: Bulk in or Intr in.
 */
static void ambarella_handle_data_in(struct ambarella_ep *ep)
{
	struct ambarella_request	*req = NULL;
	struct ambarella_udc *udc = ep->udc;

	dprintk(DEBUG_BULK_IN, "Enter\n");

	/* get request */
	if (list_empty(&ep->queue)) {
		printk(KERN_DEBUG "%s: req NULL\n", __func__);
		return ;
	}

	req = list_entry(ep->queue.next,
			struct ambarella_request,queue);

	/* If error happened, issue the request again */
	if (ambarella_check_error(ep))
		req->req.status = -EPROTO;
	else {
		/* No error happened, so all the data has been sent to host */
		req->req.actual = req->req.length;
	}

	ambarella_udc_done(ep, req, 0);

	if(ep->id == CTRL_IN){
		/* For STATUS-OUT stage */
		udc->ep[CTRL_OUT].ctrl_sts_phase = 1;
		udc->dummy_desc->status = USB_DMA_BUF_HOST_RDY | USB_DMA_LAST;
		ambarella_set_rx_dma(&udc->ep[CTRL_OUT], NULL);
	}

	dprintk(DEBUG_BULK_IN, "Exit\n");
}



/*
 * Name: ambarella_handle_data_out
 * Description:
 *	Bulk Out transfer.
 */
static int ambarella_handle_data_out(struct ambarella_ep *ep)
{
	struct ambarella_request	*req = NULL;
	struct ambarella_udc *udc = ep->udc;
	u32 recv_size = 0, req_size;

	dprintk(DEBUG_BULK_OUT, "Enter\n");

	/* get request */
	if (list_empty(&ep->queue)) {
		printk(KERN_DEBUG "%s: req NULL\n", __func__);
		return -EINVAL;
	}

	req = list_entry(ep->queue.next,	struct ambarella_request, queue);

	/* If error happened, issue the request again */
	if (ambarella_check_error(ep))
		req->req.status = -EPROTO;

	recv_size = ep->last_data_desc->status & USB_DMA_RXTX_BYTES;
	if (!recv_size && req->req.length == UDC_DMA_MAXPACKET) {
		/* on 64k packets the RXBYTES field is zero */
		recv_size = UDC_DMA_MAXPACKET;
	}

	req_size = req->req.length - req->req.actual;
	if (recv_size > req_size) {
		if ((req_size % ep->ep.maxpacket) != 0)
			req->req.status = -EOVERFLOW;
		recv_size = req_size;
	}

	req->req.actual += recv_size;

	ambarella_udc_done(ep, req, 0);

	if(ep->id == CTRL_OUT) {
		/* For STATUS-IN stage */
		ambarella_clr_ep_nak(&udc->ep[CTRL_IN]);
		/* Re-enable Rx DMA to receive next setup packet */
		ambarella_enable_rx_dma();
	}

	dprintk(DEBUG_BULK_OUT, "Exit\n");

	return 0;
}


static void ambarella_udc_done(struct ambarella_ep *ep,
		struct ambarella_request *req, int status)
{
	unsigned halted_tmp, need_queue = 0;
	struct ambarella_request *next_req;

	halted_tmp = ep->halted;

	list_del_init(&req->queue);
	ambarella_free_descriptor(ep, req);

	if(!list_empty(&ep->queue) && !ep->halted){
		need_queue = 1;
	} else if (!IS_EP0(ep) && (ep->dir == USB_DIR_IN)) {
		amba_setbits(UDC_EP_INTR_MSK_REG, 1 << ep->id);
	}

	if (likely (req->req.status == -EINPROGRESS))
		req->req.status = status;
	else
		status = req->req.status;

	if (req->mapped) {
		dma_unmap_single(ep->udc->gadget.dev.parent,
			req->req.dma, req->req.length,
			(ep->dir & USB_DIR_IN)
				? DMA_TO_DEVICE
				: DMA_FROM_DEVICE);
		req->req.dma = DMA_ADDR_INVALID;
		req->mapped = 0;
	} else
		dma_sync_single_for_cpu(ep->udc->gadget.dev.parent,
			req->req.dma, req->req.length,
			(ep->dir & USB_DIR_IN)
				? DMA_TO_DEVICE
				: DMA_FROM_DEVICE);

	ep->data_desc = NULL;
	ep->last_data_desc = NULL;

	ep->halted = 1;
	req->req.complete(&ep->ep, &req->req);
	ep->halted = halted_tmp;

	if(need_queue){
		next_req = list_entry (ep->queue.next,
			 struct ambarella_request,
			 queue);

		switch(ep->dir) {
		case USB_DIR_IN:
			ambarella_clr_ep_nak(ep);
			break;
		case USB_DIR_OUT:
			ambarella_set_rx_dma(ep, next_req);
			break;
		default:
			return;
		}
	}
}

static void ambarella_ep_nuke(struct ambarella_ep *ep, int status)
{
	/* Sanity check */
	if (&ep->queue == NULL)
		return;

	while (!list_empty (&ep->queue)) {
		struct ambarella_request *req;
		req = list_entry (ep->queue.next,
				 struct ambarella_request,
				 queue);
		ambarella_udc_done(ep, req, status);
	}
}


/*
 * Name: device_interrupt
 * Description:
 *	Process related device interrupt
 */
static void udc_device_interrupt(struct ambarella_udc *udc, u32 int_value)
{
	/* case 1. Get set_config or set_interface request from host */
	if (int_value & (UDC_INTR_SET_CFG | UDC_INTR_SET_INTF)) {
		struct usb_ctrlrequest crq;
		u32 i, ret, csr_config;

		dprintk(DEBUG_NORMAL, "set_config or set_interface IRQ"
			" - device intr reg = 0x%x\n", amba_readl(UDC_INTR_REG));

		if(int_value & UDC_INTR_SET_CFG) {
			/* Ack the SC interrupt */
			amba_writel(UDC_INTR_REG, UDC_INTR_SET_CFG);
			udc->cur_config = (u16)(amba_readl(UDC_STS_REG) & UDC_STS_CFG);

			crq.bRequestType = 0x00;
			crq.bRequest = USB_REQ_SET_CONFIGURATION;
			crq.wValue = cpu_to_le16(udc->cur_config);
			crq.wIndex = 0x0000;
			crq.wLength = 0x0000;
		} else if(int_value & UDC_INTR_SET_INTF){
			printk("set interface\n");
			/* Ack the SI interrupt */
			amba_writel(UDC_INTR_REG, UDC_INTR_SET_INTF);
			udc->cur_intf = (amba_readl(UDC_STS_REG) & UDC_STS_INTF) >> 4;
			udc->cur_alt = (amba_readl(UDC_STS_REG) & UDC_STS_ALT) >> 8;

			crq.bRequestType = 0x01;
			crq.bRequest = USB_REQ_SET_INTERFACE;
			crq.wValue = cpu_to_le16(udc->cur_alt);
			crq.wIndex = cpu_to_le16(udc->cur_intf);
			crq.wLength = 0x0000;
		}

		for (i = 0; i < EP_NUM_MAX; i++){
			udc->ep[i].halted = 0;
			amba_clrbits(udc->ep[i].ep_reg.ctrl_reg, UDC_EP_CTRL_STALL);
		}

		csr_config = (udc->cur_config << 7) | (udc->cur_intf << 11) |
			(udc->cur_alt << 15);
		amba_clrbits(USB_UDC_REG(CTRL_IN), 0xfff << 7);
		amba_setbits(USB_UDC_REG(CTRL_IN), csr_config);

		amba_clrbits(USB_UDC_REG(CTRL_OUT - CTRL_OUT_UDC_IDX), 0xfff << 7);
		amba_setbits(USB_UDC_REG(CTRL_OUT - CTRL_OUT_UDC_IDX), csr_config);

		udc->auto_ack_0_pkt = 1;
		ambarella_ep_nuke(&udc->ep[CTRL_OUT], -EPROTO);
		ret = udc->driver->setup(&udc->gadget, &crq);
		if(ret < 0)
			printk(KERN_ERR "set config failed. (%d)\n", ret);

		/* told UDC the configuration is done, and to ack HOST */
		amba_setbits(UDC_CTRL_REG, UDC_CTRL_CSR_DONE);
		udelay(150);
	}

	/* case 2. Get reset Interrupt */
	else if (int_value & UDC_INTR_RESET) {

		dprintk(DEBUG_NORMAL, "USB reset IRQ\n");

		amba_writel(UDC_INTR_REG, UDC_INTR_RESET);

		if (udc->host_suspended && udc->driver && udc->driver->resume){
			udc->driver->resume(&udc->gadget);
			udc->host_suspended = 0;
		}

		ambarella_stop_activity(udc);  // Todo

		udc->gadget.speed = USB_SPEED_UNKNOWN;
		udc->auto_ack_0_pkt = 0;
		udc->remote_wakeup_en = 0;

		/* Clear all pending endpoint interrupts */
		amba_writel(UDC_EP_INTR_REG, 0xffffffff);

		ambarella_udc_enable(udc);

#if 0
		/* enable suspend interrupt */
		amba_clrbits(UDC_INTR_MSK_REG, UDC_INTR_MSK_US);
#endif
	}

	/* case 3. Get suspend Interrupt */
	else if (int_value & UDC_INTR_SUSPEND) {

		dprintk(DEBUG_ERR, "USB suspend IRQ\n");

		amba_writel(UDC_INTR_REG, UDC_INTR_SUSPEND);

		if (udc->driver->suspend) {
			udc->host_suspended = 1;
			udc->driver->suspend(&udc->gadget);
		}
	}

	/* case 4. enumeration complete */
	else if(int_value & UDC_INTR_ENUM_CMPL) {
		u32 	value = 0;

		/* Ack the CMPL interrupt */
		amba_writel(UDC_INTR_REG, UDC_INTR_ENUM_CMPL);

		value = amba_readl(UDC_STS_REG) & UDC_STS_ENUM_SPD;

		if(value == UDC_STS_ENUM_SPD_HI) {  /* high speed */

			dprintk(DEBUG_NORMAL,"enumeration IRQ - "
					"High-speed bus detected\n");
			udc->gadget.speed = USB_SPEED_HIGH;
		}
		else if (value == UDC_STS_ENUM_SPD_FU) { /* full speed */

			dprintk(DEBUG_NORMAL,"enumeration IRQ - "
					"Full-speed bus detected\n");
			udc->gadget.speed = USB_SPEED_FULL;
		}
		else {
			printk(KERN_ERR "Not supported speed!"
					"UDC_STS_REG = 0x%x\n", value);
			udc->gadget.speed = USB_SPEED_UNKNOWN;

		}
	} /* ENUM COMPLETE */
	else {
		printk(KERN_ERR "Unknown Interrupt\n");
	}
}


/*
 * Name: udc_epin_interrupt
 * Description:
 *	Process IN(CTRL or BULK) endpoint interrupt
 */
static void udc_epin_interrupt(struct ambarella_udc *udc, u32 ep_id)
{
	u32 ep_status = 0;
	struct ambarella_ep *ep = &udc->ep[ep_id];
	struct ambarella_request *req;

	ep_status = amba_readl(ep->ep_reg.sts_reg);

	if (ep_status & UDC_EP_STS_TX_DMA_CMPL) {
		if(ep->halted || list_empty(&ep->queue))
			return;

		ep->last_data_desc = ambarella_get_last_desc(ep);
		if(ep->last_data_desc == NULL){
			printk(KERN_ERR "%s: last_data_desc is NULL\n", ep->ep.name);
			BUG();
			return;
		}

		ambarella_handle_data_in(&udc->ep[ep_id]);
	}

	if((ep_status & UDC_EP_STS_IN_TOKEN) && !(ep_status & UDC_EP_STS_TX_DMA_CMPL))
	{
		if(!list_empty(&ep->queue)){
			req = list_entry(ep->queue.next,
				struct ambarella_request, queue);
			ambarella_set_tx_dma(ep, req);
		}
	}

	ep_status &= (UDC_EP_STS_IN_TOKEN | UDC_EP_STS_TX_DMA_CMPL);
	amba_writel(ep->ep_reg.sts_reg, ep_status);
}


/*
 * Name: udc_epout_interrupt
 * Description:
 *	Process OUT endpoint interrupt
 */
static void udc_epout_interrupt(struct ambarella_udc *udc, u32 ep_id)
{
	struct ambarella_ep *ep = &udc->ep[ep_id];
	u32 desc_status = 0;
	u32 ep_status = 0;

	/* check the status bits for what kind of packets in */
	ep_status = amba_readl(ep->ep_reg.sts_reg);

	if(ep_id == CTRL_OUT) {
		/* Cope with setup-data packet  */
		if((ep_status & UDC_EP_STS_OUT_PKT_MSK) == UDC_EP_SETUP_PKT){

			amba_writel(ep->ep_reg.sts_reg, UDC_EP_SETUP_PKT);
			ep->ctrl_sts_phase = 0;
			ambarella_handle_request_packet(udc);
		}
	}

	/* Cope with normal data packet  */
	if((ep_status & UDC_EP_STS_OUT_PKT_MSK) == UDC_EP_DATA_PKT) {

		amba_writel(ep->ep_reg.sts_reg, UDC_EP_DATA_PKT);

		/* Just cope with the zero-length packet */
		if(ep->ctrl_sts_phase == 1) {
			ep->ctrl_sts_phase = 0;
			ambarella_enable_rx_dma();
			return;
		}

		if(ep->halted || list_empty(&ep->queue))
			return;

		ep->last_data_desc = ambarella_get_last_desc(ep);
		if(ep->last_data_desc == NULL){
			printk(KERN_ERR "%s: last_data_desc is NULL\n",
				ep->ep.name);
			BUG();
			return;
		}

		if(ep_id != CTRL_OUT){
			desc_status = ep->last_data_desc->status;
			/* received data */
			if((desc_status & USB_DMA_BUF_STS) == USB_DMA_BUF_DMA_DONE) {
				amba_setbits(UDC_EP_INTR_MSK_REG, 1 << ep_id);
				amba_setbits(ep->ep_reg.ctrl_reg, UDC_EP_CTRL_SNAK);
			}
		}

		ambarella_handle_data_out(ep);
	}

	return;
}


/*
 * Name: ambarella_udc_irq
 * Description:
 *	Process UDC interrupt
 */
static irqreturn_t ambarella_udc_irq(int irq, void *_dev)
{
	struct ambarella_udc *udc = _dev;
	u32 value, handled = 0, i, ep_irq;

	/* If gadget driver is not connected, do not handle the interrupt  */
	if (!udc->driver) {
		amba_writel(UDC_INTR_REG, amba_readl(UDC_INTR_REG));
		amba_writel(UDC_EP_INTR_REG, amba_readl(UDC_EP_INTR_REG));
	}

	/* 1. check if device interrupt */
	value = amba_readl(UDC_INTR_REG);
	if(value) {

		dprintk(DEBUG_NORMAL, "device int value = 0x%x\n", value);

		handled = 1;
		udc_device_interrupt(udc, value);

	}
	/* 2. check if endpoint interrupt */
	value = amba_readl(UDC_EP_INTR_REG);
	if(value) {

		dprintk(DEBUG_NORMAL, "endpoint int value = 0x%x\n", value);

		handled = 1;

		for(i = 0; i < EP_NUM_MAX; i++){
			ep_irq = 1 << i;
			if (!(value & ep_irq))
				continue;

			/* ack the endpoint interrupt */
			amba_writel(UDC_EP_INTR_REG, ep_irq);

			/* irq for out ep ? */
			if (i >= EP_IN_NUM)
				udc_epout_interrupt(udc, i);
			else
				udc_epin_interrupt(udc, i);

			value &= ~ep_irq;
			if(value == 0)
				break;
		}
	}

	return IRQ_RETVAL(handled);
}


#if 0
static int ambarella_udc_vbus_session(struct usb_gadget *gadget, int is_active);

/*
* ambarella_udc_vbus_irq - VBUS interrupt handler
*/
static irqreturn_t ambarella_udc_vbus_irq(int irq, void *_dev)
{
	struct ambarella_udc	*udc = _dev;
	//unsigned int		value;

	dprintk(DEBUG_NORMAL, "%s()\n", __func__);

	amba_writel(VIC_INTEN_CLR_REG, (0x1 << USBVBUS_IRQ));

	/* make sure vbus register is intialized before calling this */
	if(ambarella_check_connected()&& !udc->pre_vbus_status) {
		dprintk(DEBUG_NORMAL, "usb cable is inserted\n");
		udc->pre_vbus_status = 1;
		ambarella_udc_vbus_session(&udc->gadget, 1);

	} else if (!ambarella_check_connected() && udc->pre_vbus_status) {
		dprintk(DEBUG_NORMAL, "usb cable is removed\n");
		udc->pre_vbus_status = 0;
		ambarella_udc_vbus_session(&udc->gadget, 0);
	}else
		printk(KERN_ERR "Incorrect vbus interrupt!\n");

	//amba_writel(VIC_INTEN_REG, (0x1 << USBVBUS_IRQ));

	return IRQ_HANDLED;
}
#endif


static void ambarella_stop_activity(struct ambarella_udc *udc)
{
	struct usb_gadget_driver *driver = udc->driver;
	struct ambarella_ep *ep;
	u32  i;

	dprintk(DEBUG_VERBOSE, "Enter\n");

	/* Disable Tx and Rx DMA */
	amba_clrbits(UDC_CTRL_REG, UDC_CTRL_RCV_DMA_EN | UDC_CTRL_TRN_DMA_EN);

	if (udc->gadget.speed == USB_SPEED_UNKNOWN)
		driver = NULL;
	udc->gadget.speed = USB_SPEED_UNKNOWN;

	for (i = 0; i < EP_NUM_MAX; i++) {
		ep = &udc->ep[i];

		if(ep->desc == NULL && !IS_EP0(ep))
			continue;

		amba_setbits(ep->ep_reg.ctrl_reg, UDC_EP_CTRL_SNAK);

		ep->halted = 1;
		ambarella_ep_nuke(ep, -ESHUTDOWN);
	}
	if (driver) {
		driver->disconnect(&udc->gadget);
	}

	ambarella_udc_reinit(udc);

	dprintk(DEBUG_VERBOSE, "Exit\n");
}


/*
 * Name: ambarella_udc_ep_enable
 * Description:
 *	Enable endpoint
 */
static int ambarella_udc_ep_enable(struct usb_ep *_ep,
				 const struct usb_endpoint_descriptor *desc)
{
	struct ambarella_udc	*udc;
	struct ambarella_ep	*ep = to_ambarella_ep(_ep);
	u32			max_packet, tmp, type, idx = 0;
	unsigned long		flags;

	/* Sanity check  */
	if (!_ep || !desc || ep->desc || IS_EP0(ep)
		|| desc->bDescriptorType != USB_DT_ENDPOINT)
		return -EINVAL;

	udc = ep->udc;
	if (!udc->driver || udc->gadget.speed == USB_SPEED_UNKNOWN)
		return -ESHUTDOWN;

	max_packet = le16_to_cpu(desc->wMaxPacketSize) & 0x7ff;

	local_irq_save (flags);
	ep->ep.maxpacket = max_packet;
	ep->desc = desc;
	ep->halted = 0;
	ep->data_desc = NULL;
	ep->last_data_desc = NULL;
	ep->ctrl_sts_phase = 0;

	if(ep->dir == USB_DIR_IN){
		idx = ep->id;
	} else {
		idx = ep->id - CTRL_OUT_UDC_IDX;
	}

	/* setup CSR */
	amba_clrbits(USB_UDC_REG(idx), 0x3fffffff);
	tmp = (desc->bEndpointAddress & 0xf) << 0;
	tmp |= (desc->bEndpointAddress >> 7) << 4;
	tmp |= (desc->bmAttributes & 0x3) << 5;
	tmp |= udc->cur_config << 7;
	tmp |= udc->cur_intf << 11;
	tmp |= udc->cur_alt << 15;
	tmp |= max_packet << 19;
	amba_setbits(USB_UDC_REG(idx), tmp);

	type = (desc->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) << 4;
	amba_writel(ep->ep_reg.ctrl_reg, type | UDC_EP_CTRL_SNAK);

	if(ep->dir == USB_DIR_IN)
		amba_writel(ep->ep_reg.buf_sz_reg, max_packet * 2 / 4);
	amba_writel(ep->ep_reg.max_pkt_sz_reg, max_packet);

	/* print some debug message */
	dprintk (DEBUG_NORMAL, "enable %s, address = 0x%02x, max_packet = 0x%02x\n",
			ep->ep.name, desc->bEndpointAddress, max_packet);

	local_irq_restore (flags);
	ambarella_udc_set_halt(_ep, 0);

	return 0;
}


static int ambarella_udc_ep_disable(struct usb_ep *_ep)
{
	struct ambarella_ep *ep = to_ambarella_ep(_ep);
	unsigned long flags;

	dprintk(DEBUG_NORMAL, "ep_disable: %s\n", _ep->name);

	if (!_ep || !ep->desc) {
		dprintk(DEBUG_NORMAL, "%s not enabled\n",
			_ep ? ep->ep.name : NULL);
		return -EINVAL;
	}

	local_irq_save(flags);

	ep->desc = NULL;
	ep->halted = 1;
	ambarella_ep_nuke(ep, -ESHUTDOWN);

	amba_setbits(ep->ep_reg.ctrl_reg, UDC_EP_CTRL_SNAK);

	if(ep->dir == USB_DIR_IN){
		/* clear DMA poll demand bit */
		amba_clrbits(ep->ep_reg.ctrl_reg, UDC_EP_CTRL_POLL);
		/* clear status register */
		amba_setbits(ep->ep_reg.sts_reg, UDC_EP_STS_IN_TOKEN);
		/* flush the fifo */
		amba_setbits(ep->ep_reg.ctrl_reg, UDC_EP_CTRL_FLUSH);
	}

	/* disable irqs */
	amba_setbits(UDC_EP_INTR_MSK_REG, 1 << ep->id);

	local_irq_restore(flags);

	dprintk(DEBUG_NORMAL, "%s disabled\n", _ep->name);

	return 0;
}


static struct usb_request *
ambarella_udc_alloc_request(struct usb_ep *_ep, gfp_t mem_flags)
{
	struct ambarella_request *req;

	if (!_ep)
		return NULL;

	req = kzalloc (sizeof(struct ambarella_request), mem_flags);
	if (!req)
		return NULL;

	req->req.dma = DMA_ADDR_INVALID;
	INIT_LIST_HEAD (&req->queue);
	req->desc_count = 0;

	return &req->req;
}


static void
ambarella_udc_free_request(struct usb_ep *_ep, struct usb_request *_req)
{
	struct ambarella_ep	*ep = to_ambarella_ep(_ep);
	struct ambarella_request	*req = to_ambarella_req(_req);

	dprintk(DEBUG_VERBOSE, "(%p,%p)\n", _ep, _req);

	if (!ep || !_req || (!ep->desc && !IS_EP0(ep)))
		return;

	if(req->desc_count > 0)
		ambarella_free_descriptor(ep, req);

	WARN_ON (!list_empty (&req->queue));
	kfree(req);
}


static int ambarella_udc_queue(struct usb_ep *_ep, struct usb_request *_req,
		gfp_t gfp_flags)
{
	struct ambarella_request	*req = NULL;
	struct ambarella_ep	*ep = NULL;
	struct ambarella_udc	*udc;

	if (unlikely (!_ep)) {
		dprintk(DEBUG_ERR, "_ep is NULL\n");
		return -EINVAL;
	}

	ep = to_ambarella_ep(_ep);
	if (unlikely (!ep->desc && !IS_EP0(ep))) {
		dprintk(DEBUG_ERR, "%s: invalid args\n", _ep->name);
		return -EINVAL;
	}

	udc = ep->udc;
	if( unlikely( !udc->driver || udc->gadget.speed==USB_SPEED_UNKNOWN)){
		dprintk(DEBUG_NORMAL, "%s: %01d %01d\n", _ep->name,
			!udc->driver, udc->gadget.speed==USB_SPEED_UNKNOWN);
		return -ESHUTDOWN;
	}

	if (unlikely(!_req )) {
		dprintk(DEBUG_ERR, "%s: _req is NULL\n", _ep->name);
		return -EINVAL;
	}

	req = to_ambarella_req(_req);
	if (unlikely(!req->req.complete || !req->req.buf
				|| !list_empty(&req->queue))) {
		dprintk(DEBUG_ERR, "%s: %01d %01d %01d\n", _ep->name,
			!_req->complete, !_req->buf, !list_empty(&req->queue));

		return -EINVAL;
	}

	if(IS_EP0(ep) && (udc->auto_ack_0_pkt == 1)){
		/* It's status stage in setup packet. And A2/A3 will
		  * automatically send the zero-length packet to Host */
		udc->auto_ack_0_pkt = 0;
		req->req.actual = 0;
		req->req.complete(&ep->ep, &req->req);
		return 0;
	}

	/* check whether USB is suspended */
	if(amba_readl(UDC_STS_REG) & UDC_STS_SUSP){
		printk(KERN_ERR "UDC is suspended!\n");
		return -ESHUTDOWN;
	}

	/* map req.buf, and get req's dma address */
	if (req->req.dma == DMA_ADDR_INVALID) {
		req->req.dma = dma_map_single(ep->udc->gadget.dev.parent,
			req->req.buf, req->req.length,
			(ep->dir & USB_DIR_IN) ? DMA_TO_DEVICE	: DMA_FROM_DEVICE);
		req->mapped = 1;
		/* dma address isn't 8-byte align */
		if(req->req.dma & 0x7){
			printk("dma address isn't 8-byte align\n");
			BUG();
		}
	} else {
		dma_sync_single_for_device(
			ep->udc->gadget.dev.parent,
			req->req.dma, req->req.length,
			(ep->dir & USB_DIR_IN) ? DMA_TO_DEVICE	: DMA_FROM_DEVICE);
		req->mapped = 0;
	}

	/* disable IRQ handler's bottom-half  */
	spin_lock_irq(&udc->lock);

	_req->status = -EINPROGRESS;
	_req->actual = 0;

	ambarella_prepare_descriptor(ep, req, gfp_flags);

	/* kickstart this i/o queue? */
	//if (list_empty(&ep->queue) && !ep->halted) {
	if (list_empty(&ep->queue)) {
		/* when the data length in ctrl-out transfer is zero, we just
		  * need to implement the STATUS-IN stage. But we don't
		  * implement the case that the data length in ctrl-in transfer
		  * is zero. */
		if(req->req.length == 0) {
			if(ep->id == CTRL_OUT) {
				ambarella_udc_done(ep, req, 0);
				/* told UDC the configuration is done, and to ack HOST */
				//amba_setbits(UDC_CTRL_REG, UDC_CTRL_CSR_DONE);
				//udelay(150);
				/* For STATUS-IN stage */
				ambarella_clr_ep_nak(&udc->ep[CTRL_IN]);
				/* Re-enable Rx DMA to receive next setup packet */
				ambarella_enable_rx_dma();
				goto finish;
			} else if (ep->id == CTRL_IN) {
			 	printk("the data length of ctrl-in is zero\n");
			 	BUG();
			}
		}

		switch(ep->dir) {
		case USB_DIR_IN:
			/* enable dma completion interrupt for current TX data */
			amba_clrbits(UDC_EP_INTR_MSK_REG, 1 << ep->id);
			ambarella_clr_ep_nak(ep);
			break;
		case USB_DIR_OUT:
			ambarella_set_rx_dma(ep, req);
			break;
		default:
			dprintk(DEBUG_ERR, "bulk or intr queue error\n");
			BUG();
			return -EL2HLT;
		}
	}

	list_add_tail(&req->queue, &ep->queue);

finish:
	/* enable IRQ handler's bottom-half  */
	spin_unlock_irq(&udc->lock);

	return 0;
}


static int ambarella_udc_dequeue(struct usb_ep *_ep, struct usb_request *_req)
{
	struct ambarella_ep	*ep = to_ambarella_ep(_ep);
	struct ambarella_request	*req = NULL;
	int			retval = -EINVAL;
	unsigned long		flags;
	unsigned		halted;

	if (!the_controller->driver)
		return -ESHUTDOWN;

	if (!_ep || !_req)
		return retval;

	local_irq_save (flags);

	halted = ep->halted;
	ep->halted = 1;

	list_for_each_entry (req, &ep->queue, queue) {
		if (&req->req == _req) {
			list_del_init (&req->queue);
			_req->status = -ECONNRESET;
			retval = 0;
			break;
		}
	}

	if (retval != 0)
		goto out;

	dprintk(DEBUG_VERBOSE,
		"dequeued req %p from %s, len %d buf %p\n",
		req, _ep->name, _req->length, _req->buf);

	/* request in processing */
	if(ep->data_desc == req->data_desc) {
		amba_setbits(ep->ep_reg.ctrl_reg, UDC_EP_CTRL_SNAK);
		if(ep->dir == USB_DIR_IN) {
			amba_clrbits(ep->ep_reg.ctrl_reg, UDC_EP_CTRL_POLL);
			ambarella_ep_fifo_flush(ep);
		}
	}

	ambarella_udc_done(ep, req, -ECONNRESET);
	
out:
	ep->halted = halted;
	local_irq_restore (flags);

	return retval;
}

/*
 * ambarella_udc_set_halt
 */
static int ambarella_udc_set_halt(struct usb_ep *_ep, int value)
{
	struct ambarella_ep	*ep = to_ambarella_ep(_ep);
	unsigned long		flags;

	dprintk(DEBUG_NORMAL, "Enter, %s: value = %d\n", _ep->name, value);

	if (unlikely (!_ep || (!ep->desc && !IS_EP0(ep)))) {
		dprintk(DEBUG_ERR, "%s: -EINVAL 1\n",_ep->name);
		return -EINVAL;
	}
	if (!ep->udc->driver || ep->udc->gadget.speed == USB_SPEED_UNKNOWN){
		dprintk(DEBUG_ERR, "%s: -ESHUTDOWN\n",_ep->name);
		return -ESHUTDOWN;
	}
	if (ep->desc /* not ep0 */
		&& (ep->desc->bmAttributes & 0x03) == USB_ENDPOINT_XFER_ISOC){
		dprintk(DEBUG_ERR, "%s: -EINVAL 2\n",_ep->name);
		return -EINVAL;
	}

	local_irq_save (flags);

	/* set/clear, then synch memory views with the device */
	if (value) { /* stall endpoint */
		if (ep->dir == USB_DIR_IN) {

			amba_setbits(ep->ep_reg.ctrl_reg,
				UDC_EP_CTRL_STALL|UDC_EP_CTRL_FLUSH);
		} else {
			int retry_count = 10000;
			/* Wait Rx-FIFO to be empty */
			while(!(amba_readl(UDC_STS_REG) & UDC_STS_RXFIFO_EMPTY)){
				if (retry_count-- < 0) {
					printk(KERN_ERR"[USB] stall_endpoint:failed");
					break;
				}
			}
			amba_setbits(ep->ep_reg.ctrl_reg, UDC_EP_CTRL_STALL);
		}
	}else { /* clear stall endpoint */
		amba_clrbits(ep->ep_reg.ctrl_reg, UDC_EP_CTRL_STALL);
	}

	ep->halted = value ? 1 : 0;
	
	local_irq_restore (flags);

	dprintk(DEBUG_NORMAL, "Exit\n");

	return 0;
}


static const struct usb_ep_ops ambarella_ep_ops = {
	.enable		= ambarella_udc_ep_enable,
	.disable		= ambarella_udc_ep_disable,

	.alloc_request	= ambarella_udc_alloc_request,
	.free_request	= ambarella_udc_free_request,

	.queue		= ambarella_udc_queue,
	.dequeue	= ambarella_udc_dequeue,

	.set_halt	= ambarella_udc_set_halt,
	/* fifo ops not implemented */
};

/*------------------------- usb_gadget_ops ----------------------------------*/


static int ambarella_udc_get_frame(struct usb_gadget *_gadget)
{
	return amba_readl(UDC_STS_REG) >> 18;
}


static int ambarella_udc_wakeup(struct usb_gadget *_gadget)
{
	struct ambarella_udc *udc = to_ambarella_udc(_gadget);
	u32 tmp;
	
	dprintk(DEBUG_VERBOSE, "Enter\n");

	tmp = amba_readl(UDC_CFG_REG);
	/* Remote wakeup feature not enabled by host */
	if ((!udc->remote_wakeup_en) || (!(tmp & UDC_CFG_RWKP)))
		return -ENOTSUPP;

	tmp = amba_readl(UDC_STS_REG);
	/* not suspended? */
	if (!(tmp & UDC_STS_SUSP))
		return 0;
	
	/* trigger force resume */
	amba_setbits(UDC_CTRL_REG, UDC_CTRL_RES);

	dprintk(DEBUG_VERBOSE, "Exit\n");

	return 0;
}

static int ambarella_udc_set_pullup(struct ambarella_udc *udc, int is_on)
{
	if (is_on)
		ambarella_udc_enable(udc);
	else {
		if (udc->gadget.speed != USB_SPEED_UNKNOWN)
			ambarella_stop_activity(udc);

		ambarella_udc_disable(udc);
	}

	return 0;
}

static int ambarella_udc_vbus_session(struct usb_gadget *gadget, int is_active)
{
	unsigned long	flags;

	local_irq_save(flags);
	if (the_controller->driver)
		ambarella_udc_set_pullup(the_controller, is_active);
	else
		ambarella_udc_set_pullup(the_controller, 0);
	local_irq_restore(flags);

	return 0;
}

static int ambarella_udc_pullup(struct usb_gadget *gadget, int is_on)
{
	ambarella_udc_set_pullup(the_controller, is_on);
	return 0;
}


static const struct usb_gadget_ops ambarella_ops = {
	.get_frame		= ambarella_udc_get_frame,
	.wakeup		= ambarella_udc_wakeup,
	.pullup			= ambarella_udc_pullup,
	.vbus_session		= ambarella_udc_vbus_session,
	/*.set_selfpowered:  Always selfpowered */
};

/*------------------------- gadget driver handling---------------------------*/

/* Tears down device */
static void ambarella_gadget_release(struct device *dev)
{
	struct ambarella_udc *udc = dev_get_drvdata(dev);
	kfree(udc);
}

static void ambarella_init_gadget(struct ambarella_udc *udc,
	struct platform_device *pdev)
{
	struct ambarella_ep *ep;
	u32 i;

	spin_lock_init (&udc->lock);

	dev_set_name(&udc->gadget.dev, "gadget");
	udc->gadget.ops = &ambarella_ops;
	udc->gadget.name = gadget_name;
	udc->gadget.is_dualspeed = 1;
	udc->gadget.ep0 = &udc->ep[CTRL_IN].ep;
	udc->gadget.dev.parent = &pdev->dev;
	udc->gadget.dev.release = ambarella_gadget_release;
	pdev->dev.coherent_dma_mask = DMA_32BIT_MASK;

	device_initialize(&udc->gadget.dev);

	/* set basic ep parameters */
	for (i = 0; i < EP_NUM_MAX; i++) {
		ep = &udc->ep[i];
		ep->ep.name = amb_ep_string[i];
		ep->id = i;
		ep->ep.ops = &ambarella_ep_ops;
		ep->ep.maxpacket = (unsigned short) ~0;

		if (i < EP_IN_NUM) {
			ep->dir = USB_DIR_IN;
		} else {
			ep->dir = USB_DIR_OUT;
		}
	}

	udc->ep[CTRL_IN].ep.maxpacket = USB_EP_CTRL_MAX_PKT_SZ;
	udc->ep[CTRL_OUT].ep.maxpacket = USB_EP_CTRL_MAX_PKT_SZ;

	return;
}


/*
 * ambarella_udc_enable
 */
static void ambarella_udc_enable(struct ambarella_udc *udc)
{
	udc->gadget.speed = USB_SPEED_UNKNOWN; 

	/* Disable Tx and Rx DMA */
	amba_clrbits(UDC_CTRL_REG,
		UDC_CTRL_RCV_DMA_EN | UDC_CTRL_TRN_DMA_EN);
	
	/* flush all of dma fifo */
	ambarella_udc_fifo_flush();

	/* initialize ep0 register */
	init_ep0(udc);

	/* enable ep0 interrupt. */
	amba_clrbits(UDC_EP_INTR_MSK_REG,
		UDC_EP_INTR_MSK_EP0_IN | UDC_EP_INTR_MSK_EP0_OUT);

	/* enable Tx and Rx DMA */
	amba_setbits(UDC_CTRL_REG,
		UDC_CTRL_RCV_DMA_EN | UDC_CTRL_TRN_DMA_EN);

	/* enable device interrupt:
	  * Set_Configure, Set_Interface, Speed Enumerate Complete, Reset */
	amba_clrbits(UDC_INTR_MSK_REG, UDC_INTR_MSK_SC | UDC_INTR_MSK_SI |
		UDC_INTR_MSK_ENUM | UDC_INTR_MSK_UR);

	/* Reconnect if udc is soft-disconnect */
	if(ambarella_check_softdis())
		amba_clrbits(UDC_CTRL_REG, UDC_CTRL_SOFTDISC);

	/* Resume if udc is connected to host */
	if(ambarella_check_connected()) {
		amba_setbits(UDC_CTRL_REG, UDC_CTRL_RES);

	}
}

/*
 * ambarella_udc_disable
 */
static void ambarella_udc_disable(struct ambarella_udc *udc)
{
	dprintk(DEBUG_NORMAL, "%s() Enter.\n", __func__);

	/* Disable all interrupts and Clear the interrupt registers */
	ambarella_disable_all_intr();

	/* Disable Tx and Rx DMA */
	amba_clrbits(UDC_CTRL_REG, UDC_CTRL_RCV_DMA_EN | UDC_CTRL_TRN_DMA_EN);

	/* Good bye, cruel world - Set soft disconnect  */
	ambarella_set_softdis();

	udc->gadget.speed = USB_SPEED_UNKNOWN;

	dprintk(DEBUG_NORMAL, "%s() Exit.\n", __func__);
}


/*
 * ambarella_udc_reinit
 */
static void ambarella_udc_reinit(struct ambarella_udc *udc)
{
	u32 i;

	dprintk(DEBUG_NORMAL, "Enter\n");

	/* device/ep0 records init */
	INIT_LIST_HEAD (&udc->gadget.ep_list);
	INIT_LIST_HEAD (&udc->gadget.ep0->ep_list);
	udc->auto_ack_0_pkt = 0;
	udc->remote_wakeup_en = 0;

	for (i = 0; i < EP_NUM_MAX; i++) {
		struct ambarella_ep *ep = &udc->ep[i];

		if (!IS_EP0(ep))
			list_add_tail (&ep->ep.ep_list, &udc->gadget.ep_list);

		ep->udc = udc;
		ep->desc = NULL;
		ep->halted = 0;
		ep->data_desc = NULL;
		ep->last_data_desc = NULL;
		INIT_LIST_HEAD (&ep->queue);
	}

	dprintk(DEBUG_NORMAL, "Exit\n");
}


/*
 *  usb_gadget_register_driver
 */
int usb_gadget_register_driver(struct usb_gadget_driver *driver)
{
	struct ambarella_udc *udc = the_controller;
	int		retval;

	dprintk(DEBUG_NORMAL, "driver name is '%s'\n",
		driver->driver.name);

	/* Sanity checks */
	if (!udc)
		return -ENODEV;

	if (udc->driver)
		return -EBUSY;

	if (!driver->bind || !driver->setup
			|| driver->speed != USB_SPEED_HIGH) {
		printk(KERN_ERR "Invalid driver: bind %p setup %p speed %d\n",
			driver->bind, driver->setup, driver->speed);
		return -EINVAL;
	}
#if defined(MODULE)
	if (!driver->unbind) {
		printk(KERN_ERR "Invalid driver: no unbind method\n");
		return -EINVAL;
	}
#endif

	/* Hook the driver */
	driver->driver.bus = NULL;
	udc->driver = driver;
	udc->gadget.dev.driver = &driver->driver;

	/* Bind the driver */
	if ((retval = device_add(&udc->gadget.dev)) != 0) {
		printk(KERN_ERR "Error in device_add() : %d\n",retval);
		goto register_error;
	}

	dprintk(DEBUG_NORMAL, "binding gadget driver '%s'\n",
		driver->driver.name);

	if ((retval = driver->bind (&udc->gadget)) != 0) {
		device_del(&udc->gadget.dev);
		goto register_error;
	}

	/* Enable udc */
	ambarella_udc_enable(udc);

	dprintk(DEBUG_NORMAL, "usb_gadget_register_driver() Exit\n");

	return 0;

register_error:
	udc->driver = NULL;
	udc->gadget.dev.driver = NULL;
	return retval;
}

/*
 *	usb_gadget_unregister_driver
 */
int usb_gadget_unregister_driver(struct usb_gadget_driver *driver)
{
	struct ambarella_udc *udc = the_controller;

	dprintk(DEBUG_NORMAL,"usb_gadget_register_driver() '%s'\n",
		driver->driver.name);

	if (!udc)
		return -ENODEV;

	if (!driver || driver != udc->driver || !driver->unbind)
		return -EINVAL;

	ambarella_stop_activity(udc);

	if (driver->unbind)
		driver->unbind(&udc->gadget);

	device_del(&udc->gadget.dev);
	udc->driver = NULL;

	ambarella_udc_disable(udc);

	return 0;
}

/*---------------------------------------------------------------------------*/
/*
 * Name: ambarella_udc_probe
 * Description:
 *	Probe udc driver.
 */
static int ambarella_udc_probe(struct platform_device *pdev)
{
	struct ambarella_udc *udc;
	int retval;

	dprintk(DEBUG_NORMAL, "%s()\n", __func__);

	/* Initial USB PLL */
	rct_set_usb_debounce();
	rct_set_usb_ana_on();

	udc = kzalloc(sizeof(struct ambarella_udc), GFP_KERNEL);
	if (!udc) {
		retval = -ENOMEM;
		goto out;
	}
	the_controller = udc;
	platform_set_drvdata(pdev, udc);

	ambarella_init_gadget(udc, pdev);
	ambarella_udc_reinit(udc);
	ambarella_regaddr_map(udc);

	/*initial usb hardware, and set soft disconnect if VBUS is on */
	ambarella_init_usb();
	ambarella_set_softdis();

	/* DMA pool create */
	udc->desc_dma_pool = dma_pool_create("desc_dma_pool", NULL,
		sizeof(struct ambarella_data_desc), 16, 0);
	if (!udc->desc_dma_pool) {
		dprintk(DEBUG_ERR, "can't get descriptor dma pool\n");
		retval = -ENOMEM;
		goto err_out1;
	}

	udc->setup_buf = dma_pool_alloc(udc->desc_dma_pool, GFP_KERNEL,
		&udc->setup_addr);
	if(udc->setup_buf == NULL) {
		printk(KERN_ERR "No memory to DMA\n");
		retval = -ENOMEM;
		goto err_out2;
	}

	retval = init_null_pkt_desc(udc);
	if(retval){
		goto err_out3;
	}

	/* irq setup after old hardware state is cleaned up */
	retval = request_irq(USBC_IRQ, ambarella_udc_irq,
			IRQF_SHARED | IRQF_TRIGGER_HIGH, gadget_name, udc);
	if (retval != 0) {
		dprintk(DEBUG_NORMAL, "cannot get irq %i, err %d\n", USBC_IRQ, retval);
		goto err_out4;
	}

#if 0
	retval = request_irq(USBVBUS_IRQ, ambarella_udc_vbus_irq,
			0, gadget_name, udc);
	if (retval != 0) {
		dprintk(DEBUG_NORMAL, "can't get vbus irq %i, err %d\n",
			USBVBUS_IRQ, retval);
		retval = -EBUSY;
		free_irq(USBC_IRQ, udc);
		return retval;
	}
#endif
	create_proc_files();

	dprintk(DEBUG_NORMAL, "probe ok\n");

	goto out;

err_out4:
	dma_pool_free(udc->desc_dma_pool, udc->dummy_desc, udc->dummy_desc_addr);
err_out3:
	dma_pool_free(udc->desc_dma_pool, udc->setup_buf, udc->setup_addr);
err_out2:
	dma_pool_destroy(udc->desc_dma_pool);
err_out1:
	kfree(udc);
out:
	return retval;
}


/*
 * Name: ambarella_udc_remove
 * Description:
 *	Remove udc driver.
 */
static int ambarella_udc_remove(struct platform_device *pdev)
{
	struct ambarella_udc *udc = platform_get_drvdata(pdev);

	dprintk(DEBUG_NORMAL, "%s()\n", __func__);
	if (udc->driver)
		return -EBUSY;

	remove_proc_files();

	free_irq(USBC_IRQ, udc);

	dma_pool_free(udc->desc_dma_pool, udc->dummy_desc, udc->dummy_desc_addr);
	dma_pool_free(udc->desc_dma_pool, udc->setup_buf, udc->setup_addr);
	dma_pool_destroy(udc->desc_dma_pool);

	platform_set_drvdata(pdev, NULL);

	dprintk(DEBUG_NORMAL, "%s: remove ok\n", __func__);

	return 0;
}


#ifdef CONFIG_PM
static int ambarella_udc_suspend(struct platform_device *pdev, pm_message_t message)
{
	struct ambarella_udc *udc;
	int retval = 0;

	udc = platform_get_drvdata(pdev);
	udc->sys_suspended = 1;

	dev_info(&pdev->dev, "%s exit with %d @ %d\n",
		__func__, retval, message.event);

	ambarella_udc_set_pullup(udc, 0);

	return retval;
}

static int ambarella_udc_resume(struct platform_device *pdev)
{
	struct ambarella_udc *udc;
	int retval = 0;

	udc = platform_get_drvdata(pdev);
	udc->sys_suspended = 0;

	dev_info(&pdev->dev, "%s exit with %d\n", __func__, retval);

	ambarella_udc_set_pullup(udc, 1);

	return retval;
}
#endif

static struct platform_driver ambarella_udc_driver = {
	.driver		= {
		.name	= "ambarella-udc",
		.owner	= THIS_MODULE,
	},
	.probe		= ambarella_udc_probe,
	.remove	= ambarella_udc_remove,
#ifdef CONFIG_PM
	.suspend	= ambarella_udc_suspend,
	.resume		= ambarella_udc_resume,
#endif
};


static int __init udc_init(void)
{
	int retval = 0;

	printk(KERN_INFO "%s: version %s\n", gadget_name, DRIVER_VERSION);
	retval = platform_driver_register(&ambarella_udc_driver);

	return retval;
}

static void __exit udc_exit(void)
{
	platform_driver_unregister(&ambarella_udc_driver);
}

EXPORT_SYMBOL(usb_gadget_register_driver);
EXPORT_SYMBOL(usb_gadget_unregister_driver);

module_init(udc_init);
module_exit(udc_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:ambarella-usbgadget");

