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
static struct pcm_info tx_info;
static struct pcm_info rx_info;

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
 * IPC: ipc_ialsa_tx_open(). (async auto_del)
 */
int ipc_ialsa_tx_open(unsigned int ch, unsigned int freq, unsigned int base,
  int size, int desc_num)
{
	enum clnt_stat stat;

	tx_info.channels = ch;
	tx_info.freq = freq;
	tx_info.base = base;
	tx_info.size = size;
	tx_info.desc_num = desc_num;
	stat = ialsa_tx_open_1(&tx_info, NULL, IPC_i_alsa_op, NULL);

	return 0;
}
EXPORT_SYMBOL(ipc_ialsa_tx_open);

/*
 * IPC: ipc_ialsa_rx_open(). (async auto_del)
 */
int ipc_ialsa_rx_open(unsigned int ch, unsigned int freq, unsigned int base,
  int size, int desc_num)
{
	enum clnt_stat stat;

	rx_info.channels = ch;
	rx_info.freq = freq;
	rx_info.base = base;
	rx_info.size = size;
	rx_info.desc_num = desc_num;
	stat = ialsa_rx_open_1(&rx_info, NULL, IPC_i_alsa_op, NULL);
	return 0;
}
EXPORT_SYMBOL(ipc_ialsa_rx_open);

/*
 * IPC: ipc_ialsa_tx_start(). (async auto_del)
 */
int ipc_ialsa_tx_start(void)
{
	enum clnt_stat stat;
	stat = ialsa_tx_start_1(NULL, NULL, IPC_i_alsa_op, NULL);
	return 0;
}
EXPORT_SYMBOL(ipc_ialsa_tx_start);

/*
 * IPC: ipc_ialsa_tx_stop(). (async auto_del)
 */
int ipc_ialsa_tx_stop(void)
{
	enum clnt_stat stat;
	stat = ialsa_tx_stop_1(NULL, NULL, IPC_i_alsa_op, NULL);
	return 0;
}
EXPORT_SYMBOL(ipc_ialsa_tx_stop);

/*
 * IPC: ipc_ialsa_tx_close(). (async auto_del)
 */
int ipc_ialsa_tx_close(void)
{
	enum clnt_stat stat;
	stat = ialsa_tx_close_1(NULL, NULL, IPC_i_alsa_op, NULL);
	return 0;
}
EXPORT_SYMBOL(ipc_ialsa_tx_close);

/*
 * IPC: ipc_ialsa_rx_start(). (async auto_del)
 */
int ipc_ialsa_rx_start(void)
{
	enum clnt_stat stat;
	stat = ialsa_rx_start_1(NULL, NULL, IPC_i_alsa_op, NULL);
	return 0;
}
EXPORT_SYMBOL(ipc_ialsa_rx_start);

/*
 * IPC: ipc_ialsa_rx_stop(). (async auto_del)
 */
int ipc_ialsa_rx_stop(void)
{
	enum clnt_stat stat;
	stat = ialsa_rx_stop_1(NULL, NULL, IPC_i_alsa_op, NULL);
	return 0;
}
EXPORT_SYMBOL(ipc_ialsa_rx_stop);

/*
 * IPC: ipc_ialsa_rx_close(). (async auto_del)
 */
int ipc_ialsa_rx_close(void)
{
	enum clnt_stat stat;
	stat = ialsa_rx_close_1(NULL, NULL, IPC_i_alsa_op, NULL);
	return 0;
}
EXPORT_SYMBOL(ipc_ialsa_rx_close);

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
