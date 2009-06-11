/*
 * arch/arm/mach-ambarella/config.c
 *
 * Author: Anthony Ginger <hfjiang@ambarella.com>
 *
 * Copyright (C) 2004-2009, Ambarella, Inc.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/bootmem.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/dma-mapping.h>
#include <linux/proc_fs.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/mmc/host.h>
#include <linux/serial_core.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/i2c.h>

#include <asm/page.h>
#include <asm/io.h>
#include <asm/setup.h>
#include <asm/mach/map.h>

#include <mach/hardware.h>

/* ==========================================================================*/
u64 ambarella_dmamask = DMA_32BIT_MASK;
EXPORT_SYMBOL(ambarella_dmamask);

u32 ambarella_debug_level = AMBA_DEBUG_NULL;
EXPORT_SYMBOL(ambarella_debug_level);

u32 ambarella_debug_info[32];
EXPORT_SYMBOL(ambarella_debug_info);

u32 alsa_tx_enable_flag = 0;
EXPORT_SYMBOL(alsa_tx_enable_flag);

void ambarella_set_gpio_power(struct ambarella_gpio_power_info *pinfo,
	u32 poweron)
{
	if (pinfo == NULL) {
		pr_err("%s: pinfo is NULL.\n", __func__);
		return;
	}

	pr_debug("%s: Power %s gpio[%d], level[%s], delay[%dms].\n",
		__func__,
		poweron ? "ON" : "OFF",
		pinfo->power_gpio,
		pinfo->power_level ? "HIGH" : "LOW",
		pinfo->power_delay);

	if ((pinfo->power_gpio < 0 ) || (pinfo->power_gpio >= ARCH_NR_GPIOS))
		return;

	if (poweron)
		gpio_direction_output(pinfo->power_gpio, pinfo->power_level);
	else
		gpio_direction_output(pinfo->power_gpio, !pinfo->power_level);
	msleep(pinfo->power_delay);
}
EXPORT_SYMBOL(ambarella_set_gpio_power);

void ambarella_set_gpio_reset(struct ambarella_gpio_reset_info *pinfo)
{
	if (pinfo == NULL) {
		pr_err("%s: pinfo is NULL.\n", __func__);
		return;
	}

	pr_debug("%s: Reset gpio[%d], level[%s], delay[%dms], resume[%dms].\n",
		__func__,
		pinfo->reset_gpio,
		pinfo->reset_level ? "HIGH" : "LOW",
		pinfo->reset_delay,
		pinfo->resume_delay);

	if ((pinfo->reset_gpio < 0 ) || (pinfo->reset_gpio >= ARCH_NR_GPIOS))
		return;

	gpio_direction_output(pinfo->reset_gpio, pinfo->reset_level);
	msleep(pinfo->reset_delay);
	gpio_direction_output(pinfo->reset_gpio, !pinfo->reset_level);
	msleep(pinfo->resume_delay);
}
EXPORT_SYMBOL(ambarella_set_gpio_reset);

int ambarella_is_valid_gpio_irq(struct ambarella_gpio_irq_info *pinfo)
{
	int					bvalid = 0;

	if (pinfo == NULL) {
		pr_err("%s: pinfo is NULL.\n", __func__);
		goto ambarella_is_valid_gpio_irq_exit;
	}

	if ((pinfo->irq_gpio < 0 ) || (pinfo->irq_gpio >= ARCH_NR_GPIOS))
		goto ambarella_is_valid_gpio_irq_exit;

	if ((pinfo->irq_type != IRQ_TYPE_EDGE_RISING) &&
		(pinfo->irq_type != IRQ_TYPE_EDGE_FALLING) &&
		(pinfo->irq_type != IRQ_TYPE_EDGE_BOTH) &&
		(pinfo->irq_type != IRQ_TYPE_LEVEL_HIGH) &&
		(pinfo->irq_type != IRQ_TYPE_LEVEL_LOW))
		goto ambarella_is_valid_gpio_irq_exit;

	if ((pinfo->irq_gpio_val != GPIO_HIGH) &&
		(pinfo->irq_gpio_val != GPIO_LOW))
		goto ambarella_is_valid_gpio_irq_exit;

	if ((pinfo->irq_gpio_mode != GPIO_FUNC_SW_INPUT) &&
		(pinfo->irq_gpio_mode != GPIO_FUNC_SW_OUTPUT) &&
		(pinfo->irq_gpio_mode != GPIO_FUNC_HW))
		goto ambarella_is_valid_gpio_irq_exit;

	if ((pinfo->irq_gpio_mode != GPIO_FUNC_HW) &&
		((pinfo->irq_line < GPIO0_INT_VEC_OFFSET ) ||
		(pinfo->irq_line >= MAX_IRQ_NUMBER)))
		goto ambarella_is_valid_gpio_irq_exit;

	bvalid = 1;

ambarella_is_valid_gpio_irq_exit:
	return bvalid;
}
EXPORT_SYMBOL(ambarella_is_valid_gpio_irq);

