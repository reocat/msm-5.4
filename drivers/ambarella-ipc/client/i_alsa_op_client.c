/*
 * drivers/ambarella-ipc/client/i_alsa_op_client.c
 *
 * Authors:
 *	Eric Lee <cylee@ambarella.com>
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
#if !defined(__DEPENDENCY_GENERATION__)
#include "ipcgen/i_alsa_op.h"
#define __IPC_CLIENT_IMPL__
#include "ipcgen/i_alsa_op_tab.i"
#endif

static CLIENT *IPC_i_alsa_op = NULL;

static struct ipc_prog_s i_alsa_op_prog =
{
	.name = "i_alsa_op",
	.prog = I_ALSA_OP_PROG,
	.vers = I_ALSA_OP_VERS,
	.table = (struct ipcgen_table *) i_alsa_op_prog_1_table,
	.nproc = i_alsa_op_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};

/*
 * IPC: ipc_ialsa_tx_open().
 */
int ipc_ialsa_tx_open(unsigned int ch, unsigned int freq)
{
	enum clnt_stat stat;
	struct pcm_info info;
	
	info.channels = ch;
	info.freq = freq;
	stat = ialsa_tx_open_1(&info, NULL, IPC_i_alsa_op, NULL);

	return 0;
}
EXPORT_SYMBOL(ipc_ialsa_tx_open);

/*
 * IPC: ipc_ialsa_rx_open().
 */
int ipc_ialsa_rx_open(unsigned int ch, unsigned int freq)
{
	enum clnt_stat stat;
	struct pcm_info info;

	info.channels = ch;
	info.freq = freq;
	stat = ialsa_rx_open_1(&info, NULL, IPC_i_alsa_op, NULL);

	return 0;
}
EXPORT_SYMBOL(ipc_ialsa_rx_open);

/*
 * IPC: ipc_ialsa_tx_op(). (async auto_del)
 */
int ipc_ialsa_tx_op(unsigned int operation)
{
	enum clnt_stat stat;

	stat = ialsa_tx_op_1(&operation, NULL, IPC_i_alsa_op, NULL);

	return 0;
}
EXPORT_SYMBOL(ipc_ialsa_tx_op);

/*
 * IPC: ipc_ialsa_rx_op(). (async auto_del)
 */
int ipc_ialsa_rx_op(unsigned int operation)
{
	enum clnt_stat stat;

	stat = ialsa_rx_op_1(&operation, NULL, IPC_i_alsa_op, NULL);

	return 0;
}
EXPORT_SYMBOL(ipc_ialsa_rx_op);

/*
 * IPC: ipc_ialsa_get_max_channels().
 */
int ipc_ialsa_get_max_channels(void)
{
	enum clnt_stat stat;
	int res;

	stat = ialsa_get_max_channels_1(NULL, &res, IPC_i_alsa_op);
	if (stat != IPC_SUCCESS) {
		printk("ipc error: %d (%s)", stat, ipc_strerror(stat));
	}
	if (res < 0 || res > 6) {
	  printk("WRONG audio channel number !! %d\n", res);
		res = 2;
	}
	return res;
}
EXPORT_SYMBOL(ipc_ialsa_get_max_channels);

/* ------------------------------------------------------------------------- */

/*
 * Client initialization.
 */
static int __init i_alsa_op_init(void)
{
	IPC_i_alsa_op = ipc_clnt_prog_register(&i_alsa_op_prog);
	if (IPC_i_alsa_op == NULL)
		return -EPERM;

	return 0;
}

/*
 * Client exit.
 */
static void __exit i_alsa_op_cleanup(void)
{
	if (IPC_i_alsa_op) {
		ipc_clnt_prog_unregister(&i_alsa_op_prog, IPC_i_alsa_op);
		IPC_i_alsa_op = NULL;
	}
}

module_init(i_alsa_op_init);
module_exit(i_alsa_op_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eric Lee <cylee@ambarella.com>");
MODULE_DESCRIPTION("Ambarella IPC: i_alsa_op.x");
