/*
 * arch/arm/plat-ambarella/include/plat/spi.h
 *
 * Author: Anthony Ginger <hfjiang@ambarella.com>
 *
 * Copyright (C) 2004-2010, Ambarella, Inc.
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

#ifndef __PLAT_AMBARELLA_SPI_H__
#define __PLAT_AMBARELLA_SPI_H__

#include <linux/spi/spi.h>

/* ==========================================================================*/
#if (CHIP_REV == A5S)
#define SPI_INSTANCES				2
#define SPI_AHB_INSTANCES			0
#define SPI_SLAVE_INSTANCES			1
#define SPI_AHB_SLAVE_INSTANCES			0
#elif (CHIP_REV == S2)
#ifdef CONFIG_PLAT_AMBARELLA_SUPPORT_MMAP_AHB64
#define SPI_INSTANCES				2
#else
#define SPI_INSTANCES				1
#endif
#define SPI_AHB_INSTANCES			0
#define SPI_SLAVE_INSTANCES			1
#define SPI_AHB_SLAVE_INSTANCES			0
#elif (CHIP_REV == I1)
#define SPI_INSTANCES				4
#define SPI_AHB_INSTANCES			1
#define SPI_SLAVE_INSTANCES			1
#define SPI_AHB_SLAVE_INSTANCES			0
#elif (CHIP_REV == S2L)
#define SPI_INSTANCES				0
#define SPI_AHB_INSTANCES			2
#define SPI_SLAVE_INSTANCES			0
#define SPI_AHB_SLAVE_INSTANCES			1
#elif (CHIP_REV == A7L)
#define SPI_INSTANCES				1
#define SPI_AHB_INSTANCES			0
#define SPI_SLAVE_INSTANCES			1
#define SPI_AHB_SLAVE_INSTANCES			0
#else
#define SPI_INSTANCES				1
#define SPI_AHB_INSTANCES			0
#define SPI_SLAVE_INSTANCES			0
#define SPI_AHB_SLAVE_INSTANCES			0
#endif

#if (CHIP_REV == I1) || (CHIP_REV == A7L) || \
	(CHIP_REV == S2) || (CHIP_REV == A8)
#define SPI_SUPPORT_MASTER_CHANGE_ENA_POLARITY	1
#define SPI_SUPPORT_MASTER_DELAY_START_TIME	1
#define SPI_SUPPORT_NSM_SHAKE_START_BIT_CHSANGE	1
#else
#define SPI_SUPPORT_MASTER_CHANGE_ENA_POLARITY	0
#define SPI_SUPPORT_MASTER_DELAY_START_TIME	0
#define SPI_SUPPORT_NSM_SHAKE_START_BIT_CHSANGE	0
#endif

/* ==========================================================================*/
#define SPI_OFFSET			0x2000
#define SPI_BASE			(APB_BASE + SPI_OFFSET)
#define SPI_REG(x)			(SPI_BASE + (x))

#if (SPI_SLAVE_INSTANCES >= 1)
#if (CHIP_REV == A7L)
#define SPI_SLAVE_OFFSET		0x1000
#else
#define SPI_SLAVE_OFFSET		0x1E000
#endif
#define SPI_SLAVE_BASE			(APB_BASE + SPI_SLAVE_OFFSET)
#define SPI_SLAVE_REG(x)		(SPI_SLAVE_BASE + (x))
#endif

#if (SPI_INSTANCES >= 2)
#define SPI2_OFFSET			0xF000
#define SPI2_BASE			(APB_BASE + SPI2_OFFSET)
#define SPI2_REG(x)			(SPI2_BASE + (x))
#endif

#if (SPI_INSTANCES >= 3)
#define SPI3_OFFSET			0x15000
#define SPI3_BASE			(APB_BASE + SPI3_OFFSET)
#define SPI3_REG(x)			(SPI3_BASE + (x))
#endif

#if (SPI_INSTANCES >= 4)
#define SPI4_OFFSET			0x16000
#define SPI4_BASE			(APB_BASE + SPI4_OFFSET)
#define SPI4_REG(x)			(SPI4_BASE + (x))
#endif

#if (SPI_AHB_INSTANCES >= 1)
#define SSI_DMA_OFFSET			0xD000
#define SSI_DMA_BASE			(AHB_BASE + SSI_DMA_OFFSET)
#define SSI_DMA_REG(x)			(SSI_DMA_BASE + (x))
#endif

#if (SPI_AHB_SLAVE_INSTANCES >= 1)
#define SPI_AHB_SLAVE_OFFSET		0x26000
#define SPI_AHB_SLAVE_BASE		(AHB_BASE + SPI_AHB_SLAVE_OFFSET)
#define SPI_AHB_SLAVE_REG(x)		(SPI_AHB_SLAVE_BASE + (x))
#endif

