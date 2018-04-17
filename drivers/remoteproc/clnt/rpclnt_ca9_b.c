/**
 * system/src/rpclnt/rpclnt.h
 *
 * History:
 *    2012/08/15 - [Tzu-Jung Lee] created file
 *
 * Copyright 2008-2015 Ambarella Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/regmap.h>
#include <linux/of_irq.h>
#include <linux/mfd/syscon.h>

#include <plat/remoteproc_cfg.h>
#include <plat/rpdev.h>
#include <plat/chip.h>

#include "rpclnt.h"
#include "vq.h"

extern void rpdev_svq_cb(struct vq *vq);
extern void rpdev_rvq_cb(struct vq *vq);
extern struct ambalink_shared_memory_layout ambalink_shm_layout;

DECLARE_COMPLETION(rpclnt_comp);

static struct rpclnt G_rproc_ca9_b = {

	.name		= "rpclnt_ca9_a_and_b",
	.inited		= 0,

	.svq_tx_irq	= VRING_IRQ_C2_TO_C1_KICK,
	.svq_num_bufs	= MAX_RPMSG_NUM_BUFS >> 1,

	.svq_vring_algn	= RPMSG_VRING_ALIGN,

	.rvq_tx_irq	= VRING_IRQ_C2_TO_C1_ACK,
	.rvq_num_bufs	= MAX_RPMSG_NUM_BUFS >> 1,
	.rvq_vring_algn	= RPMSG_VRING_ALIGN,
};


struct rpclnt *rpclnt_sync(const char *bus_name)
{

	struct rpclnt *rpclnt = &G_rproc_ca9_b;

	if (strcmp(bus_name, "ca9_a_and_b"))
		return NULL;

	if (!rpclnt->inited)
		wait_for_completion(&rpclnt_comp);


	return &G_rproc_ca9_b;
}

static void rpclnt_complete_registration(struct rpclnt *rpclnt)
{
	void *buf = (void *)rpclnt->rvq_buf_phys;

	vq_init_unused_bufs(rpclnt->rvq, buf, RPMSG_BUF_SIZE);

	rpclnt->inited = 1;
	complete_all(&rpclnt_comp);
}

static void rpmsg_send_irq(struct regmap *reg_ahb_scr, unsigned long irq)
{
#ifdef AXI_SOFT_IRQ2
	if (irq < AXI_SOFT_IRQ2(0))
		regmap_write_bits(reg_ahb_scr, AHB_SP_SWI_SET_OFFSET,
				0x1 << (irq - AXI_SOFT_IRQ(0)),
				0x1 << (irq - AXI_SOFT_IRQ(0)));
	else {
		regmap_write_bits(reg_ahb_scr, AHB_SP_SWI_SET_OFFSET1,
				0x1 << (irq - AXI_SOFT_IRQ2(0)),
				0x1 << (irq - AXI_SOFT_IRQ2(0)));
	}
#else
		regmap_write_bits(reg_ahb_scr, AHB_SP_SWI_SET_OFFSET,
			0x1 << (irq - AXI_SOFT_IRQ(0)),
			0x1 << (irq - AXI_SOFT_IRQ(0)));
#endif
/*
	if (irq < AXI_SOFT_IRQ(4)) {
	        regmap_write_bits(reg_ahb_scr, AHB_SP_SWI_SET_OFFSET,
	                        0x1 << (irq - AXI_SOFT_IRQ(0)),
	                        0x1 << (irq - AXI_SOFT_IRQ(0)));
	} else {
		if (CHIP_REV == S5L)
			regmap_write_bits(reg_ahb_scr, AHB_SP_SWI_SET_OFFSET,
	                        0x1 << (irq - AXI_SOFT_IRQ(0)),
	                        0x1 << (irq - AXI_SOFT_IRQ(0)));
	else if (CHIP_REV == S5)
			regmap_write_bits(reg_ahb_scr, AHB_SP_SWI_SET_OFFSET1,
	                        0x1 << (irq - AXI_SOFT_IRQ2(0)),
	                        0x1 << (irq - AXI_SOFT_IRQ2(0)));
	}
*/
}

