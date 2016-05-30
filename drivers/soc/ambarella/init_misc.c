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
#include <linux/seq_file.h>

#ifdef CONFIG_ARCH_AMBARELLA_AMBALINK
#include <plat/ambalink_cfg.h>

struct ambalink_shared_memory_layout ambalink_shm_layout;
#endif

static struct proc_dir_entry *ambarella_proc_dir = NULL;

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
	ambarella_init_root_proc();

	ambalink_init_mem();

	return 0;
}

core_initcall(ambarella_init_misc);
#else
core_initcall(ambarella_init_root_proc);
#endif

#ifdef CONFIG_ARCH_AMBARELLA_AMBALINK
char wifi_mac[20];
EXPORT_SYMBOL(wifi_mac);
static int __init early_wifi_mac(char *p)
{
	strncpy(wifi_mac, p, strlen("00:00:00:00:00:00"));
	wifi_mac[strlen("00:00:00:00:00:00")] = '\0';

	return 0;
}
early_param("wifi_mac", early_wifi_mac);

char bt_mac[20];
EXPORT_SYMBOL(bt_mac);
static int __init early_bt_mac(char *p)
{
	strncpy(bt_mac, p, strlen("00:00:00:00:00:00"));
	bt_mac[strlen("00:00:00:00:00:00")] = '\0';

	return 0;
}
early_param("bt_mac", early_bt_mac);

static int ambarella_atags_proc_read(struct seq_file *m, void *v)
{

	/* keep the the same name as old platform */
	seq_printf(m, "wifi_mac: %s\n", wifi_mac);
	seq_printf(m, "wifi1_mac: %s\n", bt_mac);
	return 0;
}

static int ambarella_atags_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ambarella_atags_proc_read, NULL);
}

static const struct file_operations atags_ambarella_proc_fops = {
	.open = ambarella_atags_proc_open,
	.read = seq_read,
	.llseek	= seq_lseek,
	.release = single_release,
};

static int __init atags_ambarella_proc_init(void)
{
	/* keep the name board_info to old platform */
	proc_create("board_info", S_IRUGO, get_ambarella_proc_dir(),
		&atags_ambarella_proc_fops);

	return 0;
}
late_initcall(atags_ambarella_proc_init);

#endif

