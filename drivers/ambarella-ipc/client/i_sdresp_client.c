/*
 * drivers/ambarella-ipc/client/i_sdresp_client.c
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
#include <linux/aipc/i_sdresp.h>
#if !defined(__DEPENDENCY_GENERATION__)
#include "ipcgen/i_sdresp.h"
#define __IPC_CLIENT_IMPL__
#include "ipcgen/i_sdresp_tab.i"
#endif

static CLIENT *IPC_i_sdresp = NULL;

static struct ipc_prog_s i_sdresp_prog =
{
	.name = "i_sdresp",
	.prog = I_SDRESP_PROG,
	.vers = I_SDRESP_VERS,
	.table = (struct ipcgen_table *) i_sdresp_prog_1_table,
	.nproc = i_sdresp_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};

/*
 * IPC: i_sdresp.vsdresp_get().
 */
int ipc_sdinfo_get(int index, struct ipc_sdinfo *sdinfo)
{
	enum clnt_stat stat;

	stat = vsdinfo_get_1(&index, (struct vsdinfo *) sdinfo, IPC_i_sdresp);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return 0;
}
EXPORT_SYMBOL(ipc_sdinfo_get);

/*
 * IPC: i_sdresp.vsdresp_get().
 */
int ipc_sdresp_get(int index, struct ipc_sdresp *sdresp)
{
	enum clnt_stat stat;

	stat = vsdresp_get_1(&index, (struct vsdresp *) sdresp, IPC_i_sdresp);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return 0;
}
EXPORT_SYMBOL(ipc_sdresp_get);

/*
 * Client initialization.
 */
static int __init i_sdresp_init(void)
{
	IPC_i_sdresp = ipc_clnt_prog_register(&i_sdresp_prog);
	if (IPC_i_sdresp == NULL)
		return -EPERM;

	return 0;
}

/*
 * Client exit.
 */
static void __exit i_sdresp_cleanup(void)
{
	if (IPC_i_sdresp) {
		ipc_clnt_prog_unregister(&i_sdresp_prog, IPC_i_sdresp);
		IPC_i_sdresp = NULL;
	}
}

subsys_initcall_sync(i_sdresp_init);
module_exit(i_sdresp_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Charles Chiou <cchiou@ambarella.com>");
MODULE_DESCRIPTION("Ambarella IPC: i_sdresp.x");

