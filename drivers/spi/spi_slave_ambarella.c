/*
 * linux/drivers/spi/spi_slave_ambarella.c
 *
 * History:
 *	2012/02/21 - [Zhenwu Xue]  created file
 *
 * Copyright (C) 2004-2012, Ambarella, Inc.
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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <asm/io.h>
#include <mach/io.h>
#include <plat/spi.h>
#include <plat/ambcache.h>

#define SPI_RXFIS_MASK 		0x00000010
#define SPI_RXOIS_MASK 		0x00000008
#define SPI_RXUIS_MASK 		0x00000004

#define RX_FTLR			8
#define RX_BUFFER_LEN		4096

static int rx_ftlr = RX_FTLR;
module_param(rx_ftlr, int, 0644);

struct ambarella_spi_slave {
	u32					regbase;
	int					irq;

	u16					mode;
	u16					bpw;

	u16					r_buf[RX_BUFFER_LEN];
	u16					r_buf_start;
	u16					r_buf_end;
	u16					r_buf_empty;
	u16					r_buf_full;
	u16					r_buf_round;
	spinlock_t				r_buf_lock;
};

static int ambarella_spi_slave_inithw(struct ambarella_spi_slave *priv)
{
	u32 				ctrlr0;

	ambarella_gpio_config(GPIO(33), GPIO_FUNC_SW_OUTPUT);
	ambarella_gpio_config(GPIO(34), GPIO_FUNC_HW);
	ambarella_gpio_config(SSI_SLAVE_CLK, GPIO_FUNC_SW_INPUT);
	ambarella_gpio_config(SSI_SLAVE_EN, GPIO_FUNC_HW);
	ambarella_gpio_config(SSI_SLAVE_MOSI, GPIO_FUNC_SW_INPUT);
	ambarella_gpio_config(SSI_SLAVE_MISO, GPIO_FUNC_HW);

	/* Initial Register Settings */
	ctrlr0 = (SPI_CFS << 12) | (SPI_READ_ONLY << 8) | (priv->mode << 6) |
				(SPI_FRF << 4) | (priv->bpw - 1);
	amba_writel(priv->regbase + SPI_CTRLR0_OFFSET, ctrlr0);

	amba_writel(priv->regbase + SPI_TXFTLR_OFFSET, 0);
	amba_writel(priv->regbase + SPI_RXFTLR_OFFSET, rx_ftlr - 1);
	amba_writel(priv->regbase + SPI_IMR_OFFSET, SPI_RXFIS_MASK);

	return 0;
}

static irqreturn_t ambarella_spi_slave_isr(int irq, void *dev_data)
{
	struct ambarella_spi_slave	*priv	= dev_data;
	u32				i, rxflr;

	if (amba_readl(priv->regbase + SPI_ISR_OFFSET)) {
		rxflr = amba_readl(priv->regbase + SPI_RXFLR_OFFSET);
		spin_lock(&priv->r_buf_lock);
		if (priv->r_buf_empty) {
			priv->r_buf_start	= 0;
			priv->r_buf_end		= 0;
			priv->r_buf_empty	= 0;
		}

		for (i = 0; i < rxflr; i++) {
			if (priv->r_buf_full) {
				printk("%s: Rx buffer overflow!!\n", __FILE__);
				priv->r_buf_start++;
				if (priv->r_buf_start >= RX_BUFFER_LEN) {
					priv->r_buf_start = 0;
					priv->r_buf_round++;
				}
			}

			priv->r_buf[priv->r_buf_end++] = amba_readl(priv->regbase + SPI_DR_OFFSET);
			if (priv->r_buf_end >= RX_BUFFER_LEN) {
				priv->r_buf_end = 0;
				priv->r_buf_round++;
			}

			if (priv->r_buf_round & 0x01) {
				if (priv->r_buf_end >= priv->r_buf_start) {
					priv->r_buf_full = 1;
				}
			}
		}
		printk("Status: %d %d\n", priv->r_buf_start, priv->r_buf_end);
		spin_unlock(&priv->r_buf_lock);
		amba_writel(priv->regbase + SPI_ISR_OFFSET, 0);
	}

	return IRQ_HANDLED;
}

static int __devinit ambarella_spi_slave_probe(struct platform_device *pdev)
{
	struct ambarella_spi_slave		*priv = NULL;
	struct resource 			*res;
	int					irq, errorCode;

	/* Get Base Address */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		errorCode = -EINVAL;
		goto ambarella_spi_slave_probe_exit;
	}

	/* Get IRQ NO. */
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		errorCode = -EINVAL;
		goto ambarella_spi_slave_probe_exit;
	}

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		errorCode = -ENOMEM;
		goto ambarella_spi_slave_probe_exit;
	}

	priv->regbase		= (u32)res->start;
	priv->irq		= irq;

	priv->mode		= 0;
	priv->bpw		= 16;

	priv->r_buf_empty	= 1;
	priv->r_buf_full	= 0;
	spin_lock_init(&priv->r_buf_lock);

	ambarella_spi_slave_inithw(priv);

	/* Request IRQ */
	errorCode = request_irq(irq, ambarella_spi_slave_isr, IRQF_TRIGGER_HIGH,
			dev_name(&pdev->dev), priv);
	if (errorCode) {
		kfree(priv);
		goto ambarella_spi_slave_probe_exit;
	} else {
		pdev->dev.platform_data		= priv;
		dev_info(&pdev->dev, "Ambarella SPI Slave Controller %d created!\n", pdev->id);
		amba_writel(priv->regbase + SPI_SSIENR_OFFSET, 1);
	}

ambarella_spi_slave_probe_exit:
	return errorCode;
}

static int __devexit ambarella_spi_slave_remove(struct platform_device *pdev)
{
	struct ambarella_spi_slave	*priv;

	priv	= (struct ambarella_spi_slave *)pdev->dev.platform_data;
	free_irq(priv->irq, priv);

	return 0;
}

#ifdef CONFIG_PM
static int ambarella_spi_slave_suspend_noirq(struct device *dev)
{
	return 0;
}

static int ambarella_spi_slave_resume_noirq(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops ambarella_spi_slave_dev_pm_ops = {
	.suspend_noirq = ambarella_spi_slave_suspend_noirq,
	.resume_noirq = ambarella_spi_slave_resume_noirq,
};
#endif

static struct platform_driver ambarella_spi_slave_driver = {
	.probe		= ambarella_spi_slave_probe,
	.remove		= __devexit_p(ambarella_spi_slave_remove),
	.driver		= {
		.name	= "ambarella-spi-slave",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &ambarella_spi_slave_dev_pm_ops,
#endif
	},
};

static int __init ambarella_spi_slave_init(void)
{
	return platform_driver_register(&ambarella_spi_slave_driver);
}

static void __exit ambarella_spi_slave_exit(void)
{
	platform_driver_unregister(&ambarella_spi_slave_driver);
}

subsys_initcall(ambarella_spi_slave_init);
module_exit(ambarella_spi_slave_exit);

MODULE_DESCRIPTION("Ambarella Media Processor SPI Slave Controller");
MODULE_AUTHOR("Zhenwu Xue, <zwxue@ambarella.com>");
MODULE_LICENSE("GPL");
