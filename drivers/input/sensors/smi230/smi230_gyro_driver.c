// SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
/**
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE
 * Copyright (c) 2020-2021 Robert Bosch GmbH. All rights reserved.
 *
 * This file is free software licensed under the terms of version 2 
 * of the GNU General Public License, available from the file LICENSE-GPL 
 * in the main directory of this source tree.
 *
 * BSD LICENSE
 * Copyright (c) 2020-2021 Robert Bosch GmbH. All rights reserved.
 *
 * BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 **/

#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/timer.h>
#include <linux/jiffies.h>


#include "smi230_driver.h"
#include "smi230_data_sync.h"

#define MODULE_TAG MODULE_NAME
#include "smi230_log.h"
#include "smi230.h"

#define SMI230_MIN_VALUE      -32768
#define SMI230_MAX_VALUE      32767

#ifdef CONFIG_SMI230_GYRO_FIFO
#define SMI230_MAX_GYRO_FIFO_FRAME 100
#define SMI230_MAX_GYRO_FIFO_BYTES (SMI230_MAX_GYRO_FIFO_FRAME * SMI230_FIFO_GYRO_FRAME_LENGTH)

static uint8_t fifo_buf[SMI230_MAX_GYRO_FIFO_BYTES];
#endif

static struct timer_list pm_mode_timer;
static bool is_gyro_ready = true;

#ifdef CONFIG_ENABLE_SMI230_ACC_GYRO_BUFFERING
#define SMI_GYRO_MAXSAMPLE	4000
#define G_MAX			23920640
struct smi_gyro_sample {
	int xyz[3];
	unsigned int tsec;
	unsigned long long tnsec;
};
#endif

struct smi230_client_data {
	struct device *dev;
	struct input_dev *input;
	int IRQ;
	uint8_t gpio_pin;
	uint64_t timestamp;
	uint64_t timestamp_old;
#ifdef CONFIG_ENABLE_SMI230_ACC_GYRO_BUFFERING
	bool read_gyro_boot_sample;
	int gyro_bufsample_cnt;
	bool gyro_buffer_smi230_samples;
	bool gyro_enable;
	struct kmem_cache *smi_gyro_cachepool;
	struct smi_gyro_sample *smi230_gyro_samplist[SMI_GYRO_MAXSAMPLE];
	int max_buffer_time;
	struct input_dev *gyrobuf_dev;
	int report_evt_cnt;
	struct mutex gyro_sensor_buff;
#endif
};

static struct smi230_dev *p_smi230_dev;

static ssize_t smi230_gyro_show_chip_id(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	uint8_t chip_id[2] = {0};
	int err = 0;

	err = smi230_gyro_get_regs(SMI230_GYRO_CHIP_ID_REG, chip_id, 2, p_smi230_dev);
	if (err) {
		PERR("falied");
		return err;
	}
	return snprintf(buf, PAGE_SIZE, "chip_id=0x%x rev_id=0x%x\n",
		chip_id[0], chip_id[1]);
}
#ifdef CONFIG_ENABLE_SMI230_ACC_GYRO_BUFFERING
static inline int smi230_check_gyro_early_buff_enable_flag(
		struct smi230_client_data *client_data)
{
	if (client_data->gyro_buffer_smi230_samples)
		return 1;
	else
		return 0;
}
static void smi230_check_gyro_enable_flag(
		struct smi230_client_data *client_data, unsigned long data)
{
	if (data == 0)
		client_data->gyro_enable = true;
	else
		client_data->gyro_enable = false;
}
#else
static inline int smi230_check_gyro_early_buff_enable_flag(
		struct smi230_client_data *client_data)
{
	return 0;
}
static void smi230_check_gyro_enable_flag(
		struct smi230_client_data *client_data, unsigned long data)
{
}
#endif

static inline void get_monotonic_boottime(struct timespec *ts)
{
	*ts = ktime_to_timespec(ktime_get_boottime());
}
uint64_t smi230_gyro_get_alarm_timestamp(void)
{
	uint64_t ts_ap;
	struct timespec tmp_time;

	get_monotonic_boottime(&tmp_time);
	ts_ap = (uint64_t)tmp_time.tv_sec * 1000000000 + tmp_time.tv_nsec;
	return ts_ap;
}

static ssize_t smi230_gyro_reg_dump(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	uint8_t data = 0;
	int err = 0;
	int i;

	for (i = 0; i <= 0x3F; i++) {
		err = smi230_gyro_get_regs(i, &data, 1, p_smi230_dev);
		if (err) {
			PERR("falied");
			return err;
		}
		printk("0x%x = 0x%x", i, data);
		if ( i % 15 == 0 )
			printk("\n");
	}

	return 0;
}

static ssize_t smi230_gyro_show_fifo_wm(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int err;
	uint8_t fifo_wm;

	err = smi230_gyro_get_fifo_wm(&fifo_wm, p_smi230_dev);
	if (err) {
		PERR("read failed");
		return err;
	}
	return snprintf(buf, PAGE_SIZE, "fifo water mark is %d\n", fifo_wm);
}

