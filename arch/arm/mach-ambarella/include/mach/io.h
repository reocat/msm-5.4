/*
 * arch/arm/plat-ambarella/include/mach/io.h
 *
 * Author: Anthony Ginger <hfjiang@ambarella.com>
 *
 * Copyright (C) 2004-2029, Ambarella, Inc.
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

#ifndef __ASSEMBLER__

extern int __smccc_write(volatile void __iomem *addr, u64 val, u8 width);
extern int __smccc_read(const volatile void __iomem *addr, u64 *val, u8 width);

static inline u32 __ns_readl(volatile void __iomem *addr)
{
	return readl_relaxed(addr);
}

static inline void __ns_writel(u32 val, volatile void __iomem *addr)
{
	writel_relaxed(val, addr);
}

static inline u32 __sec_readl(volatile void __iomem *addr)
{
	u64 val;

	__smccc_read(addr, &val, 32);

	return (u32)val;
}

static inline void __sec_writel(u32 val, volatile void __iomem *addr)
{
	__smccc_write(addr, val, 32);
}

static inline void ambarella_fixup_secure_world(u8 secure,
		u32 (**readl)(volatile void __iomem *),
		void (**writel)(u32 , volatile void __iomem *))
{
	if (secure) {
		*readl = __sec_readl;
		*writel = __sec_writel;
	} else {
		(*readl) = __ns_readl;
		*writel = __ns_writel;
	}
}

#endif /* __ASSEMBLER__ */

#endif

