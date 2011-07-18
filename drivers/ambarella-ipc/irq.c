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

extern void ipc_binder_dispatch(void);

/*
 * IPC IRQ object.
 */
struct ipc_irq_s
{
	int irqno_uitron;	/* IRQ line number of uITRON */
	int irqno_linux;	/* IRQ line number of Linux */
	struct ipc_irq_stat_s stat;	/* Status */
};

static struct ipc_irq_s ipc_irq;	/* The global instance */

/*
 * Send an interrupt to the other OS.
 */
void ipc_send_irq(void)
{
	ipc_irq.stat.sent++;
	if (ipc_irq.irqno_uitron < 32) {
		__raw_writel(0x1 << ipc_irq.irqno_uitron,
			     VIC_SOFTEN_REG);
	}
#if (VIC_INSTANCES >= 2)
	else if (ipc_irq.irqno_uitron < 64) {
		__raw_writel(0x1 << (ipc_irq.irqno_uitron - 32),
			     VIC2_SOFTEN_REG);
	}
#endif
#if (VIC_INSTANCES >= 3)
	else if (ipc_irq.irqno_uitron < 96) {
		__raw_writel(0x1 << (ipc_irq.irqno_uitron - 64),
			     VIC3_SOFTEN_REG);
	}
#endif
	else {
		BUG();
	}
}

/*
 * Send a faked interrupt to ourselves... Sort of a 'loop-back' test.
 */
void ipc_fake_irq(void)
{
#if USE_VIC_IRQ
	if (ipc_irq.irqno_linux < 32) {
		__raw_writel(0x1 << ipc_irq.irqno_linux,
			VIC_SOFTEN_REG);
	}
#if (VIC_INSTANCES >= 2)
	else if (ipc_irq.irqno_linux < 64) {
		__raw_writel(0x1 << (ipc_irq.irqno_linux - 32),
			     VIC2_SOFTEN_REG);
	}
#endif
#if (VIC_INSTANCES >= 3)
	else if (ipc_irq.irqno_linux < 96) {
		__raw_writel(0x1 << (ipc_irq.irqno_linux - 64),
			     VIC3_SOFTEN_REG);
	}
#endif
	else {
		BUG();
	}
#else	/* !USE_VIC_IRQ */
#if (CHIP_REV == I1)
	__raw_writel(
	       0x1 << (ipc_irq.irqno_linux - AXI_SOFT_IRQ(0)),
	       0xe0019010);
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
	ipc_irq.stat.recv++;

#if USE_VIC_IRQ
	if (ipc_irq.irqno_linux < 32) {
		__raw_writel(0x1 << ipc_irq.irqno_linux,
			     VIC_SOFTEN_CLR_REG);
	}
#if (VIC_INSTANCES >= 2)
	else if (ipc_irq.irqno_linux < 64) {
		__raw_writel(0x1 << (ipc_irq.irqno_linux - 32),
			     VIC2_SOFTEN_CLR_REG);
	}
#endif
#if (VIC_INSTANCES >= 3)
	else if (ipc_irq.irqno_linux < 96) {
		__raw_writel(0x1 << (ipc_irq.irqno_linux - 64),
			     VIC3_SOFTEN_CLR_REG);
	}
#endif
	else {
		BUG();
	}
#else	/* !USE_VIC_IRQ */
#if (CHIP_REV == I1)
	__raw_writel (0x1 << (ipc_irq.irqno_linux - AXI_SOFT_IRQ(0)),
	AHB_SCRATCHPAD_REG(0x14));
#else
#error "GIC is only supported on i1!"
#endif
#endif

	ipc_binder_dispatch();	/* Call out to the handler in binder */

	return IRQ_HANDLED;
}

/*
 * Enable or disable IRQ.
 */
void ipc_irq_enable(int enabled)
{
	if (enabled) {
		ipc_irq.stat.enabled = 1;
#if USE_VIC_IRQ
		/* Sanitize the VIC line of the IRQ first */
		if (ipc_irq.irqno_linux < 32) {
			__raw_writel(0x1 << ipc_irq.irqno_linux,
				     VIC_SOFTEN_CLR_REG);
		}
#if (VIC_INSTANCES >= 2)
		else if (ipc_irq.irqno_linux < 64) {
			__raw_writel(0x1 << (ipc_irq.irqno_linux - 32),
				     VIC2_SOFTEN_CLR_REG);
		}
#endif
#if (VIC_INSTANCES >= 3)
		else if (ipc_irq.irqno_linux < 96) {
			__raw_writel(0x1 << (ipc_irq.irqno_linux - 64),
				     VIC3_SOFTEN_CLR_REG);
		}
#endif
		else {
			BUG();
		}
#endif
		enable_irq(ipc_irq.irqno_linux);
	} else {
		ipc_irq.stat.enabled = 0;
		disable_irq(ipc_irq.irqno_linux);
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

	ipc_irq.irqno_uitron = IPC_L2I_INT_VEC;
	ipc_irq.irqno_linux = IPC_I2L_INT_VEC;

	printk (KERN_NOTICE "aipc: irq => l2i: %d, i2l: %d\n", 
		ipc_irq.irqno_uitron, ipc_irq.irqno_linux);

	/* Register IRQ to system and enable it */
	rval = request_irq(ipc_irq.irqno_linux,
			   ipc_irq_handler,
			   IRQF_TRIGGER_HIGH | IRQF_SHARED,
			   "IPC",
			   &ipc_irq);
	BUG_ON(rval != 0);
}

void ipc_irq_free(void)
{
	free_irq(ipc_irq.irqno_linux, &ipc_irq);
}