static ssize_t smi230_gyro_store_fifo_wm(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int err = 0;
	uint8_t fifo_wm;

	err = kstrtou8(buf, 10, &fifo_wm);
	err |= smi230_gyro_set_fifo_wm(fifo_wm, p_smi230_dev);

	if (err != SMI230_OK)
	{
		PERR("set fifo wm faild");
		return err;
	}

	PDEBUG("set fifo wm to %d", fifo_wm);

	return count;
}

static ssize_t smi230_gyro_store_pwr_cfg(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int err = 0;
	unsigned long pwr_cfg;
	struct smi230_client_data *client_data = dev_get_drvdata(dev);

	err = kstrtoul(buf, 10, &pwr_cfg);
	if (err)
		return err;

	smi230_check_gyro_enable_flag(client_data, pwr_cfg);

	err = smi230_check_gyro_early_buff_enable_flag(client_data);
	if (err)
		return count;

	if (pwr_cfg == 0) {
		is_gyro_ready = false;
		p_smi230_dev->gyro_cfg.power = SMI230_GYRO_PM_NORMAL;
		err = smi230_gyro_set_power_mode(p_smi230_dev);
		client_data->timestamp_old = smi230_gyro_get_alarm_timestamp();
		mod_timer(&pm_mode_timer, jiffies + msecs_to_jiffies(600));
	}
	else if (pwr_cfg == 1) {
		p_smi230_dev->gyro_cfg.power = SMI230_GYRO_PM_SUSPEND;
		err = smi230_gyro_set_power_mode(p_smi230_dev);
	}
	else if (pwr_cfg == 2) {
		p_smi230_dev->gyro_cfg.power = SMI230_GYRO_PM_DEEP_SUSPEND;
		err = smi230_gyro_set_power_mode(p_smi230_dev);
	}
	else {
		PERR("invalid param");
		return count;
	}


	PDEBUG("set power cfg to %ld, err %d", pwr_cfg, err);

	if (err) {
		PERR("setting power config failed");
		return err;
	}
	return count;
}

static ssize_t smi230_gyro_show_pwr_cfg(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int err;

	err = smi230_gyro_get_power_mode(p_smi230_dev);
	if (err) {
		PERR("read failed");
		return err;
	}
	return snprintf(buf, PAGE_SIZE, "%x (0:active 1:suspend 2:deep suspend)\n", p_smi230_dev->gyro_cfg.power);
}

static ssize_t smi230_gyro_show_value(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int err;
	struct smi230_sensor_data data = {0};

	err = smi230_gyro_get_data(&data, p_smi230_dev);
	if (err < 0)
		return err;
	return snprintf(buf, PAGE_SIZE, "%hd %hd %hd\n",
			data.x, data.y, data.z);
}

static ssize_t smi230_gyro_show_driver_version(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE,
		"Driver version: %s\n", DRIVER_VERSION);
}
#ifdef CONFIG_ENABLE_SMI230_ACC_GYRO_BUFFERING
static int smi_gyro_read_bootsampl(struct smi230_client_data *client_data,
		unsigned long enable_read)
{
	int i = 0;

	client_data->gyro_buffer_smi230_samples = false;
	if (enable_read) {
		for (i = 0; i < client_data->gyro_bufsample_cnt; i++) {
			PDEBUG("gyro=%d,x=%d,y=%d,z=%d,sec=%d,ns=%lld\n",
				i, client_data->smi230_gyro_samplist[i]->xyz[0],
				client_data->smi230_gyro_samplist[i]->xyz[1],
				client_data->smi230_gyro_samplist[i]->xyz[2],
				client_data->smi230_gyro_samplist[i]->tsec,
				client_data->smi230_gyro_samplist[i]->tnsec);
			input_report_abs(client_data->gyrobuf_dev, ABS_X,
				client_data->smi230_gyro_samplist[i]->xyz[0]);
			input_report_abs(client_data->gyrobuf_dev, ABS_Y,
				client_data->smi230_gyro_samplist[i]->xyz[1]);
			input_report_abs(client_data->gyrobuf_dev, ABS_Z,
				client_data->smi230_gyro_samplist[i]->xyz[2]);
			input_report_abs(client_data->gyrobuf_dev, ABS_RX,
				client_data->smi230_gyro_samplist[i]->tsec);
			input_report_abs(client_data->gyrobuf_dev, ABS_RY,
				client_data->smi230_gyro_samplist[i]->tnsec);
			input_sync(client_data->gyrobuf_dev);
		}
	} else {
		/* clean up */
		if (client_data->gyro_bufsample_cnt != 0) {
			for (i = 0; i < SMI_GYRO_MAXSAMPLE; i++)
				kmem_cache_free(client_data->smi_gyro_cachepool,
				client_data->smi230_gyro_samplist[i]);
			kmem_cache_destroy(client_data->smi_gyro_cachepool);
			client_data->gyro_bufsample_cnt = 0;
		}
	}
	/*SYN_CONFIG indicates end of data*/
	input_event(client_data->gyrobuf_dev, EV_SYN, SYN_CONFIG, 0xFFFFFFFF);
	input_sync(client_data->gyrobuf_dev);
	PDEBUG("End of gyro samples bufsample_cnt=%d\n",
			client_data->gyro_bufsample_cnt);
	return 0;
}
static ssize_t read_gyro_boot_sample_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct smi230_client_data *client_data = dev_get_drvdata(dev);

	return scnprintf(buf, 16, "%u\n",
			client_data->read_gyro_boot_sample);
}
static ssize_t read_gyro_boot_sample_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	struct smi230_client_data *client_data = dev_get_drvdata(dev);
	unsigned long enable = 0;

	err = kstrtoul(buf, 10, &enable);
	if (err)
		return err;
	if (enable > 1) {
		PERR("Invalid value of input, input=%ld\n", enable);
		return -EINVAL;
	}
	mutex_lock(&client_data->gyro_sensor_buff);
	err = smi_gyro_read_bootsampl(client_data, enable);
	mutex_unlock(&client_data->gyro_sensor_buff);
	if (err)
		return err;
	client_data->read_gyro_boot_sample = enable;

	return count;
}
#endif

