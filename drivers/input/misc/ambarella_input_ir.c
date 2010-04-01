/*
 * drivers/input/misc/ambarella_input_ir.c
 *
 * Author: Qiao Wang <qwang@ambarella.com>
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/input.h>

#include <asm/irq.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/mach-types.h>

#include <mach/hardware.h>
#include "ambarella_input.h"

/* ========================================================================= */
static int ir_protocol = AMBA_IR_PROTOCOL_NEC;
MODULE_PARM_DESC(ir_protocol, "Ambarella IR Protocol ID");
static int print_ir_keycode = 0;
MODULE_PARM_DESC(print_ir_keycode, "Print Key Code");
/* ========================================================================= */

static int ambarella_input_report_ir(struct ambarella_ir_info *pinfo, u32 uid)
{
	int				i;

	if (!pinfo->pkeymap)
		return -1;

	if ((pinfo->last_ir_uid == uid) && (pinfo->last_ir_flag)) {
		pinfo->last_ir_flag--;
		return 0;
	}

	for (i = 0; i < CONFIG_AMBARELLA_INPUT_SIZE; i++) {
		if (pinfo->pkeymap[i].type == AMBA_INPUT_END)
			break;

		if ((pinfo->pkeymap[i].type & AMBA_INPUT_SOURCE_MASK) !=
			AMBA_INPUT_SOURCE_IR)
			continue;

		if ((pinfo->pkeymap[i].type == AMBA_INPUT_IR_KEY) &&
			(pinfo->pkeymap[i].ir_key.raw_id == uid)) {
			input_report_key(pinfo->dev,
				pinfo->pkeymap[i].ir_key.key_code, 1);
			input_report_key(pinfo->dev,
				pinfo->pkeymap[i].ir_key.key_code, 0);
			ambi_dbg("IR_KEY [%d]\n",
				pinfo->pkeymap[i].ir_key.key_code);
			pinfo->last_ir_flag = pinfo->pkeymap[i].ir_key.key_flag;
			break;
		}

		if ((pinfo->pkeymap[i].type == AMBA_INPUT_IR_REL) &&
			(pinfo->pkeymap[i].ir_rel.raw_id == uid)) {
			if (pinfo->pkeymap[i].ir_rel.key_code == REL_X) {
				input_report_rel(pinfo->dev,
					REL_X,
					pinfo->pkeymap[i].ir_rel.rel_step);
				input_report_rel(pinfo->dev,
					REL_Y, 0);
				input_sync(pinfo->input_center->dev);
				ambi_dbg("IR_REL X[%d]:Y[%d]\n",
					pinfo->pkeymap[i].ir_rel.rel_step, 0);
			} else
			if (pinfo->pkeymap[i].ir_rel.key_code == REL_Y) {
				input_report_rel(pinfo->dev,
					REL_X, 0);
				input_report_rel(pinfo->dev,
					REL_Y,
					pinfo->pkeymap[i].ir_rel.rel_step);
				input_sync(pinfo->input_center->dev);
				ambi_dbg("IR_REL X[%d]:Y[%d]\n", 0,
					pinfo->pkeymap[i].ir_rel.rel_step);
			}
			pinfo->last_ir_flag = 0;
			break;
		}

		if ((pinfo->pkeymap[i].type == AMBA_INPUT_IR_ABS) &&
			(pinfo->pkeymap[i].ir_abs.raw_id == uid)) {
			input_report_abs(pinfo->dev,
				ABS_X, pinfo->pkeymap[i].ir_abs.abs_x);
			input_report_abs(pinfo->dev,
				ABS_Y, pinfo->pkeymap[i].ir_abs.abs_y);
			input_sync(pinfo->input_center->dev);
			ambi_dbg("IR_ABS X[%d]:Y[%d]\n",
				pinfo->pkeymap[i].ir_abs.abs_x,
				pinfo->pkeymap[i].ir_abs.abs_y);
			pinfo->last_ir_flag = 0;
			break;
		}
	}

	return 0;
}

inline int ambarella_ir_get_tick_size(struct ambarella_ir_info *pinfo)
{
	int				size = 0;

	if (pinfo->ir_pread > pinfo->ir_pwrite)
		size = MAX_IR_BUFFER - pinfo->ir_pread + pinfo->ir_pwrite;
	else
		size = pinfo->ir_pwrite - pinfo->ir_pread;

	return size;
}

void ambarella_ir_inc_read_ptr(struct ambarella_ir_info *pinfo)
{
	BUG_ON(pinfo->ir_pread == pinfo->ir_pwrite);

	pinfo->ir_pread++;
	if (pinfo->ir_pread >= MAX_IR_BUFFER)
		pinfo->ir_pread = 0;
}

