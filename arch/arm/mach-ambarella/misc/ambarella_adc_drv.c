/*
 * arch/arm/plat-ambarella/generic/ambarella_adc_drv.c
 *
 * Author: Bing-Liang Hu <blhu@ambarella.com>
 *
 * Copyright (C) 2014-2019, Ambarella, Inc.
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
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <mach/hardware.h>
#include <plat/adc.h>
#include <plat/rct.h>
#include <plat/event.h>

struct ambarella_adc_info {
    struct platform_device	*pdev;
    void __iomem *regbase;
    bool    irq_support;
    unsigned int	irq;
    u32 clk;
    struct delayed_work work;
};
struct ambarella_adc_info *gpAdcInfo;

static int ambarella_adc_setclock(struct platform_device *pdev,
                                  struct ambarella_adc_info *pinfo)
{
    struct clk *pgclk_adc = clk_get(NULL, "gclk_adc");

    if(IS_ERR(pgclk_adc)) {
        dev_err(&pdev->dev, "No registered gclk_adc !\n");
        return -ENOENT;
    }

    clk_set_rate(pgclk_adc, pinfo->clk);

    return 0;
}

static void ambarella_adc_stop(struct ambarella_adc_info *pinfo)
{
    amba_writel(ADC16_CTRL_REG,
                (ADC_CTRL_SCALER_POWERDOWN | ADC_CTRL_POWERDOWN));

    amba_clrbitsl(pinfo->regbase + ADC_CONTROL_OFFSET, ADC_CONTROL_ENABLE);
}

static void ambarella_adc_start(struct ambarella_adc_info *pinfo)
{
    amba_writel(ADC16_CTRL_REG, 0);

#if (CHIP_REV == S2) || (CHIP_REV == S2L)
    // adc controller soft reset
    amba_writel(pinfo->regbase + ADC_CONTROL_OFFSET, ADC_CONTROL_RESET);
    //slot set
    amba_writel(pinfo->regbase + ADC_SLOT_NUM_OFFSET, 0);
    amba_writel(pinfo->regbase + ADC_SLOT_PERIOD_OFFSET, 0xffff);
    amba_writel(pinfo->regbase + ADC_SLOT_CTRL_0_OFFSET, 0xfff);

    //enable
    amba_setbitsl(
        pinfo->regbase + ADC_CONTROL_OFFSET, ADC_CONTROL_ENABLE);
#else
    amba_setbitsl(pinfo->regbase + ADC_ENABLE_OFFSET, 1);
#endif

    //set continue sample mode
    amba_setbitsl(pinfo->regbase + ADC_CONTROL_OFFSET, ADC_CONTROL_MODE);

    amba_setbitsl(pinfo->regbase + ADC_CONTROL_OFFSET, ADC_CONTROL_START);
}

static void adc_set_irq_threshold(u32 ch, u32 h_level, u32 l_level)
{
    u32 irq_control_address = 0;
    u32 value = ADC_EN_HI(!!h_level) | ADC_EN_LO(!!l_level) |
                ADC_VAL_HI(h_level) | ADC_VAL_LO(l_level);

    switch (ch) {
    case 0:
        irq_control_address = ADC_CHAN0_INTR_REG;
        break;
    case 1:
        irq_control_address = ADC_CHAN1_INTR_REG;
        break;
    case 2:
        irq_control_address = ADC_CHAN2_INTR_REG;
        break;
    case 3:
        irq_control_address = ADC_CHAN3_INTR_REG;
        break;
#if (ADC_NUM_CHANNELS >= 6)
    case 4:
        irq_control_address = ADC_CHAN4_INTR_REG;
        break;
    case 5:
        irq_control_address = ADC_CHAN5_INTR_REG;
        break;
#endif
#if (ADC_NUM_CHANNELS >= 8)
    case 6:
        irq_control_address = ADC_CHAN6_INTR_REG;
        break;
    case 7:
        irq_control_address = ADC_CHAN7_INTR_REG;
        break;
#endif
#if (ADC_NUM_CHANNELS >= 10)
    case 8:
        irq_control_address = ADC_CHAN8_INTR_REG;
        break;
    case 9:
        irq_control_address = ADC_CHAN9_INTR_REG;
        break;
#endif
#if (ADC_NUM_CHANNELS >= 12)
    case 10:
        irq_control_address = ADC_CHAN10_INTR_REG;
        break;
    case 11:
        irq_control_address = ADC_CHAN11_INTR_REG;
        break;
#endif
    default:
        printk("Don't support %d channels\n", ch);
        return;
    }
    amba_writel(irq_control_address, value);
    printk(KERN_DEBUG "%s: set ch[%d] h[%d], l[%d], 0x%08X!\n",
           __func__, ch, h_level, l_level, value);
}

static void ambarella_adc_read(u32 *data)
{
#if (CHIP_REV == A5S)
    data[0] = amba_readl(gpAdcInfo->regbase + ADC_DATA0_OFFSET);
    data[1] = amba_readl(gpAdcInfo->regbase + ADC_DATA1_OFFSET);
    data[2] = amba_readl(gpAdcInfo->regbase + ADC_DATA2_OFFSET);
    data[3] = amba_readl(gpAdcInfo->regbase + ADC_DATA3_OFFSET);
#else
    int i;
    for (i = 0; i < ADC_NUM_CHANNELS; i++)
        data[i] = amba_readl(gpAdcInfo->regbase + ADC_DATA0_OFFSET + i * 4);
#endif
}

struct ambarella_adc_client *ambarella_adc_register(
                                        struct platform_device *pdev,
                                        void *nb)
{
    struct ambarella_adc_client *adc_client;

    adc_client = kzalloc(sizeof(struct ambarella_adc_client), GFP_KERNEL);
    if(!adc_client) {
        dev_err(&pdev->dev, "no memory for adc client\n");
        return ERR_PTR(-ENOMEM);
    }

    adc_client->pdev = pdev;
    adc_client->set_irq_threshold = NULL;

    if(gpAdcInfo->irq_support && (nb != NULL)) {
        adc_client->set_irq_threshold = adc_set_irq_threshold;
        adc_client->nb = nb;
        ambarella_register_event_notifier(nb);
    } else {
        adc_client->readdata = ambarella_adc_read;
    }
    adc_client->irq_support = gpAdcInfo->irq_support;

    return adc_client;
}
EXPORT_SYMBOL_GPL(ambarella_adc_register);

int ambarella_adc_unregister(struct ambarella_adc_client *client)
{
    if(client->nb != NULL)
        ambarella_unregister_event_notifier(client->nb);
    kfree(client);
    return 0;
}
EXPORT_SYMBOL_GPL(ambarella_adc_unregister);

static ssize_t ambarella_adc_show(struct device *dev,
                                  struct device_attribute *attr,
                                  char *buf)
{
    ssize_t ret = 0;
    int i;
    u32   *adc_data;
    int total_channel = ADC_NUM_CHANNELS;

    adc_data = kzalloc(sizeof(u32) * total_channel, GFP_KERNEL);
    ambarella_adc_read(adc_data);
    for(i = 0; i < total_channel; i++ ) {
        ret += sprintf(&buf[ret], "adc%d=0x%x\n", i, adc_data[i]);
    }
    return ret;
}
static DEVICE_ATTR(adcsys, 0444, ambarella_adc_show, NULL);

static irqreturn_t ambarella_adc_irq(int irq, void *deviceinfo)
{
    int rval;
    u32 data[ADC_NUM_CHANNELS];
    struct ambarella_adc_info *pinfo = deviceinfo;
    struct platform_device *pdev = pinfo->pdev;

#if (CHIP_REV == S2) || (CHIP_REV == S2L)
    u32 regv;
    //clear interrupt status register
    regv = amba_readl(pinfo->regbase + ADC_DATA_INTR_TABLE_OFFSET);
    if(regv == 0) {
        dev_err(&pdev->dev, "no data interrupt !\n");
        return IRQ_NONE;
    }
    amba_writel(pinfo->regbase + ADC_DATA_INTR_TABLE_OFFSET, regv);
#endif

    //read data
    ambarella_adc_read(data);

    rval = notifier_to_errno(
               ambarella_set_event(AMBA_EVENT_ADC_INTERRUPT, data));
    if (rval)
        dev_err(&pdev->dev, "%s: 0x%x fail(%d)\n", __func__,
                AMBA_EVENT_ADC_INTERRUPT, rval);

    return IRQ_HANDLED;
}

static int ambarella_adc_probe(struct platform_device *pdev)
{
    struct ambarella_adc_info *pinfo;
    struct resource *mem;
    struct device_node *np = pdev->dev.of_node;
    int ret = 0;
    struct device *dev = &pdev->dev;

    mem = platform_get_resource(pdev,
                                IORESOURCE_MEM,
                                0);
    if(mem == NULL) {
        dev_err(&pdev->dev, "Get mem resource failed !\n");
        return -ENXIO;
    }

    pinfo = devm_kzalloc(&pdev->dev,
                         sizeof(struct ambarella_adc_info),
                         GFP_KERNEL);

    pinfo->regbase = devm_ioremap(&pdev->dev,
                                  mem->start,
                                  resource_size(mem));
    if(!pinfo->regbase) {
        dev_err(&pdev->dev, "devm_ioremap failed !\n");
        return -ENOMEM;
    }

    ret = of_property_read_u32(np, "clock-frequency", &pinfo->clk);
    if (ret < 0) {
        dev_err(&pdev->dev, "Get clock-frequency failed!\n");
        return -ENODEV;
    }

    pinfo->irq_support = !!of_find_property(np, "amb,irq-support", NULL);
    if(pinfo->irq_support) {
        pinfo->irq =  platform_get_irq(pdev, 0);
        if(pinfo->irq < 0) {
            dev_err(&pdev->dev, "Can not get irq !\n");
            return -ENXIO;
        }

        ret = devm_request_irq(dev, pinfo->irq,
                               ambarella_adc_irq,
                               IRQF_TRIGGER_HIGH,
                               dev_name(dev),
                               pinfo);
        if(ret < 0) {
            dev_err(&pdev->dev, "Can not request irq %d!\n", pinfo->irq);
            return -ENXIO;
        }
    }

    ret = ambarella_adc_setclock(pdev, pinfo);
    if(ret < 0)
        return ret;

    pinfo->pdev = pdev;
    ret = device_create_file(&pdev->dev, &dev_attr_adcsys);
    if (ret != 0) {
        dev_err(&pdev->dev, "can not create file : %d\n", ret);
        return ret;
    }

    ambarella_adc_start(pinfo);

    gpAdcInfo = pinfo;
    platform_set_drvdata(pdev, pinfo);

    dev_notice(&pdev->dev, "Ambarella ADC driver init\n");

    return 0;
}

static int ambarella_adc_remove(struct platform_device *pdev)
{
    struct ambarella_adc_info *pinfo = platform_get_drvdata(pdev);

    device_remove_file(&pdev->dev, &dev_attr_adcsys);

    ambarella_adc_stop(pinfo);

    dev_notice(&pdev->dev, "Ambarella ADC driver remove\n");
    return 0;
}

#ifdef CONFIG_PM
static int ambarella_adc_suspend(struct platform_device *pdev,
                                 pm_message_t state)
{
    struct ambarella_adc_info *pinfo = platform_get_drvdata(pdev);
    ambarella_adc_stop(pinfo);
    return 0;
}

static int ambarella_adc_resume(struct platform_device *pdev)
{
    struct ambarella_adc_info *pinfo = platform_get_drvdata(pdev);
    ambarella_adc_setclock(pdev, pinfo);
    ambarella_adc_start(pinfo);
    return 0;
}
#endif

static const struct of_device_id ambarella_adc_dt_ids[] = {
    {.compatible = "ambarella,adc", },
    {.compatible = "ambarella,adc-old", },
    {},
};
MODULE_DEVICE_TABLE(of, ambarella_adc_dt_ids);

static struct platform_driver ambarella_adc_driver = {
    .driver	= {
        .name	= "ambarella-adc",
        .owner	= THIS_MODULE,
        .of_match_table	= ambarella_adc_dt_ids,
    },
    .probe	= ambarella_adc_probe,
    .remove	= ambarella_adc_remove,
#ifdef CONFIG_PM
    .suspend	= ambarella_adc_suspend,
    .resume	= ambarella_adc_resume,
#endif
};

module_platform_driver(ambarella_adc_driver);
MODULE_AUTHOR("BingLiang Hu <blhu@ambarella.com>");
MODULE_DESCRIPTION("Ambarella ADC Driver");
MODULE_LICENSE("GPL");

