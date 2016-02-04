/*
 *
 * Author: Cao Rongrong <rrcao@ambarella.com>
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
#include <linux/proc_fs.h>

#ifdef CONFIG_ARCH_AMBARELLA_AMBALINK
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_fdt.h>

#include <plat/ambalink_cfg.h>
#endif

static struct proc_dir_entry *ambarella_proc_dir = NULL;
struct ambalink_shared_memory_layout ambalink_shm_layout;

struct proc_dir_entry *get_ambarella_proc_dir(void)
{
	return ambarella_proc_dir;
}
EXPORT_SYMBOL(get_ambarella_proc_dir);

static int __init ambarella_init_root_proc(void)
{
	ambarella_proc_dir = proc_mkdir("ambarella", NULL);
	if (!ambarella_proc_dir) {
		pr_err("failed to create ambarella root proc dir\n");
		return -ENOMEM;
	}

	return 0;
}

#ifdef CONFIG_ARCH_AMBARELLA_AMBALINK
static int __init ambarella_init_misc(void)
{
        u64 shm_base, shm_size;
	__be32 *reg;
	int len;
        struct device_node *memory_node;

	ambarella_init_root_proc();

        memory_node = of_find_node_by_name(NULL, "shm");

	reg = (__be32 *) of_get_property(memory_node, "reg", &len);
	if (WARN_ON(!reg || (len != 2 * sizeof(u32))))
		return 0;

        shm_base = be32_to_cpu(reg[0]);
        shm_size = be32_to_cpu(reg[1]);

        pr_info("ambalink shared memory : 0x%08lx - 0x%08lx\n",
                        (unsigned long) shm_base, (unsigned long) shm_size);

        create_pgd_mapping(&init_mm, shm_base, (unsigned long) phys_to_virt(shm_base),
                                shm_size, __pgprot(PROT_NORMAL_NC));

        ambalink_shm_layout.vring_c0_and_c1_buf         = shm_base;
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

        ambalink_shm_layout.aipc_slock_addr             =
                ambalink_shm_layout.rpmsg_profile_addr + MAX_RPC_RPMSG_PROFILE_SIZE;
        ambalink_shm_layout.aipc_mutex_addr             =
                ambalink_shm_layout.aipc_slock_addr + AIPC_SLOCK_SIZE;

	return 0;
}

core_initcall(ambarella_init_misc);
#else
core_initcall(ambarella_init_root_proc);
#endif

