/*
 * drivers/input/misc/ambarella_input_adc.c
 *
 * Author: Qiao Wang <qwang@ambarella.com>
 * Copyright (C) 2004-2009, Ambarella, Inc.
 *
 *  @History        ::
 *      Date        Name             Comments
 *      07/16/2014  Bing-Liang Hu    irq or polling depend on adc irq support
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
#include <plat/event.h>

struct ambarella_input_adckey_info {
    struct input_dev *input;
    struct ambarella_adc_keymap *keymap;

    struct ambarella_adc_client *client;
    struct notifier_block   system_event;
    u32 key_num;
    u32 old_key[ADC_NUM_CHANNELS];
    struct delayed_work work;
};

static int ambarella_scan_adc_key_irq(struct notifier_block *nb,
                                      unsigned long val,
                                      void *data)
{
    u32 i, ch, low_level, high_level;
    u32 *level;
    struct input_dev *input;
    struct ambarella_input_adckey_info *pinfo;
    struct ambarella_adc_keymap *keymap;
    struct ambarella_adc_client *adc_client;

    if(val != AMBA_EVENT_ADC_INTERRUPT)
        return -1;

    pinfo = container_of(nb, struct ambarella_input_adckey_info, system_event);
    keymap = pinfo->keymap;
    input = pinfo->input;
    level = data;
    adc_client = pinfo->client;

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
            if(adc_client->set_irq_threshold != NULL) {
                adc_client->set_irq_threshold(keymap->channel,
                                              keymap->high_level,
                                              keymap->low_level);
            }
            dev_dbg(&input->dev, "key[%d:%d] pressed %d\n",
                    ch, pinfo->old_key[ch], level[ch]);
            break;
        } else if (pinfo->old_key[ch] != KEY_RESERVED
                   && keymap->key_code == KEY_RESERVED) {
            input_report_key(input, pinfo->old_key[ch], 0);
            input_sync(input);
            if(adc_client->set_irq_threshold != NULL) {
                adc_client->set_irq_threshold(keymap->channel,
                                              keymap->high_level,
                                              keymap->low_level);
            }
            dev_dbg(&input->dev, "key[%d:%d] released %d\n",
                    ch, pinfo->old_key[ch], level[ch]);

            pinfo->old_key[ch] = KEY_RESERVED;
            break;
        }
    }

    return 0;
}

static void ambarella_scan_adc_key_polling(struct work_struct *work)
{
    u32 i, ch, low_level, high_level;
    u32 level[ADC_NUM_CHANNELS];
    struct input_dev *input;
    struct ambarella_input_adckey_info *pinfo;
    struct ambarella_adc_keymap *keymap;
    struct ambarella_adc_client *adc_client;


    pinfo = container_of(work, struct ambarella_input_adckey_info, work.work);
    keymap = pinfo->keymap;
    input = pinfo->input;
    adc_client = pinfo->client;
    adc_client->readdata(level);

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
                    ch, pinfo->old_key[ch], level[ch]);

            pinfo->old_key[ch] = KEY_RESERVED;
            break;
        }
    }

    queue_delayed_work(system_freezable_wq,
                       &pinfo->work,
                       msecs_to_jiffies(20));
}


static int ambarella_adckeymap_parse(struct platform_device *pdev,
                                     const __be32 *prop,
                                     struct ambarella_input_adckey_info *pinfo)
{
    struct ambarella_adc_keymap *keymap;
    u32 propval, i, size = 0;

    /* cells is 2 for each keymap */
    size = pinfo->key_num;

    pinfo->keymap = devm_kzalloc(&pdev->dev,
                                 sizeof(struct ambarella_adc_keymap) * size,
                                 GFP_KERNEL);
    if (pinfo->keymap == NULL) {
        dev_err(&pdev->dev, "No memory for keymap!\n");
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

        keymap++;
    }

    return 0;
}

static int ambarella_input_key_setup(struct platform_device *pdev,
                                     struct ambarella_input_adckey_info *pinfo)
{
    u32 i, num;
    struct ambarella_adc_keymap *keymap;
    struct ambarella_adc_client *adc_client;

    adc_client = pinfo->client;
    num = pinfo->key_num;
    keymap = pinfo->keymap;
    for (i = 0; i < ADC_NUM_CHANNELS; i++)
        pinfo->old_key[i] = KEY_RESERVED;

    for (i = 0; i < num; i++) {
        input_set_capability(pinfo->input, EV_KEY, keymap->key_code);
        if((keymap->key_code == KEY_RESERVED) &&
           (adc_client->set_irq_threshold != NULL)) {
            adc_client->set_irq_threshold(keymap->channel,
                                          keymap->high_level,
                                          keymap->low_level);
        }
        keymap++;
    }

