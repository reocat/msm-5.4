/*
 * drivers/staging/iio/accel/mma7455l_core.c
 *
 * Copyright (C) 2004-2012, Ambarella, Inc.
 *	Zhenwu Xue <zwxue@ambarella.com>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/spi/spi.h>
#include <linux/slab.h>

#include <linux/sysfs.h>
#include <linux/list.h>
#include <linux/delay.h>

#include <mach/board.h>

#include "../iio.h"
#include "../sysfs.h"
#include "../ring_generic.h"
#include "../ring_sw.h"

#include "accel.h"

#include "mma7455l.h"

static int mma7455l_read_raw_8bit(struct spi_device *spidev, u8 addr, u8 *data)
{
	addr <<= 1;

	return spi_write_then_read(spidev, &addr, 1, data, 1);
}

static int mma7455l_write_raw_8bit(struct spi_device *spidev, u8 addr, u8 data)
{
	u8	tmp[2];

	tmp[0] = 0x8000 | (addr << 1);
	tmp[1] = data;

	return spi_write_then_read(spidev, tmp, 2, NULL, 0);
}

static int mma7455l_read_raw_16bit(struct spi_device *spidev, u8 addr, u16 *data)
{
	int	ret;
	u8	h, l;

	ret = mma7455l_read_raw_8bit(spidev, addr, &l);
	if (ret) {
		printk("MMA7455L Error: %s %d\n", __func__, __LINE__);
		goto mma7455l_read_raw_16bit_exit;
	}

	ret = mma7455l_read_raw_8bit(spidev, addr + 1, &h);
	if (ret) {
		printk("MMA7455L Error: %s %d\n", __func__, __LINE__);
		goto mma7455l_read_raw_16bit_exit;
	}

	*data = (h << 8) | l;

mma7455l_read_raw_16bit_exit:
	return ret;
}

static int mma7455l_write_raw_16bit(struct spi_device *spidev, u8 addr, u16 data)
{
	int	ret;
	u8	h, l;

	h = data >> 8;
	l = data & 0xff;

	ret = mma7455l_write_raw_8bit(spidev, addr, l);
	if (ret) {
		printk("MMA7455L Error: %s %d\n", __func__, __LINE__);
		goto mma7455l_read_raw_16bit_exit;
	}

	ret = mma7455l_write_raw_8bit(spidev, addr + 1, h);
	if (ret) {
		printk("MMA7455L Error: %s %d\n", __func__, __LINE__);
		goto mma7455l_read_raw_16bit_exit;
	}

mma7455l_read_raw_16bit_exit:
	return ret;
}

static ssize_t mma7455l_read_offset_drift(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	int				ret;
	struct iio_dev_attr		*this_attr = to_iio_dev_attr(attr);
	struct iio_dev			*indio_dev = dev_get_drvdata(dev);
	struct iio_sw_ring_helper_state	*h = iio_dev_get_devdata(indio_dev);
	struct mma7455l_state		*priv = mma7455l_h_to_s(h);
	u16				val;

	ret = mma7455l_read_raw_16bit(priv->spidev, this_attr->address, &val);
	if (ret) {
		printk("MMA7455L Error: %s %d\n", __func__, __LINE__);
		goto mma7455l_read_offset_drift_exit;
	}

	val &= 0x07ff;
	if (val & 0x0400) {
		/* Negative */
		ret = sprintf(buf, "-%d\n", 0x0800 - val);
	} else {
		/* Positive */
		ret = sprintf(buf, "%d\n", val);
	}

mma7455l_read_offset_drift_exit:
	return ret;
}

static ssize_t mma7455l_write_offset_drift(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t len)
{
	int				ret;
	struct iio_dev_attr		*this_attr = to_iio_dev_attr(attr);
	struct iio_dev			*indio_dev = dev_get_drvdata(dev);
	struct iio_sw_ring_helper_state	*h = iio_dev_get_devdata(indio_dev);
	struct mma7455l_state		*priv = mma7455l_h_to_s(h);
	long int			val1;
	u16				val2;

	ret = strict_strtol(buf, 10, &val1);
	if (ret) {
		printk("MMA7455L Error: %s %d\n", __func__, __LINE__);
		goto mma7455l_write_offset_drift_exit;
	}
	val2 = val1 & 0x07ff;

	ret = mma7455l_write_raw_16bit(priv->spidev, this_attr->address, val2);
	if (ret) {
		printk("MMA7455L Error: %s %d\n", __func__, __LINE__);
		goto mma7455l_write_offset_drift_exit;
	}

mma7455l_write_offset_drift_exit:
	return ret;
}