static void rpmsg_ack_irq(struct regmap *reg_ahb_scr, unsigned long irq)
{
#ifdef AXI_SOFT_IRQ2
	if (irq < AXI_SOFT_IRQ2(0)+32)
		regmap_write(reg_ahb_scr, AHB_SP_SWI_CLEAR_OFFSET,
				0x1 << (irq - AXI_SOFT_IRQ(0)));
	else {
		regmap_write(reg_ahb_scr, AHB_SP_SWI_CLEAR_OFFSET1,
				0x1 << (irq - AXI_SOFT_IRQ2(0)));
	}
#else
	regmap_write(reg_ahb_scr, AHB_SP_SWI_CLEAR_OFFSET,
		0x1 << (irq - AXI_SOFT_IRQ(0)));
#endif
/*
	if (irq < (AXI_SOFT_IRQ(4)+32)) {
		regmap_write(reg_ahb_scr, AHB_SP_SWI_CLEAR_OFFSET,
				0x1 << (irq - AXI_SOFT_IRQ(0)));
	} else {
	if (CHIP_REV == S5L)
		regmap_write(reg_ahb_scr, AHB_SP_SWI_CLEAR_OFFSET,
				0x1 << (irq - AXI_SOFT_IRQ(0)));
	else if  (CHIP_REV == S5)
		regmap_write(reg_ahb_scr, AHB_SP_SWI_CLEAR_OFFSET1,
				0x1 << (irq - AXI_SOFT_IRQ2(0)));
	}
*/
}

static irqreturn_t rpclnt_isr(int irq, void *dev_id)
{

	struct rpclnt *rpclnt = dev_id;
	struct irq_data *irq_data;

	irq_data = irq_get_irq_data(irq);

	if (!rpclnt->inited && irq == rpclnt->svq_rx_irq) {

		rpclnt_complete_registration(rpclnt);

		rpmsg_ack_irq(rpclnt->reg_ahb_scr, irq_data->hwirq);

		return IRQ_HANDLED;
	}

	/*
	 * Before scheduling the bottom half for processing the messages,
	 * we tell the remote host not to notify us (generate interrupts) when
	 * subsequent messages are enqueued.  The bottom half will pull out
	 * this message and the pending ones.  Once it processed all the messages
	 * in the queue, it will re-enable remote host for the notification.
	 */
	if (irq == rpclnt->rvq_rx_irq) {


		vq_disable_used_notify(rpclnt->rvq);
		schedule_work(&rpclnt->rvq_work);

	} else if (irq == rpclnt->svq_rx_irq) {


		vq_disable_used_notify(rpclnt->svq);
		schedule_work(&rpclnt->svq_work);
	}


	rpmsg_ack_irq(rpclnt->reg_ahb_scr, irq_data->hwirq);


	return IRQ_HANDLED;
}

static void rpclnt_rvq_worker(struct work_struct *work)
{

	struct rpclnt *rpclnt = container_of(work, struct rpclnt, rvq_work);
	struct vq *vq = rpclnt->rvq;

	if (vq->cb)
		vq->cb(vq);
}

static void rpclnt_svq_worker(struct work_struct *work)
{

	struct rpclnt *rpclnt = container_of(work, struct rpclnt, svq_work);
	struct vq *vq = rpclnt->svq;

	if (vq->cb)
		vq->cb(vq);
}

static void rpclnt_kick_rvq(void *data)
{

	struct rpclnt *rpclnt = data;
	struct vq *vq = rpclnt->rvq;
	unsigned long irq = rpclnt->rvq_tx_irq;
	/*
	 * Honor the flag set by the remote host.
	 *
	 * Most of the time, the remote host want to supress their
	 * tx-complete interrupts.  In this case, we don't bother it.
	 */
	if (vq_kick_prepare(vq)) {

		rpmsg_send_irq(rpclnt->reg_ahb_scr, irq);
	}
}

static void rpclnt_kick_svq(void *data)
{

	struct rpclnt *rpclnt = data;
	struct vq *vq = rpclnt->svq;
	unsigned long irq = rpclnt->svq_tx_irq;
	/*
	 * Honor the flag set by the remote host.
	 *
	 * When the remote host is already busy enough processing the
	 * messages, it might suppress the interrupt and work in polling
	 * mode.
	 */
	if (vq_kick_prepare(vq)) {

		rpmsg_send_irq(rpclnt->reg_ahb_scr, irq);
	}
}

static const struct of_device_id ambarella_rpclnt_of_match[] __initconst = {
	{.compatible = "ambarella,rpclnt", },
	{},
};
MODULE_DEVICE_TABLE(of, ambarella_rpclnt_of_match);

