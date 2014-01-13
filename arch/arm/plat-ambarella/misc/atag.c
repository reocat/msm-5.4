/*
 * arch/arm/plat-ambarella/misc/atag.c
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
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/bootmem.h>
#include <linux/dma-mapping.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/of_fdt.h>

#include <asm/system_info.h>
#include <asm/page.h>
#include <asm/io.h>
#include <asm/setup.h>
#include <asm/mach/map.h>
#include <asm/pgtable.h>

#include <mach/hardware.h>
#include <mach/board.h>
#include <plat/debug.h>

/* ==========================================================================*/
u32 ambarella_debug_level = AMBA_DEBUG_NULL;
EXPORT_SYMBOL(ambarella_debug_level);

u32 ambarella_debug_info = 0;
EXPORT_SYMBOL(ambarella_debug_info);

u32 ambarella_boot_splash_logo = 0;
EXPORT_SYMBOL(ambarella_boot_splash_logo);

unsigned long ambarella_debug_lookup_name(const char *name)
{
#if defined(CONFIG_KALLSYMS)
	return module_kallsyms_lookup_name(name);
#else
	return 0;
#endif
}
EXPORT_SYMBOL(ambarella_debug_lookup_name);

/* ==========================================================================*/
static int __init parse_eth0_tag_mac(const struct tag *tag)
{
	u32 high, low;

	high = tag->u.serialnr.high;
	low = tag->u.serialnr.low;

	ambarella_board_generic.eth0_mac[0] = (low >> 0);
	ambarella_board_generic.eth0_mac[1] = (low >> 8);
	ambarella_board_generic.eth0_mac[2] = (low >> 16);
	ambarella_board_generic.eth0_mac[3] = (low >> 24);
	ambarella_board_generic.eth0_mac[4] = (high >> 0);
	ambarella_board_generic.eth0_mac[5] = (high >> 8);
	return 0;
}
__tagtable(ATAG_AMBARELLA_ETH0, parse_eth0_tag_mac);

static int __init parse_eth1_tag_mac(const struct tag *tag)
{
	u32 high, low;

	high = tag->u.serialnr.high;
	low = tag->u.serialnr.low;

	ambarella_board_generic.eth1_mac[0] = (low >> 0);
	ambarella_board_generic.eth1_mac[1] = (low >> 8);
	ambarella_board_generic.eth1_mac[2] = (low >> 16);
	ambarella_board_generic.eth1_mac[3] = (low >> 24);
	ambarella_board_generic.eth1_mac[4] = (high >> 0);
	ambarella_board_generic.eth1_mac[5] = (high >> 8);
	return 0;
}
__tagtable(ATAG_AMBARELLA_ETH1, parse_eth1_tag_mac);

static int __init parse_wifi0_tag_mac(const struct tag *tag)
{
	u32 high, low;

	high = tag->u.serialnr.high;
	low = tag->u.serialnr.low;

	ambarella_board_generic.wifi0_mac[0] = (low >> 0);
	ambarella_board_generic.wifi0_mac[1] = (low >> 8);
	ambarella_board_generic.wifi0_mac[2] = (low >> 16);
	ambarella_board_generic.wifi0_mac[3] = (low >> 24);
	ambarella_board_generic.wifi0_mac[4] = (high >> 0);
	ambarella_board_generic.wifi0_mac[5] = (high >> 8);
	return 0;
}
__tagtable(ATAG_AMBARELLA_WIFI0, parse_wifi0_tag_mac);

static int __init parse_wifi1_tag_mac(const struct tag *tag)
{
	u32 high, low;

	high = tag->u.serialnr.high;
	low = tag->u.serialnr.low;

	ambarella_board_generic.wifi1_mac[0] = (low >> 0);
	ambarella_board_generic.wifi1_mac[1] = (low >> 8);
	ambarella_board_generic.wifi1_mac[2] = (low >> 16);
	ambarella_board_generic.wifi1_mac[3] = (low >> 24);
	ambarella_board_generic.wifi1_mac[4] = (high >> 0);
	ambarella_board_generic.wifi1_mac[5] = (high >> 8);
	return 0;
}
__tagtable(ATAG_AMBARELLA_WIFI1, parse_wifi1_tag_mac);