static ssize_t mma7455l_read_accel_from_device(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	int				ret;
	struct iio_dev_attr		*this_attr = to_iio_dev_attr(attr);
	struct iio_dev			*indio_dev = dev_get_drvdata(dev);
	struct iio_sw_ring_helper_state	*h = iio_dev_get_devdata(indio_dev);
	struct mma7455l_state		*priv = mma7455l_h_to_s(h);
	u8				val;

	ret = mma7455l_read_raw_8bit(priv->spidev, this_attr->address, &val);
	if (ret) {
		printk("MMA7455L Error: %s %d\n", __func__, __LINE__);
		goto mma7455l_read_offset_drift_exit;
	}

	if (val & 0x80) {
		/* Negative */
		ret = sprintf(buf, "-%d\n", 0x100 - val);
	} else {
		/* Positive */
		ret = sprintf(buf, "%d\n", val);
	}

mma7455l_read_offset_drift_exit:
	return ret;
}

/* At the moment the spi framework doesn't allow global setting of cs_change.
 * It's in the likely to be added comment at the top of spi.h.
 * This means that use cannot be made of spi_write etc.
 */

/**
 * mma7455l_spi_read_reg_8() - read single byte from a single register
 * @dev: device asosciated with child of actual device (iio_dev or iio_trig)
 * @reg_address: the address of the register to be read
 * @val: pass back the resulting value
 **/
int mma7455l_spi_read_reg_8(struct device *dev, u8 reg_address, u8 *val)
{
	int ret;
	struct spi_message msg;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct iio_sw_ring_helper_state *h = iio_dev_get_devdata(indio_dev);
	struct mma7455l_state *priv = mma7455l_h_to_s(h);

	struct spi_transfer xfer = {
		.tx_buf = priv->tx,
		.rx_buf = priv->rx,
		.bits_per_word = 8,
		.len = 2,
		.cs_change = 1,
	};

	mutex_lock(&priv->buf_lock);
	priv->tx[0] = MMA7455L_READ_REG(reg_address);
	priv->tx[1] = 0;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);
	ret = spi_sync(priv->spidev, &msg);
	*val = priv->rx[1];
	mutex_unlock(&priv->buf_lock);

	return ret;
}

/**
 * mma7455l_spi_write_reg_8() - write single byte to a register
 * @dev: device associated with child of actual device (iio_dev or iio_trig)
 * @reg_address: the address of the register to be writen
 * @val: the value to write
 **/
int mma7455l_spi_write_reg_8(struct device *dev,
			      u8 reg_address,
			      u8 *val)
{
	int ret;
	struct spi_message msg;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct iio_sw_ring_helper_state *h
		= iio_dev_get_devdata(indio_dev);
	struct mma7455l_state *priv = mma7455l_h_to_s(h);
	struct spi_transfer xfer = {
		.tx_buf = priv->tx,
		.bits_per_word = 8,
		.len = 2,
		.cs_change = 1,
	};

	mutex_lock(&priv->buf_lock);
	priv->tx[0] = MMA7455L_WRITE_REG(reg_address);
	priv->tx[1] = *val;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);
	ret = spi_sync(priv->spidev, &msg);
	mutex_unlock(&priv->buf_lock);

	//spi_write();

	return ret;
}

/**
 * lisl302dq_spi_write_reg_s16() - write 2 bytes to a pair of registers
 * @dev: device associated with child of actual device (iio_dev or iio_trig)
 * @reg_address: the address of the lower of the two registers. Second register
 *               is assumed to have address one greater.
 * @val: value to be written
 **/
