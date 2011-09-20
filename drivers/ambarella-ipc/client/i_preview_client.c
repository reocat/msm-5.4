/*
 * drivers/ambarella-ipc/client/i_preview_client.c
 *
 * Authors:
 *	Dane Liu <htliu@ambarella.com>
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

#include <linux/aipc/aipc.h>
#if !defined(__DEPENDENCY_GENERATION__)
#include "ipcgen/i_preview.h"
#define __IPC_CLIENT_IMPL__
#include "ipcgen/i_preview_tab.i"
#endif

static CLIENT *IPC_i_preview = NULL;

static struct ipc_prog_s i_preview_prog =
{
	.name = "i_preview",
	.prog = I_PREVIEW_PROG,
	.vers = I_PREVIEW_VERS,
	.table = (struct ipcgen_table *) i_preview_prog_1_table,
	.nproc = i_preview_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};

/*
 * Client initialization.
 */
int ipc_i_preview_init(void)
{
	if (IPC_i_preview == NULL) {
		IPC_i_preview = ipc_clnt_prog_register(&i_preview_prog);
		if (IPC_i_preview == NULL)
			return -EPERM;
	}

	return 0;
}
EXPORT_SYMBOL(ipc_i_preview_init);

/*
 * Client exit.
 */
void ipc_i_preview_cleanup(void)
{
	if (IPC_i_preview) {
		ipc_clnt_prog_unregister(&i_preview_prog, IPC_i_preview);
		IPC_i_preview = NULL;
	}
}
EXPORT_SYMBOL(ipc_i_preview_cleanup);

int ipc_i_preview_get_previewpool_info(unsigned char **base_addr, unsigned int *size)
{
	enum clnt_stat stat;
	struct i_preview_buf_info previewpool_info;

	stat=i_preview_get_previewpool_info_1(NULL, &previewpool_info, IPC_i_preview);
	if (stat != IPC_SUCCESS) {
		pr_err("[%s] ipc error: %d (%s)\n", __func__, stat, ipc_strerror(stat));
		return -EFAULT;
	}

	*base_addr=(unsigned char *)previewpool_info.base_addr;
	*size=previewpool_info.size;

	return 0;
}
EXPORT_SYMBOL(ipc_i_preview_get_previewpool_info);