static int __init parse_usb_eth0_tag_mac(const struct tag *tag)
{
	u32 high, low;

	high = tag->u.serialnr.high;
	low = tag->u.serialnr.low;

	ambarella_board_generic.usb_eth0_mac[0] = (low >> 0);
	ambarella_board_generic.usb_eth0_mac[1] = (low >> 8);
	ambarella_board_generic.usb_eth0_mac[2] = (low >> 16);
	ambarella_board_generic.usb_eth0_mac[3] = (low >> 24);
	ambarella_board_generic.usb_eth0_mac[4] = (high >> 0);
	ambarella_board_generic.usb_eth0_mac[5] = (high >> 8);
	return 0;
}
__tagtable(ATAG_AMBARELLA_USB_ETH0, parse_usb_eth0_tag_mac);

static int __init parse_usb_eth1_tag_mac(const struct tag *tag)
{
	u32 high, low;

	high = tag->u.serialnr.high;
	low = tag->u.serialnr.low;

	ambarella_board_generic.usb_eth1_mac[0] = (low >> 0);
	ambarella_board_generic.usb_eth1_mac[1] = (low >> 8);
	ambarella_board_generic.usb_eth1_mac[2] = (low >> 16);
	ambarella_board_generic.usb_eth1_mac[3] = (low >> 24);
	ambarella_board_generic.usb_eth1_mac[4] = (high >> 0);
	ambarella_board_generic.usb_eth1_mac[5] = (high >> 8);
	return 0;
}
__tagtable(ATAG_AMBARELLA_USB_ETH1, parse_usb_eth1_tag_mac);

/* ==========================================================================*/
enum {
	AMBARELLA_IO_DESC_AHB_ID = 0,
	AMBARELLA_IO_DESC_APB_ID,
	AMBARELLA_IO_DESC_PPM_ID,
#if defined(CONFIG_PLAT_AMBARELLA_SUPPORT_MMAP_AXI)
	AMBARELLA_IO_DESC_AXI_ID,
#endif
#if defined(CONFIG_PLAT_AMBARELLA_SUPPORT_MMAP_DDD)
	AMBARELLA_IO_DESC_DDD_ID,
#endif
#if defined(CONFIG_PLAT_AMBARELLA_SUPPORT_MMAP_DRAMC)
	AMBARELLA_IO_DESC_DRAMC_ID,
#endif
#if defined(CONFIG_PLAT_AMBARELLA_SUPPORT_MMAP_CRYPT)
	AMBARELLA_IO_DESC_CRYPT_ID,
#endif
#if defined(CONFIG_PLAT_AMBARELLA_SUPPORT_MMAP_AHB64)
	AMBARELLA_IO_DESC_AHB64_ID,
#endif
#if defined(CONFIG_PLAT_AMBARELLA_SUPPORT_MMAP_DBGBUS)
	AMBARELLA_IO_DESC_DBGBUS_ID,
#endif
	AMBARELLA_IO_DESC_DSP_ID,
};

struct ambarella_mem_map_desc {
	char		name[8];
	struct map_desc	io_desc;
};

