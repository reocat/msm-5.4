/*
 * drivers/input/misc/ambarella_input_adc.c
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
#include <linux/input.h>

#include <asm/irq.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/mach-types.h>

#include <mach/hardware.h>
#include "ambarella_input.h"

/* ========================================================================= */
#define CONFIG_AMBARELLA_ADC_WAIT_COUNTER_LIMIT	(100000)

/* ========================================================================= */
static int adc_scan_delay = 20;
MODULE_PARM_DESC(adc_scan_delay, "Ambarella ADC scan (jiffies)");

int ambarella_setup_adc_key(struct ambarella_adc_info *pinfo)
{
	int				errorCode = 0;
	int				i;

	pinfo->adc_channel_num = pinfo->pcontroller_info->get_channel_num();
	ambi_dbg("adc has %d channels\n", pinfo->adc_channel_num);
	if (pinfo->adc_channel_num == 0) {
		dev_err(&pinfo->dev->dev, "Wrong adc_channel_num\n");
		errorCode = -EINVAL;
		goto ambarella_setup_adc_key_exit;
	}

	pinfo->adc_channel_info = kzalloc(sizeof(struct ambarella_adc_channel_info) *
		pinfo->adc_channel_num, GFP_KERNEL);
	if (!pinfo->adc_channel_info) {
		dev_err(&pinfo->dev->dev,
			"Fail to malloc adc_channel_info info\n");
		errorCode = -ENOMEM;
		goto ambarella_setup_adc_key_exit;
	}

	pinfo->adc_key_pressed = kzalloc(sizeof(u32) * pinfo->adc_channel_num, GFP_KERNEL);
	if (!pinfo->adc_key_pressed) {
		dev_err(&pinfo->dev->dev, "Failed to allocate adc_key_pressed!\n");
		errorCode = -ENOMEM;
		goto ambarella_setup_adc_key_free_adc_channel_info;
	}

	pinfo->adc_data = kzalloc(sizeof(u32) * pinfo->adc_channel_num, GFP_KERNEL);
	if (!pinfo->adc_data) {
		dev_err(&pinfo->dev->dev, "Failed to allocate adc_data!\n");
		errorCode = -ENOMEM;
		goto ambarella_setup_adc_key_free_adc_key_pressed;
	}

	for (i = 0; i < CONFIG_AMBARELLA_INPUT_SIZE; i++) {
		if (pinfo->pkeymap[i].type == AMBA_INPUT_END)
			break;

		if ((pinfo->pkeymap[i].type & AMBA_INPUT_SOURCE_MASK) != AMBA_INPUT_SOURCE_ADC)
			continue;

		if (pinfo->pkeymap[i].adc_key.chan < pinfo->adc_channel_num) {
			pinfo->adc_channel_info[pinfo->pkeymap[i].adc_key.chan].adc_channel_used = 1;
		} else {
			dev_err(&pinfo->dev->dev, "The adc adc_channel_info [%d] is not exist\n",
				pinfo->pkeymap[i].adc_key.chan);
			continue;
		}

		if (pinfo->pkeymap[i].adc_key.key_code == KEY_RESERVED) {
			if (pinfo->pkeymap[i].adc_key.irq_trig) {
				pinfo->adc_channel_info[pinfo->pkeymap[i].adc_key.chan].adc_high_trig
					= pinfo->pkeymap[i].adc_key.low_level;
				pinfo->adc_channel_info[pinfo->pkeymap[i].adc_key.chan].adc_low_trig
					= 0;
			} else {
				pinfo->adc_channel_info[pinfo->pkeymap[i].adc_key.chan].adc_low_trig
					= pinfo->pkeymap[i].adc_key.high_level;
				pinfo->adc_channel_info[pinfo->pkeymap[i].adc_key.chan].adc_high_trig
					= 0;
			}
		}
	}

	if (pinfo->support_irq) {
		for (i = 0; i < pinfo->adc_channel_num; i++) {
			if (pinfo->adc_channel_info[i].adc_channel_used) {
				pinfo->pcontroller_info->set_irq_threshold(i,
					pinfo->adc_channel_info[i].adc_high_trig,
					pinfo->adc_channel_info[i].adc_low_trig);
				ambi_dbg(" adc adc_channel_info %d is used\n", i);
				ambi_dbg(" adc_high_trig [%d], adc_low_trig [%d]\n",
					pinfo->adc_channel_info[i].adc_high_trig,
					pinfo->adc_channel_info[i].adc_low_trig);

				/* here, we suppose that all the adc_channel_info use the same trigger */
				pinfo->irqflags = (!!pinfo->adc_channel_info[i].adc_high_trig) * IRQF_TRIGGER_HIGH
					+ (!!pinfo->adc_channel_info[i].adc_low_trig) * IRQF_TRIGGER_LOW;
			}
		}
	}

	goto ambarella_setup_adc_key_exit;

ambarella_setup_adc_key_free_adc_key_pressed:
	kfree(pinfo->adc_key_pressed);
	pinfo->adc_key_pressed = NULL;

ambarella_setup_adc_key_free_adc_channel_info:
	kfree(pinfo->adc_channel_info);
	pinfo->adc_channel_info = NULL;

ambarella_setup_adc_key_exit:
	return errorCode;
}