/* ==========================================================================*/
/* SPI_FIFO_SIZE */
#define SPI_DATA_FIFO_SIZE_16		0x10
#define SPI_DATA_FIFO_SIZE_32		0x20
#define SPI_DATA_FIFO_SIZE_64		0x40
#define SPI_DATA_FIFO_SIZE_128		0x80

/****************************************************/
/* Controller registers definitions                 */
/****************************************************/

#define SPI_CTRLR0_OFFSET		0x00
#define SPI_CTRLR1_OFFSET		0x04
#define SPI_SSIENR_OFFSET		0x08
#define SPI_MWCR_OFFSET			0x0c
#define SPI_SER_OFFSET			0x10
#define SPI_BAUDR_OFFSET		0x14
#define SPI_TXFTLR_OFFSET		0x18
#define SPI_RXFTLR_OFFSET		0x1c
#define SPI_TXFLR_OFFSET		0x20
#define SPI_RXFLR_OFFSET		0x24
#define SPI_SR_OFFSET			0x28
#define SPI_IMR_OFFSET			0x2c
#define SPI_ISR_OFFSET			0x30
#define SPI_RISR_OFFSET			0x34
#define SPI_TXOICR_OFFSET		0x38
#define SPI_RXOICR_OFFSET		0x3c
#define SPI_RXUICR_OFFSET		0x40
#define SPI_MSTICR_OFFSET		0x44
#define SPI_ICR_OFFSET			0x48
#if (SPI_AHB_INSTANCES >= 1)
#define SPI_DMAC_OFFSET			0x4c
#endif
#define SPI_IDR_OFFSET			0x58
#define SPI_VERSION_ID_OFFSET		0x5c
#define SPI_DR_OFFSET			0x60

#if (SPI_SUPPORT_MASTER_CHANGE_ENA_POLARITY == 1)
#define SPI_SSIENPOLR_OFFSET		0x260
#endif
#if (SPI_SUPPORT_MASTER_DELAY_START_TIME == 1)
#define SPI_SCLK_OUT_DLY_OFFSET		0x264
#endif
#if (SPI_SUPPORT_NSM_SHAKE_START_BIT_CHSANGE == 1)
#define SPI_START_BIT_OFFSET		0x268
#endif

/* ==========================================================================*/

#define SPI_MASTER_INSTANCES	(SPI_INSTANCES + SPI_AHB_INSTANCES)

/* ==========================================================================*/
/* SPI rw mode */
#define SPI_WRITE_READ		0
#define SPI_WRITE_ONLY		1
#define SPI_READ_ONLY		2

/* Tx FIFO empty interrupt mask */
#define SPI_TXEIS_MASK		0x00000001
#define SPI_TXOIS_MASK 		0x00000002

/* SPI Parameters */
#define SPI_DUMMY_DATA		0xffff
#define MAX_QUERY_TIMES		10

/* Default SPI settings */
#define SPI_MODE		SPI_MODE_0
#define SPI_SCPOL		0
#define SPI_SCPH		0
#define SPI_FRF			0
#define SPI_CFS			0x0
#define SPI_DFS			0xf
#define SPI_BAUD_RATE		200000

/* ==========================================================================*/
#define SSI0_CLK	GPIO(2)
#define SSI0_MOSI	GPIO(3)
#define SSI0_MISO	GPIO(4)
#define SSI0_EN0	GPIO(5)
#define SSI0_EN1	GPIO(6)
#if (CHIP_REV == S2)
#define SSIO_EN2	GPIO(7)
#define SSIO_EN3	GPIO(9)
#else
#define SSIO_EN2	GPIO(48)
#define SSIO_EN3	GPIO(49)
#endif
#if (CHIP_REV == A7L)
#define SSI_4_N		GPIO(88)
#define SSI_5_N		GPIO(89)
#define SSI_6_N		GPIO(90)
#define SSI_7_N		GPIO(91)
#elif (CHIP_REV == S2)
#define SSI_4_N		GPIO(82)
#define SSI_5_N		GPIO(83)
#define SSI_6_N		GPIO(79)
#define SSI_7_N		GPIO(80)
#else
#define SSI_4_N		GPIO(77)
#define SSI_5_N		GPIO(78)
#define SSI_6_N		GPIO(79)
#define SSI_7_N		GPIO(80)
#endif

#define SSI2CLK		GPIO(88)
#define SSI2MOSI	GPIO(89)
#define SSI2MISO	GPIO(90)
#define SSI2_0EN	GPIO(91)

#define SSI3_EN0	GPIO(128)
#define SSI3_EN1	GPIO(129)
#define SSI3_EN2	GPIO(130)
#define SSI3_EN3	GPIO(131)
#define SSI3_EN4	GPIO(132)
#define SSI3_EN5	GPIO(133)
#define SSI3_EN6	GPIO(134)
#define SSI3_EN7	GPIO(135)
#define SSI3_MOSI	GPIO(136)
#define SSI3_CLK	GPIO(137)
#define SSI3_MISO	GPIO(138)