static int mma7455l_spi_write_reg_s16(struct device *dev,
				       u8 lower_reg_address,
				       s16 value)
{
	int ret;
	struct spi_message msg;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct iio_sw_ring_helper_state *h
		= iio_dev_get_devdata(indio_dev);
	struct mma7455l_state *priv = mma7455l_h_to_s(h);
	struct spi_transfer xfers[] = { {
			.tx_buf = priv->tx,
			.bits_per_word = 8,
			.len = 2,
			.cs_change = 1,
		}, {
			.tx_buf = priv->tx + 2,
			.bits_per_word = 8,
			.len = 2,
			.cs_change = 1,
		},
	};

	mutex_lock(&priv->buf_lock);
	priv->tx[0] = MMA7455L_WRITE_REG(lower_reg_address);
	priv->tx[1] = value & 0xFF;
	priv->tx[2] = MMA7455L_WRITE_REG(lower_reg_address + 1);
	priv->tx[3] = (value >> 8) & 0xFF;

	spi_message_init(&msg);
	spi_message_add_tail(&xfers[0], &msg);
	spi_message_add_tail(&xfers[1], &msg);
	ret = spi_sync(priv->spidev, &msg);
	mutex_unlock(&priv->buf_lock);

	return ret;
}

/**
 * lisl302dq_spi_read_reg_s16() - write 2 bytes to a pair of registers
 * @dev: device associated with child of actual device (iio_dev or iio_trig)
 * @reg_address: the address of the lower of the two registers. Second register
 *               is assumed to have address one greater.
 * @val: somewhere to pass back the value read
 **/
static int mma7455l_spi_read_reg_s16(struct device *dev,
				      u8 lower_reg_address,
				      s16 *val)
{
	struct spi_message msg;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct iio_sw_ring_helper_state *h
		= iio_dev_get_devdata(indio_dev);
	struct mma7455l_state *priv = mma7455l_h_to_s(h);
	int ret;
	struct spi_transfer xfers[] = { {
			.tx_buf = priv->tx,
			.rx_buf = priv->rx,
			.bits_per_word = 8,
			.len = 2,
			.cs_change = 1,
		}, {
			.tx_buf = priv->tx + 2,
			.rx_buf = priv->rx + 2,
			.bits_per_word = 8,
			.len = 2,
			.cs_change = 1,

		},
	};

	mutex_lock(&priv->buf_lock);
	priv->tx[0] = MMA7455L_READ_REG(lower_reg_address);
	priv->tx[1] = 0;
	priv->tx[2] = MMA7455L_READ_REG(lower_reg_address+1);
	priv->tx[3] = 0;

	spi_message_init(&msg);
	spi_message_add_tail(&xfers[0], &msg);
	spi_message_add_tail(&xfers[1], &msg);
	ret = spi_sync(priv->spidev, &msg);
	if (ret) {
		dev_err(&priv->spidev->dev, "problem when reading 16 bit register");
		goto error_ret;
	}
	*val = (s16)(priv->rx[1]) | ((s16)(priv->rx[3]) << 8);

error_ret:
	mutex_unlock(&priv->buf_lock);
	return ret;
}

static ssize_t mma7455l_read_16bit_signed(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	int ret;
	s16 val = 0;
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);

	ret = mma7455l_spi_read_reg_s16(dev, this_attr->address, &val);

	if (ret)
		return ret;

	return sprintf(buf, "%d\n", val);
}

static ssize_t mma7455l_read_accel(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct iio_dev		*indio_dev = dev_get_drvdata(dev);
	ssize_t			ret;

	/* Take the iio_dev status lock */
	mutex_lock(&indio_dev->mlock);
	if (indio_dev->currentmode == INDIO_RING_TRIGGERED)
		ret = mma7455l_read_accel_from_ring(dev, attr, buf);
	else
		ret =  mma7455l_read_accel_from_device(dev, attr, buf);
	mutex_unlock(&indio_dev->mlock);

	return ret;
}

static ssize_t mma7455l_write_16bit_signed(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf,
					    size_t len)
{
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int ret;
	long val;

	ret = strict_strtol(buf, 10, &val);
	if (ret)
		goto error_ret;
	ret = mma7455l_spi_write_reg_s16(dev, this_attr->address, val);

error_ret:
	return ret ? ret : len;
}

static ssize_t mma7455l_read_frequency(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int ret, len = 0;
	s8 t;
	ret = mma7455l_spi_read_reg_8(dev,
				       MMA7455L_REG_CTRL_1_ADDR,
				       (u8 *)&t);
	if (ret)
		return ret;
	t &= MMA7455L_DEC_MASK;
	switch (t) {
	case MMA7455L_REG_CTRL_1_DF_128:
		len = sprintf(buf, "280\n");
		break;
	case MMA7455L_REG_CTRL_1_DF_64:
		len = sprintf(buf, "560\n");
		break;
	case MMA7455L_REG_CTRL_1_DF_32:
		len = sprintf(buf, "1120\n");
		break;
	case MMA7455L_REG_CTRL_1_DF_8:
		len = sprintf(buf, "4480\n");
		break;
	}
	return len;
}

