/*
 * drivers/ambarella-ipc/client/i_touch_client.c
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

#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/proc_fs.h>

#include <linux/aipc/aipc.h>
#include <linux/aipc/i_touch.h>
#if !defined(__DEPENDENCY_GENERATION__)
#include "ipcgen/i_touch.h"
#define __IPC_CLIENT_IMPL__
#include "ipcgen/i_touch_tab.i"
#endif

static CLIENT *IPC_i_touch = NULL;

static struct ipc_prog_s i_touch_prog =
{
	.name = "i_touch",
	.prog = I_TOUCH_PROG,
	.vers = I_TOUCH_VERS,
	.table = (struct ipcgen_table *) i_touch_prog_1_table,
	.nproc = i_touch_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};

static void copy_vtouch_abs_info(vtouch_abs_info_t *dst, struct itouch_abs_info_s *src)
{
	dst->max=src->max;
	dst->min=src->min;
}

/*
 * IPC: itouch_is_module_valid().
 */
int ipc_itouch_is_module_valid()
{
	enum clnt_stat stat;
	int res;

	stat = itouch_is_module_valid_1(NULL, &res, IPC_i_touch);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return res;
}
EXPORT_SYMBOL(ipc_itouch_is_module_valid);

/*
 * IPC: itouch_get_module_info().
 */
int ipc_itouch_get_module_info(struct vtouch_module_info *info)
{
	enum clnt_stat stat;
	struct itouch_module_info data;
	int res;

	stat = itouch_get_module_info_1(&data, &res, IPC_i_touch);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	if(res>=0){
		snprintf(info->dev_name,32,"%s",data.dev_name);
		copy_vtouch_abs_info(&(info->x1),&(data.x1));
		copy_vtouch_abs_info(&(info->x2),&(data.x2));
		copy_vtouch_abs_info(&(info->y1),&(data.y1));
		copy_vtouch_abs_info(&(info->y2),&(data.y2));
		copy_vtouch_abs_info(&(info->pressure),&(data.pressure));
		copy_vtouch_abs_info(&(info->mt_major),&(data.mt_major));
	} else {
		memset(info,0,sizeof(struct vtouch_module_info));
	}
	
	return res;
}
EXPORT_SYMBOL(ipc_itouch_get_module_info);


#if defined(CONFIG_PROC_FS)

extern struct proc_dir_entry *proc_aipc;

struct proc_dir_entry *proc_itouch_check = NULL;
struct proc_dir_entry *proc_itouch_cmd = NULL;

static int i_touch_proc_fs_check(struct file *file, const char __user *buffer,
				 unsigned long count, void *data)
{
	int rval, i;
	char *buf;

	buf = kmalloc(GFP_KERNEL, count);
	if (buf == NULL)
		return -ENOMEM;

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	/* Strip trailing \r or \n */
	for (i = count - 1 ; i >= 0; i--) {
		if (buf[i] == '\r' || buf[i] == '\n')
			buf[i] = '\0';
		else
			break;
	}

	rval = ipc_itouch_is_module_valid();
	
	kfree(buf);

	if (rval < 0)
		return -EFAULT;
	return count;
}

static int i_touch_proc_fs_cmd(struct file *file, const char __user *buffer,
				 unsigned long count, void *data)
{
	int rval=0, i;
	char *buf;
	struct vtouch_module_info info;

	buf = kmalloc(GFP_KERNEL, count);
	if (buf == NULL)
		return -ENOMEM;

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	/* Strip trailing \r or \n */
	for (i = count - 1 ; i >= 0; i--) {
		if (buf[i] == '\r' || buf[i] == '\n')
			buf[i] = '\0';
		else
			break;
	}

	if (strcasecmp(buf, "init") == 0) {
		rval = ipc_itouch_get_module_info(&info);
	}
	
	kfree(buf);

	if (rval < 0)
		return -EFAULT;
	return count;
}

#endif

/*
 * Client initialization.
 */
static int __init i_touch_init(void)
{
	IPC_i_touch = ipc_clnt_prog_register(&i_touch_prog);
	if (IPC_i_touch == NULL)
		return -EPERM;

#if defined(CONFIG_PROC_FS)
	proc_itouch_check= create_proc_entry("itouch_check", 0, proc_aipc);
	if (proc_itouch_check) {
		proc_itouch_check->data = NULL;
		proc_itouch_check->read_proc = NULL;
		proc_itouch_check->write_proc = i_touch_proc_fs_check;
	}

	proc_itouch_cmd= create_proc_entry("itouch_cmd", 0, proc_aipc);
	if (proc_itouch_cmd) {
		proc_itouch_cmd->data = NULL;
		proc_itouch_cmd->read_proc = NULL;
		proc_itouch_cmd->write_proc = i_touch_proc_fs_cmd;
	}
#endif

	return 0;
}

/*
 * Client exit.
 */
static void __exit i_touch_cleanup(void)
{
#if defined(CONFIG_PROC_FS)
	if (proc_itouch_check) {
		remove_proc_entry("itouch_check", proc_aipc);
		proc_itouch_check = NULL;
	}
	if (proc_itouch_cmd) {
		remove_proc_entry("itouch_cmd", proc_aipc);
		proc_itouch_cmd = NULL;
	}
#endif
	if (IPC_i_touch) {
		ipc_clnt_prog_unregister(&i_touch_prog, IPC_i_touch);
		IPC_i_touch = NULL;
	}
}

subsys_initcall_sync(i_touch_init);
module_exit(i_touch_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Keny Huang <skhuang@ambarella.com>");
MODULE_DESCRIPTION("Ambarella IPC: i_touch.x");