static struct ambarella_mem_map_desc ambarella_io_desc[] = {
	[AMBARELLA_IO_DESC_AHB_ID] = {
		.name		= "AHB",
		.io_desc	= {
			.virtual	= AHB_BASE,
			.pfn		= __phys_to_pfn(AHB_PHYS_BASE),
			.length		= AHB_SIZE,
#if defined(CONFIG_PLAT_AMBARELLA_ADD_REGISTER_LOCK)
			.type		= MT_DEVICE_NONSHARED,
#else
			.type		= MT_DEVICE,
#endif
			},
	},
	[AMBARELLA_IO_DESC_APB_ID] = {
		.name		= "APB",
		.io_desc	= {
			.virtual	= APB_BASE,
			.pfn		= __phys_to_pfn(APB_PHYS_BASE),
			.length		= APB_SIZE,
#if defined(CONFIG_PLAT_AMBARELLA_ADD_REGISTER_LOCK)
			.type		= MT_DEVICE_NONSHARED,
#else
			.type		= MT_DEVICE,
#endif
			},
	},
	[AMBARELLA_IO_DESC_PPM_ID] = {
		.name		= "PPM",	/*Private Physical Memory*/
		.io_desc		= {
			.virtual	= AHB_BASE - CONFIG_AMBARELLA_PPM_SIZE,
			.pfn		= __phys_to_pfn(DEFAULT_MEM_START),
			.length		= CONFIG_AMBARELLA_PPM_SIZE,
#if defined(CONFIG_AMBARELLA_PPM_UNCACHED)
			.type		= MT_DEVICE,
#else
			.type		= MT_MEMORY,
#endif
			},
	},
#if defined(CONFIG_PLAT_AMBARELLA_SUPPORT_MMAP_AXI)
	[AMBARELLA_IO_DESC_AXI_ID] = {
		.name		= "AXI",
		.io_desc	= {
			.virtual	= AXI_BASE,
			.pfn		= __phys_to_pfn(AXI_PHYS_BASE),
			.length		= AXI_SIZE,
			.type		= MT_DEVICE,
			},
	},
#endif
#if defined(CONFIG_PLAT_AMBARELLA_SUPPORT_MMAP_DDD)
	[AMBARELLA_IO_DESC_DDD_ID] = {
		.name		= "DDD",
		.io_desc	= {
			.virtual= DDD_BASE,
			.pfn	= __phys_to_pfn(DDD_PHYS_BASE),
			.length	= DDD_SIZE,
			.type	= MT_DEVICE,
			},
	},
#endif
#if defined(CONFIG_PLAT_AMBARELLA_SUPPORT_MMAP_DRAMC)
	[AMBARELLA_IO_DESC_DRAMC_ID] = {
		.name		= "DRAMC",
		.io_desc	= {
			.virtual= DRAMC_BASE,
			.pfn	= __phys_to_pfn(DRAMC_PHYS_BASE),
			.length	= DRAMC_SIZE,
#if defined(CONFIG_PLAT_AMBARELLA_ADD_REGISTER_LOCK)
			.type	= MT_DEVICE_NONSHARED,
#else
			.type	= MT_DEVICE,
#endif
			},
	},
#endif
#if defined(CONFIG_PLAT_AMBARELLA_SUPPORT_MMAP_CRYPT)
	[AMBARELLA_IO_DESC_CRYPT_ID] = {
		.name		= "CRYPT",
		.io_desc	= {
			.virtual= CRYPT_BASE,
			.pfn	= __phys_to_pfn(CRYPT_PHYS_BASE),
			.length	= CRYPT_SIZE,
#if defined(CONFIG_PLAT_AMBARELLA_ADD_REGISTER_LOCK)
			.type	= MT_DEVICE_NONSHARED,
#else
			.type	= MT_DEVICE,
#endif
			},
	},
#endif
#if defined(CONFIG_PLAT_AMBARELLA_SUPPORT_MMAP_AHB64)
	[AMBARELLA_IO_DESC_AHB64_ID] = {
		.name		= "AHB64",
		.io_desc	= {
			.virtual= AHB64_BASE,
			.pfn	= __phys_to_pfn(AHB64_PHYS_BASE),
			.length	= AHB64_SIZE,
			.type	= MT_DEVICE,
			},
	},
#endif
#if defined(CONFIG_PLAT_AMBARELLA_SUPPORT_MMAP_DBGBUS)
	[AMBARELLA_IO_DESC_DBGBUS_ID] = {
		.name		= "DBGBUS",
		.io_desc	= {
			.virtual= DBGBUS_BASE,
			.pfn	= __phys_to_pfn(DBGBUS_PHYS_BASE),
			.length	= DBGBUS_SIZE,
			.type	= MT_DEVICE,
			},
	},
#endif
	[AMBARELLA_IO_DESC_DSP_ID] = {
		.name		= "IAV",
		.io_desc	= {
			.virtual	= 0x00000000,
			.pfn		= 0x00000000,
			.length		= 0x00000000,
			},
	},
};