#define SSI_SLAVE_MISO	GPIO(50)
#define SSI_SLAVE_EN	GPIO(51)
#define SSI_SLAVE_MOSI	GPIO(52)
#define SSI_SLAVE_CLK	GPIO(53)

/* ==========================================================================*/
#ifndef __ASSEMBLER__

struct ambarella_spi_hw_info {
	int	bus_id;
	int	cs_id;
};
typedef struct ambarella_spi_hw_info amba_spi_hw_t;

struct ambarella_spi_cfg_info {
	u8	spi_mode;
	u8	cfs_dfs;
	u8	cs_change;
	u32	baud_rate;
};
typedef struct ambarella_spi_cfg_info amba_spi_cfg_t;

typedef struct {
	u8	bus_id;
	u8	cs_id;
	u8	*buffer;
	u32	n_size;	// u16	n_size;
} amba_spi_write_t;

typedef struct {
	u8	bus_id;
	u8	cs_id;
	u8	*buffer;
	u16	n_size;
} amba_spi_read_t;

typedef struct {
	u8	bus_id;
	u8	cs_id;
	u8	*w_buffer;
	u8	*r_buffer;
	u16	w_size;
	u16	r_size;
} amba_spi_write_then_read_t;

typedef struct {
	u8	bus_id;
	u8	cs_id;
	u8	*w_buffer;
	u8	*r_buffer;
	u16	n_size;
} amba_spi_write_and_read_t;

struct ambarella_spi_cs_config {
	u8 bus_id;
	u8 cs_id;
	u8 cs_num;
	int *cs_pins;
	int *cs_high;
};

struct ambarella_spi_platform_info {
	int support_dma;
	int fifo_entries;
	int cs_num;
	int *cs_pins;
	int *cs_high;
	void (*cs_activate)(struct ambarella_spi_cs_config *);
	void (*cs_deactivate)(struct ambarella_spi_cs_config *);
	void (*rct_set_ssi_pll)(void);
	u32 (*get_ssi_freq_hz)(void);
};

#define AMBA_SPI_CS_PINS_PARAM_CALL(id, arg, perm) \
	module_param_cb(spi##id##_cs0, &param_ops_int, &(arg[0]), perm); \
	module_param_cb(spi##id##_cs1, &param_ops_int, &(arg[1]), perm); \
	module_param_cb(spi##id##_cs2, &param_ops_int, &(arg[2]), perm); \
	module_param_cb(spi##id##_cs3, &param_ops_int, &(arg[3]), perm); \
	module_param_cb(spi##id##_cs4, &param_ops_int, &(arg[4]), perm); \
	module_param_cb(spi##id##_cs5, &param_ops_int, &(arg[5]), perm); \
	module_param_cb(spi##id##_cs6, &param_ops_int, &(arg[6]), perm); \
	module_param_cb(spi##id##_cs7, &param_ops_int, &(arg[7]), perm)

#define AMBA_SPI_CS_HIGH_PARAM_CALL(id, arg, perm) \
	module_param_cb(spi##id##_cs_high_0, &param_ops_int, &(arg[0]), perm); \
	module_param_cb(spi##id##_cs_high_1, &param_ops_int, &(arg[1]), perm); \
	module_param_cb(spi##id##_cs_high_2, &param_ops_int, &(arg[2]), perm); \
	module_param_cb(spi##id##_cs_high_3, &param_ops_int, &(arg[3]), perm); \
	module_param_cb(spi##id##_cs_high_4, &param_ops_int, &(arg[4]), perm); \
	module_param_cb(spi##id##_cs_high_5, &param_ops_int, &(arg[5]), perm); \
	module_param_cb(spi##id##_cs_high_6, &param_ops_int, &(arg[6]), perm); \
	module_param_cb(spi##id##_cs_high_7, &param_ops_int, &(arg[7]), perm)

/* ==========================================================================*/
extern struct platform_device ambarella_spi0;
extern struct platform_device ambarella_spi1;
extern struct platform_device ambarella_spi2;
extern struct platform_device ambarella_spi3;
extern struct platform_device ambarella_spi4;
extern struct platform_device ambarella_spi_slave;

/* ==========================================================================*/
extern int ambarella_spi_write(amba_spi_cfg_t *spi_cfg,
	amba_spi_write_t *spi_write);
extern int ambarella_spi_read(amba_spi_cfg_t *spi_cfg,
	amba_spi_read_t *spi_read);
extern int ambarella_spi_write_then_read(amba_spi_cfg_t *spi_cfg,
	amba_spi_write_then_read_t *spi_write_then_read);
extern int ambarella_spi_write_and_read(amba_spi_cfg_t *spi_cfg,
	amba_spi_write_and_read_t *spi_write_and_read);

#endif /* __ASSEMBLER__ */
/* ==========================================================================*/

#endif /* __PLAT_AMBARELLA_SPI_H__ */

