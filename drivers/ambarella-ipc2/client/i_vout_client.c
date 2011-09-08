/*
 * drivers/ambarella-ipc/client/i_vout_client.c
 *
 * @Author: Hanbo Xiao
 * @Email : hbxiao@ambarella.com
 * @Time  : 07/09/2011
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
#include <linux/delay.h>

#include <linux/aipc/aipc.h>
#if !defined(__DEPENDENCY_GENERATION__)
#include "ipcgen/i_vout.h"
#define __IPC_CLIENT_IMPL__
#include "ipcgen/i_vout_tab.i"
#endif
#include <linux/aipc/i_vout.h>

static CLIENT *IPC_i_vout = NULL;

static struct ipc_prog_s i_vout_prog =
{
	.name = "i_vout",
	.prog = I_VOUT_PROG,
	.vers = I_VOUT_VERS,
	.table = (struct ipcgen_table *) i_vout_prog_1_table,
	.nproc = i_vout_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};

/*
 * IPC: i_vout.i_vout_cmd_send().
 */
int ipc_vout_cmd_send (struct i_vout_cmd *vout_cmd)
{
	int rval = 0;
	enum clnt_stat stat;

	stat = i_vout_cmd_send_1 (vout_cmd, &rval, IPC_i_vout);
	if (stat != IPC_SUCCESS) {
		pr_err ("ipc error: %d (%s)\n", stat, ipc_strerror (stat));
		return rval;
	}

	return rval;
}
EXPORT_SYMBOL(ipc_vout_cmd_send);

#if defined(CONFIG_PROC_FS)

extern struct proc_dir_entry *proc_aipc;
struct proc_dir_entry *proc_vout_cmd_send = NULL;

static int i_vout_proc_fs_vout_cmd_send (struct file *file, const char __user *buffer,
				 unsigned long count, void *data)
{
	int rval;
	struct i_vout_cmd vout_cmd;

	if (count > sizeof (vout_cmd.dsp_cmd))
		count = sizeof (vout_cmd.dsp_cmd);
	
	memset (vout_cmd.dsp_cmd, 0x0, sizeof (vout_cmd.dsp_cmd));
	if (copy_from_user(vout_cmd.dsp_cmd, buffer, count))
		return -EFAULT;

	vout_cmd.dsp_cmd[count] = '\0';
	rval = ipc_vout_cmd_send(&vout_cmd);

	if (rval < 0)
		return -EFAULT;
	return count;
}

#endif

/*
 * Client initialization.
 */
static int __init i_vout_init(void)
{
	IPC_i_vout = ipc_clnt_prog_register(&i_vout_prog);
	if (IPC_i_vout == NULL)
		return -EPERM;

#if defined(CONFIG_PROC_FS)
	proc_vout_cmd_send = create_proc_entry("vout_cmd_send", 0, proc_aipc);
	if (proc_vout_cmd_send) {
		proc_vout_cmd_send->data = NULL;
		proc_vout_cmd_send->read_proc = NULL;
		proc_vout_cmd_send->write_proc = i_vout_proc_fs_vout_cmd_send;
	}
#endif

	return 0;
}

/*
 * Client exit.
 */
static void __exit i_vout_cleanup(void)
{
#if defined(CONFIG_PROC_FS)
	if (proc_vout_cmd_send) {
		remove_proc_entry("vout_cmd_send", proc_aipc);
		proc_vout_cmd_send = NULL;
	}
#endif

	if (IPC_i_vout) {
		ipc_clnt_prog_unregister(&i_vout_prog, IPC_i_vout);
		IPC_i_vout = NULL;
	}
}

subsys_initcall_sync(i_vout_init);
module_exit(i_vout_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hanbo Xiao <hbxiao@ambarella.com>");
MODULE_DESCRIPTION("Ambarella IPC: i_vout.x");
