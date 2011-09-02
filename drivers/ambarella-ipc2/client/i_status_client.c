/*
 * drivers/ambarella-ipc/client/i_status_client.c
 *
 * Authors:
 *	Josh Wang <chwang@ambarella.com>
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

#include <linux/aipc/i_status.h>
#if !defined(__DEPENDENCY_GENERATION__)
#include "ipcgen/i_status.h"
#define __IPC_CLIENT_IMPL__
#include "ipcgen/i_status_tab.i"
#endif

static CLIENT *IPC_i_status = NULL;

static struct ipc_prog_s i_status_prog =
{
	.name = "i_status",
	.prog = I_STATUS_PROG,
	.vers = I_STATUS_VERS,
	.table = (struct ipcgen_table *) i_status_prog_1_table,
	.nproc = i_status_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};

/*
 * IPC: i_status.lk_status_report().
 */
int ipc_status_report_ready(int status)
{
	enum clnt_stat stat;

	stat = lk_status_report_1(&status, NULL, IPC_i_status);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return 0;
}
EXPORT_SYMBOL(ipc_status_report_ready);

int ipc_boss_ver_report(int ver)
{
	enum clnt_stat stat;

	stat = lk_boss_version_report_1(&ver, NULL, IPC_i_status);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}
	return 0;
}
EXPORT_SYMBOL(ipc_boss_ver_report);

/*
 * Client initialization.
 */
int i_status_init(void)
{
	IPC_i_status = ipc_clnt_prog_register(&i_status_prog);
	if (IPC_i_status == NULL)
		return -EPERM;

	return 0;
}
EXPORT_SYMBOL(i_status_init);

/*
 * Client exit.
 */
void i_status_cleanup(void)
{
	if (IPC_i_status) {
		ipc_clnt_prog_unregister(&i_status_prog, IPC_i_status);
		IPC_i_status = NULL;
	}
}
EXPORT_SYMBOL(i_status_cleanup);