void ambarella_ir_move_read_ptr(struct ambarella_ir_info *pinfo, int offset)
{
	for (; offset > 0; offset--) {
		ambarella_ir_inc_read_ptr(pinfo);
	}
}

static inline void ambarella_ir_write_data(struct ambarella_ir_info *pinfo,
	u16 val)
{
	BUG_ON(pinfo->ir_pwrite < 0);
	BUG_ON(pinfo->ir_pwrite >= MAX_IR_BUFFER);

	pinfo->tick_buf[pinfo->ir_pwrite] = val;

	pinfo->ir_pwrite++;

	if (pinfo->ir_pwrite >= MAX_IR_BUFFER)
		pinfo->ir_pwrite = 0;
}

u16 ambarella_ir_read_data(struct ambarella_ir_info *pinfo, int pointer)
{
	BUG_ON(pointer < 0);
	BUG_ON(pointer >= MAX_IR_BUFFER);
	BUG_ON(pointer == pinfo->ir_pwrite);

	return pinfo->tick_buf[pointer];
}

static inline int ambarella_ir_update_buffer(struct ambarella_ir_info *pinfo)
{
	int				count;
	int				size;

	count = amba_readl(pinfo->regbase + IR_STATUS_OFFSET);
	ambi_dbg("size we got is [%d]\n", count);
	for (; count > 0; count--) {
		ambarella_ir_write_data(pinfo,
			amba_readl(pinfo->regbase + IR_DATA_OFFSET));
	}
	size = ambarella_ir_get_tick_size(pinfo);

	return size;
}

static irqreturn_t ambarella_ir_irq(int irq, void *devid)
{
	struct ambarella_ir_info	*pinfo;
	int				size;
	int				rval;
	u32				uid;
	u32				edges;

	pinfo = (struct ambarella_ir_info *)devid;

	BUG_ON(pinfo->ir_pread < 0);
	BUG_ON(pinfo->ir_pread >= MAX_IR_BUFFER);

	if (amba_readl(pinfo->regbase + IR_CONTROL_OFFSET) & IR_CONTROL_FIFO_OV) {
		while (amba_readl(pinfo->regbase + IR_STATUS_OFFSET) > 0) {
			amba_readl(pinfo->regbase + IR_DATA_OFFSET);
		}
		amba_writel(pinfo->regbase + IR_CONTROL_OFFSET,
			(amba_readl(pinfo->regbase + IR_CONTROL_OFFSET) |
		IR_CONTROL_FIFO_OV));

		dev_err(&pinfo->input_center->dev->dev, "IR_CONTROL_FIFO_OV overflow\n");

		goto ambarella_ir_irq_exit;
	}

	size = ambarella_ir_update_buffer(pinfo);

	if(!pinfo->ir_parse)
		goto ambarella_ir_irq_exit;

	rval = pinfo->ir_parse(pinfo, &uid);

	if (rval == 0) {// yes, we find the key
		if(print_ir_keycode)
			printk(KERN_NOTICE "uid = 0x%08x\n", uid);
		ambarella_input_report_ir(pinfo, uid);
	}

	pinfo->frame_data_to_received = pinfo->frame_info.frame_data_size +
		pinfo->frame_info.frame_head_size;
	pinfo->frame_data_to_received -= ambarella_ir_get_tick_size(pinfo);// data left in buffer

	if (pinfo->frame_data_to_received <= HW_FIFO_BUFFER) {
		edges = pinfo->frame_data_to_received;
		pinfo->frame_data_to_received = 0;
	} else {// > HW_FIFO_BUFFER
		edges = HW_FIFO_BUFFER;
		pinfo->frame_data_to_received -= HW_FIFO_BUFFER;
	}

	ambi_dbg("line[%d],frame_data_to_received[%d]\n",__LINE__,edges);
	amba_clrbitsl(pinfo->regbase + IR_CONTROL_OFFSET, IR_CONTROL_INTLEV(0x3F));
	amba_setbitsl(pinfo->regbase + IR_CONTROL_OFFSET, IR_CONTROL_INTLEV(edges));

ambarella_ir_irq_exit:
	amba_writel(pinfo->regbase + IR_CONTROL_OFFSET,
		(amba_readl(pinfo->regbase + IR_CONTROL_OFFSET) |
		IR_CONTROL_LEVINT));

	return IRQ_HANDLED;
}

void ambarella_ir_enable(struct ambarella_ir_info *pinfo)
{
	u32 edges;

	pinfo->frame_data_to_received = pinfo->frame_info.frame_head_size
		+ pinfo->frame_info.frame_data_size;

	BUG_ON(pinfo->frame_data_to_received > MAX_IR_BUFFER);//the max frame that we can process

	if (pinfo->frame_data_to_received > HW_FIFO_BUFFER) {
		edges = HW_FIFO_BUFFER;
		pinfo->frame_data_to_received -= HW_FIFO_BUFFER;
	} else {
		edges = pinfo->frame_data_to_received;
		pinfo->frame_data_to_received = 0;
	}
	amba_writel(pinfo->regbase + IR_CONTROL_OFFSET, IR_CONTROL_RESET);
	amba_setbitsl(pinfo->regbase + IR_CONTROL_OFFSET,
		IR_CONTROL_ENB | IR_CONTROL_INTLEV(edges) | IR_CONTROL_INTENB);

	enable_irq(pinfo->irq);
}

