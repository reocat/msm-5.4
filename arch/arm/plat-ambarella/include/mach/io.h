/*
 * arch/arm/plat-ambarella/include/mach/io.h
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

#ifndef __ASM_ARCH_IO_H
#define __ASM_ARCH_IO_H

#include <mach/hardware.h>

#define IO_SPACE_LIMIT		0xffffffff

#define __io(a)			((void __iomem *)(a))
#define __mem_pci(a)		(a)

#define io_p2v(x)		(((x) >= APB_START) ? ((x) - APB_START + APB_BASE) : ((x) - AHB_START + AHB_BASE))
#define io_v2p(x)		(((x) >= APB_BASE) ? ((x) - APB_BASE + APB_START) : ((x) - AHB_BASE + AHB_START))

#ifndef __ASSEMBLER__

#ifndef CONFIG_AMBARELLA_DEBUG_IO_ACCESS
#define amba_readb(v)		__raw_readb(v)
#define amba_readw(v)		__raw_readw(v)
#define amba_readl(v)		__raw_readl(v)
#define amba_writeb(v,d)	__raw_writeb(d,v)
#define amba_writew(v,d)	__raw_writew(d,v)
#define amba_writel(v,d)	__raw_writel(d,v)
#else
#include <linux/kernel.h>
static inline u8 amba_readb_printk(const volatile void __iomem *vaddress)
{
	u8					data;

	data = __raw_readb(vaddress);
	printk("%s: %p=0x%02x\n", __func__, vaddress, data);

	return data;
}
static inline u16 amba_readw_printk(const volatile void __iomem *vaddress)
{
	u16					data;

	data = __raw_readw(vaddress);
	printk("%s: %p=0x%04x\n", __func__, vaddress, data);

	return data;
}
static inline u32 amba_readl_printk(const volatile void __iomem *vaddress)
{
	u32					data;

	data = __raw_readl(vaddress);
	printk("%s: %p=0x%08x\n", __func__, vaddress, data);

	return data;
}

static inline void amba_writeb_printk(
	const volatile void __iomem *vaddress, u8 data)
{
	printk("%s: %p=0x%08x\n", __func__, vaddress, data);
	__raw_writeb(data, vaddress);
}
static inline void amba_writew_printk(
	const volatile void __iomem *vaddress, u16 data)
{
	printk("%s: %p=0x%08x\n", __func__, vaddress, data);
	__raw_writew(data, vaddress);
}
static inline void amba_writel_printk(
	const volatile void __iomem *vaddress, u32 data)
{
	printk("%s: %p=0x%08x\n", __func__, vaddress, data);
	__raw_writel(data, vaddress);
}

#define amba_readb(v)		amba_readb_printk((const volatile void __iomem *)v)
#define amba_readw(v)		amba_readw_printk((const volatile void __iomem *)v)
#define amba_readl(v)		amba_readl_printk((const volatile void __iomem *)v)
#define amba_writeb(v,d)	amba_writeb_printk((const volatile void __iomem *)v, d)
#define amba_writew(v,d)	amba_writew_printk((const volatile void __iomem *)v, d)
#define amba_writel(v,d)	amba_writel_printk((const volatile void __iomem *)v, d)

#endif

#define amba_inb(p)		amba_readb(io_p2v(p))
#define amba_inw(p)		amba_readw(io_p2v(p))
#define amba_inl(p)		amba_readl(io_p2v(p))
#define amba_outb(p,d)		amba_writeb(io_p2v(p),d)
#define amba_outw(p,d)		amba_writew(io_p2v(p),d)
#define amba_outl(p,d)		amba_writel(io_p2v(p),d)

#define amba_setbitsb(v, mask)	amba_writeb((v),(amba_readb(v) | (mask)))
#define amba_setbitsw(v, mask)	amba_writew((v),(amba_readw(v) | (mask)))
#define amba_setbitsl(v, mask)	amba_writel((v),(amba_readl(v) | (mask)))

#define amba_clrbitsb(v, mask)	amba_writeb((v),(amba_readb(v) & ~(mask)))
#define amba_clrbitsw(v, mask)	amba_writew((v),(amba_readw(v) & ~(mask)))
#define amba_clrbitsl(v, mask)	amba_writel((v),(amba_readl(v) & ~(mask)))

#define amba_tstbitsb(v, mask)	(amba_readb(v) & (mask))
#define amba_tstbitsw(v, mask)	(amba_readw(v) & (mask))
#define amba_tstbitsl(v, mask)	(amba_readl(v) & (mask))

static inline void amba_change_bit(
	const volatile void __iomem *vaddress,
	unsigned int bit)
{
	u32					mask = 1UL << (bit & 31);
	u32					data;

	data = amba_readl(vaddress);
	data ^= mask;
	amba_writel(vaddress, data);
}
static inline int amba_test_and_set_bit(
	const volatile void __iomem *vaddress,
	unsigned int bit)
{
	u32					mask = 1UL << (bit & 31);
	u32					data;
	u32					tmp;

	data = amba_readl(vaddress);
	tmp = data | mask;
	amba_writel(vaddress, tmp);

	return data & mask;
}
static inline int amba_test_and_clear_bit(
	const volatile void __iomem *vaddress,
	unsigned int bit)
{
	u32					mask = 1UL << (bit & 31);
	u32					data;
	u32					tmp;

	data = amba_readl(vaddress);
	tmp = data & ~mask;
	amba_writel(vaddress, tmp);

	return data & mask;
}
static inline int amba_test_and_change_bit(
	const volatile void __iomem *vaddress,
	unsigned int bit)
{
	u32					mask = 1UL << (bit & 31);
	u32					data;
	u32					tmp;

	data = amba_readl(vaddress);
	tmp = data ^ mask;
	amba_writel(vaddress, tmp);

	return data & mask;
}
static inline int amba_test_and_set_mask(
	const volatile void __iomem *vaddress,
	unsigned int mask)
{
	u32					data;
	u32					tmp;

	data = amba_readl(vaddress);
	tmp = data | mask;
	amba_writel(vaddress, tmp);

	return data & mask;
}
static inline int amba_test_and_clear_mask(
	const volatile void __iomem *vaddress,
	unsigned int mask)
{
	u32					data;
	u32					tmp;

	data = amba_readl(vaddress);
	tmp = data & ~mask;
	amba_writel(vaddress, tmp);

	return data & mask;
}
#endif /* __ASSEMBLER__ */

#endif

