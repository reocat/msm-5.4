/*
 * drivers/ambarella-ipc/irq.c
 *
 * Ambarella IPC in Linux kernel-space.
 *
 * Authors:
 *	Charles Chiou <cchiou@ambarella.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * Copyright (C) 2009-2010, Ambarella Inc.
 */

#include <asm/mach-types.h>
#include <asm/io.h>
#include <mach/boss.h>
#include <linux/interrupt.h>
#include <linux/aipc/aipc.h>
#include <linux/aipc/irq.h>

#if (CHIP_REV == I1)
#define USE_VIC_IRQ	0
#elif (CHIP_REV == A5S)
#define USE_VIC_IRQ	1
#else
#error "Boss is not supported on this chip!"
#endif

extern void ipc_binder_request(void);
extern void ipc_binder_reply(void);

/*
 * IPC IRQ object.
 */
struct ipc_irq_s
{
	int irqno_uitron_req;	/* IRQ line number of uITRON */
	int irqno_uitron_rly;	/* IRQ line number of uITRON */
	int irqno_linux_req;	/* IRQ line number of Linux */
	int irqno_linux_rly;	/* IRQ line number of Linux */
	struct ipc_irq_stat_s stat;	/* Status */
};

static struct ipc_irq_s ipc_irq;	/* The global instance */

/*
 * Send an interrupt to the other OS.
 */
void ipc_send_irq(int type)
{
	int irqno;

	if (type == IPC_IRQ_REQ) {
		irqno = ipc_irq.irqno_uitron_req;
	} else {
		irqno = ipc_irq.irqno_uitron_rly;
	}

	ipc_irq.stat.sent++;
	if (irqno < 32) {
		__amba_writel(VIC_SOFTEN_REG, 0x1 << irqno);
	}
#if (VIC_INSTANCES >= 2)
	else if (irqno < 64) {
		__amba_writel(VIC2_SOFTEN_REG, 0x1 << (irqno - 32));
	}
#endif
#if (VIC_INSTANCES >= 3)
	else if (irqno < 96) {
		__amba_writel(VIC3_SOFTEN_REG, 0x1 << (irqno - 64));
	}
#endif
	else {
		BUG();
	}
}

/*
 * Send a faked interrupt to ourselves... Sort of a 'loop-back' test.
 */
void ipc_fake_irq(int type)
{
	int irqno;

	if (type == IPC_IRQ_REQ) {
		irqno = ipc_irq.irqno_linux_req;
	} else {
		irqno = ipc_irq.irqno_linux_rly;
	}

#if USE_VIC_IRQ
	if (irqno < 32) {
		__amba_writel(VIC_SOFTEN_REG, 0x1 << irqno);
	}
#if (VIC_INSTANCES >= 2)
	else if (irqno < 64) {
		__amba_writel(VIC2_SOFTEN_REG, 0x1 << (irqno - 32));
	}
#endif
#if (VIC_INSTANCES >= 3)
	else if (irqno < 96) {
		__amba_writel(VIC3_SOFTEN_REG, 0x1 << (irqno - 64));
	}
#endif
	else {
		BUG();
	}
#else	/* !USE_VIC_IRQ */
#if (CHIP_REV == I1)
	__amba_writel(AHB_SCRATCHPAD_REG(0x10),
		0x1 << (irqno - AXI_SOFT_IRQ(0)));
#else
#error "GIC is only supported on i1!"
#endif
#endif
}

/*
 * IRQ handler for interrupt sent from the other OS.
 */