static ssize_t mma7455l_write_frequency(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf,
					 size_t len)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	long val;
	int ret;
	u8 t;

	ret = strict_strtol(buf, 10, &val);
	if (ret)
		return ret;

	mutex_lock(&indio_dev->mlock);
	ret = mma7455l_spi_read_reg_8(dev,
				       MMA7455L_REG_CTRL_1_ADDR,
				       &t);
	if (ret)
		goto error_ret_mutex;
	/* Wipe the bits clean */
	t &= ~MMA7455L_DEC_MASK;
	switch (val) {
	case 280:
		t |= MMA7455L_REG_CTRL_1_DF_128;
		break;
	case 560:
		t |= MMA7455L_REG_CTRL_1_DF_64;
		break;
	case 1120:
		t |= MMA7455L_REG_CTRL_1_DF_32;
		break;
	case 4480:
		t |= MMA7455L_REG_CTRL_1_DF_8;
		break;
	default:
		ret = -EINVAL;
		goto error_ret_mutex;
	};

	ret = mma7455l_spi_write_reg_8(dev,
					MMA7455L_REG_CTRL_1_ADDR,
					&t);

error_ret_mutex:
	mutex_unlock(&indio_dev->mlock);

	return ret ? ret : len;
}

static int mma7455l_setup(struct mma7455l_state *priv)
{
	int				ret;
	u8				val;
	struct ambarella_gpio_io_info	gsensor_power;
	struct spi_device		*spidev;

	/* Power On */
	gsensor_power = ambarella_board_generic.gsensor_power;
	if (gsensor_power.gpio_id >= 0) {
		ambarella_gpio_config(gsensor_power.gpio_id, GPIO_FUNC_SW_OUTPUT);
		ambarella_gpio_set(gsensor_power.gpio_id, gsensor_power.active_level);
		msleep(gsensor_power.active_delay);
	}

	/* Set up SPI */
	spidev = priv->spidev;
	spidev->mode		= SPI_MODE_0;
	spidev->bits_per_word	= 8;
	spidev->max_speed_hz	= 1000000;
	spi_setup(spidev);

	/* Check whether MMA7455L is working */
	ret = mma7455l_read_raw_8bit(spidev, MMA7455L_WHOAMI, &val);
	if (ret) {
		printk("MMA7455L Error: %s %d", __func__, __LINE__);
	} else {
		printk("MMA7455L WHO AM I: 0x%02x\n", val);
	}

	return ret;
}

#define MMA7455L_OFFSET_DRIFT_ATTR(name, reg)		\
	IIO_DEVICE_ATTR(name,				\
			S_IWUSR | S_IRUGO,		\
			mma7455l_read_offset_drift,	\
			mma7455l_write_offset_drift,	\
			reg);

static MMA7455L_OFFSET_DRIFT_ATTR(accel_x_offset_drift,
			     MMA7455L_XOFFL);
static MMA7455L_OFFSET_DRIFT_ATTR(accel_y_offset_drift,
			     MMA7455L_YOFFL);
static MMA7455L_OFFSET_DRIFT_ATTR(accel_z_offset_drift,
			     MMA7455L_ZOFFL);

static IIO_DEVICE_ATTR(accel_raw_mag_value,
		       S_IWUSR | S_IRUGO,
		       mma7455l_read_16bit_signed,
		       mma7455l_write_16bit_signed,
		       MMA7455L_REG_THS_L_ADDR);

static IIO_DEV_ATTR_ACCEL_X(mma7455l_read_accel,
			    MMA7455L_XOUT8);

static IIO_DEV_ATTR_ACCEL_Y(mma7455l_read_accel,
			    MMA7455L_YOUT8);

static IIO_DEV_ATTR_ACCEL_Z(mma7455l_read_accel,
			    MMA7455L_ZOUT8);

static IIO_DEV_ATTR_SAMP_FREQ(S_IWUSR | S_IRUGO,
			      mma7455l_read_frequency,
			      mma7455l_write_frequency);

static IIO_CONST_ATTR_SAMP_FREQ_AVAIL("280 560 1120 4480");

