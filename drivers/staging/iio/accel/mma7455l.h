/*
 * drivers/staging/iio/accel/mma7455l.h
 *
 * Copyright (C) 2004-2012, Ambarella, Inc.
 *	Zhenwu Xue <zwxue@ambarella.com>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#ifndef SPI_MMA7455L_H_
#define SPI_MMA7455L_H_

enum {
	MMA7455L_XOUTL		= 0x00,
	MMA7455L_XOUTH,
	MMA7455L_YOUTL,
	MMA7455L_YOUTH,
	MMA7455L_ZOUTL,
	MMA7455L_ZOUTH,
	MMA7455L_XOUT8,
	MMA7455L_YOUT8,
	MMA7455L_ZOUT8,
	MMA7455L_STATUS,
	MMA7455L_DETSRC,
	MMA7455L_TOUT,
	MMA7455L_RESERVED,
	MMA7455L_I2CAD,
	MMA7455L_USRINF,
	MMA7455L_WHOAMI,
	MMA7455L_XOFFL,
	MMA7455L_XOFFH,
	MMA7455L_YOFFL,
	MMA7455L_YOFFH,
	MMA7455L_ZOFFL,
	MMA7455L_ZOFFH,
	MMA7455L_MCTL,
	MMA7455L_INTRST,
	MMA7455L_CTL1,
	MMA7455L_CTL2,
	MMA7455L_LDTH,
	MMA7455L_PDTH,
	MMA7455L_PW,
	MMA7455L_LT,
	MMA7455L_TW,
};

#define MMA7455L_READ_REG(a) ((a) | 0x80)
#define MMA7455L_WRITE_REG(a) a

/* Calibration parameters */
#define MMA7455L_REG_OFFSET_X_ADDR		0x16
#define MMA7455L_REG_OFFSET_Y_ADDR		0x17
#define MMA7455L_REG_OFFSET_Z_ADDR		0x18

#define MMA7455L_REG_GAIN_X_ADDR		0x19
#define MMA7455L_REG_GAIN_Y_ADDR		0x1A
#define MMA7455L_REG_GAIN_Z_ADDR		0x1B

/* Control Register (1 of 2) */
#define MMA7455L_REG_CTRL_1_ADDR		0x20
/* Power ctrl - either bit set corresponds to on*/
#define MMA7455L_REG_CTRL_1_PD_ON		0xC0

/* Decimation Factor  */
#define MMA7455L_DEC_MASK			0x30
#define MMA7455L_REG_CTRL_1_DF_128		0x00
#define MMA7455L_REG_CTRL_1_DF_64		0x10
#define MMA7455L_REG_CTRL_1_DF_32		0x20
#define MMA7455L_REG_CTRL_1_DF_8		(0x10 | 0x20)

/* Self Test Enable */
#define MMA7455L_REG_CTRL_1_SELF_TEST_ON	0x08

/* Axes enable ctrls */
#define MMA7455L_REG_CTRL_1_AXES_Z_ENABLE	0x04
#define MMA7455L_REG_CTRL_1_AXES_Y_ENABLE	0x02
#define MMA7455L_REG_CTRL_1_AXES_X_ENABLE	0x01

/* Control Register (2 of 2) */
#define MMA7455L_REG_CTRL_2_ADDR		0x21

/* Block Data Update only after MSB and LSB read */
#define MMA7455L_REG_CTRL_2_BLOCK_UPDATE	0x40

/* Set to big endian output */
#define MMA7455L_REG_CTRL_2_BIG_ENDIAN		0x20

/* Reboot memory content */
#define MMA7455L_REG_CTRL_2_REBOOT_MEMORY	0x10

/* Interupt Enable - applies data ready to the RDY pad */
#define MMA7455L_REG_CTRL_2_ENABLE_INTERRUPT	0x08

/* Enable Data Ready Generation - relationship with previous unclear in docs */
#define MMA7455L_REG_CTRL_2_ENABLE_DATA_READY_GENERATION 0x04

/* SPI 3 wire mode */
#define MMA7455L_REG_CTRL_2_THREE_WIRE_SPI_MODE	0x02

/* Data alignment, default is 12 bit right justified
 * - option for 16 bit left justified */
#define MMA7455L_REG_CTRL_2_DATA_ALIGNMENT_16_BIT_LEFT_JUSTIFIED	0x01

/* Interupt related stuff */
#define MMA7455L_REG_WAKE_UP_CFG_ADDR			0x23

/* Switch from or combination fo conditions to and */
#define MMA7455L_REG_WAKE_UP_CFG_BOOLEAN_AND		0x80

/* Latch interupt request,
 * if on ack must be given by reading the ack register */
#define MMA7455L_REG_WAKE_UP_CFG_LATCH_SRC		0x40

/* Z Interupt on High (above threshold)*/
#define MMA7455L_REG_WAKE_UP_CFG_INTERRUPT_Z_HIGH	0x20
/* Z Interupt on Low */
#define MMA7455L_REG_WAKE_UP_CFG_INTERRUPT_Z_LOW	0x10
/* Y Interupt on High */
#define MMA7455L_REG_WAKE_UP_CFG_INTERRUPT_Y_HIGH	0x08
/* Y Interupt on Low */
#define MMA7455L_REG_WAKE_UP_CFG_INTERRUPT_Y_LOW	0x04
/* X Interupt on High */
#define MMA7455L_REG_WAKE_UP_CFG_INTERRUPT_X_HIGH	0x02
/* X Interupt on Low */
#define MMA7455L_REG_WAKE_UP_CFG_INTERRUPT_X_LOW 0x01