static void __init rpclnt_drv_init(struct device_node *np)
{


	int ret;
	struct rpclnt *rpclnt = &G_rproc_ca9_b;
	/*--------*/
	rpclnt->name		= "rpclnt_ca9_a_and_b",
	rpclnt->inited		= 0,

	rpclnt->svq_tx_irq	= VRING_IRQ_C2_TO_C1_KICK,
	rpclnt->svq_num_bufs	= MAX_RPMSG_NUM_BUFS >> 1,

	rpclnt->svq_vring_algn	= RPMSG_VRING_ALIGN,

	rpclnt->rvq_tx_irq	= VRING_IRQ_C2_TO_C1_ACK,
	rpclnt->rvq_num_bufs	= MAX_RPMSG_NUM_BUFS >> 1,
	rpclnt->rvq_vring_algn	= RPMSG_VRING_ALIGN,

	/*--------*/

	rpclnt->svq_buf_phys	= ambalink_shm_layout.vring_c0_and_c1_buf;
	rpclnt->svq_vring_phys	= ambalink_shm_layout.vring_c0_to_c1;
	rpclnt->rvq_buf_phys	= ambalink_shm_layout.vring_c0_and_c1_buf + (RPMSG_TOTAL_BUF_SPACE >> 1);
	rpclnt->rvq_vring_phys	= ambalink_shm_layout.vring_c1_to_c0;

	rpclnt->svq = vq_create(rpdev_svq_cb,
				rpclnt_kick_svq,
				rpclnt->svq_num_bufs,
				phys_to_virt(rpclnt->svq_vring_phys),
				rpclnt->svq_vring_algn);

	rpclnt->rvq = vq_create(rpdev_rvq_cb,
				rpclnt_kick_rvq,
				rpclnt->rvq_num_bufs,
				phys_to_virt(rpclnt->rvq_vring_phys),
				rpclnt->rvq_vring_algn);

	INIT_WORK(&rpclnt->svq_work, rpclnt_svq_worker);
	INIT_WORK(&rpclnt->rvq_work, rpclnt_rvq_worker);

	rpclnt->reg_ahb_scr = syscon_regmap_lookup_by_phandle(np, "amb,scr-regmap");

        if (IS_ERR(rpclnt->reg_ahb_scr)) {
                printk(KERN_ERR "no scr regmap!\n");
                ret = PTR_ERR(rpclnt->reg_ahb_scr);
        }

        rpclnt->svq_rx_irq = irq_of_parse_and_map(np, 1);


	ret = request_irq(rpclnt->svq_rx_irq, rpclnt_isr,
			  IRQF_SHARED | IRQF_TRIGGER_HIGH, "rpclnt_svq_rx", rpclnt);
printk("==> %s:%d: request_irq=rpclnt->svq_rx_irq= %d retval= %d",__func__,__LINE__,rpclnt->svq_rx_irq, ret);



	if (ret) {
		printk("Error: failed to request svq_rx_irq: %d, err: %d\n",
		       rpclnt->svq_rx_irq, ret);
	}

        rpclnt->rvq_rx_irq = irq_of_parse_and_map(np, 0);

	ret = request_irq(rpclnt->rvq_rx_irq, rpclnt_isr,
			  IRQF_SHARED | IRQF_TRIGGER_HIGH, "rpclnt_rvq_rx", rpclnt);




	if (ret) {
		printk("Error: failed to request rvq_rx_irq: %d, err: %d\n",
		       rpclnt->rvq_rx_irq, ret);
	}



}
OF_DECLARE_1(rpclnt, ambarella_rpclnt, "ambarella,rpclnt", rpclnt_drv_init);

extern struct of_device_id __rpclnt_of_table[];

static const struct of_device_id __rpclnt_of_table_sentinel
	__used __section(__rpclnt_of_table_end);

static int __init rpclnt_probe(void)
{
	int ret = 0;
	struct device_node *np;
	const struct of_device_id *match;
	of_init_fn_1 init_func;

	for_each_matching_node_and_match(np, __rpclnt_of_table, &match) {
		if (!of_device_is_available(np))
			continue;

		init_func = match->data;
		init_func(np);
	}

        return ret;
}
subsys_initcall(rpclnt_probe);
