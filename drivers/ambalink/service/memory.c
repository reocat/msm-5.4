/*
 *
 * Copyright (C) 2012-2016, Ambarella, Inc.
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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/memblock.h>

#include <plat/ambalink_cfg.h>

enum {
	AMBARELLA_IO_DESC_PPM_ID = 0,
	AMBARELLA_IO_DESC_PPM2_ID,
};

struct ambarella_mem_map_desc {
	char		name[8];
	unsigned long	virtual;
	unsigned long	physical;
	unsigned long	length;
};

static struct ambarella_mem_map_desc ambarella_io_desc[] = {
	[AMBARELLA_IO_DESC_PPM_ID] = {
		.name		= "PPM",    /* Private Physical Memory (shared memory) */
		.virtual	= 0,
		.physical	= 0,
		.length		= 0,
	},
	[AMBARELLA_IO_DESC_PPM2_ID] = {
		.name		= "PPM2",   /* Private Physical Memory (RTOS memory) */
		.virtual	= 0,
		.physical	= 0,
		.length		= 0,
	},
};

#if 0
unsigned long ambarella_phys_to_virt(unsigned long paddr)
{
	int i;
	unsigned long phystart, phylength, phyoffset, vstart;

	for (i = 0; i < ARRAY_SIZE(ambarella_io_desc); i++) {
		phystart = ambarella_io_desc[i].physical;
		phylength = ambarella_io_desc[i].length;
		vstart = ambarella_io_desc[i].virtual;
		if ((paddr >= phystart) && (paddr < (phystart + phylength))) {
			phyoffset = paddr - phystart;
			return (unsigned long)(vstart + phyoffset);
		}
	}

	return __raw_phys_to_virt(paddr);
}
EXPORT_SYMBOL(ambarella_phys_to_virt);

unsigned long ambarella_virt_to_phys(unsigned long vaddr)
{
	int i;
	unsigned long phystart, vlength, voffset, vstart;

	for (i = 0; i < ARRAY_SIZE(ambarella_io_desc); i++) {
		phystart = ambarella_io_desc[i].physical;
		vlength = ambarella_io_desc[i].length;
		vstart = ambarella_io_desc[i].virtual;
		if ((vaddr >= vstart) && (vaddr < (vstart + vlength))) {
			voffset = vaddr - vstart;
			return (unsigned long)(phystart + voffset);
		}
	}

	return __raw_virt_to_phys((void *) vaddr);
}
EXPORT_SYMBOL(ambarella_virt_to_phys);
#endif

/* An example memory layout for multios:
		    +-----------------------------------+   0x0000_0000     0 KB
		    | Reserved memory                   |
		    |                                   |
		    +-----------------------------------+   0x0000_4000    16 KB
		    | ATF (ARM Trust Firmware)          |
		    |   Size 112KB                      |
		    +-----------------------------------+   0x0002_0000    128KB
		    | ThreadX                           |
		    |                                   |
		    +-----------------------------------+   0x5400-0000  1344 MB
		    | CV                                |
		    |   Size: 512 MB                    |
		    |                                   |
		    +-----------------------------------+   0x7400-0000  1856 MB
		    | Linux 2 Share memory              |
		    |   Size: 6 MB                      |
		    +-----------------------------------+   0x7460-0000  1862 MB
		    | Linux 2                           |
		    |   Size: 90 MB                     |
		    |                                   |
		    +-----------------------------------+   0x7a00-0000  1952 MB
		    | Linux 1 Share memory              |
		    |   Size: 6 MB                      |
		    +-----------------------------------+   0x7a60-0000  1958 MB
		    | Linux 1                           |
		    |   Size: 90 MB                     |
		    |                                   |
		    +-----------------------------------+   0x8000_0000  2048 MB
*/
unsigned long ambalink_phys_to_virt(phys_addr_t paddr)
{
	int                   i;
	unsigned long         phystart;
	unsigned long         phylength;
	unsigned long         phyoffset;
	unsigned long         vstart;

	for (i = 0; i < ARRAY_SIZE(ambarella_io_desc); i++) {
		phystart = ambarella_io_desc[i].physical;
		phylength = ambarella_io_desc[i].length;
		vstart = ambarella_io_desc[i].virtual;
		if ((paddr >= phystart) && (paddr < (phystart + phylength))) {
			phyoffset = paddr - phystart;
			return (vstart + phyoffset);
		}
	}

	return (uintptr_t)phys_to_virt(paddr);
}
EXPORT_SYMBOL(ambalink_phys_to_virt);

