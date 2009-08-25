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

#include <linux/i2c/tsc2007.h>

#include "init.h"

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

u32 ambarella_get_gpio_input(struct ambarella_gpio_input_info *pinfo)
{
	u32					gpio_value;

	if (pinfo == NULL) {
		pr_err("%s: pinfo is NULL.\n", __func__);
		return 0;
	}

	gpio_direction_input(pinfo->input_gpio);
	msleep(pinfo->input_delay);
	gpio_value = gpio_get_value(pinfo->input_gpio);

	pr_debug("%s: {gpio[%d], level[%s], delay[%dms]} get[%d].\n",
		__func__,
		pinfo->input_gpio,
		pinfo->input_level ? "HIGH" : "LOW",
		pinfo->input_delay,
		gpio_value);

	return (gpio_value == pinfo->input_level) ? 1 : 0;
}
EXPORT_SYMBOL(ambarella_get_gpio_input);

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

#if (CHIP_REV == A5S)
	amb_hal_success_t			errorCode;

	errorCode = amb_set_peripherals_base_address(HAL_BASE_VP,
		(void *)APB_BASE, (void *)AHB_BASE);
	BUG_ON(errorCode != AMB_HAL_SUCCESS);
#endif

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
	unsigned long				start = 0;
	unsigned long				size = 0;

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
	unsigned long				start = 0;
	unsigned long				size = 0;

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

u32 ambarella_phys_to_virt(u32 paddr)
{
	int					i;
	u32					phystart;
	u32					phylength;
	u32					phyoffset;
	u32					vstart;

	for (i = 0; i < ARRAY_SIZE(ambarella_io_desc); i++) {
		phystart = __pfn_to_phys(ambarella_io_desc[i].io_desc.pfn);
		phylength = ambarella_io_desc[i].io_desc.length;
		vstart = ambarella_io_desc[i].io_desc.virtual;
		if ((paddr >= phystart) && (paddr < (phystart + phylength))) {
			phyoffset = paddr - phystart;
			return (vstart + phyoffset);
		}
	}

	return __phys_to_virt(paddr);
}
EXPORT_SYMBOL(ambarella_phys_to_virt);

u32 ambarella_virt_to_phys(u32 vaddr)
{
	int					i;
	u32					phystart;
	u32					vlength;
	u32					voffset;
	u32					vstart;

	for (i = 0; i < ARRAY_SIZE(ambarella_io_desc); i++) {
		phystart = __pfn_to_phys(ambarella_io_desc[i].io_desc.pfn);
		vlength = ambarella_io_desc[i].io_desc.length;
		vstart = ambarella_io_desc[i].io_desc.virtual;
		if ((vaddr >= vstart) && (vaddr < (vstart + vlength))) {
			voffset = vaddr - vstart;
			return (phystart + voffset);
		}
	}

	return __virt_to_phys(vaddr);
}
EXPORT_SYMBOL(ambarella_virt_to_phys);

/* ==========================================================================*/
static struct ambarella_mem_hal_desc ambarella_hal_info = {
	.physaddr			= DEFAULT_HAL_BASE,
	.size				= DEFAULT_HAL_SIZE,
	.virtual			= __phys_to_virt(DEFAULT_HAL_BASE),
};

static void __init early_hal(char **p)
{
	unsigned long				start = 0;
	unsigned long				size = 0;

	start = memparse(*p, p);
	if (**p == ',')
		size = memparse((*p) + 1, p);

	if ((start & MEMORY_RESERVE_CHECK_MASK) || (start < PHYS_OFFSET)) {
		pr_err("Ambarella: Bad HAL start 0x%lx\n", start);
		return;
	}

	ambarella_hal_info.physaddr = start;
	ambarella_hal_info.size = size;
	ambarella_hal_info.virtual =
		__phys_to_virt(ambarella_hal_info.physaddr);
}
__early_param("hal=", early_hal);