static ssize_t mma7455l_read_interrupt_config(struct device *dev,
					       struct device_attribute *attr,
					       char *buf)
{
	int ret;
	s8 val;
	struct iio_event_attr *this_attr = to_iio_event_attr(attr);

	ret = mma7455l_spi_read_reg_8(dev->parent,
				       MMA7455L_REG_WAKE_UP_CFG_ADDR,
				       (u8 *)&val);

	return ret ? ret : sprintf(buf, "%d\n", !!(val & this_attr->mask));
}

static ssize_t mma7455l_write_interrupt_config(struct device *dev,
						struct device_attribute *attr,
						const char *buf,
						size_t len)
{
	struct iio_event_attr *this_attr = to_iio_event_attr(attr);
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int ret, currentlyset, changed = 0;
	u8 valold, controlold;
	bool val;

	val = !(buf[0] == '0');

	mutex_lock(&indio_dev->mlock);
	/* read current value */
	ret = mma7455l_spi_read_reg_8(dev->parent,
				       MMA7455L_REG_WAKE_UP_CFG_ADDR,
				       &valold);
	if (ret)
		goto error_mutex_unlock;

	/* read current control */
	ret = mma7455l_spi_read_reg_8(dev,
				       MMA7455L_REG_CTRL_2_ADDR,
				       &controlold);
	if (ret)
		goto error_mutex_unlock;
	currentlyset = !!(valold & this_attr->mask);
	if (val == false && currentlyset) {
		valold &= ~this_attr->mask;
		changed = 1;
		iio_remove_event_from_list(this_attr->listel,
						 &indio_dev->interrupts[0]
						 ->ev_list);
	} else if (val == true && !currentlyset) {
		changed = 1;
		valold |= this_attr->mask;
		iio_add_event_to_list(this_attr->listel,
					    &indio_dev->interrupts[0]->ev_list);
	}

	if (changed) {
		ret = mma7455l_spi_write_reg_8(dev,
						MMA7455L_REG_WAKE_UP_CFG_ADDR,
						&valold);
		if (ret)
			goto error_mutex_unlock;
		/* This always enables the interrupt, even if we've remove the
		 * last thing using it. For this device we can use the reference
		 * count on the handler to tell spidev if anyone wants the interrupt
		 */
		controlold = this_attr->listel->refcount ?
			(controlold | MMA7455L_REG_CTRL_2_ENABLE_INTERRUPT) :
			(controlold & ~MMA7455L_REG_CTRL_2_ENABLE_INTERRUPT);
		ret = mma7455l_spi_write_reg_8(dev,
						MMA7455L_REG_CTRL_2_ADDR,
						&controlold);
		if (ret)
			goto error_mutex_unlock;
	}
error_mutex_unlock:
	mutex_unlock(&indio_dev->mlock);

	return ret ? ret : len;
}


static int mma7455l_thresh_handler_th(struct iio_dev *indio_dev,
				       int index,
				       s64 timestamp,
				       int no_test)
{
	struct iio_sw_ring_helper_state *h
		= iio_dev_get_devdata(indio_dev);
	struct mma7455l_state *priv = mma7455l_h_to_s(h);

	/* Stash the timestamp somewhere convenient for the bh */
	priv->thresh_timestamp = timestamp;
	schedule_work(&priv->work_thresh);

	return 0;
}


/* Unforunately it appears the interrupt won't clear unless you read from the
 * src register.
 */
