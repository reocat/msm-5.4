/*
 * drivers/ambarella-ipc/client/i_flpart_client.c
 *
 * Authors:
 *	Charles Chiou <cchiou@ambarella.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * Copyright (C) 2009-2010, Ambarella Inc.
 */

#include <linux/module.h>
#include <linux/proc_fs.h>

#include <linux/aipc/aipc.h>
#include <linux/aipc/i_flpart.h>
#if !defined(__DEPENDENCY_GENERATION__)
#include "ipcgen/i_flpart.h"
#define __IPC_CLIENT_IMPL__
#include "ipcgen/i_flpart_tab.i"
#endif

static CLIENT *IPC_i_flpart = NULL;

static struct ipc_prog_s i_flpart_prog =
{
	.name = "i_flpart",
	.prog = I_FLPART_PROG,
	.vers = I_FLPART_VERS,
	.table = (struct ipcgen_table *) i_flpart_prog_1_table,
	.nproc = i_flpart_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};

/*
 * IPC: i_flpart.vflpart_num_parts().
 */
int ipc_flpart_num_parts(void)
{
	enum clnt_stat stat;
	int res;

	stat = vflpart_num_parts_1(NULL, &res, IPC_i_flpart);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return res;
}
EXPORT_SYMBOL(ipc_flpart_num_parts);

/*
 * IPC: i_flpart.vflpart_get().
 */
int ipc_flpart_get(int index, struct ipc_flpart *part)
{
	enum clnt_stat stat;

	stat = vflpart_get_1(&index, (struct vflpart *) part, IPC_i_flpart);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return 0;
}
EXPORT_SYMBOL(ipc_flpart_get);

#if defined(CONFIG_PROC_FS)

extern struct proc_dir_entry *proc_aipc;

static struct proc_dir_entry *proc_vflpart = NULL;
static struct proc_dir_entry *proc_part = NULL;
static struct proc_dir_entry *proc_num = NULL;
static struct proc_dir_entry *proc_list = NULL;

struct proc_part_list
{
	struct list_head list;
	struct proc_dir_entry *proc;
	char name[32];
};

static LIST_HEAD(pl_head);

static int i_vflpart_proc_fs_num(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	int rval, num;
	int len = 0;

	if (off != 0)
		return 0;

	rval = num = ipc_flpart_num_parts();
	if (rval < 0)
		return -EFAULT;

	len += sprintf(page, "%d\n", num);
	*eof = 1;

	return len;
}

static int i_vflpart_proc_fs_list(char *page, char **start, off_t off,
				  int count, int *eof, void *data)
{
	struct ipc_flpart part;
	int rval, i, num;
	int len = 0;

	if (off != 0)
		return 0;

	rval = num = ipc_flpart_num_parts();
	if (rval < 0)
		return -EFAULT;

	for (i = 0; i < num; i++) {
		rval = ipc_flpart_get(i, &part);
		if (rval < 0)
			return -EFAULT;
		len += sprintf(page + len, "%s\n", part.name);
	}

	*eof = 1;

	return len;
}

static int i_vflpart_proc_fs_part(char *page, char **start, off_t off,
				  int count, int *eof, void *data)
{
	struct ipc_flpart part;
	int rval;
	int len = 0;

	if (off != 0)
		return 0;

	rval = ipc_flpart_get((int) data, &part);
	if (rval < 0)
		return -EFAULT;
	len += sprintf(page + len, "crc32=0x%.8x\n", part.crc32);
	len += sprintf(page + len, "ver_num=%d.%d\n",
		       part.ver_num >> 16,
		       part.ver_num & 0xffff);
	len += sprintf(page + len, "ver_date=%.4d/%d/%d\n",
		       part.ver_date >> 16,
		       (part.ver_date >> 8) & 0xff,
		       part.ver_date & 0xff);
	len += sprintf(page + len, "img_len=0x%.8x\n", part.img_len);
	len += sprintf(page + len, "mem_addr=0x%.8x\n", part.mem_addr);
	len += sprintf(page + len, "flag=0x%.8x\n", part.flag);
	len += sprintf(page + len, "magic=0x%.8x\n", part.magic);
	len += sprintf(page + len, "sblk=%d\n", part.sblk);
	len += sprintf(page + len, "nblk=%d\n", part.nblk);

	*eof = 1;

	return len;
}

