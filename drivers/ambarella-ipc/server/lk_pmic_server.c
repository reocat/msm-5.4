/*
 * drivers/ambarella-ipc/server/lk_pmic_server.c
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

#include <linux/slab.h>
#include <linux/module.h>

#include <linux/aipc/aipc.h>
#include <linux/aipc/lk_pmic.h>
#if !defined(__DEPENDENCY_GENERATION__)
#include "ipcgen/lk_pmic.h"
#define __IPC_SERVER_IMPL__
#include "ipcgen/lk_pmic_tab.i"
#endif

static struct ipc_prog_s lk_pmic_prog =
{
	.name = "lk_pmic",
	.prog = LK_PMIC_PROG,
	.vers = LK_PMIC_VERS,
	.table =  (struct ipcgen_table *) &lk_pmic_prog_1_table,
	.nproc = lk_pmic_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};

/* ------------------------------------------------------------------------- */

bool_t lk_pmic_notify_1_svc(u_int *arg, int *res,
	struct svc_req *rqstp)
{

	*res = ambapmic_pwr_src_notify(*arg);
	
	rqstp->svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(rqstp->svcxprt, NULL);

	return 1;
}

/* ------------------------------------------------------------------------- */

/*
 * Service initialization.
 */
static int __init lk_pmic_init(void)
{
	return ipc_svc_prog_register(&lk_pmic_prog);
}

/*
 * Service removal.
 */
static void __exit lk_pmic_cleanup(void)
{
	ipc_svc_prog_unregister(&lk_pmic_prog);
}


module_init(lk_pmic_init);
module_exit(lk_pmic_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Charles Chiou <cchiou@ambarella.com>");
MODULE_DESCRIPTION("Ambarella IPC: lk_pmic.x");