static void mma7455l_thresh_handler_bh_no_check(struct work_struct *work_s)
{
       struct mma7455l_state *priv
	       = container_of(work_s,
		       struct mma7455l_state, work_thresh);

	u8 t;

	mma7455l_spi_read_reg_8(&priv->help.indio_dev->dev,
				 MMA7455L_REG_WAKE_UP_SRC_ADDR,
				 &t);

	if (t & MMA7455L_REG_WAKE_UP_SRC_INTERRUPT_Z_HIGH)
		iio_push_event(priv->help.indio_dev, 0,
			       IIO_MOD_EVENT_CODE(IIO_EV_CLASS_ACCEL,
						  0,
						  IIO_EV_MOD_Z,
						  IIO_EV_TYPE_THRESH,
						  IIO_EV_DIR_RISING),
			       priv->thresh_timestamp);

	if (t & MMA7455L_REG_WAKE_UP_SRC_INTERRUPT_Z_LOW)
		iio_push_event(priv->help.indio_dev, 0,
			       IIO_MOD_EVENT_CODE(IIO_EV_CLASS_ACCEL,
						  0,
						  IIO_EV_MOD_Z,
						  IIO_EV_TYPE_THRESH,
						  IIO_EV_DIR_FALLING),
			       priv->thresh_timestamp);

	if (t & MMA7455L_REG_WAKE_UP_SRC_INTERRUPT_Y_HIGH)
		iio_push_event(priv->help.indio_dev, 0,
			       IIO_MOD_EVENT_CODE(IIO_EV_CLASS_ACCEL,
						  0,
						  IIO_EV_MOD_Y,
						  IIO_EV_TYPE_THRESH,
						  IIO_EV_DIR_RISING),
			       priv->thresh_timestamp);

	if (t & MMA7455L_REG_WAKE_UP_SRC_INTERRUPT_Y_LOW)
		iio_push_event(priv->help.indio_dev, 0,
			       IIO_MOD_EVENT_CODE(IIO_EV_CLASS_ACCEL,
						  0,
						  IIO_EV_MOD_Y,
						  IIO_EV_TYPE_THRESH,
						  IIO_EV_DIR_FALLING),
			       priv->thresh_timestamp);

	if (t & MMA7455L_REG_WAKE_UP_SRC_INTERRUPT_X_HIGH)
		iio_push_event(priv->help.indio_dev, 0,
			       IIO_MOD_EVENT_CODE(IIO_EV_CLASS_ACCEL,
						  0,
						  IIO_EV_MOD_X,
						  IIO_EV_TYPE_THRESH,
						  IIO_EV_DIR_RISING),
			       priv->thresh_timestamp);

	if (t & MMA7455L_REG_WAKE_UP_SRC_INTERRUPT_X_LOW)
		iio_push_event(priv->help.indio_dev, 0,
			       IIO_MOD_EVENT_CODE(IIO_EV_CLASS_ACCEL,
						  0,
						  IIO_EV_MOD_X,
						  IIO_EV_TYPE_THRESH,
						  IIO_EV_DIR_FALLING),
			       priv->thresh_timestamp);
	/* reenable the irq */
	enable_irq(priv->spidev->irq);
	/* Ack and allow for new interrupts */
	mma7455l_spi_read_reg_8(&priv->help.indio_dev->dev,
				 MMA7455L_REG_WAKE_UP_ACK_ADDR,
				 &t);

	return;
}

/* A shared handler for a number of threshold types */
IIO_EVENT_SH(threshold, &mma7455l_thresh_handler_th);

IIO_EVENT_ATTR_SH(accel_x_thresh_rising_en,
		  iio_event_threshold,
		  mma7455l_read_interrupt_config,
		  mma7455l_write_interrupt_config,
		  MMA7455L_REG_WAKE_UP_CFG_INTERRUPT_X_HIGH);

IIO_EVENT_ATTR_SH(accel_y_thresh_rising_en,
		  iio_event_threshold,
		  mma7455l_read_interrupt_config,
		  mma7455l_write_interrupt_config,
		  MMA7455L_REG_WAKE_UP_CFG_INTERRUPT_Y_HIGH);

IIO_EVENT_ATTR_SH(accel_z_thresh_rising_en,
		  iio_event_threshold,
		  mma7455l_read_interrupt_config,
		  mma7455l_write_interrupt_config,
		  MMA7455L_REG_WAKE_UP_CFG_INTERRUPT_Z_HIGH);

IIO_EVENT_ATTR_SH(accel_x_thresh_falling_en,
		  iio_event_threshold,
		  mma7455l_read_interrupt_config,
		  mma7455l_write_interrupt_config,
		  MMA7455L_REG_WAKE_UP_CFG_INTERRUPT_X_LOW);

IIO_EVENT_ATTR_SH(accel_y_thresh_falling_en,
		  iio_event_threshold,
		  mma7455l_read_interrupt_config,
		  mma7455l_write_interrupt_config,
		  MMA7455L_REG_WAKE_UP_CFG_INTERRUPT_Y_LOW);

IIO_EVENT_ATTR_SH(accel_z_thresh_falling_en,
		  iio_event_threshold,
		  mma7455l_read_interrupt_config,
		  mma7455l_write_interrupt_config,
		  MMA7455L_REG_WAKE_UP_CFG_INTERRUPT_Z_LOW);


