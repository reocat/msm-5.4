/*
 * drivers/ambarella-ipc/server/lk_vserial_server.c
 *
 * Authors:
 *	Eric Chen <pzchen@ambarella.com>
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

#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/time.h>
#include <linux/module.h>

#include <linux/aipc/aipc.h>
#include <linux/aipc/lk_vserial.h>
#if !defined(__DEPENDENCY_GENERATION__)
#include "ipcgen/lk_vserial.h"
#define __IPC_SERVER_IMPL__
#include "ipcgen/lk_vserial_tab.i"
#endif

static struct ipc_prog_s lk_vserial_prog =
{
	.name = "lk_vserial",
	.prog = LK_VSERIAL_PROG,
	.vers = LK_VSERIAL_VERS,
	.table =  (struct ipcgen_table *) &lk_vserial_prog_1_table,
	.nproc = lk_vserial_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};

/* ------------------------------------------------------------------------- */

static bool_t __receive_msg_1_svc(struct lk_vserial_msg *arg, int *res,
	SVCXPRT *svcxprt)
{
	struct amba_vserial_msg msg;
	msg.size = arg->size;
	msg.base_addr = arg->base_addr;

	//printk("receive msg addr : %p, size : %d \n", arg->base_addr, arg->size);
	amba_vserial_report_msg(&msg);
	*res = 0;

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t receive_msg_1_svc(struct lk_vserial_msg *arg, int *res,
	struct svc_req *rqstp)
{
	//use bh to exist interrup domain
	ipc_bh_queue((ipc_bh_f) __receive_msg_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/* ------------------------------------------------------------------------- */

/*
 * Service initialization.
 */
static int __init lk_vserial_init(void)
{
	return ipc_svc_prog_register(&lk_vserial_prog);
}

/*
 * Service removal.
 */
static void __exit lk_vserial_cleanup(void)
{
	ipc_svc_prog_unregister(&lk_vserial_prog);
}


module_init(lk_vserial_init);
module_exit(lk_vserial_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eric Chen <pzchen@ambarella.com>");
MODULE_DESCRIPTION("Ambarella IPC: lk_vserial.x");