phys_addr_t ambalink_virt_to_phys(unsigned long vaddr)
{
	int                 i;
	unsigned long       phystart;
	unsigned long       vlength;
	unsigned long       voffset;
	unsigned long       vstart;

	for (i = 0; i < ARRAY_SIZE(ambarella_io_desc); i++) {
		phystart = ambarella_io_desc[i].physical;
		vlength = ambarella_io_desc[i].length;
		vstart = ambarella_io_desc[i].virtual;
		if ((vaddr >= vstart) && (vaddr < (vstart + vlength))) {
			voffset = vaddr - vstart;
			return (phystart + voffset);
		}
	}

	return virt_to_phys((void *)vaddr);
}
EXPORT_SYMBOL(ambalink_virt_to_phys);

static int ambalink_ioremap(phys_addr_t phys_addr, size_t size, unsigned long *virt)
{
	int err = 0;
#ifdef CONFIG_ARM
	void *ptr;

	ptr = ioremap(phys_addr, size);
	if(ptr == NULL) {
		return 1;
	}
	*virt = (u64)(uintptr_t)ptr;
#else

	memblock_add(phys_addr, size);
	*virt = (u64) phys_to_virt(phys_addr);
	err = ioremap_page_range(*virt, *virt + size, phys_addr, __pgprot(PROT_NORMAL_NC));
#endif
	return err;
}

static int ambalink_ioremap_cache(phys_addr_t phys_addr, size_t size, unsigned long *virt)
{
	int err = 0;
#ifdef CONFIG_ARM
	void *ptr;

	ptr = ioremap_cache(phys_addr, size);
	if(ptr == NULL) {
		return 1;
	}
	*virt = (u64)(uintptr_t) ptr;
#else

	memblock_add(phys_addr, size);
	*virt = (u64) phys_to_virt(phys_addr);
	err = ioremap_page_range(*virt, *virt + size, phys_addr, __pgprot(PROT_NORMAL));
#endif
	return err;
}