void ambarella_ir_disable(struct ambarella_ir_info *pinfo)
{
	disable_irq(pinfo->irq);
}

void ambarella_ir_set_protocol(struct ambarella_ir_info *pinfo,
	enum ambarella_ir_protocol protocol_id)
{
	memset(pinfo->tick_buf, 0x0, sizeof(pinfo->tick_buf));
	pinfo->ir_pread  = 0;
	pinfo->ir_pwrite = 0;

	switch (protocol_id) {
	case AMBA_IR_PROTOCOL_NEC:
		dev_notice(&pinfo->input_center->dev->dev, "Protocol NEC[%d]\n", protocol_id);
		pinfo->ir_parse = ambarella_ir_nec_parse;
		ambarella_ir_get_nec_info(&pinfo->frame_info);
		break;
	case AMBA_IR_PROTOCOL_PANASONIC:
		dev_notice(&pinfo->input_center->dev->dev, "Protocol PANASONIC[%d]\n", protocol_id);
		pinfo->ir_parse = ambarella_ir_panasonic_parse;
		ambarella_ir_get_panasonic_info(&pinfo->frame_info);
		break;
	case AMBA_IR_PROTOCOL_SONY:
		dev_notice(&pinfo->input_center->dev->dev, "Protocol SONY[%d]\n", protocol_id);
		pinfo->ir_parse = ambarella_ir_sony_parse;
		ambarella_ir_get_sony_info(&pinfo->frame_info);
		break;
	case AMBA_IR_PROTOCOL_PHILIPS:
		dev_notice(&pinfo->input_center->dev->dev, "Protocol PHILIPS[%d]\n", protocol_id);
		pinfo->ir_parse = ambarella_ir_philips_parse;
		ambarella_ir_get_philips_info(&pinfo->frame_info);
		break;
	default:
		dev_notice(&pinfo->input_center->dev->dev, "Protocol default NEC[%d]\n", protocol_id);
		pinfo->ir_parse = ambarella_ir_nec_parse;
		ambarella_ir_get_nec_info(&pinfo->frame_info);
		break;
	}
}
void ambarella_ir_init(struct ambarella_ir_info *pinfo)
{
	ambarella_ir_disable(pinfo);

	pinfo->pcontroller_info->set_pll();

	ambarella_gpio_config(pinfo->gpio_id, GPIO_FUNC_HW);

	ambarella_ir_set_protocol(pinfo, ir_protocol);

	ambarella_ir_enable(pinfo);
}

