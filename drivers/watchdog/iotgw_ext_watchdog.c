
/* External watchdog driver for IOTGW
 * External watchdog should be enabled by bootloader or hardware 
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/virtio.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/watchdog.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>

#define IMX6_WDT_WCR		0x00		/* Control Register */
#define IMX6_WDT_WCR_WDE	BIT(2)		/* -> Watchdog Enable */
#define IMX6_WDT_WCR_WDA	BIT(5)		/* -> External Reset WDOG_B */

static int wdt_in_gpio;
static int wdt_feed_interval = 10;

#ifdef CONFIG_IOTGW_WATCHDOG_ENHANCE
static struct regmap *wdt_regmap;
#endif

static bool wdtnowayout = WATCHDOG_NOWAYOUT;

#ifdef CONFIG_IOTGW_WATCHDOG_ENHANCE
static const struct regmap_config imx6_wdt_regmap_config = {
	.reg_bits = 16,
	.reg_stride = 2,
	.val_bits = 16,
	.max_register = 0x8,
};
#endif

static int iotgw_ext_wdt_start(struct watchdog_device *wdog)
{
	return 0;
}

static void feed_ext_wdt(void)
{
	/* refresh the wdt counter to keepalive */
	gpio_direction_output(wdt_in_gpio, 0);
	udelay(20);
	gpio_direction_output(wdt_in_gpio, 1);
	udelay(20);
}

static int iotgw_ext_wdt_ping(struct watchdog_device *wdog)
{
	feed_ext_wdt();
	return 0;
}

#ifdef CONFIG_IOTGW_WATCHDOG_ENHANCE
static int iotgw_wdt_restart(struct watchdog_device *wdog, unsigned long action,
			    void *data)
{
	unsigned int wcr_enable = IMX6_WDT_WCR_WDE | IMX6_WDT_WCR_WDA;
	
	regmap_write(wdt_regmap, IMX6_WDT_WCR, wcr_enable);
	/*
	 * Due to imx6q errata ERR004346 (WDOG: WDOG SRS bit requires to be
	 * written twice), we add another two writes to ensure there must be at
	 * least two writes happen in the same one 32kHz clock period.  We save
	 * the target check here, since the writes shouldn't be a huge burden
	 * for other platforms.
	 */
	regmap_write(wdt_regmap, IMX6_WDT_WCR, wcr_enable);
	regmap_write(wdt_regmap, IMX6_WDT_WCR, wcr_enable);

	/* wait for reset to assert... */
	mdelay(100);
	pr_emerg("failed to assert watchdog reset \n");

	return 0;

}
#endif

static const struct watchdog_ops iotgw_ext_wdt_ops = {
	.owner = THIS_MODULE,
	.start = iotgw_ext_wdt_start,
	.ping = iotgw_ext_wdt_ping,
#ifdef CONFIG_IOTGW_WATCHDOG_ENHANCE
	.restart = iotgw_wdt_restart,
#endif
};

static const struct watchdog_info iotgw_ext_wdt_info = {
	.identity	= "IOTGW external watchdog timer",
	.options	= WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE ,
};

static ssize_t iotgw_ext_wdt_show_timeout_status(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	int ret;

	ret = sprintf(buf, "gpio:%d,feed_interval:%d \n",wdt_in_gpio, wdt_feed_interval);

	return ret;
}

static DEVICE_ATTR(config, S_IRUGO, iotgw_ext_wdt_show_timeout_status,NULL);

static struct attribute *iotgw_ext_wdt_attrs[] = {
	&dev_attr_config.attr,
	NULL
};
ATTRIBUTE_GROUPS(iotgw_ext_wdt);

static struct watchdog_device iotgw_ext_wdt_dev = {
	.groups = iotgw_ext_wdt_groups,
	.info = &iotgw_ext_wdt_info,
	.ops = &iotgw_ext_wdt_ops,
	.min_timeout = 1,
	.max_timeout = 60,
	.max_hw_heartbeat_ms = 128000,
	.timeout = 5,
};

