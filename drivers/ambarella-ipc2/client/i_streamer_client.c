/*
 * drivers/ambarella-ipc/client/i_streamer_client.c
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

#include <linux/aipc/aipc.h>
#if !defined(__DEPENDENCY_GENERATION__)
#include "ipcgen/i_streamer.h"
#define __IPC_CLIENT_IMPL__
#include "ipcgen/i_streamer_tab.i"
#endif

static CLIENT *IPC_i_streamer = NULL;

static struct ipc_prog_s i_streamer_prog =
{
	.name = "i_streamer",
	.prog = I_STREAMER_PROG,
	.vers = I_STREAMER_VERS,
	.table = (struct ipcgen_table *) i_streamer_prog_1_table,
	.nproc = i_streamer_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};

/*
 * Client initialization.
 */
int ipc_i_streamer_init(void)
{
	if (IPC_i_streamer == NULL) {
		IPC_i_streamer = ipc_clnt_prog_register(&i_streamer_prog);
		if (IPC_i_streamer == NULL)
			return -EPERM;
	}

	return 0;
}
EXPORT_SYMBOL(ipc_i_streamer_init);

/*
 * Client exit.
 */
void ipc_i_streamer_cleanup(void)
{
	if (IPC_i_streamer) {
		ipc_clnt_prog_unregister(&i_streamer_prog, IPC_i_streamer);
		IPC_i_streamer = NULL;
	}
}
EXPORT_SYMBOL(ipc_i_streamer_cleanup);

int ipc_i_streamer_get_iavpool_info(unsigned char **base_addr, unsigned int *size)
{
	enum clnt_stat stat;
	struct i_streamer_buf_info iavpool_info;

	stat=i_streamer_get_iavpool_info_1(NULL, &iavpool_info, IPC_i_streamer);
	if (stat != IPC_SUCCESS) {
		pr_err("[%s] ipc error: %d (%s)\n", __func__, stat, ipc_strerror(stat));
		return -EFAULT;
	}

	*base_addr=(unsigned char *)iavpool_info.base_addr;
	*size=iavpool_info.size;

	return 0;
}
EXPORT_SYMBOL(ipc_i_streamer_get_iavpool_info);
