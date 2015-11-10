/*
 * arch/arm/plat-ambarella/include/mach/memory.h
 *
 * History:
 *	2006/12/27 - [Charles Chiou] created file
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

#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

/*
 * MAP Sample	Phisical		Virtual			Size
 * --------------------------------------------------------------------------
 * AHB(IO)	AHB_PHYS_BASE		AHB_BASE		AHB_SIZE
 * APB(IO)	APB_PHYS_BASE		APB_BASE		APB_BASE
 * MISC(IO)	MISC_PHYS_BASE		MISC_BASE		MISC_SIZE
 * PPM		DEFAULT_MEM_START	AHB_BASE - Size		CONFIG_AMBARELLA_PPM_SIZE
 * Linux MEM	PHYS_OFFSET		CONFIG_VMSPLIT_xG	Linux MEM Size
 */

/* ==========================================================================*/
#if defined(CONFIG_ARCH_AMBARELLA_MEM_START_LOW)
#define DEFAULT_MEM_START		(0x00000000)
#else
#define DEFAULT_MEM_START		(0xc0000000)
#endif

/* ==========================================================================*/
/* Physical Address and Size */
#if defined(CONFIG_ARCH_AMBARELLA_AHB_APB_HIGH)
#define AHB_PHYS_BASE			(0xe0000000)
#define APB_PHYS_BASE			(0xe8000000)
#else
#define AHB_PHYS_BASE			(0x60000000)
#define APB_PHYS_BASE			(0x70000000)
#endif
#define AHB_SIZE			(0x01000000)
#define APB_SIZE			(0x01000000)

#if defined(CONFIG_ARCH_AMBARELLA_SUPPORT_MMAP_AXI)
#define AXI_PHYS_BASE			(0xf0000000)
#define AXI_SIZE			(0x00030000)
#endif

#if defined(CONFIG_ARCH_AMBARELLA_SUPPORT_MMAP_DRAMC)
#define DRAMC_PHYS_BASE			(0xdffe0000)
#define DRAMC_SIZE			(0x00020000)
#endif

#if defined(CONFIG_ARCH_AMBARELLA_SUPPORT_MMAP_DBGBUS)
#define DBGBUS_PHYS_BASE		(0xec000000)
#define DBGBUS_SIZE			(0x00200000)
#define DBGFMEM_PHYS_BASE		(0xee000000)
#define DBGFMEM_SIZE			(0x01000000)
#endif

/* ==========================================================================*/

/* Virtual Address */
#if defined(CONFIG_VMSPLIT_3G)
#define AHB_BASE			(0xf0000000)
#define APB_BASE			(0xf1000000)
#if defined(CONFIG_ARCH_AMBARELLA_SUPPORT_MMAP_AXI)
#define AXI_BASE			(0xf2000000)
#endif
#if defined(CONFIG_ARCH_AMBARELLA_SUPPORT_MMAP_DRAMC)
#define DRAMC_BASE			(0xf2040000)
#endif
#if defined(CONFIG_ARCH_AMBARELLA_SUPPORT_MMAP_DBGBUS)
#define DBGBUS_BASE			(0xf2200000)
#define DBGFMEM_BASE			(0xf3000000)
#endif

#elif defined(CONFIG_VMSPLIT_2G)
#define AHB_BASE			(0xe0000000)
#define APB_BASE			(0xe8000000)
#if defined(CONFIG_ARCH_AMBARELLA_SUPPORT_MMAP_AXI)
#define AXI_BASE			(0xf0000000)
#endif
#if defined(CONFIG_ARCH_AMBARELLA_SUPPORT_MMAP_DRAMC)
#define DRAMC_BASE			(0xef000000)
#endif
#if defined(CONFIG_ARCH_AMBARELLA_SUPPORT_MMAP_DBGBUS)
#define DBGBUS_BASE			(0xec000000)
#define DBGFMEM_BASE			(0xee000000)
#endif

#else /* CONFIG_VMSPLIT_1G */
#define AHB_BASE			(0xe0000000)
#define APB_BASE			(0xe8000000)
#if defined(CONFIG_ARCH_AMBARELLA_SUPPORT_MMAP_AXI)
#define AXI_BASE			(0xf0000000)
#endif
#if defined(CONFIG_ARCH_AMBARELLA_SUPPORT_MMAP_DRAMC)
#define DRAMC_BASE			(0xef000000)
#endif
#if defined(CONFIG_ARCH_AMBARELLA_SUPPORT_MMAP_DBGBUS)
#define DBGBUS_BASE			(0xec000000)
#define DBGFMEM_BASE			(0xee000000)
#endif
#endif	/* CONFIG_VMSPLIT_3G */

/* ==========================================================================*/
#ifdef CONFIG_ARM_PATCH_PHYS_VIRT
#error "CONFIG_ARM_PATCH_PHYS_VIRT needs the physical memory base at a 16MB boundary"
#endif

/* ==========================================================================*/
#define MAX_PHYSMEM_BITS		32
#define SECTION_SIZE_BITS		24
/* ==========================================================================*/

#endif