/* Register that gives description of what caused interupt
 * - latched if set in CFG_ADDRES */
#define MMA7455L_REG_WAKE_UP_SRC_ADDR			0x24
/* top bit ignored */
/* Interupt Active */
#define MMA7455L_REG_WAKE_UP_SRC_INTERRUPT_ACTIVATED	0x40
/* Interupts that have been triggered */
#define MMA7455L_REG_WAKE_UP_SRC_INTERRUPT_Z_HIGH	0x20
#define MMA7455L_REG_WAKE_UP_SRC_INTERRUPT_Z_LOW	0x10
#define MMA7455L_REG_WAKE_UP_SRC_INTERRUPT_Y_HIGH	0x08
#define MMA7455L_REG_WAKE_UP_SRC_INTERRUPT_Y_LOW	0x04
#define MMA7455L_REG_WAKE_UP_SRC_INTERRUPT_X_HIGH	0x02
#define MMA7455L_REG_WAKE_UP_SRC_INTERRUPT_X_LOW	0x01

#define MMA7455L_REG_WAKE_UP_ACK_ADDR			0x25

/* Status register */
#define MMA7455L_REG_STATUS_ADDR			0x27
/* XYZ axis data overrun - first is all overrun? */
#define MMA7455L_REG_STATUS_XYZ_OVERRUN		0x80
#define MMA7455L_REG_STATUS_Z_OVERRUN			0x40
#define MMA7455L_REG_STATUS_Y_OVERRUN			0x20
#define MMA7455L_REG_STATUS_X_OVERRUN			0x10
/* XYZ new data available - first is all 3 available? */
#define MMA7455L_REG_STATUS_XYZ_NEW_DATA 0x08
#define MMA7455L_REG_STATUS_Z_NEW_DATA			0x04
#define MMA7455L_REG_STATUS_Y_NEW_DATA			0x02
#define MMA7455L_REG_STATUS_X_NEW_DATA			0x01

/* The accelerometer readings - low and high bytes.
Form of high byte dependant on justification set in ctrl reg */
#define MMA7455L_REG_OUT_X_L_ADDR			0x28
#define MMA7455L_REG_OUT_X_H_ADDR			0x29
#define MMA7455L_REG_OUT_Y_L_ADDR			0x2A
#define MMA7455L_REG_OUT_Y_H_ADDR			0x2B
#define MMA7455L_REG_OUT_Z_L_ADDR			0x2C
#define MMA7455L_REG_OUT_Z_H_ADDR			0x2D

/* Threshold values for all axes and both above and below thresholds
 * - i.e. there is only one value */
#define MMA7455L_REG_THS_L_ADDR			0x2E
#define MMA7455L_REG_THS_H_ADDR			0x2F

#define MMA7455L_DEFAULT_CTRL1 (MMA7455L_REG_CTRL_1_PD_ON	      \
				 | MMA7455L_REG_CTRL_1_AXES_Z_ENABLE \
				 | MMA7455L_REG_CTRL_1_AXES_Y_ENABLE \
				 | MMA7455L_REG_CTRL_1_AXES_X_ENABLE \
				 | MMA7455L_REG_CTRL_1_DF_128)

#define MMA7455L_DEFAULT_CTRL2	0

#define MMA7455L_MAX_TX 12
#define MMA7455L_MAX_RX 12
/**
 * struct mma7455l_state - device instance specific data
 * @helper:		data and func pointer allowing generic functions
 * @spidev:		actual spi_device
 * @work_thresh:	bh for threshold events
 * @thresh_timestamp:	timestamp for threshold interrupts.
 * @inter:		used to check if new interrupt has been triggered
 * @trig:		data ready trigger registered with iio
 * @tx:			transmit buffer
 * @rx:			recieve buffer
 * @buf_lock:		mutex to protect tx and rx
 **/
struct mma7455l_state {
	struct iio_sw_ring_helper_state	help;
	struct spi_device		*spidev;
	struct work_struct		work_thresh;
	s64				thresh_timestamp;
	bool				inter;
	struct iio_trigger		*trig;
	u8				*tx;
	u8				*rx;
	struct mutex			buf_lock;
};

#define mma7455l_h_to_s(_h)				\
	container_of(_h, struct mma7455l_state, help)

int mma7455l_spi_read_reg_8(struct device *dev,
			     u8 reg_address,
			     u8 *val);

int mma7455l_spi_write_reg_8(struct device *dev,
			      u8 reg_address,
			      u8 *val);

#ifdef CONFIG_IIO_RING_BUFFER
/* At the moment triggers are only used for ring buffer
 * filling. This may change!
 */
void mma7455l_remove_trigger(struct iio_dev *indio_dev);
int mma7455l_probe_trigger(struct iio_dev *indio_dev);

ssize_t mma7455l_read_accel_from_ring(struct device *dev,
				       struct device_attribute *attr,
				       char *buf);


int mma7455l_configure_ring(struct iio_dev *indio_dev);
void mma7455l_unconfigure_ring(struct iio_dev *indio_dev);

#else /* CONFIG_IIO_RING_BUFFER */

static inline void mma7455l_remove_trigger(struct iio_dev *indio_dev)
{
}
static inline int mma7455l_probe_trigger(struct iio_dev *indio_dev)
{
	return 0;
}

static inline ssize_t
mma7455l_read_accel_from_ring(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	return 0;
}

static int mma7455l_configure_ring(struct iio_dev *indio_dev)
{
	return 0;
}
static inline void mma7455l_unconfigure_ring(struct iio_dev *indio_dev)
{
}
#endif /* CONFIG_IIO_RING_BUFFER */
#endif /* SPI_MMA7455L_H_ */