void __init ambalink_init_mem(void)
{
	unsigned long base, size, virt = 0;
	__be32 *reg;
	int len, err;
	struct device_node *memory_node;

	/* Create ppm2 (rtos region) memory mapping for each OS, example:
	 *     ppm2 {
	 *         reg = <0x20000 0x53fe0000>;
	 *         device_type = "ppm2";
	 *     };
	 */
	memory_node = of_find_node_by_name(NULL, "ppm2");

	reg = (__be32 *) of_get_property(memory_node, "reg", &len);
	if (WARN_ON(!reg || (len != 2 * sizeof(u32))))
		return;

	base = be32_to_cpu(reg[0]);
	size = be32_to_cpu(reg[1]);
	err = ambalink_ioremap_cache(base, size, &virt);
	if (err) {
		vunmap((void *) virt);
		pr_info("%s: ioremap_page_range failed: 0x%08lx - 0x%08lx (0x%016lx)\n",
			__func__, base, base + size, virt);
		return;
	}

	ambarella_io_desc[AMBARELLA_IO_DESC_PPM2_ID].virtual	= virt;
	ambarella_io_desc[AMBARELLA_IO_DESC_PPM2_ID].physical	= base;
	ambarella_io_desc[AMBARELLA_IO_DESC_PPM2_ID].length	= size;

	pr_info("ambalink ppm2 memory (rtos region): 0x%08lx - 0x%08lx (0x%016lx)\n",
		base, base + size, virt);

	/* Create shm (shared memory) mapping for each guest OS.
	 * i.e. shm is the same as guest1_shm for guest1, guest2_shm for guest2
	 * example:
	 *     shm {
	 *         reg = <0x7a000000 0x600000>;
	 *         device_type = "shm";
	 *     };
	 */
	memory_node = of_find_node_by_name(NULL, "shm");

	reg = (__be32 *) of_get_property(memory_node, "reg", &len);
	if (WARN_ON(!reg || (len != 2 * sizeof(u32))))
		return;

	base = be32_to_cpu(reg[0]);
	size = be32_to_cpu(reg[1]);
	/* MUST match with RTOS setting */
#ifdef CONFIG_AMBALINK_GUEST_2
	err = ambalink_ioremap(base, size, &virt);
#else
	err = ambalink_ioremap_cache(base, size, &virt);
#endif
	if (err) {
		vunmap((void *)virt);
		pr_info("%s: ioremap_page_range failed: 0x%08lx - 0x%08lx (0x%016lx)\n",
			__func__, base, base + size,virt);
		return;
	}

	ambarella_io_desc[AMBARELLA_IO_DESC_PPM_ID].virtual	= virt;
	ambarella_io_desc[AMBARELLA_IO_DESC_PPM_ID].physical	= base;
	ambarella_io_desc[AMBARELLA_IO_DESC_PPM_ID].length	= size;

	pr_info("ambalink shm (shared memory): 0x%08lx - 0x%08lx (0x%016lx)\n",
		base, base + size, virt);

	ambalink_shm_layout.vring_c0_and_c1_buf         = base;
	ambalink_shm_layout.vring_c0_to_c1              =
		ambalink_shm_layout.vring_c0_and_c1_buf + RPMSG_TOTAL_BUF_SPACE;

	ambalink_shm_layout.vring_c1_to_c0              =
		ambalink_shm_layout.vring_c0_to_c1 + VRING_SIZE;

	ambalink_shm_layout.rpmsg_suspend_backup_addr   =
		ambalink_shm_layout.vring_c1_to_c0 + VRING_SIZE;

	ambalink_shm_layout.rpc_profile_addr            =
		ambalink_shm_layout.rpmsg_suspend_backup_addr + RPMSG_SUSPEND_BACKUP_SIZE;
	ambalink_shm_layout.rpmsg_profile_addr          =
		ambalink_shm_layout.rpc_profile_addr + RPC_PROFILE_SIZE;

	aipc_shm_info[AMBARELLA_RPROC].vring_host_and_clnt_buf = ambalink_shm_layout.vring_c0_and_c1_buf;
	aipc_shm_info[AMBARELLA_RPROC].vring_host_to_clnt = ambalink_shm_layout.vring_c0_to_c1;
	aipc_shm_info[AMBARELLA_RPROC].vring_clnt_to_host = ambalink_shm_layout.vring_c1_to_c0;

	ambalink_shm_layout.aipc_slock_addr             =
		ambalink_shm_layout.rpmsg_profile_addr + MAX_RPC_RPMSG_PROFILE_SIZE;

	ambalink_shm_layout.aipc_mutex_addr             =
			ambalink_shm_layout.aipc_slock_addr + AIPC_SLOCK_SIZE;

#ifdef CONFIG_AMBALINK_GUEST_2
	/* Create guest1_shm (guest1's shm) mapping for guest2
	 * This node only exists when multios enabled, example:
	 *     guest1_shm {
	 *         reg = <0x7a000000 0x600000>;
	 *         device_type = "guest1_shm";
	 *     };
	 */
	memory_node = of_find_node_by_name(NULL, "guest1_shm");
	if (memory_node)
	{
		reg = (__be32 *) of_get_property(memory_node, "reg", &len);
		if (WARN_ON(!reg || (len != 2 * sizeof(u32))))
			return;

		base = be32_to_cpu(reg[0]);
		size = be32_to_cpu(reg[1]);
		err = ambalink_ioremap(base, size, &virt);
		if (err) {
			vunmap((void *)virt);
			pr_info("%s: ioremap_page_range failed: 0x%08lx - 0x%08lx (0x%016lx)\n",
				__func__, base, base + size,virt);
			return;
		}
		pr_info("ambalink guest1_shm (guest1's shm): 0x%08lx - 0x%08lx (0x%016lx)\n",
			base, base + size, virt);
	}
#else
	/* Create guest2_shm (guest2's shm) mapping for guest1
	 * This node only exists when multios enabled, example:
	 *     guest2_shm {
	 *         reg = <0x74000000 0x600000>;
	 *         device_type = "guest2_shm";
	 *     };
	 */
	memory_node = of_find_node_by_name(NULL, "guest2_shm");
	if (memory_node)
	{

		reg = (__be32 *) of_get_property(memory_node, "reg", &len);
		if (WARN_ON(!reg || (len != 2 * sizeof(u32))))
			return;

		base = be32_to_cpu(reg[0]);
		size = be32_to_cpu(reg[1]);
		err = ambalink_ioremap(base, size, &virt);
		if (err) {
			vunmap((void *)virt);
			pr_info("%s: ioremap_page_range failed: 0x%08lx - 0x%08lx (0x%016lx)\n",
				__func__, base, base + size, virt);
			return;
		}
		pr_info("ambalink guest2_shm (guest2's shm): 0x%08lx - 0x%08lx (0x%016lx)\n",
			base, base + size, virt);

		aipc_shm_info[C1_AND_C2].vring_host_and_clnt_buf = base;
		aipc_shm_info[C1_AND_C2].vring_host_to_clnt = base + RPMSG_TOTAL_BUF_SPACE;
		aipc_shm_info[C1_AND_C2].vring_clnt_to_host = aipc_shm_info[C1_AND_C2].vring_host_to_clnt + VRING_SIZE;

		/* multios: update aipc_slock_addr by guest2_shm
		   must put global locks at non-cacheable region
		   CV1 did not implement outer cacheable coherence logics between clusters */
		ambalink_shm_layout.aipc_slock_addr =
			base                         /* vring_c0_and_c1_buf */
			+ RPMSG_TOTAL_BUF_SPACE      /* vring_c0_to_c1 */
			+ VRING_SIZE                 /* vring_c1_to_c0 */
			+ VRING_SIZE                 /* rpmsg_suspend_backup_addr */
			+ RPMSG_SUSPEND_BACKUP_SIZE  /* rpc_profile_addr */
			+ RPC_PROFILE_SIZE           /* rpmsg_profile_addr */
			+ MAX_RPC_RPMSG_PROFILE_SIZE;

		ambalink_shm_layout.aipc_mutex_addr =
			ambalink_shm_layout.aipc_slock_addr + AIPC_SLOCK_SIZE;
	}
#endif

#if 0
	/* Create cv_shm (CV mem) mapping for each OS, example:
	 *     cv_shm {
	 *         reg = <0x54000000 0x20000000>;
	 *         device_type = "cv_shm";
	 *     };
	 */
	memory_node = of_find_node_by_name(NULL, "cv_shm");
	if(!memory_node)
		return;

	reg = (__be32 *) of_get_property(memory_node, "reg", &len);
	if (WARN_ON(!reg || (len != 2 * sizeof(u32))))
		return;

	base = be32_to_cpu(reg[0]);
	size = be32_to_cpu(reg[1]);

	err = ambalink_ioremap(base, size, &virt);
	if (err) {
		vunmap((void *)virt);
		pr_info("%s: ioremap_page_range failed: 0x%08lx - 0x%08lx (0x%016lx)\n",
			__func__, base, base + size, virt);
		return;
	}
	pr_info("ambalink cv_shm (cv shared memory): 0x%08lx - 0x%08lx (0x%016lx)\n",
		base, base + size, virt);
#endif
}
EXPORT_SYMBOL(ambalink_init_mem);