static int __init parse_mem_tag_hal(const struct tag *tag)
{
	if ((tag->u.mem.start & MEMORY_RESERVE_CHECK_MASK) ||
		(tag->u.mem.start < PHYS_OFFSET)) {
		pr_err("Ambarella: Bad HAL start 0x%x\n", tag->u.mem.start);
		return -EINVAL;
	}

	ambarella_hal_info.physaddr = tag->u.mem.start;
	ambarella_hal_info.size = tag->u.mem.size;
	ambarella_hal_info.virtual =
		__phys_to_virt(ambarella_hal_info.physaddr);

	return 0;
}
__tagtable(ATAG_AMBARELLA_HAL, parse_mem_tag_hal);

void *get_ambarella_hal_vp(void)
{
	return (void *)ambarella_hal_info.virtual;
}
EXPORT_SYMBOL(get_ambarella_hal_vp);

/* ==========================================================================*/
static struct ambarella_mem_rev_info ambarella_reserve_mem_info = {
	.counter			= 1,
	.desc				= {
		[0]			= {
			.physaddr	= RESERVE_MEM_P_START,
			.size		= RESERVE_MEM_SIZE,
		},
	},
};

static void __init early_revmem(char **p)
{
	unsigned long				start = 0;
	unsigned long				size = 0;
	u32					index;

	start = memparse(*p, p);
	if (**p == ',')
		size = memparse((*p) + 1, p);

	if ((start & MEMORY_RESERVE_CHECK_MASK) || (start < PHYS_OFFSET)) {
		pr_err("Ambarella: Bad revmem start 0x%lx\n", start);
		return;
	}

	if (size & MEMORY_RESERVE_CHECK_MASK) {
		pr_err("Ambarella: Bad revmem size 0x%lx\n", size);
		return;
	}

	index = ambarella_reserve_mem_info.counter;
	if (index < MEMORY_RESERVE_MAX_NR) {
		ambarella_reserve_mem_info.desc[index].physaddr = start;
		ambarella_reserve_mem_info.desc[index].size = size;
		ambarella_reserve_mem_info.counter++;
	} else {
		pr_err("Ambarella: too much revmem %d!\n",
			MEMORY_RESERVE_MAX_NR);
	}
}
__early_param("revmem=", early_revmem);

static int __init parse_mem_tag_revmem(const struct tag *tag)
{
	u32					index;

	if ((tag->u.mem.start & MEMORY_RESERVE_CHECK_MASK) ||
		(tag->u.mem.start < PHYS_OFFSET)) {
		pr_err("Ambarella: Bad revmem start 0x%x\n", tag->u.mem.start);
		return -EINVAL;
	}

	if (tag->u.mem.size & MEMORY_RESERVE_CHECK_MASK) {
		pr_err("Ambarella: Bad revmem size 0x%x\n", tag->u.mem.size);
		return -EINVAL;
	}

	index = ambarella_reserve_mem_info.counter;
	if (index < MEMORY_RESERVE_MAX_NR) {
		ambarella_reserve_mem_info.desc[index].physaddr =
			tag->u.mem.start;
		ambarella_reserve_mem_info.desc[index].size = tag->u.mem.size;
		ambarella_reserve_mem_info.counter++;
	} else {
		pr_err("Ambarella: too much revmem %d!\n",
			MEMORY_RESERVE_MAX_NR);
		return -EINVAL;
	}

	return 0;
}
__tagtable(ATAG_AMBARELLA_REVMEM, parse_mem_tag_revmem);