static struct ambarella_mem_rev_desc ambarella_bst_info = {
	.physaddr			= DEFAULT_BST_START,
	.size				= DEFAULT_BST_SIZE,
};

static int __init ambarella_dt_scan_iavmem(unsigned long node,
			const char *uname,  int depth, void *data)
{
	const char *type;
	__be32 *reg;
	unsigned long len;
	struct ambarella_mem_map_desc *iavmem_desc;

	type = of_get_flat_dt_prop(node, "device_type", NULL);
	if (type == NULL || strcmp(type, "iavmem") != 0)
		return 0;

	reg = of_get_flat_dt_prop(node, "reg", &len);
	if (WARN_ON(!reg || (len != 2 * sizeof(unsigned long))))
		return 0;

	iavmem_desc = &ambarella_io_desc[AMBARELLA_IO_DESC_DSP_ID];
	iavmem_desc->io_desc.pfn = __phys_to_pfn(be32_to_cpu(reg[0]));
	iavmem_desc->io_desc.length = be32_to_cpu(reg[1]);

	return 1;
}

void __init ambarella_map_io(void)
{
	u32 i, iop, ios, iov;

	for (i = 0; i < AMBARELLA_IO_DESC_DSP_ID; i++) {
		iop = __pfn_to_phys(ambarella_io_desc[i].io_desc.pfn);
		ios = ambarella_io_desc[i].io_desc.length;
		iov = ambarella_io_desc[i].io_desc.virtual;
		if (ios > 0) {
			iotable_init(&(ambarella_io_desc[i].io_desc), 1);
			pr_info("Ambarella: %8s = 0x%08x[0x%08x],0x%08x %d\n",
				ambarella_io_desc[i].name, iop, iov, ios,
				ambarella_io_desc[i].io_desc.type);
		}
	}

	/* scan and hold the memory information for IAV */
	of_scan_flat_dt(ambarella_dt_scan_iavmem, NULL);
}

/* ==========================================================================*/

static int __init parse_system_revision(char *p)
{
	system_rev = simple_strtoul(p, NULL, 0);
	return 0;
}
early_param("system_rev", parse_system_revision);

static int __init parse_mem_tag_bst(const struct tag *tag)
{
	ambarella_bst_info.physaddr = tag->u.mem.start;
	ambarella_bst_info.size = tag->u.mem.size;

	return 0;
}
__tagtable(ATAG_AMBARELLA_BST, parse_mem_tag_bst);

u32 get_ambarella_ppm_phys(void)
{
	return __pfn_to_phys(
		ambarella_io_desc[AMBARELLA_IO_DESC_PPM_ID].io_desc.pfn);
}
EXPORT_SYMBOL(get_ambarella_ppm_phys);

u32 get_ambarella_ppm_virt(void)
{
	return ambarella_io_desc[AMBARELLA_IO_DESC_PPM_ID].io_desc.virtual;
}
EXPORT_SYMBOL(get_ambarella_ppm_virt);

u32 get_ambarella_ppm_size(void)
{
	return ambarella_io_desc[AMBARELLA_IO_DESC_PPM_ID].io_desc.length;
}
EXPORT_SYMBOL(get_ambarella_ppm_size);

