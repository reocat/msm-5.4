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

/* ========================================================================= */
#define CONFIG_AMBARELLA_ADC_WAIT_COUNTER_LIMIT	(100000)

/* ========================================================================= */
static int adc_scan_delay = 20;
MODULE_PARM_DESC(adc_scan_delay, "Ambarella ADC scan (jiffies)");

/*
 * scan all the adc channel, and report the key to IR event channel
 */
void ambarella_scan_adc(struct work_struct *work)
{
	int				i;
	struct ambarella_adc_info	*pinfo;
	int				adc_index;
	u32				adc_data[ADC_MAX_INSTANCES];
	u32				counter = 0;
	u32				adc_size = ADC_MAX_INSTANCES;

	pinfo = container_of(work, struct ambarella_adc_info, detect_adc.work);


	if (!pinfo->pkeymap ||!local_info)
		return;

	memset(adc_data, 0x00, sizeof(adc_data));
	while ((__raw_readl(ADC_CONTROL_REG) & ADC_CONTROL_STATUS) == 0) {
		counter++;
		if (counter > CONFIG_AMBARELLA_ADC_WAIT_COUNTER_LIMIT) {
			printk("ambarella_scan_adc ADC_CONTROL_STATUS Timeout!\n");
			goto ambarella_scan_adc_exit;
		}
	};

	pinfo->pcontroller_info->read_channels(adc_data, &adc_size);

	ambi_dbg("adc0 = 0x%03x\t""adc1 = 0x%03x\t""adc2 = 0x%03x\t""adc3 = 0x%03x\n",
			adc_data[0], adc_data[1], adc_data[2], adc_data[3]);
#if (ADC_MAX_INSTANCES == 8)
	ambi_dbg("adc4 = 0x%03x\t""adc5 = 0x%03x\t""adc6 = 0x%03x\t""adc7 = 0x%03x\n",
			adc_data[4], adc_data[5], adc_data[6], adc_data[7]);
#endif

	for (i = 0; i < CONFIG_AMBARELLA_INPUT_SIZE; i++) {
		if (pinfo->pkeymap[i].type == AMBA_INPUT_END)
			break;

		if ((pinfo->pkeymap[i].type & AMBA_INPUT_SOURCE_MASK) !=
			AMBA_INPUT_SOURCE_ADC)
			continue;

		adc_index = pinfo->pkeymap[i].adc_key.chan;
		if (adc_index >= ADC_MAX_INSTANCES)
			break;

		if ((pinfo->pkeymap[i].adc_key.low_level >
			adc_data[adc_index]) ||
			(pinfo->pkeymap[i].adc_key.high_level <
			adc_data[adc_index]))
			continue;

		/* scan the key and stop when the first key action is detected. If you want to
		   monitor multi-key in different ADC channels, remove "break" */
		if (pinfo->pkeymap[i].type == AMBA_INPUT_ADC_KEY) {
			if (pinfo->adc_key_pressed[adc_index] != AMBA_ADC_NO_KEY_PRESSED
				&& pinfo->pkeymap[i].adc_key.key_code == KEY_RESERVED) {
				input_report_key(local_info->dev,
					pinfo->adc_key_pressed[adc_index], 0);//key relase
				ambi_dbg("key[%d] released\n",pinfo->adc_key_pressed[adc_index]);
				pinfo->adc_key_pressed[adc_index] = AMBA_ADC_NO_KEY_PRESSED;
				if (pinfo->support_irq) {
					enable_irq(pinfo->irq);
					pinfo->work_mode = AMBA_ADC_IRQ_MODE;
				}
				break;
			} else if (pinfo->adc_key_pressed[adc_index] == AMBA_ADC_NO_KEY_PRESSED
				&& pinfo->pkeymap[i].adc_key.key_code != KEY_RESERVED) {
				input_report_key(local_info->dev,
					pinfo->pkeymap[i].adc_key.key_code, 1);// key press
				pinfo->adc_key_pressed[adc_index] =
					pinfo->pkeymap[i].adc_key.key_code;
				ambi_dbg("key[%d] pressed\n",pinfo->adc_key_pressed[adc_index]);
				break;
			}
		}

		if (pinfo->pkeymap[i].type == AMBA_INPUT_ADC_REL) {
			if (pinfo->pkeymap[i].adc_rel.key_code == REL_X) {
				input_report_rel(local_info->dev,
					REL_X,
					pinfo->pkeymap[i].adc_rel.rel_step);
				input_report_rel(local_info->dev,
					REL_Y, 0);
				input_sync(local_info->dev);
				ambi_dbg("report REL_X %d & %d\n",
					pinfo->pkeymap[i].adc_rel.rel_step,
					adc_data[adc_index]);
				if (pinfo->support_irq) {
					enable_irq(pinfo->irq);
					pinfo->work_mode = AMBA_ADC_IRQ_MODE;
				}
				break;
			} else if (pinfo->pkeymap[i].adc_rel.key_code == REL_Y) {
				input_report_rel(local_info->dev,
					REL_X, 0);
				input_report_rel(local_info->dev,
					REL_Y,
					pinfo->pkeymap[i].adc_rel.rel_step);
				input_sync(local_info->dev);
				ambi_dbg("report REL_Y %d & %d\n",
					pinfo->pkeymap[i].adc_rel.rel_step,
					adc_data[adc_index]);
				if (pinfo->support_irq) {
					enable_irq(pinfo->irq);
					pinfo->work_mode = AMBA_ADC_IRQ_MODE;
				}
				break;
			}
		}

		if (pinfo->pkeymap[i].type == AMBA_INPUT_ADC_REL) {
			input_report_abs(local_info->dev,
				ABS_X, pinfo->pkeymap[i].adc_abs.abs_x);
			input_report_abs(local_info->dev,
				ABS_Y, pinfo->pkeymap[i].adc_abs.abs_y);
			input_sync(local_info->dev);
			ambi_dbg("report ABS %d:%d & %d\n",
				pinfo->pkeymap[i].adc_abs.abs_x,
				pinfo->pkeymap[i].adc_abs.abs_y,
				adc_data[adc_index]);
				if (pinfo->support_irq) {
					enable_irq(pinfo->irq);
					pinfo->work_mode = AMBA_ADC_IRQ_MODE;
				}
			break;
		}
	}

ambarella_scan_adc_exit:
	if (pinfo->work_mode == AMBA_ADC_POL_MODE)
		queue_delayed_work(pinfo->workqueue, &pinfo->detect_adc, adc_scan_delay);
}

