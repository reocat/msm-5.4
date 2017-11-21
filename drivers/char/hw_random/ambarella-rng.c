/*
 * Copyright (c) 2017 Ambarella, Inc.
 *
 * This program is free software; you can distribute it and/or modify it
 * under the terms of the GNU General Public License (Version 2) as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include <linux/delay.h>
#include <linux/hw_random.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>

#define	AMBA_RNG_CONTROL			0x40
#define	AMBA_RCT_CONTROL			0x1A4
#define	AMBA_RCT_BASE_ADDRESS			0xED080000
#define	AMBA_RNG_DATA				0x44
#define AMBA_RNG_DELAY_MS			25

struct ambarella_rng_private {
	struct hwrng rng;
	void __iomem *base;
};

static int ambarella_rng_read(struct hwrng *rng, void *buf, size_t max, bool wait)
{

	struct ambarella_rng_private *priv =
	    container_of(rng, struct ambarella_rng_private, rng);
	u32	val;
	size_t read = 0;
	u32 *data = buf;
	unsigned int start_time, timeout;

	/* start rng */
	val = readl_relaxed(priv->base + AMBA_RNG_CONTROL);
	val |= BIT(1);
	val &= ~BIT(0);
	writel_relaxed(val, priv->base + AMBA_RNG_CONTROL);

	start_time = jiffies_to_msecs(jiffies);
	while ((readl_relaxed(priv->base + AMBA_RNG_CONTROL) & 0x2) && wait) {
		timeout = jiffies_to_msecs(jiffies) - start_time;
		if (timeout < AMBA_RNG_DELAY_MS) {
			cpu_relax();
		} else {
			pr_info("ambarella hwrng timeout\n");
			read = -ETIMEDOUT;
			goto out;
		}
	}

	while (read*8 < max) {
		*data++ = readl_relaxed(priv->base + AMBA_RNG_DATA + read);
		read += 4;
	}

out:
	return read;

}

static int ambarella_rng_probe(struct platform_device *pdev)
{
	struct ambarella_rng_private *priv;
	struct resource *res;
	u32	val;
	void __iomem *rct_rng;

	dev_info(&pdev->dev, "rng probed\r\n");
	priv = devm_kzalloc(&pdev->dev, sizeof(struct ambarella_rng_private), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	priv->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(priv->base)) {
		dev_err(&pdev->dev, "ambarella-hwrng can't map mem\n");
		return PTR_ERR(priv->base);
	}

	priv->rng.name = dev_driver_string(&pdev->dev);

	priv->rng.read = ambarella_rng_read;
	priv->rng.priv = (unsigned long)&pdev->dev;

	platform_set_drvdata(pdev, priv);

	/* set sample rate */
	val = readl_relaxed(priv->base + AMBA_RNG_CONTROL);
	val |= BIT(4);
	val &= ~BIT(5);
	writel_relaxed(val, priv->base + AMBA_RNG_CONTROL);

	/* rct control powerdown */
	rct_rng = ioremap(AMBA_RCT_BASE_ADDRESS, 0x20);
	if (rct_rng == NULL) {
		dev_err(&pdev->dev, "ambarella rct_rng ioremap() failed\n");
		return -ENOMEM;
	}
	val = readl_relaxed(rct_rng + AMBA_RCT_CONTROL);
	val &= ~BIT(0);
	writel_relaxed(val, rct_rng + AMBA_RCT_CONTROL);

	return devm_hwrng_register(&pdev->dev, &priv->rng);
}

static const struct of_device_id ambarella_rng_match[] = {
	{
		.compatible = "ambarella,hw-rng",
	},
	{},
};
MODULE_DEVICE_TABLE(of, ambarella_rng_match);

static struct platform_driver ambarella_rng_driver = {
	.driver = {
		.name = "ambarella-hwrng",
		.of_match_table = ambarella_rng_match,
	},
	.probe = ambarella_rng_probe,
};

module_platform_driver(ambarella_rng_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kai Qiang <kqiang@ambarella.com>");
MODULE_DESCRIPTION("Ambarella RNG device driver");