static int __devinit ambarella_ir_probe(struct platform_device *pdev)
{
	int				errorCode;
	struct ambarella_ir_info	*pinfo;
	struct resource 		*irq;
	struct resource 		*mem;
	struct resource 		*io;
	struct resource 		*ioarea;

	pinfo = kzalloc(sizeof(struct ambarella_ir_info), GFP_KERNEL);
	if (!pinfo) {
		dev_err(&pdev->dev, "Failed to allocate pinfo!\n");
		errorCode = -ENOMEM;
		goto ir_errorCode_na;
	}

	pinfo->input_center = amba_input_dev;
	if (!pinfo->input_center){
		dev_err(&pdev->dev, "input_center not registered!\n");
		errorCode = -ENOMEM;
		goto ir_errorCode_pinfo;
	}

	pinfo->pcontroller_info =
		(struct ambarella_ir_controller *)pdev->dev.platform_data;
	if ((pinfo->pcontroller_info == NULL) ||
		(pinfo->pcontroller_info->set_pll == NULL) ||
		(pinfo->pcontroller_info->get_pll == NULL) ) {
		dev_err(&pdev->dev, "Platform data is NULL!\n");
		errorCode = -ENXIO;
		goto ir_errorCode_pinfo;
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (mem == NULL) {
		dev_err(&pdev->dev, "Get mem resource failed!\n");
		errorCode = -ENXIO;
		goto ir_errorCode_pinfo;
	}

	irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (irq == NULL) {
		dev_err(&pdev->dev, "Get irq resource failed!\n");
		errorCode = -ENXIO;
		goto ir_errorCode_pinfo;
	}

	io = platform_get_resource(pdev, IORESOURCE_IO, 0);
	if (io == NULL) {
		dev_err(&pdev->dev, "Get GPIO resource failed!\n");
		errorCode = -ENXIO;
		goto ir_errorCode_pinfo;
	}

	ioarea = request_mem_region(mem->start,
			(mem->end - mem->start) + 1, pdev->name);
	if (ioarea == NULL) {
		dev_err(&pdev->dev, "Request ioarea failed!\n");
		errorCode = -EBUSY;
		goto ir_errorCode_pinfo;
	}

	pinfo->regbase = (unsigned char __iomem *)mem->start;
	pinfo->id = pdev->id;
	pinfo->mem = mem;
	pinfo->irq = irq->start;
	pinfo->gpio_id = io->start;
	pinfo->last_ir_uid = 0;
	pinfo->last_ir_flag = 0;

	platform_set_drvdata(pdev, pinfo);

	errorCode = request_irq(pinfo->irq,
		ambarella_ir_irq, IRQF_TRIGGER_HIGH,
		dev_name(&pdev->dev), pinfo);
	if (errorCode) {
		dev_err(&pdev->dev, "Request IRQ failed!\n");
		goto ir_errorCode_free_platform;
	}

	ambarella_ir_init(pinfo);

	pinfo->input_center->pir_info = pinfo;
	pinfo->dev = pinfo->input_center->dev;
	pinfo->pkeymap = pinfo->input_center->pkeymap;

	dev_notice(&pdev->dev,
		"Ambarella Media Processor IR Host Controller %d probed!\n",
		pdev->id);

	goto ir_errorCode_na;

ir_errorCode_free_platform:
	platform_set_drvdata(pdev, NULL);
	release_mem_region(mem->start, (mem->end - mem->start) + 1);
ir_errorCode_pinfo:
	kfree(pinfo);

ir_errorCode_na:
	return errorCode;
}

static int __devexit ambarella_ir_remove(struct platform_device *pdev)
{
	struct ambarella_ir_info	*pinfo;
	int				errorCode = 0;

	pinfo = platform_get_drvdata(pdev);

	if (pinfo) {
		free_irq(pinfo->irq, pinfo);
		platform_set_drvdata(pdev, NULL);
		release_mem_region(pinfo->mem->start,
			(pinfo->mem->end - pinfo->mem->start) + 1);
		kfree(pinfo);
	}

	dev_notice(&pdev->dev,
		"Remove Ambarella Media Processor IR Host Controller.\n");

	return errorCode;
}

#if (defined CONFIG_PM)
static int ambarella_ir_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	int					errorCode = 0;
	struct ambarella_ir_info		*pinfo;

	pinfo = platform_get_drvdata(pdev);

	if (!device_may_wakeup(&pdev->dev)) {
		amba_clrbitsl(pinfo->regbase + IR_CONTROL_OFFSET,
			IR_CONTROL_INTENB);
		disable_irq(pinfo->irq);
	}

	dev_dbg(&pdev->dev, "%s exit with %d @ %d\n",
		__func__, errorCode, state.event);
	return errorCode;
}

static int ambarella_ir_resume(struct platform_device *pdev)
{
	int					errorCode = 0;
	struct ambarella_ir_info		*pinfo;

	pinfo = platform_get_drvdata(pdev);

	if (!device_may_wakeup(&pdev->dev)) {
		amba_setbitsl(pinfo->regbase + IR_CONTROL_OFFSET,
			IR_CONTROL_INTENB);
		ambarella_ir_enable(pinfo);
	}

	dev_dbg(&pdev->dev, "%s exit with %d\n", __func__, errorCode);

	return errorCode;
}
#endif

static struct platform_driver ambarella_ir_driver = {
	.probe		= ambarella_ir_probe,
	.remove		= __devexit_p(ambarella_ir_remove),
#if (defined CONFIG_PM)
	.suspend	= ambarella_ir_suspend,
	.resume		= ambarella_ir_resume,
#endif
	.driver		= {
		.name	= "ambarella-ir",
		.owner	= THIS_MODULE,
	},
};

int platform_driver_register_ir(void)
{
	return platform_driver_register(&ambarella_ir_driver);
}

void platform_driver_unregister_ir(void)
{
	platform_driver_unregister(&ambarella_ir_driver);
}

static int ambarella_input_set_ir_protocol(const char *val,
	struct kernel_param *kp)
{
	int				errorCode = 0;
	struct ambarella_input_info	*input_center;

	input_center = amba_input_dev;
	if (!input_center){
		printk(KERN_ERR "input_center not registered!\n");
		return-ENOMEM;
	}

	errorCode = param_set_int(val, kp);

	if (input_center->pir_info)
		ambarella_ir_init(input_center->pir_info);

	return errorCode;
}

module_param_call(ir_protocol, ambarella_input_set_ir_protocol,
	param_get_int, &ir_protocol, 0644);
module_param(print_ir_keycode, int, 0644);