void ambarella_scan_adc(struct work_struct *work)
{
	int				i;
	struct ambarella_adc_info	*pinfo;
	int				adc_index;
	u32				adc_size;

	pinfo = container_of(work, struct ambarella_adc_info, detect_adc.work);

	if (unlikely(pinfo->pkeymap == NULL)) {
		ambi_dbg("pinfo->pkeymap is null");
		return;
	}

	adc_size = pinfo->adc_channel_num;
	pinfo->pcontroller_info->read_channels(pinfo->adc_data, &adc_size);

	for (i = 0; i < CONFIG_AMBARELLA_INPUT_SIZE; i++) {
		if (pinfo->pkeymap[i].type == AMBA_INPUT_END)
			break;

		if ((pinfo->pkeymap[i].type & AMBA_INPUT_SOURCE_MASK) != AMBA_INPUT_SOURCE_ADC)
			continue;

		adc_index = pinfo->pkeymap[i].adc_key.chan;
		if (adc_index >= adc_size)
			continue;

		if ((pinfo->pkeymap[i].adc_key.low_level > pinfo->adc_data[adc_index]) ||
			(pinfo->pkeymap[i].adc_key.high_level < pinfo->adc_data[adc_index]))
			continue;

		/* scan the key and stop when the first key action is detected. If you want to
		   monitor multi-key in different ADC channels, remove "break" */
		if (pinfo->pkeymap[i].type == AMBA_INPUT_ADC_KEY) {
			if (pinfo->adc_key_pressed[adc_index] != AMBA_ADC_NO_KEY_PRESSED
				&& pinfo->pkeymap[i].adc_key.key_code == KEY_RESERVED) {
				input_report_key(pinfo->dev,
					pinfo->adc_key_pressed[adc_index], 0);//key relase
				ambi_dbg("key[%d:%d] released %d\n", adc_index,
					pinfo->adc_key_pressed[adc_index],
					pinfo->adc_data[adc_index]);
				pinfo->adc_key_pressed[adc_index] = AMBA_ADC_NO_KEY_PRESSED;
				if (pinfo->support_irq) {
					pinfo->work_mode = AMBA_ADC_IRQ_MODE;
				}
				break;
			} else if (pinfo->adc_key_pressed[adc_index] == AMBA_ADC_NO_KEY_PRESSED
				&& pinfo->pkeymap[i].adc_key.key_code != KEY_RESERVED) {
				input_report_key(pinfo->dev,
					pinfo->pkeymap[i].adc_key.key_code, 1);// key press
				pinfo->adc_key_pressed[adc_index] =
					pinfo->pkeymap[i].adc_key.key_code;
				if (pinfo->support_irq) {
					pinfo->work_mode = AMBA_ADC_POL_MODE;
				}
				ambi_dbg("key[%d:%d] pressed %d\n", adc_index,
					pinfo->adc_key_pressed[adc_index],
					pinfo->adc_data[adc_index]);
				break;
			}
		}

		if (pinfo->pkeymap[i].type == AMBA_INPUT_ADC_REL) {
			if (pinfo->pkeymap[i].adc_rel.key_code == REL_X) {
				input_report_rel(pinfo->dev,
					REL_X,
					pinfo->pkeymap[i].adc_rel.rel_step);
				input_report_rel(pinfo->dev,
					REL_Y, 0);
				input_sync(pinfo->dev);
				ambi_dbg("report REL_X %d & %d\n",
					pinfo->pkeymap[i].adc_rel.rel_step,
					pinfo->adc_data[adc_index]);
				break;
			} else if (pinfo->pkeymap[i].adc_rel.key_code == REL_Y) {
				input_report_rel(pinfo->dev,
					REL_X, 0);
				input_report_rel(pinfo->dev,
					REL_Y,
					pinfo->pkeymap[i].adc_rel.rel_step);
				input_sync(pinfo->dev);
				ambi_dbg("report REL_Y %d & %d\n",
					pinfo->pkeymap[i].adc_rel.rel_step,
					pinfo->adc_data[adc_index]);
				break;
			}
		}

		if (pinfo->pkeymap[i].type == AMBA_INPUT_ADC_REL) {
			input_report_abs(pinfo->dev,
				ABS_X, pinfo->pkeymap[i].adc_abs.abs_x);
			input_report_abs(pinfo->dev,
				ABS_Y, pinfo->pkeymap[i].adc_abs.abs_y);
			input_sync(pinfo->dev);
			ambi_dbg("report ABS %d:%d & %d\n",
				pinfo->pkeymap[i].adc_abs.abs_x,
				pinfo->pkeymap[i].adc_abs.abs_y,
				pinfo->adc_data[adc_index]);
			break;
		}
	}

	if (pinfo->work_mode == AMBA_ADC_POL_MODE)
		queue_delayed_work(pinfo->workqueue, &pinfo->detect_adc, adc_scan_delay);
	else
		enable_irq(pinfo->irq);
}

