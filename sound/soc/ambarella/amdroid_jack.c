/*
 * sound/soc/amdroid_jack.c
 *
 * History:
 *	2012/03/23 - [Cao Rongrong] Created file
 *
 * Copyright (C) 2012-2016, Ambarella, Inc.
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
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/switch.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/gpio.h>

#include <plat/adc.h>
#include "amdroid_jack.h"

struct amdroid_jack_info {
	struct amdroid_jack_platform_data *pdata;
	struct wake_lock det_wake_lock;
	struct amdroid_jack_zone *zone;
	int detect_irq;
	unsigned int cur_jack_type;
	struct delayed_work work;
};

/* sysfs name HeadsetObserver.java looks for to track headset state */
static struct switch_dev switch_jack_detection = {
	.name = "h2w",
};

static void amdroid_switch_set_state(struct amdroid_jack_info *jack_info, int jack_type)
{
	/* this can happen during slow inserts where we think we identified
	 * the type but then we get another interrupt and do it again */
	if (jack_type == jack_info->cur_jack_type)
		return;

	jack_info->cur_jack_type = jack_type;

	/* prevent suspend to allow user space to respond to switch */
	wake_lock_timeout(&jack_info->det_wake_lock, WAKE_LOCK_TIME);

	switch_set_state(&switch_jack_detection, jack_type);
}

static int adc_get_value(int ch)
{
	int adc_size = ADC_NUM_CHANNELS;
	u32 adc_data[ADC_NUM_CHANNELS];

	if (ch >= ADC_NUM_CHANNELS) {
		pr_err("%s: Invalid ADC channel\n", __func__);
		return -1;
	}

	ambarella_adc_get_array(adc_data, &adc_size);

	return adc_data[ch];
}

static void adc_check_jack_type(struct amdroid_jack_info *jack_info)
{
	struct amdroid_jack_zone *zones = jack_info->pdata->zones;
	int size = jack_info->pdata->num_zones;
	int count[MAX_ZONE_LIMIT] = {0};
	int npolarity = !jack_info->pdata->active_high;
	int i, adc;

	while (gpio_get_value(jack_info->pdata->detect_gpio) ^ npolarity) {
		adc = adc_get_value(jack_info->pdata->adc_channel);
		if (adc < 0)
			return;

		/* determine the type of headset based on the
		 * adc value.  An adc value can fall in various
		 * ranges or zones.  Within some ranges, the type
		 * can be returned immediately.  Within others, the
		 * value is considered unstable and we need to sample
		 * a few more types (up to the limit determined by
		 * the range) before we return the type for that range.
		 */
		for (i = 0; i < size; i++) {
			if (adc <= zones[i].adc_high) {
				if (++count[i] > zones[i].check_count) {
					amdroid_switch_set_state(jack_info,
							  zones[i].jack_type);
					return;
				}
				msleep(zones[i].delay_ms);
				break;
			}
		}
	}
	/* jack removed before detection complete */
	amdroid_switch_set_state(jack_info, AMDROID_JACK_NO_DEVICE);
}

/* thread run whenever the headset detect state changes (either insertion
 * or removal).
 */
static irqreturn_t amdroid_jack_detect_irq_thread(int irq, void *dev_id)
{
	struct amdroid_jack_info *jack_info = dev_id;

	schedule_delayed_work(&jack_info->work, 0);

	return IRQ_HANDLED;
}

static void amdroid_jack_detect_worker(struct work_struct *work)
{
	struct amdroid_jack_info *jack_info;
	struct amdroid_jack_platform_data *pdata;
	int npolarity, time_left_ms;

	jack_info = container_of(work, struct amdroid_jack_info, work.work);
	pdata = jack_info->pdata;
	npolarity = !jack_info->pdata->active_high;
	time_left_ms = DET_CHECK_TIME_MS;

	/* set mic bias to enable adc */
	pdata->set_micbias_state(pdata->private_data, true);

	/* debounce headset jack.  don't try to determine the type of
	 * headset until the detect state is true for a while. */
	while (time_left_ms > 0) {
		if (!(gpio_get_value(jack_info->pdata->detect_gpio) ^ npolarity))
			break;
		msleep(10);
		time_left_ms -= 10;
	}

	if (time_left_ms <= 0) {
		/* jack presence was detected the whole time, figure out which type */
		adc_check_jack_type(jack_info);
	} else {
		/* jack not detected. */
		amdroid_switch_set_state(jack_info, AMDROID_JACK_NO_DEVICE);
	}

	/* disable micbias */
	pdata->set_micbias_state(pdata->private_data, false);
}