/* ==========================================================================*/
struct ambarella_mem_map_desc {
	char		name[32];
	struct map_desc	io_desc;
};
static struct ambarella_mem_map_desc ambarella_io_desc[] = {
	[0] = {
		.name		= "AHB",
		.io_desc	= {
			.virtual= AHB_BASE,
			.pfn	= __phys_to_pfn(AHB_START),
			.length	= AHB_SIZE,
			.type	= MT_DEVICE,
			},
	},
	[1] = {
		.name		= "APB",
		.io_desc	= {
			.virtual= APB_BASE,
			.pfn	= __phys_to_pfn(APB_START),
			.length	= APB_SIZE,
			.type	= MT_DEVICE,
			},
	},
	[2] = {
		.name		= "DSP",
		.io_desc	= {
			.virtual= (NOLINUX_MEM_V_START + DEFAULT_DSP_MEM_OFFSET),
			.pfn	= __phys_to_pfn(PHYS_OFFSET + DEFAULT_DSP_MEM_OFFSET),
			.length	= DEFAULT_DSP_MEM_SIZE,
			.type	= MT_DEVICE,
			},
	},
	[3] = {
		.name		= "BSB",
		.io_desc	= {
			.virtual= (NOLINUX_MEM_V_START + DEFAULT_BSB_MEM_OFFSET),
			.pfn	= __phys_to_pfn(PHYS_OFFSET + DEFAULT_BSB_MEM_OFFSET),
			.length	= (DEFAULT_DSP_MEM_OFFSET - DEFAULT_BSB_MEM_OFFSET),
			.type	= MT_DEVICE_CACHED,
			},
	},
};

void __init ambarella_map_io(void)
{
	int					i;

	for (i = 0; i < ARRAY_SIZE(ambarella_io_desc); i++) {
		if (ambarella_io_desc[i].io_desc.length > 0) {
			pr_info("Ambarella: %s\t= 0x%08lx[0x%08lx],0x%08lx %d\n",
				ambarella_io_desc[i].name,
				__pfn_to_phys(ambarella_io_desc[i].io_desc.pfn),
				ambarella_io_desc[i].io_desc.virtual,
				ambarella_io_desc[i].io_desc.length,
				ambarella_io_desc[i].io_desc.type);
			iotable_init(&(ambarella_io_desc[i].io_desc), 1);
		}
	}
}

static void __init early_dsp(char **p)
{
	unsigned long		start = 0;
	unsigned long		size = 0;

	start = memparse(*p, p);
	if (**p == ',')
		size = memparse((*p) + 1, p);

	if ((start & MEM_MAP_CHECK_MASK) || (start < PHYS_OFFSET)) {
		pr_err("Ambarella: Bad dsp mem start 0x%lx\n", start);
		return;
	}

	if (size & MEM_MAP_CHECK_MASK) {
		pr_err("Ambarella: Bad dsp mem size 0x%lx\n", size);
		return;
	}

	ambarella_io_desc[2].io_desc.virtual =
		(start - PHYS_OFFSET) + NOLINUX_MEM_V_START;
	ambarella_io_desc[2].io_desc.pfn = __phys_to_pfn(start);
	ambarella_io_desc[2].io_desc.length = size;
}
__early_param("dsp=", early_dsp);

static int __init parse_mem_tag_dsp(const struct tag *tag)
{
	if ((tag->u.mem.start & MEM_MAP_CHECK_MASK) ||
		(tag->u.mem.start < PHYS_OFFSET)) {
		pr_err("Ambarella: Bad dsp mem start 0x%x\n", tag->u.mem.start);
		return -EINVAL;
	}

	if (tag->u.mem.size & MEM_MAP_CHECK_MASK) {
		pr_err("Ambarella: Bad dsp mem size 0x%x\n", tag->u.mem.size);
		return -EINVAL;
	}

	ambarella_io_desc[2].io_desc.virtual = 
		(tag->u.mem.start - PHYS_OFFSET) + NOLINUX_MEM_V_START;
	ambarella_io_desc[2].io_desc.pfn = __phys_to_pfn(tag->u.mem.start);
	ambarella_io_desc[2].io_desc.length = tag->u.mem.size;

	return 0;
}
__tagtable(ATAG_AMBARELLA_DSP, parse_mem_tag_dsp);

