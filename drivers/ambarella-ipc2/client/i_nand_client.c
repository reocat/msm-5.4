/*
 * drivers/ambarella-ipc/client/i_nand_client.c
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

#include <linux/aipc/aipc.h>
#include <linux/aipc/i_nand.h>
#if !defined(__DEPENDENCY_GENERATION__)
#include "ipcgen/i_nand.h"
#define __IPC_CLIENT_IMPL__
#include "ipcgen/i_nand_tab.i"
#endif

static CLIENT *IPC_i_nand = NULL;

static struct ipc_prog_s i_nand_prog =
{
	.name = "i_nand",
	.prog = I_NAND_PROG,
	.vers = I_NAND_VERS,
	.table = (struct ipcgen_table *) i_nand_prog_1_table,
	.nproc = i_nand_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};

/*
 * IPC: i_nand.vnand_reset().
 */
int ipc_nand_reset(unsigned int bank)
{
	enum clnt_stat stat;
	struct vnand_reset_arg arg;
	struct vnand_reset_res res;

	arg.bank = bank;

	stat = vnand_reset_1(&arg, &res, IPC_i_nand);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return res.rval;
}
EXPORT_SYMBOL(ipc_nand_reset);

/*
 * IPC: i_nand.vnand_read_id().
 */
int ipc_nand_read_id(unsigned int addr_hi, unsigned int addr,
			 unsigned int *id)
{
	enum clnt_stat stat;
	struct vnand_read_id_arg arg;
	struct vnand_read_id_res res;

	arg.addr_hi = addr_hi;
	arg.addr = addr;

	stat = vnand_read_id_1(&arg, &res, IPC_i_nand);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	*id = res.id;

	return res.rval;
}
EXPORT_SYMBOL(ipc_nand_read_id);

/*
 * IPC: i_nand.read_status().
 */
int ipc_nand_read_status(unsigned int addr_hi, unsigned int addr,
			 unsigned int *status)
{
	enum clnt_stat stat;
	struct vnand_read_status_arg arg;
	struct vnand_read_status_res res;

	arg.addr_hi = addr_hi;
	arg.addr = addr;

	stat = vnand_read_status_1(&arg, &res, IPC_i_nand);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	*status = res.status;

	return res.rval;
}
EXPORT_SYMBOL(ipc_nand_read_status);

/*
 * IPC: i_nand.copyback().
 */
int ipc_nand_copyback(unsigned int addr_hi, unsigned int addr,
		      unsigned int dst, int interleave)
{
	enum clnt_stat stat;
	struct vnand_copyback_arg arg;
	struct vnand_copyback_res res;

	arg.addr_hi = addr_hi;
	arg.addr = addr;
	arg.dst = dst;
	arg.interleave = interleave;

	stat = vnand_copyback_1(&arg, &res, IPC_i_nand);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return res.rval;
}
EXPORT_SYMBOL(ipc_nand_copyback);

/*
 * IPC: i_nand.erase().
 */
int ipc_nand_erase(unsigned int addr_hi, unsigned int addr,
		   unsigned int interleave)
{
	enum clnt_stat stat;
	struct vnand_erase_arg arg;
	struct vnand_erase_res res;

	arg.addr_hi = addr_hi;
	arg.addr = addr;
	arg.interleave = interleave;

	stat = vnand_erase_1(&arg, &res, IPC_i_nand);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return res.rval;
}
EXPORT_SYMBOL(ipc_nand_erase);

/*
 * IPC: i_nand.read().
 */
int ipc_nand_read(unsigned int addr_hi, unsigned int addr,
		  char *buf, unsigned int len,
		  int area, int ecc, int interleave)
{
	enum clnt_stat stat;
	struct vnand_read_arg arg;
	struct vnand_read_res res;

	arg.addr_hi = addr_hi;
	arg.addr = addr;
	arg.buf = buf;
	arg.len = len;
	arg.area = area;
	arg.ecc = ecc;
	arg.interleave = interleave;

	ambcache_inv_range (buf, len);

	stat = vnand_read_1(&arg, &res, IPC_i_nand);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	ambcache_inv_range (buf, len);

	return res.rval;
}
EXPORT_SYMBOL(ipc_nand_read);

/*
 * IPC: i_nand.burst_read().
 */
int ipc_nand_burst_read(unsigned int addr_hi, unsigned int addr,
			char *buf, unsigned int len, int nbr,
			int area, int ecc, int interleave)
{
	enum clnt_stat stat;
	struct vnand_burst_read_arg arg;
	struct vnand_burst_read_res res;

	arg.addr_hi = addr_hi;
	arg.addr = addr;
	arg.buf = buf;
	arg.len = len;
	arg.area = area;
	arg.nbr = nbr;
	arg.ecc = ecc;
	arg.interleave = interleave;

	ambcache_inv_range (buf, len);

	stat = vnand_burst_read_1(&arg, &res, IPC_i_nand);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	ambcache_inv_range (buf, len);

	return res.rval;
}
EXPORT_SYMBOL(ipc_nand_burst_read);

/*
 * IPC: i_nand.write().
 */
int ipc_nand_write(unsigned int addr_hi, unsigned int addr,
		   char *buf, unsigned int len,
		   int area, int ecc, int interleave)
{
	enum clnt_stat stat;
	struct vnand_write_arg arg;
	struct vnand_write_res res;

	arg.addr_hi = addr_hi;
	arg.addr = addr;
	arg.buf = buf;
	arg.len = len;
	arg.area = area;
	arg.ecc = ecc;
	arg.interleave = interleave;

	ambcache_clean_range (buf, len);

	stat = vnand_write_1(&arg, &res, IPC_i_nand);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return res.rval;
}
EXPORT_SYMBOL(ipc_nand_write);

/*
 * Client initialization.
 */
static int __init i_nand_init(void)
{
	IPC_i_nand = ipc_clnt_prog_register(&i_nand_prog);
	if (IPC_i_nand == NULL)
		return -EPERM;

	return 0;
}

/*
 * Client exit.
 */
static void __exit i_nand_cleanup(void)
{
	if (IPC_i_nand) {
		ipc_clnt_prog_unregister(&i_nand_prog, IPC_i_nand);
		IPC_i_nand = NULL;
	}
}

subsys_initcall_sync(i_nand_init);
module_exit(i_nand_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Charles Chiou <cchiou@ambarella.com>");
MODULE_DESCRIPTION("Ambarella IPC: i_nand.x");
