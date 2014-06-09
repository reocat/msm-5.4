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
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <mach/hardware.h>
#include <plat/adc.h>
#include <plat/rct.h>

struct ambarella_adc_keymap {
	u32 key_code;
	u32 channel : 4;
	u32 low_level : 12;
	u32 high_level : 12;
};

struct ambarella_adc_info {
	void __iomem *regbase;
	unsigned int irq;

	struct input_dev *input;
	struct delayed_work work;

	u32 old_key[ADC_NUM_CHANNELS];
	struct ambarella_adc_keymap *keymap;
	u32 key_num;

	/* for pm */
	u32 control_reg;
	u32 enable_reg;
	u32 ch_intr_reg[ADC_NUM_CHANNELS];
};

static void ambarella_adc_read_level(struct ambarella_adc_info *pinfo, u32 *data)
{
	int i;

	amba_setbitsl(ADC_CONTROL_REG, ADC_CONTROL_START);

	while (amba_tstbitsl(ADC_STATUS_REG, ADC_CONTROL_STATUS) == 0x0);

	/* note: only available for s2/s2l, because the register definition
	 * for a5s/iONE are different. */
	for (i = 0; i < ADC_NUM_CHANNELS; i++)
		data[i] = amba_readl(pinfo->regbase + ADC_DATA0_OFFSET + i * 4);
}

static void ambarella_scan_adc_key(struct work_struct *work)
{
	struct ambarella_adc_info *pinfo;
	struct input_dev *input;
	struct ambarella_adc_keymap *keymap;
	u32 i, ch, low_level, high_level;
	u32 level[ADC_NUM_CHANNELS];

	pinfo = container_of(work, struct ambarella_adc_info, work.work);
	keymap = pinfo->keymap;
	input = pinfo->input;

	ambarella_adc_read_level(pinfo, level);

	for (i = 0; i < pinfo->key_num; i++, keymap++) {
		low_level = keymap->low_level;
		high_level = keymap->high_level;
		ch = keymap->channel;

		if (level[ch] < low_level || level[ch] > high_level)
			continue;

		if (pinfo->old_key[ch] == KEY_RESERVED
				&& keymap->key_code != KEY_RESERVED) {
			input_report_key(input, keymap->key_code, 1);
			input_sync(input);
			pinfo->old_key[ch] = keymap->key_code;

			dev_dbg(&input->dev, "key[%d:%d] pressed %d\n",
					ch, pinfo->old_key[ch], level[ch]);
			break;
		} else if (pinfo->old_key[ch] != KEY_RESERVED
				&& keymap->key_code == KEY_RESERVED) {
			input_report_key(input, pinfo->old_key[ch], 0);
			input_sync(input);

			dev_dbg(&input->dev, "key[%d:%d] released %d\n",
					ch, pinfo->old_key[ch],level[ch]);

			pinfo->old_key[ch] = 0;
			break;
		}
	}

	queue_delayed_work(system_freezable_wq,
				&pinfo->work, msecs_to_jiffies(20));
}

static int ambarella_adc_of_parse(struct platform_device *pdev,
					struct ambarella_adc_info *pinfo)
{
	struct device_node *np = pdev->dev.of_node;
	struct ambarella_adc_keymap *keymap;
	const __be32 *prop;
	u32 propval, i, size;

	prop = of_get_property(np, "amb,keymap", &size);
	if (!prop || size % (sizeof(__be32) * 2)) {
		dev_err(&pdev->dev, "Invalid keymap!\n");
		return -ENOENT;
	}

	/* cells is 2 for each keymap */
	size /= sizeof(__be32) * 2;

	pinfo->key_num = size;
	pinfo->keymap = devm_kzalloc(&pdev->dev,
			sizeof(struct ambarella_adc_keymap) * size, GFP_KERNEL);
	if (pinfo->keymap == NULL){
		dev_err(&pdev->dev, "No memory!\n");
		return -ENOMEM;
	}

	keymap = pinfo->keymap;