u32 get_ambarella_iavmem_phys(void)
{
	return __pfn_to_phys(
		ambarella_io_desc[AMBARELLA_IO_DESC_DSP_ID].io_desc.pfn);
}
EXPORT_SYMBOL(get_ambarella_iavmem_phys);

u32 get_ambarella_iavmem_size(void)
{
	return ambarella_io_desc[AMBARELLA_IO_DESC_DSP_ID].io_desc.length;
}
EXPORT_SYMBOL(get_ambarella_iavmem_size);

u32 get_ambarella_bstmem_info(u32 *bstadd, u32 *bstsize)
{
#if (CHIP_REV != A8)
	if ((ambarella_bst_info.size == 0) ||
		(ambarella_bst_info.physaddr < get_ambarella_ppm_phys()) ||
		((ambarella_bst_info.physaddr + ambarella_bst_info.size) >
		(get_ambarella_ppm_phys() + get_ambarella_ppm_size())))
		return AMB_BST_INVALID;
#endif

	*bstadd = ambarella_bst_info.physaddr;
	*bstsize = ambarella_bst_info.size;

	return AMB_BST_MAGIC;
}
EXPORT_SYMBOL(get_ambarella_bstmem_info);

u32 *get_ambarella_bstmem_head(void)
{
	u32 *pstart_address = NULL;
	u32 *phead_address = NULL;
	u32 offset = 0;
	u32 valid = 0;

	if (ambarella_bst_info.size == 0)
		return (u32 *)AMB_BST_INVALID;

	pstart_address = (u32 *)ambarella_phys_to_virt(
		ambarella_bst_info.physaddr);
	for (phead_address =
		&pstart_address[(ambarella_bst_info.size >> 2) - 1];
		phead_address > pstart_address; phead_address--) {
		if (*phead_address == AMB_BST_MAGIC) {
			valid = 1;
			break;
		}
	}
	for (; phead_address > pstart_address; phead_address--) {
		if (*phead_address != AMB_BST_MAGIC) {
			offset = *phead_address;
			break;
		}
	}
	if ((offset == 0) || (valid == 0))
		return (u32 *)AMB_BST_INVALID;

	phead_address -= (offset >> 2);

	return phead_address;
}
EXPORT_SYMBOL(get_ambarella_bstmem_head);

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

	return __amb_raw_phys_to_virt(paddr);
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

	return __amb_raw_virt_to_phys(vaddr);
}
EXPORT_SYMBOL(ambarella_virt_to_phys);

u32 get_ambarella_ahb_phys(void)
{
	return __pfn_to_phys(
		ambarella_io_desc[AMBARELLA_IO_DESC_AHB_ID].io_desc.pfn);
}
EXPORT_SYMBOL(get_ambarella_ahb_phys);

u32 get_ambarella_ahb_virt(void)
{
	return ambarella_io_desc[AMBARELLA_IO_DESC_AHB_ID].io_desc.virtual;
}
EXPORT_SYMBOL(get_ambarella_ahb_virt);

u32 get_ambarella_ahb_size(void)
{
	return ambarella_io_desc[AMBARELLA_IO_DESC_AHB_ID].io_desc.length;
}
EXPORT_SYMBOL(get_ambarella_ahb_size);

u32 get_ambarella_apb_phys(void)
{
	return __pfn_to_phys(
		ambarella_io_desc[AMBARELLA_IO_DESC_APB_ID].io_desc.pfn);
}
EXPORT_SYMBOL(get_ambarella_apb_phys);

u32 get_ambarella_apb_virt(void)
{
	return ambarella_io_desc[AMBARELLA_IO_DESC_APB_ID].io_desc.virtual;
}
EXPORT_SYMBOL(get_ambarella_apb_virt);

u32 get_ambarella_apb_size(void)
{
	return ambarella_io_desc[AMBARELLA_IO_DESC_APB_ID].io_desc.length;
}
EXPORT_SYMBOL(get_ambarella_apb_size);

