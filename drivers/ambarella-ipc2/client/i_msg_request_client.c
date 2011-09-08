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
#include "ipcgen/i_msg_request.h"
#define __IPC_CLIENT_IMPL__
#include "ipcgen/i_msg_request_tab.i"
#endif
#include <linux/aipc/i_msg_request.h>

static CLIENT *IPC_i_msg_request = NULL;

static struct ipc_prog_s i_msg_request_prog =
{
	.name = "i_msg_request",
	.prog = I_MSG_REQUEST_PROG,
	.vers = I_MSG_REQUEST_VERS,
	.table = (struct ipcgen_table *) i_msg_request_prog_1_table,
	.nproc = i_msg_request_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};

/*
 * IPC: i_msg_request.i_msg_request_cmd_send().
 */
int ipc_msg_request (struct i_msg_request_s *msg)
{
	int rval = 0;
	enum clnt_stat stat;

	stat = i_msg_request_1 (msg, &rval, IPC_i_msg_request);
	if (stat != IPC_SUCCESS) {
		pr_err ("ipc error: %d (%s)\n", stat, ipc_strerror (stat));
		return rval;
	}

	return rval;
}
EXPORT_SYMBOL(ipc_msg_request);

#if defined(CONFIG_PROC_FS)

extern struct proc_dir_entry *proc_aipc;
struct proc_dir_entry *proc_msg_request = NULL;

static int i_msg_request_proc_fs_msg_request (struct file *file, const char __user *buffer,
				 unsigned long count, void *data)
{
	int rval;
	struct i_msg_request_s msg;

	if (count > sizeof (msg.msg_content))
		count = sizeof (msg.msg_content);
	
	memset (msg.msg_content, 0x0, sizeof (msg.msg_content));
	if (copy_from_user(msg.msg_content, buffer, count))
		return -EFAULT;

	msg.msg_type = 0;
	msg.msg_size = count;
	msg.msg_content[count] = '\0';

	rval = ipc_msg_request(&msg);

	if (rval < 0)
		return -EFAULT;
	
	return count;
}

#endif

/*
 * Client initialization.
 */
static int __init i_msg_request_init(void)
{
	IPC_i_msg_request = ipc_clnt_prog_register(&i_msg_request_prog);
	if (IPC_i_msg_request == NULL)
		return -EPERM;

#if defined(CONFIG_PROC_FS)
	proc_msg_request = create_proc_entry("msg_request", 0, proc_aipc);
	if (proc_msg_request) {
		proc_msg_request->data = NULL;
		proc_msg_request->read_proc = NULL;
		proc_msg_request->write_proc = i_msg_request_proc_fs_msg_request;
	}
#endif

	return 0;
}

/*
 * Client exit.
 */
static void __exit i_msg_request_cleanup(void)
{
#if defined(CONFIG_PROC_FS)
	if (proc_msg_request) {
		remove_proc_entry("vout_cmd_send", proc_aipc);
		proc_msg_request = NULL;
	}
#endif

	if (IPC_i_msg_request) {
		ipc_clnt_prog_unregister(&i_msg_request_prog, IPC_i_msg_request);
		IPC_i_msg_request = NULL;
	}
}

subsys_initcall_sync(i_msg_request_init);
module_exit(i_msg_request_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hanbo Xiao <hbxiao@ambarella.com>");
MODULE_DESCRIPTION("Ambarella IPC: i_msg_request.x");
