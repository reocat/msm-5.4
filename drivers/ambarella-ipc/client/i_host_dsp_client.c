/*
 * drivers/ambarella-ipc/client/i_host_dsp_client.c
 *
 * Authors:
 *	Chris Lin <tflin@ambarella.com>
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
 * Copyright (C) 2010-2011, Ambarella Inc.
 */

#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>

#include <linux/aipc/aipc.h>
#include <linux/aipc/i_host_dsp.h>
#if !defined(__DEPENDENCY_GENERATION__)
#include "ipcgen/i_host_dsp.h"
#define __IPC_CLIENT_IMPL__
#include "ipcgen/i_host_dsp_tab.i"
#endif

static CLIENT *IPC_i_host_dsp = NULL;

static struct ipc_prog_s i_host_dsp_prog =
{
	.name = "i_host_dsp",
	.prog = I_HOST_DSP_PROG,
	.vers = I_HOST_DSP_VERS,
	.table = (struct ipcgen_table *) i_host_dsp_prog_1_table,
	.nproc = i_host_dsp_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};


int ipc_c2a_dsp_host(void)
{
	enum clnt_stat stat;
	int res;

	stat = c2a_dsp_host_1(NULL, &res, IPC_i_host_dsp);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return res;
}
EXPORT_SYMBOL(ipc_c2a_dsp_host);

#if defined(CONFIG_PROC_FS)

extern struct proc_dir_entry *proc_aipc;
struct proc_dir_entry *proc_host_dsp = NULL;

static int i_host_dsp_proc_fs_c2a(void)
{
  int rval;
  rval = ipc_c2a_dsp_host();
  if (rval < 0)
    return -EFAULT;
  return 0;

}

#endif

/*
 * Client initialization.
 */
static int __init i_host_dsp_init(void)
{
	IPC_i_host_dsp = ipc_clnt_prog_register(&i_host_dsp_prog);
	if (IPC_i_host_dsp == NULL)
		return -EPERM;

#if defined(CONFIG_PROC_FS)
	proc_host_dsp = create_proc_entry("host_dsp", 0, proc_aipc);
	if (proc_host_dsp) {
		proc_host_dsp->data = NULL;
		proc_host_dsp->read_proc = i_host_dsp_proc_fs_c2a;
		proc_host_dsp->write_proc = NULL;
	}
#endif

	return 0;
}

/*
 * Client exit.
 */
static void __exit i_host_dsp_cleanup(void)
{
#if defined(CONFIG_PROC_FS)
	if (proc_host_dsp) {
		remove_proc_entry("host_dsp", proc_aipc);
		proc_host_dsp = NULL;
	}
#endif

	if (IPC_i_host_dsp) {
		ipc_clnt_prog_unregister(&i_host_dsp_prog, IPC_i_host_dsp);
		IPC_i_host_dsp = NULL;
	}
}

subsys_initcall_sync(i_host_dsp_init);
module_exit(i_host_dsp_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chris Lin <tflin@ambarella.com>");
MODULE_DESCRIPTION("Ambarella IPC: i_host_dsp.x");