static irqreturn_t ambarella_adc_irq(int irq, void *devid)
{
	struct ambarella_adc_info	*pinfo;

	pinfo = (struct ambarella_adc_info *)devid;

	disable_irq(pinfo->irq); /* turn off the irq, until the key is released */
	pinfo->work_mode = AMBA_ADC_POL_MODE;
	queue_delayed_work(pinfo->workqueue, &pinfo->detect_adc, 10);

	ambi_dbg("line%d, %s\n", __LINE__, __func__);

	return IRQ_HANDLED;
}

static int __devinit ambarella_adc_probe(struct platform_device *pdev)
{
	int				errorCode = 0;
	struct ambarella_adc_info	*pinfo;
	int				irq;
	struct resource 		*mem;
	struct resource 		*ioarea;
	int				i, flags = 0;

	pinfo = kmalloc(sizeof(struct ambarella_adc_info), GFP_KERNEL);
	if (!pinfo) {
		dev_err(&pdev->dev, "Failed to allocate pinfo!\n");
		errorCode = -ENOMEM;
		goto adc_errorCode_na;
	}

	pinfo->pcontroller_info =
		(struct ambarella_adc_controller *)pdev->dev.platform_data;
	if ((pinfo->pcontroller_info == NULL) ||
		(pinfo->pcontroller_info->read_channels == NULL) ||
		(pinfo->pcontroller_info->is_irq_supported == NULL) ||
		(pinfo->pcontroller_info->set_irq_threshold == NULL) ||
		(pinfo->pcontroller_info->reset == NULL) ) {
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
		for (i = 0; i < ADC_MAX_INSTANCES; i++) {
			if (local_info->adc_channel_used[i]) {
				pinfo->pcontroller_info->set_irq_threshold(i,
					local_info->adc_high_trig[i],
					local_info->adc_low_trig[i]);
				ambi_dbg(" adc channel %d is used\n", i);
				ambi_dbg(" adc_high_trig [%d], adc_low_trig [%d]\n",
					local_info->adc_high_trig[i], local_info->adc_low_trig[i]);
				/* here, we suppose that all the channel use the same trigger */
				flags = (!!local_info->adc_high_trig[i]) * IRQF_TRIGGER_HIGH
					+ (!!local_info->adc_low_trig[i]) * IRQF_TRIGGER_LOW;
			}
		}

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

	memset(pinfo->adc_key_pressed, 0, sizeof(pinfo->adc_key_pressed));

	platform_set_drvdata(pdev, pinfo);

	pinfo->pkeymap = default_ambarella_key_table;
	pinfo->pcontroller_info->reset();

	pinfo->workqueue = create_singlethread_workqueue("Amba adc monitor thread");
	if (!pinfo->workqueue) {
		errorCode = -ENOMEM;
		dev_err(&pdev->dev, "Setup key map failed!\n");
		goto adc_errorCode_free_dev;
	}
	INIT_DELAYED_WORK(&pinfo->detect_adc, ambarella_scan_adc);

	if (pinfo->support_irq) {// irq mode
		pinfo->work_mode = AMBA_ADC_IRQ_MODE;
		errorCode = request_irq(pinfo->irq,
			ambarella_adc_irq, flags, pdev->name, pinfo);

		if (errorCode) {
			dev_err(&pdev->dev, "Request IRQ failed!\n");
			goto adc_errorCode_free_queue;
		}
	} else {//polling mode
		pinfo->work_mode = AMBA_ADC_POL_MODE;
		queue_delayed_work(pinfo->workqueue, &pinfo->detect_adc, 100);
	}

	dev_notice(&pdev->dev,
		"Ambarella Media Processor ADC Host Controller %d probed",
		pdev->id);
	if (pinfo->support_irq)
		printk("(interrupt mode)!\n");
	else
		printk("(polling mode)!\n");

	goto adc_errorCode_na;

adc_errorCode_free_queue:
	destroy_workqueue(pinfo->workqueue);
adc_errorCode_free_dev:
	platform_set_drvdata(pdev, NULL);
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

	if (!device_may_wakeup(&pdev->dev) ) {
		ambarella_adc_stop();
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
module_param(adc_scan_delay, int, 0644);