static int __init iotgw_wdt_probe(struct platform_device *pdev)
{
	int err;
	void __iomem *base;
	struct resource *res;

	pr_info("Probe for IOTGW external watchdog driver\n");
	
	struct device_node *np = pdev->dev.of_node;
	wdt_in_gpio = of_get_named_gpio(np, "wdt-in", 0);
	if (wdt_in_gpio == -EPROBE_DEFER)
	{
		dev_err(&pdev->dev, "Get wdt-in gpio failed \n");
		return -EPROBE_DEFER;
	}
	
	if (!gpio_is_valid(wdt_in_gpio))
	{
		dev_err(&pdev->dev, "wdt_in_gpio%d is invalid \n",wdt_in_gpio);
		return 0;
	}
	dev_info(&pdev->dev, "Get wdt_in_gpio:%d \n",wdt_in_gpio);
	
	err = of_property_read_u32(np, "wdt-feed-interval", &wdt_feed_interval);
	if (err < 0)
	{
		dev_info(&pdev->dev,"Get wdt_feed_interval err:%d \n",err);
	}

	if  (wdt_feed_interval > 60)
	{
		wdt_feed_interval = 60;
	}
	dev_info(&pdev->dev, "Get wdt_feed_interval:%d \n",wdt_feed_interval);
	iotgw_ext_wdt_dev.timeout = wdt_feed_interval*2;

#ifdef CONFIG_IOTGW_WATCHDOG_ENHANCE
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Can't get device resources \n");
		return -ENODEV;
	}

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	wdt_regmap = devm_regmap_init_mmio_clk(&pdev->dev, NULL, base,&imx6_wdt_regmap_config);
	if (IS_ERR(wdt_regmap)) {
		dev_err(&pdev->dev, "Regmap init failed \n");
		return PTR_ERR(wdt_regmap);
	}
#endif
	watchdog_set_nowayout(&iotgw_ext_wdt_dev, wdtnowayout);
	set_bit(WDOG_HW_RUNNING, &(iotgw_ext_wdt_dev.status));

	err = watchdog_register_device(&iotgw_ext_wdt_dev);
	if (err) {
		dev_err(&pdev->dev,"Failed to register external watchdog device \n");
		return err;
	}

	dev_info(&pdev->dev, "Init external watchdog driver done \n");
	return err;
}

static int __exit iotgw_wdt_remove(struct platform_device *pdev)
{
	watchdog_unregister_device(&iotgw_ext_wdt_dev);
	return 0;
}

static void iotgw_wdt_shutdown(struct platform_device *pdev)
{
	return;
}

static const struct of_device_id iotgw_ext_dt_ids[] = {
	{ .compatible = "fsl,external_watchdog", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, iotgw_ext_dt_ids);

static struct platform_driver iotgw_wdt_driver =
{
	.remove	= __exit_p(iotgw_wdt_remove),
	.shutdown = iotgw_wdt_shutdown,
	.driver	= {
		.name = "iotgw-external-wdt",
		.of_match_table = iotgw_ext_dt_ids,
	},
};

static int __init iotgw_ext_wdt_init(void)
{
	return platform_driver_probe(&iotgw_wdt_driver, iotgw_wdt_probe);
}
//module_init(iotgw_ext_wdt_init);
late_initcall(iotgw_ext_wdt_init);

static void __exit iotgw_ext_wdt_exit(void)
{
	pr_info("Exit from IOTGW external watchdog driver\n");

	watchdog_unregister_device(&iotgw_ext_wdt_dev);
	platform_driver_unregister(&iotgw_wdt_driver);
}
module_exit(iotgw_ext_wdt_exit);

MODULE_DESCRIPTION("IOTGW External Watchdog driver");
MODULE_LICENSE("GPL v2");
MODULE_SOFTDEP("pre: external watchdog driver for iot gateway");
