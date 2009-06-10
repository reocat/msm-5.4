/*
 * arch/arm/mach-ambarella/include/mach/io.h
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

#ifndef __ASSEMBLER__
// We do not lock IRQ in following func and they are not atomic
#define amba_readb(v)		__raw_readb(v)
#define amba_readw(v)		__raw_readw(v)
#define amba_readl(v)		__raw_readl(v)
#define amba_writeb(v,d)	__raw_writeb(d,v)
#define amba_writew(v,d)	__raw_writew(d,v)
#define amba_writel(v,d)	__raw_writel(d,v)

#define amba_inb(p)		__raw_readb(io_p2v(p))
#define amba_inw(p)		__raw_readw(io_p2v(p))
#define amba_inl(p)		__raw_readl(io_p2v(p))
#define amba_outb(p,d)		__raw_writeb(d,io_p2v(p))
#define amba_outw(p,d)		__raw_writew(d,io_p2v(p))
#define amba_outl(p,d)		__raw_writel(d,io_p2v(p))

#define amba_setbits(v, mask)	amba_writel((v),(amba_readl(v) | (mask)))
#define amba_clrbits(v, mask)	amba_writel((v),(amba_readl(v) & ~(mask)))
#define amba_tstbits(v, mask)	(amba_readl(v) & (mask))
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