static ssize_t smi230_gyro_show_bw_odr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int err;

        err = smi230_gyro_get_meas_conf(p_smi230_dev);
	if (err) {
		PERR("read ODR failed");
		return err;
	}

	switch(p_smi230_dev->gyro_cfg.odr) {
	case SMI230_GYRO_BW_523_ODR_2000_HZ:
		return snprintf(buf, PAGE_SIZE, "%s\n", "BW:523 ODR:2000");
	case SMI230_GYRO_BW_230_ODR_2000_HZ:
		return snprintf(buf, PAGE_SIZE, "%s\n", "BW:230 ODR:2000");
	case SMI230_GYRO_BW_116_ODR_1000_HZ:
		return snprintf(buf, PAGE_SIZE, "%s\n", "BW:116 ODR:1000");
	case SMI230_GYRO_BW_47_ODR_400_HZ:
		return snprintf(buf, PAGE_SIZE, "%s\n", "BW:47 ODR:400");
	case SMI230_GYRO_BW_23_ODR_200_HZ:
		return snprintf(buf, PAGE_SIZE, "%s\n", "BW:23 ODR:200");
	case SMI230_GYRO_BW_12_ODR_100_HZ:
		return snprintf(buf, PAGE_SIZE, "%s\n", "BW:12 ODR:100");
	case SMI230_GYRO_BW_64_ODR_200_HZ:
		return snprintf(buf, PAGE_SIZE, "%s\n", "BW:64 ODR:200");
	case SMI230_GYRO_BW_32_ODR_100_HZ:
		return snprintf(buf, PAGE_SIZE, "%s\n", "BW:32 ODR:100");
	default:
		return snprintf(buf, PAGE_SIZE, "%s\n", "error");
	}
}

static ssize_t smi230_gyro_store_bw_odr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int err = 0, bw, odr;

	err = kstrtoint(buf, 10, &bw);
	if (err) {
		PERR("invalid params");
		return err;
	}

	switch(bw) {
	case 523:
		p_smi230_dev->gyro_cfg.odr = SMI230_GYRO_BW_523_ODR_2000_HZ;
		odr = 2000;
		break;
	case 230:
		p_smi230_dev->gyro_cfg.odr = SMI230_GYRO_BW_230_ODR_2000_HZ;
		odr = 2000;
		break;
	case 116:
		p_smi230_dev->gyro_cfg.odr = SMI230_GYRO_BW_116_ODR_1000_HZ;
		odr = 1000;
		break;
	case 47:
		p_smi230_dev->gyro_cfg.odr = SMI230_GYRO_BW_47_ODR_400_HZ;
		odr = 400;
		break;
	case 23:
		p_smi230_dev->gyro_cfg.odr = SMI230_GYRO_BW_23_ODR_200_HZ;
		odr = 200;
		break;
	case 12:
		p_smi230_dev->gyro_cfg.odr = SMI230_GYRO_BW_12_ODR_100_HZ;
		odr = 100;
		break;
	case 64:
		p_smi230_dev->gyro_cfg.odr = SMI230_GYRO_BW_64_ODR_200_HZ;
		odr = 200;
		break;
	case 32:
		p_smi230_dev->gyro_cfg.odr = SMI230_GYRO_BW_32_ODR_100_HZ;
		odr = 100;
		break;
	default:
		PERR("ODR not supported");
		return count;
	}

        err |= smi230_gyro_set_meas_conf(p_smi230_dev);

	PDEBUG("set bw to %d, odr to %d, err %d", bw, odr, err);

	if (err) {
		PERR("setting ODR failed");
		return err;
	}
	return count;
}

