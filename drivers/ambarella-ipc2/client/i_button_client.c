/*
 * drivers/ambarella-ipc/client/i_button_client.c
 *
 * Authors:
 *	Keny Huang <skhuang@ambarella.com>
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
 * Copyright (C) 2009-2011, Ambarella Inc.
 */

#include <asm/uaccess.h>
#include <linux/module.h>

#include <linux/aipc/aipc.h>
#include <linux/aipc/i_button.h>
#if !defined(__DEPENDENCY_GENERATION__)
#include "ipcgen/i_button.h"
#define __IPC_CLIENT_IMPL__
#include "ipcgen/i_button_tab.i"
#endif

static CLIENT *IPC_i_button = NULL;

static struct ipc_prog_s i_button_prog =
{
	.name = "i_button",
	.prog = I_BUTTON_PROG,
	.vers = I_BUTTON_VERS,
	.table = (struct ipcgen_table *) i_button_prog_1_table,
	.nproc = i_button_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};

/*
 * IPC: ibutton_is_module_valid().
 */
int ipc_ibutton_is_module_valid(unsigned int version)
{
	enum clnt_stat stat;
	int res;

	stat = ibutton_is_module_valid_1(&version, &res, IPC_i_button);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return res;
}
EXPORT_SYMBOL(ipc_ibutton_is_module_valid);

/*
 * Client initialization.
 */
static int __init i_button_init(void)
{
	IPC_i_button = ipc_clnt_prog_register(&i_button_prog);
	if (IPC_i_button == NULL)
		return -EPERM;

	return 0;
}

/*
 * Client exit.
 */
static void __exit i_button_cleanup(void)
{
	if (IPC_i_button) {
		ipc_clnt_prog_unregister(&i_button_prog, IPC_i_button);
		IPC_i_button = NULL;
	}
}

subsys_initcall_sync(i_button_init);
module_exit(i_button_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Keny Huang <skhuang@ambarella.com>");
MODULE_DESCRIPTION("Ambarella IPC: i_button.x");

