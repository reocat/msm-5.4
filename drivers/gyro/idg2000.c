#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>

#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>

#include <plat/spi.h>

//register definition
#define ID 		0x0
#define SMPLRT_DIV 	0x15
#define DLPF_FS_SYNC 	0x16
#define INT_CFG 	0x17
#define INT_STATUS 	0x1a
#define GYRO_XOUT_H 	0x1d
#define GYRO_XOUT_L 	0x1e
#define GYRO_YOUT_H 	0x1f
#define GYRO_YOUT_L 	0x20
#define PWR_MGM 	0x3E

//bit and data definition
#define I2C_IF_DIS			1 << 7
#define GYRO_INVENSENSE_IDG2000_ID	0x68
#define DIV			0x0   //change me if needed

#define EXT_SYNC_SET			0 << 5
#define FS_SEL				0 << 3
#define DLPF_CFG			1 << 0

#define RAW_RDY_EN			1
#define IDG_RDY_EN			1 << 2
#define INT_ANYRD_2CLEAR		1 << 4
#define LATCH_INT_EN			1 << 5
#define OPEN				1 << 6
#define ACTL				1 << 7

#define RAW_DATA_RDY			0
#define IDG_RDY				1 << 2

#define CLK_SEL				1 << 0
#define STBY_YG				1 << 4
#define STBY_XG				1 << 5
#define SLEEP				1 << 6
#define H_RESET				1 << 7

#define SPI_MODE_3 3



static int spi_bus = 0;
MODULE_PARM_DESC(spi_bus, "SPI controller ID");
static int spi_cs = 0;
MODULE_PARM_DESC(spi_cs, "SPI CS");

static struct kobject * kobject;

static int idg2000_write_reg(u8 subaddr, u8 data)
{
	int	errCode = 0;
	u8	pbuf[2];
	amba_spi_cfg_t config;
	amba_spi_write_t write;

	pbuf[0] = subaddr & ~(1 << 7);
	pbuf[1] = data;

	config.cfs_dfs = 8;//bits
	config.baud_rate = 1000000;
	config.lsb_first = 0;
	config.cs_change = 0;
	config.spi_mode = SPI_MODE_3;

	write.buffer = pbuf;
	write.n_size = 2;
	write.cs_id = spi_cs;
	write.bus_id = spi_bus;
	ambarella_spi_write(&config, &write);

	return errCode;
}

static int idg2000_read_reg(u8 subaddr, u8 *pdata)
{
	int				errCode = 0;
	u8	pbuf[2];
	u8	tmp;
	amba_spi_cfg_t config;
	amba_spi_write_then_read_t write;


	pbuf[0] = subaddr | (1 << 7) ;

	config.cfs_dfs = 8;//bits
	config.baud_rate = 1000000;
	config.lsb_first = 0;
	config.cs_change = 0;
	config.spi_mode = SPI_MODE_3;

	write.w_buffer = pbuf;
	write.w_size = 1;
	write.r_buffer = &tmp;
	write.r_size = 1;
	write.cs_id = spi_cs;
	write.bus_id = spi_bus;

	ambarella_spi_write_then_read(&config, &write);
	*pdata = tmp;

	return errCode;
}

static int pre_set_idg2000(void)
{
	u8 data;

	data = I2C_IF_DIS|GYRO_INVENSENSE_IDG2000_ID;
	idg2000_write_reg(ID, data);

	data = DIV;
	idg2000_write_reg(SMPLRT_DIV, data);

	data = EXT_SYNC_SET|FS_SEL|DLPF_CFG;
	idg2000_write_reg(DLPF_FS_SYNC, data);

	data = RAW_RDY_EN|LATCH_INT_EN;
	idg2000_write_reg(INT_CFG, data);

	data = CLK_SEL;
	idg2000_write_reg(PWR_MGM, data);

	return 0;

}

static int get_gyro(u16 *xdata, u16 *ydata)
{
	int errorCode = 0;
	u8 a[2];

	errorCode = idg2000_read_reg(GYRO_XOUT_H,&a[0]);
	errorCode = idg2000_read_reg(GYRO_XOUT_L,&a[1]);
	*xdata = (a[0]<<8)|a[1];

	errorCode = idg2000_read_reg(GYRO_YOUT_H,&a[0]);
	errorCode = idg2000_read_reg(GYRO_YOUT_L,&a[1]);
	*ydata = (a[0]<<8)|a[1];

	return errorCode;
}


/*
 * SysFS support
 */

static ssize_t show_gyro(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int errorCode = 0;
	u16 data[2];

	errorCode = get_gyro(&data[0],&data[1]);
	if (errorCode < 0)
		return errorCode;

	return sprintf(buf, "Ax:%d,Ay:%d\n", data[0], data[1]);
}

static DEVICE_ATTR(gyro, S_IRUGO, show_gyro, NULL);

static struct attribute *idg2000_attributes[] = {
	&dev_attr_gyro.attr,
	NULL,
};

static const struct attribute_group idg2000_attr_group = {
	.attrs = idg2000_attributes,
};



static int idg2000_probe(void)
{
	int errorCode = 0;
	u8 id;

	idg2000_read_reg(ID,&id);

	if(id != 0x68)
	{
		printk("ERROR: can't read register from gyro sensor through SPI!\n");
		return -1;
	}

	pre_set_idg2000();

	kobject = kobject_create_and_add("gyro-data",kernel_kobj);
	/* Register sysfs hooks */
	errorCode = sysfs_create_group(kobject, &idg2000_attr_group);

	printk("IDG2000 Init\n");

	return errorCode;
}

static int __init idg2000_init(void)
{
	return idg2000_probe();
}

static void __exit idg2000_exit(void)
{
	sysfs_remove_group(kobject, &idg2000_attr_group);
}

module_init(idg2000_init);
module_exit(idg2000_exit);

MODULE_DESCRIPTION("IDG 2000 single-chip 2-axis digital MEMS Gyro Sensor");
MODULE_AUTHOR("Zhangkai Wang, <zkwang@ambarella.com>");
MODULE_LICENSE("GPL");