static irqreturn_t ipc_irq_handler(int irqno, void *args)
{
	int type = ((int) args) - 1;

	ipc_irq.stat.recv++;

#if USE_VIC_IRQ
	if (irqno < 32) {
		__amba_writel(VIC_SOFTEN_CLR_REG, 0x1 << irqno);
	}
#if (VIC_INSTANCES >= 2)
	else if (irqno < 64) {
		__amba_writel(VIC2_SOFTEN_CLR_REG, 0x1 << (irqno - 32));
	}
#endif
#if (VIC_INSTANCES >= 3)
	else if (irqno < 96) {
		__amba_writel(VIC3_SOFTEN_CLR_REG, 0x1 << (irqno - 64));
	}
#endif
	else {
		BUG();
	}
#else	/* !USE_VIC_IRQ */
#if (CHIP_REV == I1)
	__amba_writel(AHB_SCRATCHPAD_REG(0x14),
		0x1 << (irqno - AXI_SOFT_IRQ(0)));
#else
#error "GIC is only supported on i1!"
#endif
#endif

	/* Call out to the handler in binder */
	if (type == IPC_IRQ_REQ) {
		ipc_binder_request();
	} else {
		ipc_binder_reply();
	}

	return IRQ_HANDLED;
}

/*
 * Enable or disable IRQ.
 */
void ipc_irq_enable(int type, int enabled)
{
	int irqno;

	if (type == IPC_IRQ_REQ) {
		irqno = ipc_irq.irqno_linux_req;
	} else {
		irqno = ipc_irq.irqno_linux_rly;
	}

	if (enabled) {
		ipc_irq.stat.enabled |= (1 << irqno);
#if USE_VIC_IRQ
		/* Sanitize the VIC line of the IRQ first */
		if (irqno < 32) {
			__amba_writel(VIC_SOFTEN_CLR_REG, 0x1 << irqno);
		}
#if (VIC_INSTANCES >= 2)
		else if (irqno < 64) {
			__amba_writel(VIC2_SOFTEN_CLR_REG, 0x1 << (irqno - 32));
		}
#endif
#if (VIC_INSTANCES >= 3)
		else if (irqno < 96) {
			__amba_writel(VIC3_SOFTEN_CLR_REG, 0x1 << (irqno - 64));
		}
#endif
		else {
			BUG();
		}
#endif
		enable_irq(irqno);
	} else {
		ipc_irq.stat.enabled &= ~(1 << irqno);
		disable_irq(irqno);
	}
}

/*
 * Retrieve status.
 */
void ipc_irq_get_stat(struct ipc_irq_stat_s *stat)
{
	BUG_ON(!stat);
	memcpy(stat, &ipc_irq.stat, sizeof(*stat));
}

/*
 * Entry point for performing IPC IRQ initialization.
 */
void ipc_irq_init(void)
{
	int rval;

	ipc_irq.irqno_uitron_req = IPC_L2I_INT_REQ_VEC;
	ipc_irq.irqno_uitron_rly = IPC_L2I_INT_RLY_VEC;
	ipc_irq.irqno_linux_req = IPC_I2L_INT_REQ_VEC;
	ipc_irq.irqno_linux_rly = IPC_I2L_INT_RLY_VEC;

	printk (KERN_NOTICE "aipc: irq => l2i: %d %d, i2l: %d %d\n",
		ipc_irq.irqno_uitron_req, ipc_irq.irqno_uitron_rly,
		ipc_irq.irqno_linux_req, ipc_irq.irqno_linux_rly);

	/* Register IRQ to system and enable it */
	rval = request_irq(ipc_irq.irqno_linux_req,
			   ipc_irq_handler,
			   IRQF_TRIGGER_HIGH | IRQF_SHARED,
			   "IPC_REQ",
			   (void *) IPC_IRQ_REQ + 1);
	rval = request_irq(ipc_irq.irqno_linux_rly,
			   ipc_irq_handler,
			   IRQF_TRIGGER_HIGH | IRQF_SHARED,
			   "IPC_RLY",
			   (void *) IPC_IRQ_RLY + 1);

	BUG_ON(rval != 0);
}

void ipc_irq_free(void)
{
	free_irq(ipc_irq.irqno_linux_req, (void *) IPC_IRQ_REQ);
	free_irq(ipc_irq.irqno_linux_rly, (void *) IPC_IRQ_RLY);
}