static void __init early_bsb(char **p)
{
	unsigned long		start = 0;
	unsigned long		size = 0;

	start = memparse(*p, p);
	if (**p == ',')
		size = memparse((*p) + 1, p);

	if ((start & MEM_MAP_CHECK_MASK) || (start < PHYS_OFFSET)) {
		pr_err("Ambarella: Bad bsb mem start 0x%lx\n", start);
		return;
	}

	if (size & MEM_MAP_CHECK_MASK) {
		pr_err("Ambarella: Bad bsb mem size 0x%lx\n", size);
		return;
	}

	ambarella_io_desc[3].io_desc.virtual =
		(start - PHYS_OFFSET) + NOLINUX_MEM_V_START;
	ambarella_io_desc[3].io_desc.pfn = __phys_to_pfn(start);
	ambarella_io_desc[3].io_desc.length = size;
}
__early_param("bsb=", early_bsb);

static int __init parse_mem_tag_bsb(const struct tag *tag)
{
	if ((tag->u.mem.start & MEM_MAP_CHECK_MASK) ||
		(tag->u.mem.start < PHYS_OFFSET)) {
		pr_err("Ambarella: Bad bsb mem start 0x%x\n", tag->u.mem.start);
		return -EINVAL;
	}

	if (tag->u.mem.size & MEM_MAP_CHECK_MASK) {
		pr_err("Ambarella: Bad bsb mem size 0x%x\n", tag->u.mem.size);
		return -EINVAL;
	}

	ambarella_io_desc[3].io_desc.virtual = 
		(tag->u.mem.start - PHYS_OFFSET) + NOLINUX_MEM_V_START;
	ambarella_io_desc[3].io_desc.pfn = __phys_to_pfn(tag->u.mem.start);
	ambarella_io_desc[3].io_desc.length = tag->u.mem.size;

	return 0;
}
__tagtable(ATAG_AMBARELLA_BSB, parse_mem_tag_bsb);

u32 get_ambarella_dspmem_phys(void)
{
	return __pfn_to_phys(ambarella_io_desc[2].io_desc.pfn);
}
EXPORT_SYMBOL(get_ambarella_dspmem_phys);

u32 get_ambarella_dspmem_virt(void)
{
	return ambarella_io_desc[2].io_desc.virtual;
}
EXPORT_SYMBOL(get_ambarella_dspmem_virt);

u32 get_ambarella_dspmem_size(void)
{
	return ambarella_io_desc[2].io_desc.length;
}
EXPORT_SYMBOL(get_ambarella_dspmem_size);

u32 get_ambarella_bsbmem_phys(void)
{
	return __pfn_to_phys(ambarella_io_desc[3].io_desc.pfn);
}
EXPORT_SYMBOL(get_ambarella_bsbmem_phys);

u32 get_ambarella_bsbmem_virt(void)
{
	return ambarella_io_desc[3].io_desc.virtual;
}
EXPORT_SYMBOL(get_ambarella_bsbmem_virt);

u32 get_ambarella_bsbmem_size(void)
{
	return ambarella_io_desc[3].io_desc.length;
}
EXPORT_SYMBOL(get_ambarella_bsbmem_size);

/* ==========================================================================*/
static char ambarella_nand_default_partition_name[MAX_AMBOOT_PARTITION_NR][MAX_AMBOOT_PARTITION_NANE_SIZE];
static struct mtd_partition ambarella_nand_default_partition[MAX_AMBOOT_PARTITION_NR];

static struct ambarella_nand_set ambarella_nand_default_set = {
	.name		= "ambarella_nand_set",
	.nr_chips	= 1,
	.nr_partitions	= 0,
	.partitions	= ambarella_nand_default_partition,
};

// Based on st128w3a
static struct ambarella_nand_timing ambarella_nand_default_timing = {
	.control	= 0x00200110,
	.size		= 0x00004000,
	.timing0	= 0x01010105,
	.timing1	= 0x03030303,
	.timing2	= 0x07041805,
	.timing3	= 0x07041803,
	.timing4	= 0x08030f01,
	.timing5	= 0x00070702,
};

static struct ambarella_platform_nand ambarella_platform_default_nand = {
	.nr_sets    	= 1,
	.sets	        = &ambarella_nand_default_set,
	.timing		= &ambarella_nand_default_timing,
};

