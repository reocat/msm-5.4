#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/mfd/tps6586x.h>


struct tps658640_backlight_data {
	struct device *parent;
	int reg;
	int current_brightness;
};

static int tps658640_backlight_get_brightness(struct backlight_device *bl)
{
	struct tps658640_backlight_data *data = bl_get_data(bl);
	return data->current_brightness;
}

static int tps658640_backlight_set(struct backlight_device *bl, int brightness)
{
	int ret;
	struct tps658640_backlight_data *data = bl_get_data(bl);
	ret = tps6586x_write(data->parent,data->reg,brightness);
	if(ret < 0)
		return ret;

	data->current_brightness = brightness;

	return 0;
}

static int tps658640_backlight_update_status(struct backlight_device *bl)
{
	int brightness = bl->props.brightness;

	if (bl->props.power != FB_BLANK_UNBLANK)
		brightness = 0;

	if (bl->props.fb_blank != FB_BLANK_UNBLANK)
		brightness = 0;

	if (bl->props.state & BL_CORE_SUSPENDED)
		brightness = 0;
	return tps658640_backlight_set(bl, brightness);
}

static const struct backlight_ops tps658640_backlight_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status	= tps658640_backlight_update_status,
	.get_brightness	= tps658640_backlight_get_brightness,
};

static int tps658640_backlight_prob(struct platform_device *pdev)
{
	struct tps658640_backlight_pdata *pdata = pdev->dev.platform_data;
	struct tps658640_backlight_data *data;
	struct backlight_properties props;
	struct backlight_device *bl;
	struct device *parent = pdev->dev.parent;

	if(!pdata){
		dev_err(&pdev->dev,"No platform data supplied\n");
		return -EINVAL;
	}

	if(pdata->mode != 0){
		dev_err(&pdev->dev,"Only support mode 0\n");
		return -EINVAL;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;


	switch (pdata->pwm_out) {
	case 0:
		data->reg = 0x5A;
		break;
	case 1:
		data->reg = 0x5C;
		break;
	default:
		dev_err(&pdev->dev, "Invalid pwm output %d\n", pdata->pwm_out);
		return -EINVAL;
	}

	data->parent = parent;
	data->current_brightness = 0;
	props.max_brightness = pdata->max_cycle;
	bl = backlight_device_register("tps6586x", &pdev->dev, data,
				       &tps658640_backlight_ops, &props);
	if (IS_ERR(bl)) {
		dev_err(&pdev->dev, "failed to register backlight\n");
		kfree(data);
		return PTR_ERR(bl);
	}

	bl->props.brightness = pdata->max_cycle;
	bl->props.power = FB_BLANK_UNBLANK;
	bl->props.fb_blank = FB_BLANK_UNBLANK;

	platform_set_drvdata(pdev, bl);

	backlight_update_status(bl);
	return 0;
}

static int tps658640_backlight_remove(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct tps658640_backlight_data *data = bl_get_data(bl);

	backlight_device_unregister(bl);
	kfree(data);
	return 0;
}

static struct platform_driver tps658640_backlight_driver = {
	.driver		= {
		.name	= "tps6586x-backlight",
		.owner	= THIS_MODULE,
	},
	.probe		= tps658640_backlight_prob,
	.remove		= tps658640_backlight_remove,
};


static int __init tps658640_backlight_init(void)
{
	return platform_driver_register(&tps658640_backlight_driver);
}

static void __exit tps658640_backlight_exit(void)
{
	platform_driver_unregister(&tps658640_backlight_driver);
}

module_init(tps658640_backlight_init);
module_exit(tps658640_backlight_exit);

MODULE_DESCRIPTION("Backlight driver for tps658640 PMIC");
MODULE_AUTHOR("Bingliang Hu <blhu@ambarella.com>");