static int amdroid_jack_probe(struct platform_device *pdev)
{
	struct amdroid_jack_info *jack_info;
	struct amdroid_jack_platform_data *pdata = pdev->dev.platform_data;
	int rval;

	pr_info("%s : Registering Jack detection driver\n", __func__);
	if (!pdata) {
		pr_err("%s : pdata is NULL.\n", __func__);
		return -ENODEV;
	}

	if (!pdata->zones || pdata->num_zones > MAX_ZONE_LIMIT
			 || !pdata->set_micbias_state) {
		pr_err("%s : Invalid pdata\n", __func__);
		return -ENODEV;
	}

	jack_info = kzalloc(sizeof(struct amdroid_jack_info), GFP_KERNEL);
	if (jack_info == NULL) {
		pr_err("%s : Failed to allocate memory.\n", __func__);
		return -ENOMEM;
	}

	jack_info->pdata = pdata;
	jack_info->cur_jack_type = AMDROID_JACK_NO_DEVICE;

	rval = switch_dev_register(&switch_jack_detection);
	if (rval < 0) {
		pr_err("%s : Failed to register switch device\n", __func__);
		goto err_switch_dev_register;
	}

	wake_lock_init(&jack_info->det_wake_lock, WAKE_LOCK_SUSPEND, "hs_jack_det");

	rval = gpio_request_one(pdata->detect_gpio, GPIOF_IN, "jack_detect_gpio");
	if (rval) {
		pr_err("%s : gpio_request failed for %d\n",
		       __func__, pdata->detect_gpio);
		goto err_gpio_request;
	}

	jack_info->detect_irq = gpio_to_irq(pdata->detect_gpio);

	rval = request_threaded_irq(jack_info->detect_irq, NULL,
			amdroid_jack_detect_irq_thread,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			pdev->name, jack_info);
	if (rval) {
		pr_err("%s : Failed to request_irq.\n", __func__);
		goto err_request_irq_failed;
	}

	/* to handle insert/removal when we're sleeping in a call */
	rval = enable_irq_wake(jack_info->detect_irq);
	if (rval) {
		pr_err("%s : Failed to enable_irq_wake.\n", __func__);
		goto err_enable_irq_wake;
	}

	dev_set_drvdata(&pdev->dev, jack_info);

	/* Perform initial detection,
	 * delay 300ms to wait for sound card init completion */
	INIT_DELAYED_WORK(&jack_info->work, amdroid_jack_detect_worker);
	schedule_delayed_work(&jack_info->work, msecs_to_jiffies(300));

	return 0;

err_enable_irq_wake:
	free_irq(jack_info->detect_irq, jack_info);
err_request_irq_failed:
	gpio_free(pdata->detect_gpio);
err_gpio_request:
	wake_lock_destroy(&jack_info->det_wake_lock);
	switch_dev_unregister(&switch_jack_detection);
err_switch_dev_register:
	kfree(jack_info);

	return rval;
}

static int amdroid_jack_remove(struct platform_device *pdev)
{

	struct amdroid_jack_info *jack_info = dev_get_drvdata(&pdev->dev);

	disable_irq_wake(jack_info->detect_irq);
	free_irq(jack_info->detect_irq, jack_info);
	wake_lock_destroy(&jack_info->det_wake_lock);
	switch_dev_unregister(&switch_jack_detection);
	gpio_free(jack_info->pdata->detect_gpio);
	kfree(jack_info);

	return 0;
}

static struct platform_driver amdroid_jack_driver = {
	.probe = amdroid_jack_probe,
	.remove = amdroid_jack_remove,
	.driver = {
			.name = "amdroid_jack",
			.owner = THIS_MODULE,
		   },
};
static int __init amdroid_jack_init(void)
{
	return platform_driver_register(&amdroid_jack_driver);
}

static void __exit amdroid_jack_exit(void)
{
	platform_driver_unregister(&amdroid_jack_driver);
}

module_init(amdroid_jack_init);
module_exit(amdroid_jack_exit);

MODULE_AUTHOR("Cao Rongrong <rrcao@ambarella.com>");
MODULE_DESCRIPTION("Ambarella Ear-Jack Detection Driver");
MODULE_LICENSE("GPL");