	for (i = 0; i < size; i++) {
		propval = be32_to_cpup(prop + i * 2);
		keymap->low_level = propval & 0xfff;
		keymap->high_level = (propval >> 16) & 0xfff;
		keymap->channel = propval >> 28;
		if (keymap->channel >= ADC_NUM_CHANNELS) {
			dev_err(&pdev->dev, "Invalid channel: %d\n",
						keymap->channel);
			return -EINVAL;
		}

		propval = be32_to_cpup(prop + i * 2 + 1);
		keymap->key_code = propval;

		input_set_capability(pinfo->input, EV_KEY, keymap->key_code);

		keymap++;
	}

	return 0;
}

static ssize_t adc_show(struct device *dev, struct device_attribute *attr,
                        char *buf)
{
        ssize_t ret = 0;
        int i;
        int total_channel = ADC_NUM_CHANNELS;
        u32   *adc_data;
        struct ambarella_adc_info	*pinfo;

        pinfo = dev_get_drvdata(dev);

        adc_data = kzalloc(sizeof(u32) * total_channel, GFP_KERNEL);

        ambarella_adc_read_level(pinfo, adc_data);

        for(i = 0; i < total_channel; i++ ) {
                ret += sprintf(&buf[ret], "adc%d=0x%x\n", i, adc_data[i]);
        }
        return ret;
}
static DEVICE_ATTR(adcsys, 0444, adc_show, NULL);

static int ambarella_input_adc_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct input_dev *input;
	struct ambarella_adc_info *pinfo;
	struct resource *mem;
	int i, rval = 0;

	if (of_device_is_compatible(np, "ambarella,adc-old")) {
		dev_err(&pdev->dev, "Not implemented yet for adc-old(a5s)!\n");
		return -ENODEV;
	}

	pinfo = devm_kzalloc(&pdev->dev,
			sizeof(struct ambarella_adc_info), GFP_KERNEL);
	if (!pinfo) {
		dev_err(&pdev->dev, "Failed to allocate pinfo!\n");
		return -ENOMEM;
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (mem == NULL) {
		dev_err(&pdev->dev, "Get mem resource failed!\n");
		return -ENXIO;
	}

	pinfo->regbase = devm_ioremap(&pdev->dev, mem->start, resource_size(mem));
	if (!pinfo->regbase) {
		dev_err(&pdev->dev, "devm_ioremap() failed\n");
		return -ENOMEM;
	}

	/* currently irq mode is not supported */
	pinfo->irq = platform_get_irq(pdev, 0);
	if (pinfo->irq < 0) {
		dev_err(&pdev->dev, "Get irq resource failed!\n");
		return -ENXIO;
	}

	input = input_allocate_device();
	if (!input) {
		dev_err(&pdev->dev, "input_allocate_device fail!\n");
		return -ENOMEM;
	}

	input->name = "AmbADC";
	input->phys = "ambadc/input0";
	input->id.bustype = BUS_HOST;

	rval = input_register_device(input);
	if (rval) {
		dev_err(&pdev->dev, "Register input_dev failed!\n");
		goto adc_err0;
	}

	pinfo->input = input;

	for (i = 0; i < ADC_NUM_CHANNELS; i++)
		pinfo->old_key[i] = KEY_RESERVED;

	rval = ambarella_adc_of_parse(pdev, pinfo);
	if (rval < 0)
		goto adc_err1;

	amba_writel(SCALER_ADC_REG, 0x00010002);
	amba_writel(SCALER_ADC_REG, 0x00000002);

	amba_setbitsl(ADC_CONTROL_REG, ADC_CONTROL_RESET);
	amba_clrbitsl(ADC_CONTROL_REG, ADC_CONTROL_MODE); /* single mode */
	amba_setbitsl(ADC_CONTROL_REG, ADC_CONTROL_ENABLE);
	msleep(1);

	amba_writel(ADC_SLOT_NUM_REG, 0);
	amba_writel(ADC_SLOT_PERIOD_REG, 60);
	amba_writel(ADC_SLOT_CTRL_0_REG, 0xfff);

	INIT_DELAYED_WORK(&pinfo->work, ambarella_scan_adc_key);
	queue_delayed_work(system_freezable_wq,
				&pinfo->work, msecs_to_jiffies(20));

        rval = device_create_file(&pdev->dev, &dev_attr_adcsys);
        if (rval != 0) {
            dev_err(&pdev->dev, "can not create file : %d\n", rval);
            return rval;
        }

	platform_set_drvdata(pdev, pinfo);
	dev_notice(&pdev->dev, "ADC Host Controller [Polling] probed!\n");

	return 0;

adc_err1:
	input_unregister_device(input);
adc_err0:
	input_free_device(input);
	return rval;
}