    return 0;
}

static int ambarella_input_adckey_probe(struct platform_device *pdev)
{
    int rval = 0;
    u32 size = 0;
    const __be32 *prop;
    struct input_dev *input;
    struct ambarella_input_adckey_info *pinfo;
    struct ambarella_adc_client *adc_client;
    struct device_node *np = pdev->dev.of_node;

    pinfo = devm_kzalloc(&pdev->dev,
                         sizeof(struct ambarella_input_adckey_info),
                         GFP_KERNEL);
    if (!pinfo) {
        dev_err(&pdev->dev, "Failed to allocate pinfo!\n");
        return -ENOMEM;
    }

    // get adc keymap data if adc used for keybutton
    prop = of_get_property(np, "amb,adckeymap", &size);
    if(!prop) {
        dev_err(&pdev->dev, "no adc keymap info \n");
        return -EINVAL;
    } else {
        //get adc key level value
        size /= sizeof(__be32) * 2;
        pinfo->key_num = size;
        rval = ambarella_adckeymap_parse(pdev, prop, pinfo);
        if(rval)
            return rval;
    }

    input = input_allocate_device();
    if (!input) {
        dev_err(&pdev->dev, "input_allocate_device fail!\n");
        return -ENOMEM;
    }

    input->name = "AmbADCkey";
    input->phys = "ambadckey/input0";
    input->id.bustype = BUS_HOST;
    rval = input_register_device(input);
    if (rval) {
        dev_err(&pdev->dev, "Register input_dev failed!\n");
        goto INPUT_REGISTER_ERR;
    }

    pinfo->input = input;
    pinfo->system_event.notifier_call = ambarella_scan_adc_key_irq;
    adc_client = ambarella_adc_register(
                     pdev,
                     &pinfo->system_event);
    if(!adc_client) {
        dev_err(&pdev->dev, "Failed to register adc client\n");
        rval = PTR_ERR(adc_client);
        goto CLIENT_REGISTER_ERR;
    }

    pinfo->client = adc_client;
    ambarella_input_key_setup(pdev, pinfo);

    if(!adc_client->irq_support) {
        INIT_DELAYED_WORK(&pinfo->work, ambarella_scan_adc_key_polling);
        queue_delayed_work(system_freezable_wq,
                           &pinfo->work,
                           msecs_to_jiffies(20));
    }

    platform_set_drvdata(pdev, pinfo);
    dev_notice(&pdev->dev, "ADC key input driver probed!\n");

    return 0;

CLIENT_REGISTER_ERR:
    input_unregister_device(pinfo->input);
INPUT_REGISTER_ERR:
    input_free_device(input);
    return rval;
}

static int ambarella_input_adckey_remove(struct platform_device *pdev)
{
    struct ambarella_input_adckey_info	*pinfo;
    int rval = 0;
    pinfo = platform_get_drvdata(pdev);
    cancel_delayed_work_sync(&pinfo->work);
    ambarella_adc_unregister(pinfo->client);
    input_unregister_device(pinfo->input);
    input_free_device(pinfo->input);
    dev_notice(&pdev->dev,
               "Remove Ambarella ADC Key driver.\n");

    return rval;
}

#ifdef CONFIG_PM
static int ambarella_input_adckey_suspend(struct platform_device *pdev,
                                          pm_message_t state)
{
    return 0;
}

static int ambarella_input_adckey_resume(struct platform_device *pdev)
{
    return 0;
}
#endif

static const struct of_device_id ambarella_input_adckey_dt_ids[] = {
    {.compatible = "ambarella,input_adckey", },
    {},
};
MODULE_DEVICE_TABLE(of, ambarella_input_adckey_dt_ids);

static struct platform_driver ambarella_input_adckey_driver = {
    .probe		= ambarella_input_adckey_probe,
    .remove		= ambarella_input_adckey_remove,
#ifdef CONFIG_PM
    .suspend	= ambarella_input_adckey_suspend,
    .resume		= ambarella_input_adckey_resume,
#endif
    .driver		= {
        .name	= "ambarella-input-adc",
        .owner	= THIS_MODULE,
        .of_match_table = ambarella_input_adckey_dt_ids,
    },
};

module_platform_driver(ambarella_input_adckey_driver);

MODULE_AUTHOR("Bing-Liang Hu <blhu@ambarella.com>");
MODULE_DESCRIPTION("Ambarella ADC key Driver");
MODULE_LICENSE("GPL");