u32 get_ambarella_mem_rev_info(struct ambarella_mem_rev_info *pinfo)
{
	int					i;
	u32					revp, revs;
#if (CHIP_REV == A5S)
	u32					halp, hals;
	u32					index;
	int					bhal_reserved = 0;
	int					version;
	amb_hal_success_t			errorCode;
#endif

	pr_info("Ambarella Reserve Memory:\n");

#if (CHIP_REV == A5S)
	halp = ambarella_hal_info.physaddr;
	hals = ambarella_hal_info.size;

	errorCode = amb_get_version(HAL_BASE_VP, &version);
	BUG_ON(errorCode != AMB_HAL_SUCCESS);
#endif

	for (i = 0; i < ambarella_reserve_mem_info.counter; i++) {
		revp = ambarella_reserve_mem_info.desc[i].physaddr;
		revs = ambarella_reserve_mem_info.desc[i].size;
		pr_info("\t%02d:\t0x%08x[0x%08x]\tNormal\n", i, revp, revs);
#if (CHIP_REV == A5S)
		if ((halp >= revp) && ((halp + hals) <= (revp + revs))) {
			bhal_reserved = 1;
			pr_info("\t--:\t0x%08x[0x%08x]\tHAL[v%d]\n",
				halp, hals, version);
		}
#endif
	}

#if (CHIP_REV == A5S)
	if (!bhal_reserved) {
		index = ambarella_reserve_mem_info.counter;
		hals = ((hals + (PAGE_SIZE - 1)) & PAGE_MASK);
		if (index < MEMORY_RESERVE_MAX_NR) {
			ambarella_reserve_mem_info.desc[index].physaddr = halp;
			ambarella_reserve_mem_info.desc[index].size = hals;
			pr_info("\t%02d:\t0x%08x[0x%08x]\tHAL[v%d]\n",
				ambarella_reserve_mem_info.counter,
				halp, hals, version);
			ambarella_reserve_mem_info.counter++;
		} else {
			pr_err("Ambarella: Can't add HAL MEM into revmem!\n");
			return -EINVAL;
		}
	}
#endif

	*pinfo = ambarella_reserve_mem_info;

	return 0;
}
EXPORT_SYMBOL(get_ambarella_mem_rev_info);

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
	.sets		= &ambarella_nand_default_set,
	.timing		= &ambarella_nand_default_timing,
};

static int __init ambarella_add_partition(const struct tag *tag,
	char *name,
	u32 check_size)
{
	int					errCode = 0;
	int					nr_partitions;

	nr_partitions = ambarella_nand_default_set.nr_partitions;
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
#if (UART_INSTANCE >= 2)
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
#if (UART_INSTANCE >= 2)
	.amba_port[1]		= {
		.port		= &ambarella_uart_port_resource[1],
		.name		= "ambarella-uart1",
		.flow_control	= 1,
	},
#endif
	.set_pll		= rct_set_uart_pll,
	.get_pll		= get_uart_freq_hz,
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

#if (UART_INSTANCE >= 2)
struct platform_device ambarella_uart1 = {
	.name		= "ambarella-uart",
	.id		= 1,
	.resource	= NULL,
	.num_resources	= 0,
	.dev		= {
		.platform_data		= &ambarella_uart_ports,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_32BIT_MASK,
	}
};
#endif

/* ==========================================================================*/
static u32 sd_pll_enabled = 0;
void set_sd_pll(void)
{
	if (!sd_pll_enabled) {
		sd_pll_enabled = 1;
		rct_set_sd_pll(48000000);
	}
}

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
		.gpio_wp	= {
			.input_gpio	= -1,
			.input_level	= GPIO_LOW,
			.input_delay	= 1,
		},
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
		.gpio_wp	= {
			.input_gpio	= -1,
			.input_level	= GPIO_LOW,
			.input_delay	= 1,
		},
	},
	.num_slots		= 2,
#else
	.num_slots		= 1,
#endif
	.clk_limit		= 12500000,
	.wait_tmo		= (5 * HZ),
	.set_pll		= set_sd_pll,
	.get_pll		= get_sd_freq_hz,
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
		.gpio_wp	= {
			.input_gpio	= -1,
			.input_level	= GPIO_LOW,
			.input_delay	= 1,
		},
	},
	.num_slots		= 1,
	.clk_limit		= 25000000,
	.wait_tmo		= (5 * HZ),
	.set_pll		= set_sd_pll,
	.get_pll		= get_sd_freq_hz,
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
#define DEFAULT_I2C_CLASS	(I2C_CLASS_HWMON | I2C_CLASS_TV_ANALOG | I2C_CLASS_TV_DIGITAL | I2C_CLASS_SPD)

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