static irqreturn_t ambarella_adc_irq(int irq, void *devid)
{
	struct ambarella_adc_info	*pinfo;

	pinfo = (struct ambarella_adc_info *)devid;

	ambi_dbg("%s:%d\n", __func__, __LINE__);

	disable_irq(pinfo->irq);
	queue_delayed_work(pinfo->workqueue, &pinfo->detect_adc, 0);

	return IRQ_HANDLED;
}

static int __devinit ambarella_adc_probe(struct platform_device *pdev)
{
	int				errorCode = 0;
	struct ambarella_adc_info	*pinfo;
	int				irq;
	struct resource 		*mem;
	struct resource 		*ioarea;

	pinfo = kmalloc(sizeof(struct ambarella_adc_info), GFP_KERNEL);
	if (!pinfo) {
		dev_err(&pdev->dev, "Failed to allocate pinfo!\n");
		errorCode = -ENOMEM;
		goto adc_errorCode_na;
	}

	pinfo->input_center = amba_input_dev;
	if (pinfo->input_center == NULL) {
		dev_err(&pdev->dev, "input_center is NULL!\n");
		errorCode = -ENOMEM;
		goto adc_errorCode_pinfo;
	}

	pinfo->pcontroller_info =
		(struct ambarella_adc_controller *)pdev->dev.platform_data;
	if ((pinfo->pcontroller_info == NULL) ||
		(pinfo->pcontroller_info->read_channels == NULL) ||
		(pinfo->pcontroller_info->is_irq_supported == NULL) ||
		(pinfo->pcontroller_info->set_irq_threshold == NULL) ||
		(pinfo->pcontroller_info->reset == NULL) ||
		(pinfo->pcontroller_info->stop == NULL) ||
		(pinfo->pcontroller_info->get_channel_num == NULL) ) {
		dev_err(&pdev->dev, "Platform data is NULL!\n");
		errorCode = -ENXIO;
		goto adc_errorCode_pinfo;
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (mem == NULL) {
		dev_err(&pdev->dev, "Get mem resource failed!\n");
		errorCode = -ENXIO;
		goto adc_errorCode_pinfo;
	}
	ioarea = request_mem_region(mem->start,
			(mem->end - mem->start) + 1, pdev->name);
	if (ioarea == NULL) {
		dev_err(&pdev->dev, "Request ioarea failed!\n");
		errorCode = -EBUSY;
		goto adc_errorCode_pinfo;
	}

	pinfo->support_irq = pinfo->pcontroller_info->is_irq_supported();
	if (pinfo->support_irq) {
		irq = platform_get_irq_byname(pdev, "adc-level-irq");
		if (irq == -ENXIO) {
			dev_err(&pdev->dev, "Get irq resource failed!\n");
			errorCode = -ENXIO;
			goto adc_errorCode_mem;
		} else {
			pinfo->irq = irq;
		}
	}

	pinfo->regbase = (unsigned char __iomem *)mem->start;
	pinfo->id = pdev->id;
	pinfo->mem = mem;
	pinfo->input_center->padc_info = pinfo;
	pinfo->dev = pinfo->input_center->dev;
	pinfo->pkeymap = pinfo->input_center->pkeymap;

	pinfo->pcontroller_info->reset();

	errorCode = ambarella_setup_adc_key(pinfo);
	if (errorCode) {
		dev_err(&pdev->dev, "ambarella_setup_adc_key failed!\n");
		goto adc_errorCode_mem;
	}

	pinfo->workqueue = create_singlethread_workqueue("AMBA ADC");
	if (!pinfo->workqueue) {
		errorCode = -ENOMEM;
		dev_err(&pdev->dev, "Create ADC workqueue failed!\n");
		goto adc_errorCode_setup_adc_key;
	}
	INIT_DELAYED_WORK(&pinfo->detect_adc, ambarella_scan_adc);

	if (pinfo->support_irq) {// irq mode
		pinfo->work_mode = AMBA_ADC_IRQ_MODE;
		errorCode = request_irq(pinfo->irq, ambarella_adc_irq,
			pinfo->irqflags, pdev->name, pinfo);
		if (errorCode) {
			dev_err(&pdev->dev, "Request IRQ failed!\n");
			goto adc_errorCode_free_queue;
		}
	} else {//polling mode
		pinfo->work_mode = AMBA_ADC_POL_MODE;
		queue_delayed_work(pinfo->workqueue, &pinfo->detect_adc, adc_scan_delay);
	}

	platform_set_drvdata(pdev, pinfo);
	dev_notice(&pdev->dev, "Ambarella Media Processor ADC Host Controller"
		" %d probed %s mode\n", pdev->id,
		(pinfo->support_irq) ? "interrupt" : "polling");

	goto adc_errorCode_na;

adc_errorCode_free_queue:
	destroy_workqueue(pinfo->workqueue);
adc_errorCode_setup_adc_key:
	kfree(pinfo->adc_key_pressed);
	pinfo->adc_key_pressed = NULL;
	kfree(pinfo->adc_channel_info);
	pinfo->adc_channel_info = NULL;
adc_errorCode_mem:
	release_mem_region(pinfo->mem->start, (pinfo->mem->end - pinfo->mem->start) + 1);
adc_errorCode_pinfo:
	kfree(pinfo);
adc_errorCode_na:
	return errorCode;
}

static int __devexit ambarella_adc_remove(struct platform_device *pdev)
{
	struct ambarella_adc_info	*pinfo;
	int				errorCode = 0;

	pinfo = platform_get_drvdata(pdev);

	if (pinfo) {
		if (pinfo->support_irq)
			free_irq(pinfo->irq, pinfo);
		platform_set_drvdata(pdev, NULL);
		cancel_delayed_work(&pinfo->detect_adc);
		flush_workqueue(pinfo->workqueue);
		destroy_workqueue(pinfo->workqueue);
		release_mem_region(pinfo->mem->start, (pinfo->mem->end - pinfo->mem->start) + 1);
		kfree(pinfo->adc_data);
		kfree(pinfo->adc_key_pressed);
		kfree(pinfo->adc_channel_info);
		kfree(pinfo);
	}

	dev_notice(&pdev->dev, "Remove Ambarella Media Processor ADC Host Controller.\n");

	return errorCode;
}

#if (defined CONFIG_PM)
static int ambarella_adc_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	int					errorCode = 0;
	struct ambarella_adc_info		*pinfo;

	pinfo = platform_get_drvdata(pdev);

	if (!device_may_wakeup(&pdev->dev)) {
		pinfo->pcontroller_info->stop();
		if (pinfo->support_irq)
			disable_irq(pinfo->irq);
	}

	dev_info(&pdev->dev, "%s exit with %d @ %d\n",
		__func__, errorCode, state.event);
	return errorCode;
}

static int ambarella_adc_resume(struct platform_device *pdev)
{
	int					errorCode = 0;
	struct ambarella_adc_info		*pinfo;

	pinfo = platform_get_drvdata(pdev);

	if (!device_may_wakeup(&pdev->dev)) {
		if (pinfo->support_irq)
			enable_irq(pinfo->irq);
	}

	pinfo->pcontroller_info->reset();
	dev_info(&pdev->dev, "%s exit with %d\n", __func__, errorCode);

	return errorCode;
}
#endif

static struct platform_driver ambarella_adc_driver = {
	.probe		= ambarella_adc_probe,
	.remove		= __devexit_p(ambarella_adc_remove),
#ifdef CONFIG_PM
	.suspend	= ambarella_adc_suspend,
	.resume		= ambarella_adc_resume,
#endif
	.driver		= {
		.name	= "ambarella-adc",
		.owner	= THIS_MODULE,
	},
};

int platform_driver_register_adc(void)
{
	return platform_driver_register(&ambarella_adc_driver);
}
void platform_driver_unregister_adc(void)
{
	platform_driver_unregister(&ambarella_adc_driver);
}

module_param(adc_scan_delay, int, 0644);