static int ambarella_input_adc_remove(struct platform_device *pdev)
{
	struct ambarella_adc_info	*pinfo;
	int rval = 0;

	pinfo = platform_get_drvdata(pdev);

	amba_clrbitsl(ADC_CONTROL_REG, ADC_CONTROL_ENABLE);
	cancel_delayed_work_sync(&pinfo->work);

        device_remove_file(&pdev->dev, &dev_attr_adcsys);
	input_unregister_device(pinfo->input);
	input_free_device(pinfo->input);

	dev_notice(&pdev->dev, "Remove Ambarella Media Processor ADC Host Controller.\n");

	return rval;
}

#if (defined CONFIG_PM)
static int ambarella_input_adc_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	struct ambarella_adc_info *pinfo;
	void __iomem *reg;
	int i, rval = 0;

	pinfo = platform_get_drvdata(pdev);
	reg = pinfo->regbase;

	pinfo->control_reg = amba_readl(reg + ADC_CONTROL_OFFSET);
	pinfo->enable_reg = amba_readl(reg + ADC_ENABLE_OFFSET);

	for (i = 0; i < ADC_NUM_CHANNELS; i++) {
		pinfo->ch_intr_reg[i] =
			amba_readl(reg + ADC_CHAN0_INTR_OFFSET + i * 4);
	}

	dev_dbg(&pdev->dev, "%s exit with %d @ %d\n",
		__func__, rval, state.event);

	return rval;
}

static int ambarella_input_adc_resume(struct platform_device *pdev)
{
	struct ambarella_adc_info *pinfo;
	void __iomem *reg;
	int i, rval = 0;

	pinfo = platform_get_drvdata(pdev);
	reg = pinfo->regbase;

	for (i = 0; i < ADC_NUM_CHANNELS; i++) {
		amba_writel(reg + ADC_CHAN0_INTR_OFFSET + i * 4,
				pinfo->ch_intr_reg[i]);
	}

	amba_writel(reg + ADC_ENABLE_OFFSET, pinfo->enable_reg);
	amba_writel(reg + ADC_CONTROL_OFFSET, pinfo->control_reg);

	dev_dbg(&pdev->dev, "%s exit with %d\n", __func__, rval);

	return rval;
}
#endif

static const struct of_device_id ambarella_adc_dt_ids[] = {
	{.compatible = "ambarella,adc", },
	{.compatible = "ambarella,adc-old", },
	{},
};
MODULE_DEVICE_TABLE(of, ambarella_adc_dt_ids);

static struct platform_driver ambarella_adc_driver = {
	.probe		= ambarella_input_adc_probe,
	.remove		= ambarella_input_adc_remove,
#ifdef CONFIG_PM
	.suspend	= ambarella_input_adc_suspend,
	.resume		= ambarella_input_adc_resume,
#endif
	.driver		= {
		.name	= "ambarella-adc",
		.owner	= THIS_MODULE,
		.of_match_table = ambarella_adc_dt_ids,
	},
};

module_platform_driver(ambarella_adc_driver);

MODULE_AUTHOR("Cao Rongrong <rrcao@ambarella.com>");
MODULE_DESCRIPTION("Ambarella ADC Input Driver");
MODULE_LICENSE("GPL");