static void i_flpart_procfs_init(void)
{
	struct ipc_flpart part;
	int rval, i, num;
	struct proc_part_list *pl;
	struct proc_dir_entry *proc;

	proc_vflpart = proc_mkdir("vflpart", proc_aipc);
	if (proc_vflpart == NULL) {
		pr_err("create vflpart dir failed!\n");
		return;
	}

	proc_part = proc_mkdir("part", proc_vflpart);
	if (proc_part == NULL) {
		pr_err("create proc dir failed!\n");
		return;
	}

	proc_num = create_proc_entry("num", 0, proc_vflpart);
	if (proc_num) {
		proc_num->data = NULL;
		proc_num->read_proc = i_vflpart_proc_fs_num;
		proc_num->write_proc = NULL;
	}

	proc_list = create_proc_entry("list", 0, proc_vflpart);
	if (proc_list) {
		proc_list->data = NULL;
		proc_list->read_proc = i_vflpart_proc_fs_list;
		proc_list->write_proc = NULL;
	}

	rval = num = ipc_flpart_num_parts();
	if (rval < 0)
		return;	/* IPC error ?! */

	for (i = 0; i < num; i++) {
		rval = ipc_flpart_get(i, &part);
		if (rval < 0)
			return;	/* IPC error ?! */

		pl = kzalloc(sizeof(*pl), GFP_KERNEL);
		if (pl) {
			pl->proc = NULL;
			strncpy(pl->name, part.name, sizeof(pl->name));
			list_add_tail(&pl->list, &pl_head);
		} else
			return;  /* Out of memory */

		proc = create_proc_entry(pl->name, 0, proc_part);
		if (proc) {
			proc->data = (void *) i;
			proc->read_proc = i_vflpart_proc_fs_part;
			proc->write_proc = NULL;
			pl->proc = proc;
		}

	}
}

static void i_flpart_procfs_cleanup(void)
{
	struct proc_part_list *pl;
	struct list_head *pos, *q;

	if (proc_num) {
		remove_proc_entry("num", proc_vflpart);
		proc_num = NULL;
	}

	if (proc_list) {
		remove_proc_entry("list", proc_vflpart);
		proc_list = NULL;
	}

	list_for_each_safe(pos, q, &pl_head) {
		pl = list_entry(pos, struct proc_part_list, list);
		remove_proc_entry(pl->name, proc_part);
		list_del(pos);
		kfree(pl);
	}

	remove_proc_entry("part", proc_vflpart);
	proc_part = NULL;

	remove_proc_entry("vflpart", proc_aipc);
	proc_vflpart = NULL;
}

#endif

/*
 * Client initialization.
 */
static int __init i_flpart_init(void)
{
	IPC_i_flpart = ipc_clnt_prog_register(&i_flpart_prog);
	if (IPC_i_flpart == NULL)
		return -EPERM;

#if defined(CONFIG_PROC_FS)
	i_flpart_procfs_init();
#endif

	return 0;
}

/*
 * Client exit.
 */
static void __exit i_flpart_cleanup(void)
{
	if (IPC_i_flpart) {
		ipc_clnt_prog_unregister(&i_flpart_prog, IPC_i_flpart);
		IPC_i_flpart = NULL;
	}

#if defined(CONFIG_PROC_FS)
	i_flpart_procfs_cleanup();
#endif
}

subsys_initcall_sync(i_flpart_init);
module_exit(i_flpart_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Charles Chiou <cchiou@ambarella.com>");
MODULE_DESCRIPTION("Ambarella IPC: i_flpart.x");