static int __init ambarella_add_partition(const struct tag *tag,
	char *name,
	u32 check_size)
{
	int nr_partitions = ambarella_nand_default_set.nr_partitions;
	int errCode = 0;

	if (nr_partitions < MAX_AMBOOT_PARTITION_NR) {
		if (check_size && (tag->u.ramdisk.size == 0)) {
			errCode = -1;
			goto ambarella_add_partition_exit;
		}
		snprintf(ambarella_nand_default_partition_name[nr_partitions],
			MAX_AMBOOT_PARTITION_NANE_SIZE,
			"%s@%d",
			name, nr_partitions);
		ambarella_nand_default_partition[nr_partitions].name =
			ambarella_nand_default_partition_name[nr_partitions];
		ambarella_nand_default_partition[nr_partitions].offset =
			tag->u.ramdisk.start;
		ambarella_nand_default_partition[nr_partitions].size =
			tag->u.ramdisk.size;
		ambarella_nand_default_partition[nr_partitions].mask_flags =
			tag->u.ramdisk.flags;
		ambarella_nand_default_set.nr_partitions++;
	}

ambarella_add_partition_exit:

	return errCode;
}

static int __init parse_partition_tag_bst(const struct tag *tag)
{
	return ambarella_add_partition(tag, "BST", 1);
}
__tagtable(ATAG_AMBARELLA_NAND_BST, parse_partition_tag_bst);

static int __init parse_partition_tag_ptb(const struct tag *tag)
{
	return ambarella_add_partition(tag, "PTB", 1);
}
__tagtable(ATAG_AMBARELLA_NAND_PTB, parse_partition_tag_ptb);

static int __init parse_partition_tag_bld(const struct tag *tag)
{
	return ambarella_add_partition(tag, "BLD", 1);
}
__tagtable(ATAG_AMBARELLA_NAND_BLD, parse_partition_tag_bld);

static int __init parse_partition_tag_pri(const struct tag *tag)
{
	return ambarella_add_partition(tag, "PRI", 1);
}
__tagtable(ATAG_AMBARELLA_NAND_PRI, parse_partition_tag_pri);

static int __init parse_partition_tag_rom(const struct tag *tag)
{
	return ambarella_add_partition(tag, "ROM", 1);
}
__tagtable(ATAG_AMBARELLA_NAND_ROM, parse_partition_tag_rom);

static int __init parse_partition_tag_ram(const struct tag *tag)
{
	return ambarella_add_partition(tag, "RAM", 1);
}
__tagtable(ATAG_AMBARELLA_NAND_RAM, parse_partition_tag_ram);

static int __init parse_partition_tag_bak(const struct tag *tag)
{
	return ambarella_add_partition(tag, "BAK", 1);
}
__tagtable(ATAG_AMBARELLA_NAND_BAK, parse_partition_tag_bak);

static int __init parse_partition_tag_pba(const struct tag *tag)
{
	return ambarella_add_partition(tag, "PBA", 1);
}
__tagtable(ATAG_AMBARELLA_NAND_PBA, parse_partition_tag_pba);

static int __init parse_partition_tag_dsp(const struct tag *tag)
{
	return ambarella_add_partition(tag, "DSP", 1);
}
__tagtable(ATAG_AMBARELLA_NAND_DSP, parse_partition_tag_dsp);

static int __init parse_partition_tag_nftl(const struct tag *tag)
{
	return ambarella_add_partition(tag, "UserFS", 0);
}
__tagtable(ATAG_AMBARELLA_NAND_NFTL, parse_partition_tag_nftl);

static int __init parse_nand_tag_cs(const struct tag *tag)
{
	ambarella_nand_default_timing.control = tag->u.serialnr.low;
	ambarella_nand_default_timing.size = tag->u.serialnr.high;

	return 0;
}
__tagtable(ATAG_AMBARELLA_NAND_CS, parse_nand_tag_cs);

static int __init parse_nand_tag_t0(const struct tag *tag)
{
	ambarella_nand_default_timing.timing0 = tag->u.serialnr.low;
	ambarella_nand_default_timing.timing1 = tag->u.serialnr.high;

	return 0;
}
__tagtable(ATAG_AMBARELLA_NAND_T0, parse_nand_tag_t0);

static int __init parse_nand_tag_t1(const struct tag *tag)
{
	ambarella_nand_default_timing.timing2 = tag->u.serialnr.low;
	ambarella_nand_default_timing.timing3 = tag->u.serialnr.high;

	return 0;
}
__tagtable(ATAG_AMBARELLA_NAND_T1, parse_nand_tag_t1);

static int __init parse_nand_tag_t2(const struct tag *tag)
{
	ambarella_nand_default_timing.timing4 = tag->u.serialnr.low;
	ambarella_nand_default_timing.timing5 = tag->u.serialnr.high;

	return 0;
}
__tagtable(ATAG_AMBARELLA_NAND_T2, parse_nand_tag_t2);