static ssize_t smi230_gyro_show_range(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int err, range = 0;

        err = smi230_gyro_get_meas_conf(p_smi230_dev);
	if (err) {
		PERR("read range failed");
		return err;
	}

	switch(p_smi230_dev->gyro_cfg.range) {
	case SMI230_GYRO_RANGE_2000_DPS:
		range = 2000;
		break;
	case SMI230_GYRO_RANGE_1000_DPS:
		range = 1000;
		break;
	case SMI230_GYRO_RANGE_500_DPS:
		range = 500;
		break;
	case SMI230_GYRO_RANGE_250_DPS:
		range = 250;
		break;
	case SMI230_GYRO_RANGE_125_DPS:
		range = 125;
		break;
	default:
		PERR("wrong range read");
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", range);
}

static ssize_t smi230_gyro_store_range(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int err = 0, range;

	err = kstrtoint(buf, 10, &range);
	if (err) {
		PERR("invalid params");
		return err;
	}

	switch(range) {
	case 2000:
		p_smi230_dev->gyro_cfg.range = SMI230_GYRO_RANGE_2000_DPS;
		break;
	case 1000:
		p_smi230_dev->gyro_cfg.range = SMI230_GYRO_RANGE_1000_DPS;
		break;
	case 500:
		p_smi230_dev->gyro_cfg.range = SMI230_GYRO_RANGE_500_DPS;
		break;
	case 250:
		p_smi230_dev->gyro_cfg.range = SMI230_GYRO_RANGE_250_DPS;
		break;
	case 125:
		p_smi230_dev->gyro_cfg.range = SMI230_GYRO_RANGE_125_DPS;
		break;
	default:
		PERR("range not supported");
		return count;
	}

        err |= smi230_gyro_set_meas_conf(p_smi230_dev);

	PDEBUG("set range to %d, err %d", range, err);

	if (err) {
		PERR("setting range failed");
		return err;
	}
	return count;
}

static DEVICE_ATTR(chip_id, S_IRUGO,
	smi230_gyro_show_chip_id, NULL);
static DEVICE_ATTR(regs_dump, S_IRUGO,
	smi230_gyro_reg_dump, NULL);
static DEVICE_ATTR(fifo_wm, S_IRUGO|S_IWUSR|S_IWGRP,
	smi230_gyro_show_fifo_wm, smi230_gyro_store_fifo_wm);
static DEVICE_ATTR(pwr_cfg, S_IRUGO|S_IWUSR|S_IWGRP,
	smi230_gyro_show_pwr_cfg, smi230_gyro_store_pwr_cfg);
static DEVICE_ATTR(bw_odr, S_IRUGO|S_IWUSR|S_IWGRP,
	smi230_gyro_show_bw_odr, smi230_gyro_store_bw_odr);
static DEVICE_ATTR(range, S_IRUGO|S_IWUSR|S_IWGRP,
	smi230_gyro_show_range, smi230_gyro_store_range);
static DEVICE_ATTR(gyro_value, S_IRUGO,
	smi230_gyro_show_value, NULL);
static DEVICE_ATTR(driver_version, S_IRUGO,
	smi230_gyro_show_driver_version, NULL);
#ifdef CONFIG_ENABLE_SMI230_ACC_GYRO_BUFFERING
static DEVICE_ATTR_RW(read_gyro_boot_sample);
#endif
static struct attribute *smi230_attributes[] = {
	&dev_attr_chip_id.attr,
	&dev_attr_regs_dump.attr,
	&dev_attr_fifo_wm.attr,
	&dev_attr_pwr_cfg.attr,
	&dev_attr_bw_odr.attr,
	&dev_attr_range.attr,
	&dev_attr_gyro_value.attr,
	&dev_attr_driver_version.attr,
#ifdef CONFIG_ENABLE_SMI230_ACC_GYRO_BUFFERING
	&dev_attr_read_gyro_boot_sample.attr,
#endif
	NULL
};

static struct attribute_group smi230_attribute_group = {
	.attrs = smi230_attributes
};

static int smi230_input_init(struct smi230_client_data *client_data)
{
	int err = 0;
	struct input_dev *dev = input_allocate_device();

	if (dev == NULL)
		return -ENOMEM;

	dev->id.bustype = BUS_I2C;
	dev->name = SENSOR_GYRO_NAME;
	//dev_set_name(&dev->dev, SENSOR_GYRO_NAME);
	input_set_drvdata(dev, client_data);
	client_data->input = dev;

	input_set_capability(dev, EV_MSC, MSC_RAW);
	input_set_capability(dev, EV_MSC, MSC_GESTURE);
	input_set_capability(dev, EV_MSC, MSC_TIMESTAMP);
	input_set_abs_params(dev, ABS_X, SMI230_MIN_VALUE, SMI230_MAX_VALUE, 0, 0);
	input_set_abs_params(dev, ABS_Y, SMI230_MIN_VALUE, SMI230_MAX_VALUE, 0, 0);
	input_set_abs_params(dev, ABS_Z, SMI230_MIN_VALUE, SMI230_MAX_VALUE, 0, 0);

	err = input_register_device(dev);
	if (err)
		input_free_device(dev);
	return err;
}

#ifdef CONFIG_ENABLE_SMI230_ACC_GYRO_BUFFERING
static void store_gyro_boot_sample(struct smi230_client_data *client_data,
		int x, int y, int z, struct timespec ts)
{
	int err = 0;

	if (!client_data->gyro_buffer_smi230_samples)
		return;
	mutex_lock(&client_data->gyro_sensor_buff);
	if (ts.tv_sec <  client_data->max_buffer_time) {
		if (client_data->gyro_bufsample_cnt < SMI_GYRO_MAXSAMPLE) {
			client_data->smi230_gyro_samplist[client_data
				->gyro_bufsample_cnt]->xyz[0] = x;
			client_data->smi230_gyro_samplist[client_data
				->gyro_bufsample_cnt]->xyz[1] = y;
			client_data->smi230_gyro_samplist[client_data
				->gyro_bufsample_cnt]->xyz[2] = z;
			client_data->smi230_gyro_samplist[client_data
				->gyro_bufsample_cnt]->tsec = ts.tv_sec;
			client_data->smi230_gyro_samplist[client_data
				->gyro_bufsample_cnt]->tnsec = ts.tv_nsec;
			client_data->gyro_bufsample_cnt++;
		}
	} else {
		PINFO("End of GYRO buffering %d",
				client_data->gyro_bufsample_cnt);
		client_data->gyro_buffer_smi230_samples = false;
		if (!client_data->gyro_enable) {
			p_smi230_dev->gyro_cfg.power = SMI230_GYRO_PM_SUSPEND;
			err |= smi230_gyro_set_power_mode(p_smi230_dev);
			if (err != SMI230_OK)
				PERR("set power mode failed");
		}
	}
	mutex_unlock(&client_data->gyro_sensor_buff);
}
#else
static void store_gyro_boot_sample(struct smi230_client_data *client_data,
		int x, int y, int z, struct timespec ts)
{
}
#endif

#ifdef CONFIG_ENABLE_SMI230_ACC_GYRO_BUFFERING
static int smi230_gyro_early_buff_init(struct smi230_client_data *client_data)
{
	int i, err;

	client_data->gyro_bufsample_cnt = 0;
	client_data->report_evt_cnt = 5;
	client_data->max_buffer_time = 40;

	client_data->smi_gyro_cachepool = kmem_cache_create("gyro_sensor_sample"
			, sizeof(struct smi_gyro_sample), 0,
			SLAB_HWCACHE_ALIGN, NULL);
	if (!client_data->smi_gyro_cachepool) {
		PERR("smi_gyro_cachepool cache create failed\n");
		err = -ENOMEM;
		return err;
	}

	for (i = 0; i < SMI_GYRO_MAXSAMPLE; i++) {
		client_data->smi230_gyro_samplist[i] =
			kmem_cache_alloc(client_data->smi_gyro_cachepool,
					GFP_KERNEL);
		if (!client_data->smi230_gyro_samplist[i]) {
			err = -ENOMEM;
			goto clean_exit1;
		}
	}

	client_data->gyrobuf_dev = input_allocate_device();
	if (!client_data->gyrobuf_dev) {
		err = -ENOMEM;
		PERR("input device allocation failed\n");
		goto clean_exit1;
	}
	client_data->gyrobuf_dev->name = "smi230_gyrobuf";
	client_data->gyrobuf_dev->id.bustype = BUS_I2C;
	input_set_events_per_packet(client_data->gyrobuf_dev,
			client_data->report_evt_cnt * SMI_GYRO_MAXSAMPLE);
	set_bit(EV_ABS, client_data->gyrobuf_dev->evbit);
	input_set_abs_params(client_data->gyrobuf_dev, ABS_X,
			-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(client_data->gyrobuf_dev, ABS_Y,
			-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(client_data->gyrobuf_dev, ABS_Z,
			-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(client_data->gyrobuf_dev, ABS_RX,
			-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(client_data->gyrobuf_dev, ABS_RY,
			-G_MAX, G_MAX, 0, 0);
	err = input_register_device(client_data->gyrobuf_dev);
	if (err) {
		PERR("unable to register input device %s\n",
				client_data->gyrobuf_dev->name);
		goto clean_exit2;
	}

	client_data->gyro_buffer_smi230_samples = true;
	client_data->gyro_enable = false;

	mutex_init(&client_data->gyro_sensor_buff);

	p_smi230_dev->gyro_cfg.odr = SMI230_GYRO_BW_32_ODR_100_HZ;
	p_smi230_dev->gyro_cfg.range = SMI230_GYRO_RANGE_125_DPS;
	err |= smi230_gyro_set_meas_conf(p_smi230_dev);
	smi230_delay(100);

	/* gyro driver should be initialized before acc */
	p_smi230_dev->gyro_cfg.power = SMI230_GYRO_PM_NORMAL;
	smi230_gyro_set_power_mode(p_smi230_dev);
	client_data->timestamp_old = smi230_gyro_get_alarm_timestamp();
	is_gyro_ready = false;
	mod_timer(&pm_mode_timer, jiffies + msecs_to_jiffies(600));

	return 1;
clean_exit2:
	input_free_device(client_data->gyrobuf_dev);
clean_exit1:
	for (i = 0; i < SMI_GYRO_MAXSAMPLE; i++)
		kmem_cache_free(client_data->smi_gyro_cachepool,
				client_data->smi230_gyro_samplist[i]);
	kmem_cache_destroy(client_data->smi_gyro_cachepool);
	return err;
}

static void smi230_gyro_input_cleanup(struct smi230_client_data *client_data)
{
	int i = 0;

	input_unregister_device(client_data->gyrobuf_dev);
	input_free_device(client_data->gyrobuf_dev);
	for (i = 0; i < SMI_GYRO_MAXSAMPLE; i++)
		kmem_cache_free(client_data->smi_gyro_cachepool,
				client_data->smi230_gyro_samplist[i]);
	kmem_cache_destroy(client_data->smi_gyro_cachepool);
}

#else
static int smi230_gyro_early_buff_init(struct smi230_client_data *client_data)
{
	return 1;
}
static void smi230_gyro_input_cleanup(struct smi230_client_data *client_data)
{
}
#endif

#ifdef CONFIG_SMI230_RAW_DATA
static void smi230_gyro_raw_data_ready_handle(
	struct smi230_client_data *client_data)
{
	struct smi230_sensor_data gyro_data;
	int err = 0;
	struct timespec ts;

	ts = ns_to_timespec(client_data->timestamp);

	err = smi230_gyro_get_data(&gyro_data, p_smi230_dev);
	if (err != SMI230_OK)
		return;
	if (!is_gyro_ready) {
		PINFO("gyro not ready, discard data of first 200ms period after active");
		return;
	}

	input_event(client_data->input, EV_MSC, MSC_TIMESTAMP, ts.tv_sec);
	input_event(client_data->input, EV_MSC, MSC_TIMESTAMP, ts.tv_nsec);
	input_event(client_data->input, EV_MSC, MSC_GESTURE, (int)gyro_data.x);
	input_event(client_data->input, EV_MSC, MSC_GESTURE, (int)gyro_data.y);
	input_event(client_data->input, EV_MSC, MSC_GESTURE, (int)gyro_data.z);

	input_sync(client_data->input);
	store_gyro_boot_sample(client_data, gyro_data.x,
			gyro_data.y, gyro_data.z, ts);
}
#endif

#ifndef CONFIG_SMI230_DATA_SYNC
#ifdef CONFIG_SMI230_GYRO_FIFO
static struct smi230_sensor_data fifo_gyro_data[SMI230_MAX_GYRO_FIFO_FRAME];

static void smi230_gyro_fifo_handle(
	struct smi230_client_data *client_data)
{
	struct smi230_fifo_frame fifo;
	int err = 0, i;
	uint8_t fifo_length, extract_length;
	uint16_t step = 1;
	struct timespec ts;
	uint64_t data_ts = 0;

	err = smi230_gyro_get_fifo_length(&fifo_length, p_smi230_dev);
	if (err != SMI230_OK) {
		PERR("FIFO get length error!");
		return;
	}

#if 0
	PINFO("GYRO FIFO length %d", fifo_length);
#endif

	fifo.data = fifo_buf;
	fifo.length = fifo_length;
	err = smi230_gyro_read_fifo_data(&fifo, p_smi230_dev);
	if (err != SMI230_OK) {
		PERR("FIFO read data error %d", err);
		return;
	}

#if 0
	/* this event shall never be mixed with sensor data  */
	/* this event here is to indicate IRQ timing if needed */
	input_event(client_data->input, EV_MSC, MSC_RAW, (int)fifo.length);
	input_sync(client_data->input);
#endif

	extract_length = SMI230_MAX_GYRO_FIFO_FRAME;
	err = smi230_gyro_extract_fifo(fifo_gyro_data,
                            &extract_length,
                            &fifo,
                            p_smi230_dev);

	if (!is_gyro_ready) {
		PINFO("gyro not ready, discard data of first 200ms period after active");
		client_data->timestamp_old = client_data->timestamp;
		return;
	}
	for (i = 0; i < extract_length; i++) {
		data_ts = client_data->timestamp_old +
			step * div_s64((client_data->timestamp - client_data->timestamp_old),
					extract_length);
		step++;
		ts = ns_to_timespec(data_ts);
		input_event(client_data->input, EV_MSC, MSC_TIMESTAMP, ts.tv_sec);
		input_event(client_data->input, EV_MSC, MSC_TIMESTAMP, ts.tv_nsec);
		input_event(client_data->input, EV_MSC, MSC_GESTURE, (int)fifo_gyro_data[i].x);
		input_event(client_data->input, EV_MSC, MSC_GESTURE, (int)fifo_gyro_data[i].y);
		input_event(client_data->input, EV_MSC, MSC_GESTURE, (int)fifo_gyro_data[i].z);
		input_sync(client_data->input);
		store_gyro_boot_sample(client_data, fifo_gyro_data[i].x,
				fifo_gyro_data[i].y, fifo_gyro_data[i].z, ts);
	}
	client_data->timestamp_old = client_data->timestamp;
}

#else /* new data */

static void smi230_new_data_ready_handle(
	struct smi230_client_data *client_data)
{
	struct smi230_sensor_data gyro_data;
	int err = 0;
	struct timespec ts;
	ts = ns_to_timespec(client_data->timestamp);

	err = smi230_gyro_get_data(&gyro_data, p_smi230_dev);
	if (err != SMI230_OK)
		return;

	input_event(client_data->input, EV_MSC, MSC_TIMESTAMP, ts.tv_sec);
	input_event(client_data->input, EV_MSC, MSC_TIMESTAMP, ts.tv_nsec);
	input_event(client_data->input, EV_MSC, MSC_GESTURE, (int)gyro_data.x);
	input_event(client_data->input, EV_MSC, MSC_GESTURE, (int)gyro_data.y);
	input_event(client_data->input, EV_MSC, MSC_GESTURE, (int)gyro_data.z);

	input_sync(client_data->input);
}
#endif
#endif
static irqreturn_t smi230_irq_work_func(int irq, void *handle)
{
	struct smi230_client_data *client_data = handle;
#ifndef CONFIG_SMI230_DATA_SYNC
	//struct smi230_client_data *client_data =
		//container_of(work, struct smi230_client_data, irq_work);
#ifdef CONFIG_SMI230_GYRO_FIFO
	smi230_gyro_fifo_handle(client_data);
#else
	smi230_new_data_ready_handle(client_data);
#endif

#endif
	return IRQ_HANDLED;
}

static irqreturn_t smi230_irq_handle(int irq, void *handle)
{
	struct smi230_client_data *client_data = handle;

	client_data->timestamp = smi230_gyro_get_alarm_timestamp();

	return IRQ_WAKE_THREAD;
}

static void smi230_free_irq(struct smi230_client_data *client_data)
{
	free_irq(client_data->IRQ, client_data);
	gpio_free(client_data->gpio_pin);
}

static int smi230_request_irq(struct smi230_client_data *client_data, struct smi230_dev *smi230_dev)
{
	int err = 0;
	client_data->IRQ = smi230_dev->irq;
	err = request_threaded_irq(client_data->IRQ, smi230_irq_handle, smi230_irq_work_func,
			IRQF_TRIGGER_RISING,
			SENSOR_GYRO_NAME, client_data);
	if (err < 0) {
		PDEBUG("request_irq\n");
		return err;
	}
	return err;
}

static void smi230_input_destroy(struct smi230_client_data *client_data)
{
	struct input_dev *dev = client_data->input;
	input_unregister_device(dev);
	/* to avoid underflow of refcount, do a checck before call free device*/
	if (dev->devres_managed)
		input_free_device(dev);
}

static void pm_mode_callback(struct timer_list *t)
{
	is_gyro_ready = true;
}

int smi230_gyro_remove(struct device *dev)
{
	int err = 0;
	struct smi230_client_data *client_data = dev_get_drvdata(dev);

	if (NULL != client_data) {
		smi230_gyro_input_cleanup(client_data);
		smi230_free_irq(client_data);
		sysfs_remove_group(&client_data->input->dev.kobj,
				&smi230_attribute_group);
		smi230_input_destroy(client_data);
		kfree(client_data);
		del_timer(&pm_mode_timer);
	}
	return err;
}

int smi230_gyro_probe(struct device *dev, struct smi230_dev *smi230_dev)
{
	int err = 0;
	struct smi230_client_data *client_data = NULL;
#ifdef CONFIG_SMI230_GYRO_FIFO
	struct gyro_fifo_config fifo_config;
#endif
	struct smi230_int_cfg int_config;

	if (dev == NULL || smi230_dev == NULL)
		return -EINVAL;

	p_smi230_dev = smi230_dev;

	client_data = kzalloc(sizeof(struct smi230_client_data),
						GFP_KERNEL);
	if (NULL == client_data) {
		PERR("no memory available");
		err = -ENOMEM;
		goto exit_directly;
	}

	client_data->dev = dev;

	/* gyro driver should be initialized before acc */
	p_smi230_dev->gyro_cfg.power = SMI230_GYRO_PM_NORMAL;
	err |= smi230_gyro_set_power_mode(p_smi230_dev);

#ifdef CONFIG_SMI230_GYRO_INT3
	/*enable gyro fifo int on channel 3 */
	int_config.gyro_int_config_1.int_pin_cfg.enable_int_pin = SMI230_ENABLE;

	/*disable gyro int on channel 4 */
	int_config.gyro_int_config_2.int_pin_cfg.enable_int_pin = SMI230_DISABLE;
#endif

#ifdef CONFIG_SMI230_GYRO_INT4
	/*disable gyro int on channel 3 */
	int_config.gyro_int_config_1.int_pin_cfg.enable_int_pin = SMI230_DISABLE;

	/*enable gyro fifo int on channel 4 */
	int_config.gyro_int_config_2.int_pin_cfg.enable_int_pin = SMI230_ENABLE;
#endif

	p_smi230_dev->gyro_cfg.odr = SMI230_GYRO_BW_32_ODR_100_HZ;
	p_smi230_dev->gyro_cfg.range = SMI230_GYRO_RANGE_125_DPS;
        err |= smi230_gyro_set_meas_conf(p_smi230_dev);
	smi230_delay(100);


#ifdef CONFIG_SMI230_GYRO_FIFO
	PINFO("GYRO FIFO is enabled");

	int_config.gyro_int_config_1.int_channel = SMI230_INT_CHANNEL_3;
	int_config.gyro_int_config_1.int_pin_cfg.lvl = SMI230_INT_ACTIVE_HIGH;
	int_config.gyro_int_config_1.int_pin_cfg.output_mode = SMI230_INT_MODE_PUSH_PULL;

	int_config.gyro_int_config_2.int_channel = SMI230_INT_CHANNEL_4;
	int_config.gyro_int_config_2.int_pin_cfg.lvl = SMI230_INT_ACTIVE_HIGH;
	int_config.gyro_int_config_2.int_pin_cfg.output_mode = SMI230_INT_MODE_PUSH_PULL;

	int_config.gyro_int_config_1.int_type = SMI230_GYRO_FIFO_INT;
	int_config.gyro_int_config_2.int_type = SMI230_GYRO_FIFO_INT;

#ifdef CONFIG_SMI230_GYRO_INT3
	PINFO("GYRO FIFO set int3 config");
	err |= smi230_gyro_set_int_config(&int_config.gyro_int_config_1, p_smi230_dev);
#endif
#ifdef CONFIG_SMI230_GYRO_INT4
	PINFO("GYRO FIFO set int4 config");
	err |= smi230_gyro_set_int_config(&int_config.gyro_int_config_2, p_smi230_dev);
#endif

	p_smi230_dev->gyro_cfg.odr = SMI230_GYRO_BW_32_ODR_100_HZ;
	p_smi230_dev->gyro_cfg.range = SMI230_GYRO_RANGE_125_DPS;
        err |= smi230_gyro_set_meas_conf(p_smi230_dev);
	smi230_delay(100);

	fifo_config.mode = SMI230_GYRO_FIFO_MODE;
#ifdef CONFIG_SMI230_GYRO_FIFO_WM
	/* 0x88 to enable wm int, 0x80 to disable */
	fifo_config.wm_en = 0x88;

	PINFO("GYRO FIFO set water mark");
	err |= smi230_gyro_set_fifo_wm(60, p_smi230_dev);
#endif
#ifdef CONFIG_SMI230_GYRO_FIFO_FULL
	PINFO("GYRO FIFO full enabled");
	fifo_config.wm_en = 0x80;
#endif

	/* disable external event sync on both int3 and int 4 */
	fifo_config.int3_en = 0;
	fifo_config.int4_en = 0;

	PINFO("GYRO FIFO set fifo config");
	err |= smi230_gyro_set_fifo_config(&fifo_config, p_smi230_dev);

	if (err != SMI230_OK)
	{
		PERR("FIFO HW init failed");
		goto exit_free_client_data;
	}

	smi230_delay(100);

#else /* new data */

	PINFO("GYRO new data is enabled");

	/*disable gyro int on channel 3 */
	int_config.gyro_int_config_1.int_channel = SMI230_INT_CHANNEL_3;
	int_config.gyro_int_config_1.int_type = SMI230_GYRO_DATA_RDY_INT;
	int_config.gyro_int_config_1.int_pin_cfg.lvl = SMI230_INT_ACTIVE_HIGH;
	int_config.gyro_int_config_1.int_pin_cfg.output_mode = SMI230_INT_MODE_PUSH_PULL;

	/*enable gyro fifo int on channel 4 */
	int_config.gyro_int_config_2.int_channel = SMI230_INT_CHANNEL_4;
	int_config.gyro_int_config_2.int_type = SMI230_GYRO_DATA_RDY_INT;
	int_config.gyro_int_config_2.int_pin_cfg.lvl = SMI230_INT_ACTIVE_HIGH;
	int_config.gyro_int_config_2.int_pin_cfg.output_mode = SMI230_INT_MODE_PUSH_PULL;

	PINFO("GYRO FIFO set int3 config");
	err |= smi230_gyro_set_int_config(&int_config.gyro_int_config_1, p_smi230_dev);
	PINFO("GYRO FIFO set int4 config");
	err |= smi230_gyro_set_int_config(&int_config.gyro_int_config_2, p_smi230_dev);

	if (err != SMI230_OK)
	{
		PERR("FIFO HW init failed");
		goto exit_free_client_data;
	}

	smi230_delay(100);
#endif

#ifndef CONFIG_SMI230_DATA_SYNC
#ifndef CONFIG_SMI230_DEBUG
	/*if not for the convenience of debuging, sensors should be disabled at startup*/
	p_smi230_dev->gyro_cfg.power = SMI230_GYRO_PM_SUSPEND;
	err |= smi230_gyro_set_power_mode(p_smi230_dev);
	if (err != SMI230_OK) {
		PERR("set power mode failed");
		goto exit_free_client_data;
	}
#endif
#endif

	/*input device init */
	err = smi230_input_init(client_data);
	if (err < 0) {
		PERR("input init failed");
		goto exit_free_client_data;
	}

	/* sysfs node creation */
	err = sysfs_create_group(&client_data->input->dev.kobj,
			&smi230_attribute_group);
	if (err < 0) {
		PERR("sysfs create failed");
		goto exit_cleanup_input;
	}

	/*request irq and config*/
	err = smi230_request_irq(client_data, smi230_dev);
	if (err < 0) {
		PERR("Request irq failed");
		goto exit_cleanup_sysfs;
	}

	timer_setup(&pm_mode_timer, pm_mode_callback, 0);

	err = smi230_gyro_early_buff_init(client_data);
	if (err != 1)
		return err;

	PINFO("Sensor %s was probed successfully", SENSOR_GYRO_NAME);

	return 0;
exit_cleanup_sysfs:
	sysfs_remove_group(&client_data->input->dev.kobj,
			&smi230_attribute_group);
exit_cleanup_input:
	smi230_input_destroy(client_data);
exit_free_client_data:
	if (client_data != NULL)
		kfree(client_data);
exit_directly:
	return err;
}