static struct attribute *mma7455l_event_attributes[] = {
	&iio_event_attr_accel_x_thresh_rising_en.dev_attr.attr,
	&iio_event_attr_accel_y_thresh_rising_en.dev_attr.attr,
	&iio_event_attr_accel_z_thresh_rising_en.dev_attr.attr,
	&iio_event_attr_accel_x_thresh_falling_en.dev_attr.attr,
	&iio_event_attr_accel_y_thresh_falling_en.dev_attr.attr,
	&iio_event_attr_accel_z_thresh_falling_en.dev_attr.attr,
	&iio_dev_attr_accel_raw_mag_value.dev_attr.attr,
	NULL
};

static struct attribute_group mma7455l_event_attribute_group = {
	.attrs = mma7455l_event_attributes,
};

static IIO_CONST_ATTR_NAME("mma7455l");

static struct attribute *mma7455l_attributes[] = {
	&iio_dev_attr_accel_x_raw.dev_attr.attr,
	&iio_dev_attr_accel_y_raw.dev_attr.attr,
	&iio_dev_attr_accel_z_raw.dev_attr.attr,

	&iio_dev_attr_accel_x_offset_drift.dev_attr.attr,
	&iio_dev_attr_accel_y_offset_drift.dev_attr.attr,
	&iio_dev_attr_accel_z_offset_drift.dev_attr.attr,

	&iio_dev_attr_sampling_frequency.dev_attr.attr,
	&iio_const_attr_sampling_frequency_available.dev_attr.attr,
	&iio_const_attr_name.dev_attr.attr,
	NULL
};

static const struct attribute_group mma7455l_attribute_group = {
	.attrs = mma7455l_attributes,
};

static int __devinit mma7455l_probe(struct spi_device *spi)
{
	int ret, regdone = 0;
	struct mma7455l_state *priv = kzalloc(sizeof *priv, GFP_KERNEL);
	if (!priv) {
		ret =  -ENOMEM;
		goto error_ret;
	}
	INIT_WORK(&priv->work_thresh, mma7455l_thresh_handler_bh_no_check);
	/* this is only used tor removal purposes */
	spi_set_drvdata(spi, priv);

	/* Allocate the comms buffers */
	priv->rx = kzalloc(sizeof(*priv->rx)*MMA7455L_MAX_RX, GFP_KERNEL);
	if (priv->rx == NULL) {
		ret = -ENOMEM;
		goto error_free_st;
	}
	priv->tx = kzalloc(sizeof(*priv->tx)*MMA7455L_MAX_TX, GFP_KERNEL);
	if (priv->tx == NULL) {
		ret = -ENOMEM;
		goto error_free_rx;
	}
	priv->spidev = spi;
	mutex_init(&priv->buf_lock);
	/* setup the industrialio driver allocated elements */
	priv->help.indio_dev = iio_allocate_device();
	if (priv->help.indio_dev == NULL) {
		ret = -ENOMEM;
		goto error_free_tx;
	}

	priv->help.indio_dev->dev.parent = &spi->dev;
	priv->help.indio_dev->num_interrupt_lines = 1;
	priv->help.indio_dev->event_attrs = &mma7455l_event_attribute_group;
	priv->help.indio_dev->attrs = &mma7455l_attribute_group;
	priv->help.indio_dev->dev_data = (void *)(&priv->help);
	priv->help.indio_dev->driver_module = THIS_MODULE;
	priv->help.indio_dev->modes = INDIO_DIRECT_MODE;

	ret = mma7455l_configure_ring(priv->help.indio_dev);
	if (ret)
		goto error_free_dev;

	ret = iio_device_register(priv->help.indio_dev);
	if (ret)
		goto error_unreg_ring_funcs;
	regdone = 1;

	ret = iio_ring_buffer_register(priv->help.indio_dev->ring, 0);
	if (ret) {
		printk(KERN_ERR "failed to initialize the ring\n");
		goto error_unreg_ring_funcs;
	}

	if (spi->irq && gpio_is_valid(irq_to_gpio(spi->irq)) > 0) {
		priv->inter = 0;
		ret = iio_register_interrupt_line(spi->irq,
						  priv->help.indio_dev,
						  0,
						  IRQF_TRIGGER_RISING,
						  "mma7455l");
		if (ret)
			goto error_uninitialize_ring;

		ret = mma7455l_probe_trigger(priv->help.indio_dev);
		if (ret)
			goto error_unregister_line;
	}

	/* Get the device into a sane initial state */
	ret = mma7455l_setup(priv);
	if (ret)
		goto error_remove_trigger;
	return 0;

error_remove_trigger:
	if (priv->help.indio_dev->modes & INDIO_RING_TRIGGERED)
		mma7455l_remove_trigger(priv->help.indio_dev);
error_unregister_line:
	if (priv->help.indio_dev->modes & INDIO_RING_TRIGGERED)
		iio_unregister_interrupt_line(priv->help.indio_dev, 0);
error_uninitialize_ring:
	iio_ring_buffer_unregister(priv->help.indio_dev->ring);
error_unreg_ring_funcs:
	mma7455l_unconfigure_ring(priv->help.indio_dev);
error_free_dev:
	if (regdone)
		iio_device_unregister(priv->help.indio_dev);
	else
		iio_free_device(priv->help.indio_dev);
error_free_tx:
	kfree(priv->tx);
error_free_rx:
	kfree(priv->rx);
error_free_st:
	kfree(priv);
error_ret:
	return ret;
}