/* ==========================================================================*/
static struct ambarella_mem_rev_info ambarella_reserve_mem_info = {
	.counter			= 0,
};

static int __init reserve_mem_check(u32 start, u32 size)
{
	u32					index;

	if ((start & MEM_MAP_CHECK_MASK) || (start < PHYS_OFFSET)) {
		pr_err("Ambarella: Bad revmem start 0x%08x\n", start);
		return -EINVAL;
	}

	if (size & MEM_MAP_CHECK_MASK) {
		pr_err("Ambarella: Bad revmem size 0x%08x\n", size);
		return -EINVAL;
	}

	index = ambarella_reserve_mem_info.counter;
	if (index < MEMORY_RESERVE_MAX_NR) {
		ambarella_reserve_mem_info.desc[index].physaddr = start;
		ambarella_reserve_mem_info.desc[index].size = size;
		ambarella_reserve_mem_info.counter++;
	} else {
		pr_err("Ambarella: too much revmem %d!\n", index);
		return -EINVAL;
	}

	return 0;
}

static int __init early_revmem(char *p)
{
	unsigned long				start = 0;
	unsigned long				size = 0;
	char					*endp;

	start = memparse(p, &endp);
	if (*endp == ',')
		size = memparse(endp + 1, NULL);

	return reserve_mem_check(start, size);
}
early_param("revmem", early_revmem);

static int __init parse_mem_tag_revmem(const struct tag *tag)
{
	return reserve_mem_check(tag->u.mem.start, tag->u.mem.size);
}
__tagtable(ATAG_AMBARELLA_REVMEM, parse_mem_tag_revmem);

u32 get_ambarella_mem_rev_info(struct ambarella_mem_rev_info *pinfo)
{
	int					i;
	u32					revp, revs;

	if (ambarella_reserve_mem_info.counter)
		pr_info("Ambarella Reserve Memory:\n");

	for (i = 0; i < ambarella_reserve_mem_info.counter; i++) {
		revp = ambarella_reserve_mem_info.desc[i].physaddr;
		revs = ambarella_reserve_mem_info.desc[i].size;
		pr_info("\t%02d:\t0x%08x[0x%08x]\tNormal\n", i, revp, revs);
	}

	*pinfo = ambarella_reserve_mem_info;

	return 0;
}
EXPORT_SYMBOL(get_ambarella_mem_rev_info);

/* ==========================================================================*/
int arch_pfn_is_nosave(unsigned long pfn)
{
	int					i;
	unsigned long				nosave_begin_pfn;
	unsigned long				nosave_end_pfn;

	nosave_begin_pfn =
		ambarella_io_desc[AMBARELLA_IO_DESC_PPM_ID].io_desc.pfn;
	nosave_end_pfn = PFN_PHYS(nosave_begin_pfn) +
		ambarella_io_desc[AMBARELLA_IO_DESC_PPM_ID].io_desc.length;
	nosave_end_pfn = PFN_UP(nosave_end_pfn);

	if ((pfn >= nosave_begin_pfn) && (pfn < nosave_end_pfn))
		return 1;

	for (i = 0; i < ambarella_reserve_mem_info.counter; i++) {
		nosave_begin_pfn = __phys_to_pfn(
			ambarella_reserve_mem_info.desc[i].physaddr);
		nosave_end_pfn = __phys_to_pfn(
			ambarella_reserve_mem_info.desc[i].physaddr +
			ambarella_reserve_mem_info.desc[i].size);
		if ((pfn >= nosave_begin_pfn) && (pfn < nosave_end_pfn))
			return 1;
	}

	return 0;
}

/* ==========================================================================*/
static int __init early_boot_splash_logo(char *p)
{
	ambarella_boot_splash_logo = 1;
	return 0;
}
early_param("boot_splash_logo_on", early_boot_splash_logo);