#if (IDC_SUPPORT_PIN_MUXING_FOR_HDMI == 1)
static void ambarella_idc_set_pin_muxing(u32 on)
{
	if (on)
		ambarella_gpio_config(IDC_BUS_HDMI, GPIO_FUNC_HW);
	else
		ambarella_gpio_config(IDC_BUS_HDMI, GPIO_FUNC_SW_OUTPUT);
}
#elif (IDC_SUPPORT_INTERNAL_MUX == 1)
static void ambarella_idc_set_pin_muxing(u32 on)
{
	if (on)
		ambarella_gpio_config(IDC3_BUS_MUX, GPIO_FUNC_HW);
	else
		ambarella_gpio_config(IDC3_BUS_MUX, GPIO_FUNC_SW_INPUT);
}
#endif

static struct ambarella_idc_platform_info ambarella_idc0_platform_info = {
	.clk_limit	= 100000,
	.bulk_write_num	= 60,
#if (IDC_SUPPORT_PIN_MUXING_FOR_HDMI == 1)
	.i2c_class	= DEFAULT_I2C_CLASS | I2C_CLASS_DDC,
	.set_pin_muxing	= ambarella_idc_set_pin_muxing,
#elif (IDC_SUPPORT_INTERNAL_MUX == 1)
	.i2c_class	= DEFAULT_I2C_CLASS,
	.set_pin_muxing	= ambarella_idc_set_pin_muxing,
#else
	.i2c_class	= DEFAULT_I2C_CLASS,
	.set_pin_muxing	= NULL,
#endif
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

#if (IDC_INSTANCES >= 2)
static struct resource ambarella_idc1_resources[] = {
	[0] = {
		.start	= IDC2_BASE,
		.end	= IDC2_BASE + 0x0FFF,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IDC2_IRQ,
		.end	= IDC2_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct ambarella_idc_platform_info ambarella_idc1_platform_info = {
	.clk_limit	= 100000,
	.bulk_write_num	= 60,
	.i2c_class	= I2C_CLASS_DDC,
	.set_pin_muxing	= NULL,
};
AMBA_IDC_PARAM_CALL(1, ambarella_idc1_platform_info, 0644);

struct platform_device ambarella_idc1 = {
	.name		= "ambarella-i2c",
	.id		= 1,
	.resource	= ambarella_idc1_resources,
	.num_resources	= ARRAY_SIZE(ambarella_idc1_resources),
	.dev		= {
		.platform_data		= &ambarella_idc1_platform_info,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_32BIT_MASK,
	}
};
#endif

/* ==========================================================================*/
static void ambarella_spi_cs_activate(struct ambarella_spi_cs_config *cs_config)
{
	u8			cs_pin;

	if (cs_config->bus_id >= SPI_INSTANCES || cs_config->cs_id >= cs_config->cs_num)
		return;

	cs_pin = cs_config->cs_pins[cs_config->cs_id];
	if (cs_config->cs_change) {
		if (cs_config->bus_id == 0 && (cs_config->cs_id == 2 || cs_config->cs_id == 3))
			amba_writel(HOST_ENABLE_REG, amba_readl(HOST_ENABLE_REG) | SPI0_CS2_CS3_EN);
		ambarella_gpio_config(cs_pin, GPIO_FUNC_HW);
	} else {
		if (cs_config->bus_id == 0 && (cs_config->cs_id == 2 || cs_config->cs_id == 3))
			amba_writel(HOST_ENABLE_REG, amba_readl(HOST_ENABLE_REG) & ~SPI0_CS2_CS3_EN);
		ambarella_gpio_config(cs_pin, GPIO_FUNC_SW_OUTPUT);
		ambarella_gpio_set(cs_pin, 0);
	}
}

static void ambarella_spi_cs_deactivate(struct ambarella_spi_cs_config *cs_config)
{
	u8			cs_pin;

	if (cs_config->bus_id >= SPI_INSTANCES || cs_config->cs_id >= cs_config->cs_num)
		return;

	cs_pin = cs_config->cs_pins[cs_config->cs_id];
	if (cs_config->cs_change)
		return;
	else
		ambarella_gpio_set(cs_pin, 1);
}

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

static int ambarella_spi0_cs_pins[] = {SSI0_EN0, SSI0_EN1, SSIO_EN2, SSIO_EN3, -1, -1, -1, -1};
AMBA_SPI_PARAM_CALL(0, ambarella_spi0_cs_pins, 0644);
static struct ambarella_spi_platform_info ambarella_spi0_platform_info = {
	.use_interrupt		= 1,
	.cs_num			= ARRAY_SIZE(ambarella_spi0_cs_pins),
	.cs_pins		= ambarella_spi0_cs_pins,
	.cs_activate		= ambarella_spi_cs_activate,
	.cs_deactivate		= ambarella_spi_cs_deactivate,
	.rct_set_ssi_pll	= rct_set_ssi_pll,
	.get_ssi_freq_hz	= get_ssi_freq_hz
};

struct platform_device ambarella_spi0 = {
	.name		= "ambarella-spi",
	.id		= 0,
	.resource	= ambarella_spi0_resources,
	.num_resources	= ARRAY_SIZE(ambarella_spi0_resources),
	.dev		= {
		.platform_data		= &ambarella_spi0_platform_info,
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

static int ambarella_spi1_cs_pins[] = {SSI_4_N, -1, -1, -1, -1, -1, -1, -1};
AMBA_SPI_PARAM_CALL(1, ambarella_spi1_cs_pins, 0644);
static struct ambarella_spi_platform_info ambarella_spi1_platform_info = {
	.use_interrupt		= 1,
	.cs_num			= ARRAY_SIZE(ambarella_spi1_cs_pins),
	.cs_pins		= ambarella_spi1_cs_pins,
	.cs_activate		= ambarella_spi_cs_activate,
	.cs_deactivate		= ambarella_spi_cs_deactivate,
	.rct_set_ssi_pll	= rct_set_ssi2_pll,
	.get_ssi_freq_hz	= get_ssi2_freq_hz
};

struct platform_device ambarella_spi1 = {
	.name		= "ambarella-spi",
	.id		= 1,
	.resource	= ambarella_spi1_resources,
	.num_resources	= ARRAY_SIZE(ambarella_spi1_resources),
	.dev		= {
		.platform_data		= &ambarella_spi1_platform_info,
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
	/* aucodec_digitalio_on */
#if (CHIP_REV == A2S) || (CHIP_REV == A2M)
	amba_setbits(GPIO2_AFSEL_REG, (0xf << 18) | (0xf << 13));

#elif (CHIP_REV == A2)
	amba_setbits(GPIO2_AFSEL_REG, (0x3 << 15) | (0x3 << 20));

#elif (CHIP_REV == A3) || (CHIP_REV == A5) || (CHIP_REV == A6)
	amba_clrbits(GPIO1_AFSEL_REG, 0x80000000);
	/* GPIO77~GPIO85 program as hardware mode */
	amba_setbits(GPIO2_AFSEL_REG, 0x003fe000);
#elif (CHIP_REV == A5S)
	amba_clrbits(GPIO1_AFSEL_REG, 0x80000000);
	amba_setbits(GPIO2_AFSEL_REG, (0xf << 18) | (0xf << 13));
#else
	pr_err("aucodec_digitalio_on: Unknown Chip Architecture\n");
#endif
}

static void i2s_channel_select(u32 ch)
{
#if (CHIP_REV == A3) || (CHIP_REV == A5) || (CHIP_REV == A6)
	switch (ch) {
	case 2:
		amba_writel(I2S_CHANNEL_SELECT_REG, I2S_2CHANNELS_ENB);
		break;
	case 4:
		amba_writel(I2S_CHANNEL_SELECT_REG, I2S_4CHANNELS_ENB);
		break;
	case 6:
		amba_writel(I2S_CHANNEL_SELECT_REG, I2S_6CHANNELS_ENB);
		break;
	default:
		printk("Don't support %d channels\n", ch);
		break;
	}
#endif
}

static void set_audio_pll(u8 mclk)
{
	rct_set_aud_ctrl2_reg();
	rct_set_pll_frac_mode();
	rct_set_audio_pll_fs(AUC_CLK_ONCHIP_PLL_27MHZ, mclk);
}

static struct ambarella_i2s_controller ambarella_platform_i2s_controller0 = {
	.aucodec_digitalio	= aucodec_digitalio_on,
	.channel_select		= i2s_channel_select,
	.set_audio_pll		= set_audio_pll,
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

static struct ambarella_wdt_controller ambarella_platform_wdt_controller0 = {
	.get_pll		= get_apb_bus_freq_hz,
};

struct platform_device ambarella_wdt0 = {
	.name		= "ambarella-wdt",
	.id		= 0,
	.resource	= ambarella_wdt0_resources,
	.num_resources	= ARRAY_SIZE(ambarella_wdt0_resources),
	.dev		= {
		.platform_data		= &ambarella_platform_wdt_controller0,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_32BIT_MASK,
	}
};

/* ==========================================================================*/
static struct resource ambarella_eth0_resources[] = {
	[0] = {
		.start	= ETH_BASE,
		.end	= ETH_BASE + 0x1FFF,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= ETH_IRQ,
		.end	= ETH_IRQ,
		.flags	= IORESOURCE_IRQ,
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

static struct ambarella_udc_controller ambarella_platform_udc_controller0 = {
	.set_pll	= rct_set_usb_ana_on,
};

struct platform_device ambarella_udc0 = {
	.name		= "ambarella-udc",
	.id		= 0,
	.resource	= ambarella_udc_resources,
	.num_resources	= ARRAY_SIZE(ambarella_udc_resources),
	.dev		= {
		.platform_data		= &ambarella_platform_udc_controller0,
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

static struct ambarella_ir_controller ambarella_platform_ir_controller0 = {
	.set_pll		= rct_set_ir_pll,
	.get_pll		= get_ir_freq_hz,
};

struct platform_device ambarella_ir0 = {
	.name		= "ambarella-ir",
	.id		= 0,
	.resource	= ambarella_ir_resources,
	.num_resources	= ARRAY_SIZE(ambarella_ir_resources),
	.dev			= {
		.platform_data		= &ambarella_platform_ir_controller0,
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

/* ==========================================================================*/
#ifdef CONFIG_I2C_AMBARELLA_TSC2007

#define TS_GPIO		56

static int ambarella_tsc2007_get_pendown_state(void)
{
	if (ambarella_gpio_get(TS_GPIO))
		return 0;
	else
		return 1;
}

static void ambarella_tsc2007_clear_penirq(void)
{
	ambarella_gpio_ack_irq(gpio_to_irq(TS_GPIO));
}

static int ambarella_tsc2007_init_platform_hw(void)
{
	ambarella_gpio_config(TS_GPIO, GPIO_FUNC_SW_INPUT);

	return set_irq_type(gpio_to_irq(TS_GPIO), IRQF_TRIGGER_FALLING);
}

static void ambarella_tsc2007_exit_platform_hw(void)
{
}

static struct tsc2007_platform_data ambarella_tsc2007_pdata = {
	.model = 2007,
	.x_plate_ohms = 310,
	.fix = {
		.x_invert = 1,
		.y_invert = 0,
		.x_rescale = 1,
		.y_rescale = 1,
		.x_min = 220,
		.x_max = 3836,
		.y_min = 150,
		.y_max = 3768,
	},
	.get_pendown_state = ambarella_tsc2007_get_pendown_state,
	.clear_penirq = ambarella_tsc2007_clear_penirq,
	.init_platform_hw = ambarella_tsc2007_init_platform_hw,
	.exit_platform_hw = ambarella_tsc2007_exit_platform_hw
};

static struct i2c_board_info ambarella_tsc2007_board_info = {
	.type = "tsc2007",
	.addr = 0x90 >> 1,
	.platform_data = &ambarella_tsc2007_pdata,
};

#endif

void __init ambarella_register_i2c_device(void)
{
#ifdef CONFIG_I2C_AMBARELLA_TSC2007
	ambarella_tsc2007_board_info.irq = gpio_to_irq(TS_GPIO);
	i2c_register_board_info(0, &ambarella_tsc2007_board_info, 1);
#endif
}