/* Power down the device */
static int mma7455l_stop_device(struct iio_dev *indio_dev)
{
	int ret;
	struct iio_sw_ring_helper_state *h
		= iio_dev_get_devdata(indio_dev);
	struct mma7455l_state *priv = mma7455l_h_to_s(h);
	u8 val = 0;

	mutex_lock(&indio_dev->mlock);
	ret = mma7455l_spi_write_reg_8(&indio_dev->dev,
					MMA7455L_REG_CTRL_1_ADDR,
					&val);
	if (ret) {
		dev_err(&priv->spidev->dev, "problem with turning device off: ctrl1");
		goto err_ret;
	}

	ret = mma7455l_spi_write_reg_8(&indio_dev->dev,
					MMA7455L_REG_CTRL_2_ADDR,
					&val);
	if (ret)
		dev_err(&priv->spidev->dev, "problem with turning device off: ctrl2");
err_ret:
	mutex_unlock(&indio_dev->mlock);
	return ret;
}

/* fixme, confirm ordering in this function */
static int mma7455l_remove(struct spi_device *spi)
{
	int ret;
	struct mma7455l_state *priv = spi_get_drvdata(spi);
	struct iio_dev *indio_dev = priv->help.indio_dev;
	struct ambarella_gpio_io_info	gsensor_power;

	ret = mma7455l_stop_device(indio_dev);
	if (ret)
		goto err_ret;

	flush_scheduled_work();

	mma7455l_remove_trigger(indio_dev);
	if (spi->irq && gpio_is_valid(irq_to_gpio(spi->irq)) > 0)
		iio_unregister_interrupt_line(indio_dev, 0);

	iio_ring_buffer_unregister(indio_dev->ring);
	mma7455l_unconfigure_ring(indio_dev);
	iio_device_unregister(indio_dev);
	kfree(priv->tx);
	kfree(priv->rx);
	kfree(priv);

	gsensor_power = ambarella_board_generic.gsensor_power;
	if (gsensor_power.gpio_id >= 0) {
		ambarella_gpio_config(gsensor_power.gpio_id, GPIO_FUNC_SW_OUTPUT);
		if (gsensor_power.active_level == GPIO_HIGH) {
			ambarella_gpio_set(gsensor_power.gpio_id, GPIO_LOW);
		} else {
			ambarella_gpio_set(gsensor_power.gpio_id, GPIO_HIGH);
		}
		msleep(gsensor_power.active_delay);
	}

	return 0;

err_ret:
	return ret;
}

static struct spi_driver mma7455l_driver = {
	.driver = {
		.name = "mma7455l",
		.owner = THIS_MODULE,
	},
	.probe = mma7455l_probe,
	.remove = __devexit_p(mma7455l_remove),
};

static __init int mma7455l_init(void)
{
	return spi_register_driver(&mma7455l_driver);
}
module_init(mma7455l_init);

static __exit void mma7455l_exit(void)
{
	spi_unregister_driver(&mma7455l_driver);
}
module_exit(mma7455l_exit);

MODULE_AUTHOR("Zhenwu Xue <zwxue@ambarella.com>");
MODULE_DESCRIPTION("Freescale Semiconductor MMA7455L Accelerometer Driver");
MODULE_LICENSE("GPL v2");