static struct resource ambarella_nand_resources[] = {
	[0] = {
		.start	= FIO_BASE,
		.end	= FIO_BASE + 0x0FFF,
		.name	= "registers",
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= FIOCMD_IRQ,
		.end	= FIOCMD_IRQ,
		.name	= "ambarella-fio-cmd",
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= FIODMA_IRQ,
		.end	= FIODMA_IRQ,
		.name	= "ambarella-fio-dma",
		.flags	= IORESOURCE_IRQ,
	},
	[3] = {
		.start	= FL_WP,
		.end	= FL_WP,
		.name	= "wp_gpio",
		.flags	= IORESOURCE_IO,
	},
	[4] = {
		.start	= FIO_FIFO_BASE,
		.end	= FIO_FIFO_BASE + 0x0FFF,
		.name	= "dma",
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device ambarella_nand = {
	.name		= "ambarella-nand",
	.id		= -1,
	.resource	= ambarella_nand_resources,
	.num_resources	= ARRAY_SIZE(ambarella_nand_resources),
	.dev		= {
		.platform_data		= &ambarella_platform_default_nand,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_32BIT_MASK,
	}
};

int __init ambarella_init_nand(void *nand_info)
{
	int					errCode = 0;

	if (nand_info)
		ambarella_nand.dev.platform_data = nand_info;

	return errCode;
}

/* ==========================================================================*/
static struct uart_port	ambarella_uart_port_resource[] = {
	[0] = {
		.type		= PORT_UART00,
		.iotype		= UPIO_MEM,
		.membase	= (void *)UART0_BASE,
		.mapbase	= (unsigned long)io_v2p(UART0_BASE),
		.irq		= UART0_IRQ,
		.uartclk	= 27000000,
		.fifosize	= UART_FIFO_SIZE,
		.line		= 0,
	},
#if (UART_PORT_MAX >= 2)
	[1] = {
		.type		= PORT_UART00,
		.iotype		= UPIO_MEM,
		.membase	= (void *)UART1_BASE,
		.mapbase	= (unsigned long)io_v2p(UART1_BASE),
		.irq		= UART1_IRQ,
		.uartclk	= 27000000,
		.fifosize	= UART_FIFO_SIZE,
		.line		= 0,
	},
#endif
};

struct ambarella_uart_platform_info ambarella_uart_ports = {
	.total_port_num		= ARRAY_SIZE(ambarella_uart_port_resource),
	.registed_port_num	= 0,
	.amba_port[0]		= {
		.port		= &ambarella_uart_port_resource[0],
		.name		= "ambarella-uart0",
		.flow_control	= 0,
	},
#if (UART_PORT_MAX >= 2)
	.amba_port[1]		= {
		.port		= &ambarella_uart_port_resource[1],
		.name		= "ambarella-uart1",
		.flow_control	= 1,
	},
#endif
};

struct platform_device ambarella_uart = {
	.name		= "ambarella-uart",
	.id		= 0,
	.resource	= NULL,
	.num_resources	= 0,
	.dev		= {
		.platform_data		= &ambarella_uart_ports,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_32BIT_MASK,
	}
};

/* ==========================================================================*/
static struct resource ambarella_sd0_resources[] = {
	[0] = {
		.start	= SD_BASE,
		.end	= SD_BASE + 0x0FFF,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= SD_IRQ,
		.end	= SD_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static void fio_amb_sd0_slot1_request(void)
{
	fio_select_lock(SELECT_FIO_SD, 1);
}

static void fio_amb_sd0_slot1_release(void)
{
	fio_unlock(SELECT_FIO_SD, 1);
}

#if (SD_HAS_INTERNAL_MUXER == 1)
static void fio_amb_sd0_slot2_request(void)
{
	fio_select_lock(SELECT_FIO_SDIO, 1);
}

static void fio_amb_sd0_slot2_release(void)
{
	fio_unlock(SELECT_FIO_SDIO, 1);
}
#endif

static struct ambarella_sd_controller ambarella_platform_sd_controller0 = {
	.slot[0] = {
		.check_owner	= fio_amb_sd0_is_enable,
		.request	= fio_amb_sd0_slot1_request,
		.release	= fio_amb_sd0_slot1_release,
		.ext_power	= {
			.power_gpio	= -1,
			.power_level	= GPIO_LOW,
			.power_delay	= 1,
		},
		.bounce_buffer	= 1,
#if (SD_HAS_INTERNAL_MUXER == 1)
		.gpio_cd	= {
			.irq_gpio	= SD1_CD,
			.irq_line	= SD1CD_IRQ,
			.irq_type	= IRQ_TYPE_EDGE_BOTH,
			.irq_gpio_val	= GPIO_LOW,
			.irq_gpio_mode	= GPIO_FUNC_HW,
		},
#else
		.gpio_cd	= {
			.irq_gpio	= -1,
			.irq_line	= -1,
			.irq_type	= -1,
			.irq_gpio_val	= GPIO_LOW,
			.irq_gpio_mode	= GPIO_FUNC_SW_INPUT,
		},
#endif
		.cd_delay	= 50,
	},
#if (SD_HAS_INTERNAL_MUXER == 1)
	.slot[1] = {
		.check_owner	= fio_amb_sdio0_is_enable,
		.request	= fio_amb_sd0_slot2_request,
		.release	= fio_amb_sd0_slot2_release,
		.ext_power	= {
			.power_gpio	= -1,
			.power_level	= GPIO_LOW,
			.power_delay	= 1,
		},
		.bounce_buffer	= 0,
		.gpio_cd	= {
			.irq_gpio	= -1,
			.irq_line	= -1,
			.irq_type	= -1,
			.irq_gpio_val	= GPIO_LOW,
			.irq_gpio_mode	= GPIO_FUNC_SW_INPUT,
		},
		.cd_delay	= 50,
	},
	.num_slots		= 2,
#else
	.num_slots		= 1,
#endif
	.clk_limit		= 12500000,
	.wait_tmo		= (5 * HZ),
};

struct platform_device ambarella_sd0 = {
	.name		= "ambarella-sd",
	.id		= 0,
	.resource	= ambarella_sd0_resources,
	.num_resources	= ARRAY_SIZE(ambarella_sd0_resources),
	.dev		= {
		.platform_data		= &ambarella_platform_sd_controller0,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_32BIT_MASK,
	}
};
module_param_call(sd0_clk_limit, param_set_int, param_get_int,
	&(ambarella_platform_sd_controller0.clk_limit), 0644);
module_param_call(sd0_wait_timoout, param_set_int, param_get_int,
	&(ambarella_platform_sd_controller0.wait_tmo), 0644);
AMBA_SD_PARAM_CALL(0, 0, ambarella_platform_sd_controller0, 0644);
#if (SD_HAS_INTERNAL_MUXER == 1)
AMBA_SD_PARAM_CALL(0, 1, ambarella_platform_sd_controller0, 0644);
#endif

#if (SD_INSTANCES >= 2)
static struct resource ambarella_sd1_resources[] = {
	[0] = {
		.start	= SD2_BASE,
		.end	= SD2_BASE + 0x0FFF,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= SD2_IRQ,
		.end	= SD2_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static void fio_amb_sd2_request(void)
{
	fio_select_lock(SELECT_FIO_SD2, 1);
}

static void fio_amb_sd2_release(void)
{
	fio_unlock(SELECT_FIO_SD2, 1);
}

static struct ambarella_sd_controller ambarella_platform_sd_controller1 = {
	.slot[0] = {
		.check_owner	= fio_amb_sd2_is_enable,
		.request	= fio_amb_sd2_request,
		.release	= fio_amb_sd2_release,
		.ext_power	= {
			.power_gpio	= -1,
			.power_level	= GPIO_LOW,
			.power_delay	= 1,
		},
		.bounce_buffer	= 0,
		.gpio_cd	= {
			.irq_gpio	= -1,
			.irq_line	= -1,
			.irq_type	= -1,
			.irq_gpio_val	= GPIO_LOW,
			.irq_gpio_mode	= GPIO_FUNC_SW_INPUT,
		},
		.cd_delay	= 50,
	},
	.num_slots		= 1,
	.clk_limit		= 25000000,
	.wait_tmo		= (5 * HZ),
};

struct platform_device ambarella_sd1 = {
#ifdef CONFIG_MMC_AMBARELLA_SDIO
	.name		= "ambarella-sd",
#else
	.name		= "ambarella-sdio",
#endif
	.id		= 1,
	.resource	= ambarella_sd1_resources,
	.num_resources	= ARRAY_SIZE(ambarella_sd1_resources),
	.dev		= {
		.platform_data		= &ambarella_platform_sd_controller1,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_32BIT_MASK,
	}
};
module_param_call(sd1_clk_limit, param_set_int, param_get_int,
	&(ambarella_platform_sd_controller1.clk_limit), 0644);
module_param_call(sd1_wait_timoout, param_set_int, param_get_int,
	&(ambarella_platform_sd_controller1.wait_tmo), 0644);
AMBA_SD_PARAM_CALL(1, 0, ambarella_platform_sd_controller1, 0644);
#endif

/* ==========================================================================*/
static struct resource ambarella_idc0_resources[] = {
	[0] = {
		.start	= IDC_BASE,
		.end	= IDC_BASE + 0x0FFF,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IDC_IRQ,
		.end	= IDC_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct ambarella_idc_platform_info ambarella_idc0_platform_info = {
	.clk_limit	= 100000,
	.bulk_write_num	= 60,
	.class		= (I2C_CLASS_HWMON | I2C_CLASS_TV_ANALOG |
			I2C_CLASS_TV_DIGITAL | I2C_CLASS_DDC | I2C_CLASS_SPD),
};
AMBA_IDC_PARAM_CALL(0, ambarella_idc0_platform_info, 0644);

struct platform_device ambarella_idc0 = {
	.name		= "ambarella-i2c",
	.id		= 0,
	.resource	= ambarella_idc0_resources,
	.num_resources	= ARRAY_SIZE(ambarella_idc0_resources),
	.dev		= {
		.platform_data		= &ambarella_idc0_platform_info,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_32BIT_MASK,
	}
};

/* ==========================================================================*/
static struct resource ambarella_spi0_resources[] = {
	[0] = {
		.start	= SPI_BASE,
		.end	= SPI_BASE + 0x0FFF,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= SSI_IRQ,
		.end	= SSI_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device ambarella_spi0 = {
	.name		= "ambarella-spi",
	.id		= 0,
	.resource	= ambarella_spi0_resources,
	.num_resources	= ARRAY_SIZE(ambarella_spi0_resources),
	.dev		= {
		.platform_data		= NULL,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_32BIT_MASK,
	}
};

#if (SPI_INSTANCES >= 2 )
static struct resource ambarella_spi1_resources[] = {
	[0] = {
		.start	= SPI2_BASE,
		.end	= SPI2_BASE + 0x0FFF,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= SSI2_IRQ,
		.end	= SSI2_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device ambarella_spi1 = {
	.name		= "ambarella-spi",
	.id		= 1,
	.resource	= ambarella_spi1_resources,
	.num_resources	= ARRAY_SIZE(ambarella_spi1_resources),
	.dev		= {
		.platform_data		= NULL,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_32BIT_MASK,
	}
};
#endif

static struct spi_board_info ambarella_spi_devices[] = {
#ifdef CONFIG_SPI_AMBARELLA_SPIDEV_SSI0_EN0
	{
		.modalias	= "spidev",
		.bus_num	= 0,
		.chip_select	= 0,
	},
#endif
#ifdef CONFIG_SPI_AMBARELLA_SPIDEV_SSI0_EN1
	{
		.modalias	= "spidev",
		.bus_num	= 0,
		.chip_select	= 1,
	},
#endif
#ifdef CONFIG_SPI_AMBARELLA_SPIDEV_SSI0_EN2_3
	{
		.modalias	= "spidev",
		.bus_num	= 0,
		.chip_select	= 2,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 0,
		.chip_select	= 3,
	},
#endif
#ifdef CONFIG_SPI_AMBARELLA_SPIDEV_SSI1_EN0
	{
		.modalias	= "spidev",
		.bus_num	= 1,
		.chip_select	= 0,
	}
#endif
};

void __init ambarella_register_spi_device(void)
{
	spi_register_board_info(ambarella_spi_devices,
		ARRAY_SIZE(ambarella_spi_devices));
}

/* ==========================================================================*/
static struct resource ambarella_i2s0_resources[] = {
	[0] = {
		.start	= I2S_BASE,
		.end	= I2S_BASE + 0x0FFF,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= I2STX_IRQ,
		.end	= I2SRX_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static void aucodec_digitalio_on(void)
{
#if (CHIP_REV == A2S) || (CHIP_REV == A2M)
	/* aucodec_digitalio_on */
	amba_setbits(GPIO2_AFSEL_REG, (0xf << 18) | (0xf << 13));
#elif (CHIP_REV == A2)
	/* aucodec_digitalio_on */
	amba_setbits(GPIO2_AFSEL_REG, (0x3 << 15) | (0x3 << 20));
#endif
}

static struct ambarella_i2s_controller ambarella_platform_i2s_controller0 = {
	.aucodec_digitalio = aucodec_digitalio_on,
};

struct platform_device ambarella_i2s0 = {
	.name		= "ambarella-i2s",
	.id		= 0,
	.resource	= ambarella_i2s0_resources,
	.num_resources	= ARRAY_SIZE(ambarella_i2s0_resources),
	.dev		= {
		.platform_data		= &ambarella_platform_i2s_controller0,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_32BIT_MASK,
	}
};

/* ==========================================================================*/
struct platform_device ambarella_rtc0 = {
	.name		= "ambarella-rtc",
	.id		= 0,
	.resource	= NULL,
	.num_resources	= 0,
	.dev		= {
		.platform_data		= NULL,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_32BIT_MASK,
	}
};

/* ==========================================================================*/
static struct resource ambarella_wdt0_resources[] = {
	[0] = {
		.start	= WDOG_BASE,
		.end	= WDOG_BASE + 0x0FFF,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= WDT_IRQ,
		.end	= WDT_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device ambarella_wdt0 = {
	.name		= "ambarella-wdt",
	.id		= 0,
	.resource	= ambarella_wdt0_resources,
	.num_resources	= ARRAY_SIZE(ambarella_wdt0_resources),
	.dev		= {
		.platform_data		= NULL,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_32BIT_MASK,
	}
};

/* ==========================================================================*/
static struct resource ambarella_eth0_resources[] = {
	[0] = {
		.start	= ETH_BASE,
		.end	= ETH_BASE + 0x0FFF,
		.name	= "registers",
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= ETH_IRQ,
		.end	= ETH_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= ETH_DMA_BASE,
		.end	= ETH_DMA_BASE + 0x0FFF,
		.name	= "dma",
		.flags	= IORESOURCE_MEM,
	},
};

struct ambarella_eth_platform_info ambarella_eth0_platform_info = {
	.mac_addr	= {0, 0, 0, 0, 0, 0},
	.napi_weight	= 64,
	.max_work_count	= 5,
	.mii_id		= -1,
	.phy_id		= 0x00008201,
	.mii_power	= {
		.power_gpio	= -1,
		.power_level	= GPIO_LOW,
		.power_delay	= 1,
	},
	.mii_reset	= {
		.reset_gpio	= -1,
		.reset_level	= GPIO_LOW,
		.reset_delay	= 1,
		.resume_delay	= 1,
	},
	.is_enabled	= rct_is_eth_enabled,
};
AMBA_ETH_PARAM_CALL(0, ambarella_eth0_platform_info, 0644);

int __init ambarella_init_eth0(unsigned int high, unsigned int low)
{
	int					errCode = 0;

	ambarella_eth0_platform_info.mac_addr[0] = (low >> 0);
	ambarella_eth0_platform_info.mac_addr[1] = (low >> 8);
	ambarella_eth0_platform_info.mac_addr[2] = (low >> 16);
	ambarella_eth0_platform_info.mac_addr[3] = (low >> 24);
	ambarella_eth0_platform_info.mac_addr[4] = (high >> 0);
	ambarella_eth0_platform_info.mac_addr[5] = (high >> 8);

	return errCode;
}

struct platform_device ambarella_eth0 = {
	.name		= "ambarella-eth",
	.id		= 0,
	.resource	= ambarella_eth0_resources,
	.num_resources	= ARRAY_SIZE(ambarella_eth0_resources),
	.dev		= {
		.platform_data		= &ambarella_eth0_platform_info,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_32BIT_MASK,
	}
};

/* ==========================================================================*/
struct resource ambarella_udc_resources[] = {
	[0] = {
		.start	= USBDC_BASE,
		.end	= USBDC_BASE + 0x1FFF,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= USBC_IRQ,
		.end	= USBC_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device ambarella_udc0 = {
	.name		= "ambarella-udc",
	.id		= 0,
	.resource	= ambarella_udc_resources,
	.num_resources	= ARRAY_SIZE(ambarella_udc_resources),
	.dev		= {
		.platform_data		= NULL,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_32BIT_MASK,
	}
};

/* ==========================================================================*/
struct resource ambarella_ir_resources[] = {
	[0] = {
		.start	= IR_BASE,
		.end	= IR_BASE + 0x0FFF,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRIF_IRQ,
		.end	= IRIF_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= IR_IN,
		.end	= IR_IN,
		.flags	= IORESOURCE_IO,
	},
};

struct platform_device ambarella_ir0 = {
	.name		= "ambarella-ir",
	.id		= 0,
	.resource	= ambarella_ir_resources,
	.num_resources	= ARRAY_SIZE(ambarella_ir_resources),
	.dev			= {
		.platform_data		= NULL,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_32BIT_MASK,
	}
};

/* ==========================================================================*/
struct resource ide0_resources[] = {
	[0] = {
		.start	= FIO_BASE,
		.end	= FIO_BASE + 0x0FFF,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device ambarella_ide0 = {
	.name		= "ambarella-ide",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(ide0_resources),
	.resource	= ide0_resources,
	.dev			= {
		.platform_data		= NULL,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_32BIT_MASK,
	}
};

/* ==========================================================================*/
static struct proc_dir_entry *ambarella_proc_dir = NULL;

int __init ambarella_create_proc_dir(void)
{
	int					errCode = 0;

	ambarella_proc_dir = proc_mkdir("ambarella", NULL);
	if (!ambarella_proc_dir)
		errCode = -ENOMEM;

	return errCode;
}

struct proc_dir_entry *get_ambarella_proc_dir(void)
{
	return ambarella_proc_dir;
}
EXPORT_SYMBOL(get_ambarella_proc_dir);

