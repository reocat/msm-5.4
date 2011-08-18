/*
 * drivers/ambarella-ipc/server/lk_sdresp_server.c
 *
 * Authors:
 *	Evan Chen <kfchen@ambarella.com>
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
 * Copyright (C) 2011-2015, Ambarella Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/aipc/aipc.h>
#if !defined(__DEPENDENCY_GENERATION__)
#include "ipcgen/lk_sdresp.h"
#define __IPC_SERVER_IMPL__
#include "ipcgen/lk_sdresp_tab.i"
#endif

//#define ENABLE_DEBUG_MSG_LKSDRESP
#ifdef ENABLE_DEBUG_MSG_LKSDRESP
#define DEBUG_MSG printk
#else
#define DEBUG_MSG(...)
#endif

static struct ipc_prog_s lk_sdresp_prog =
{
	.name = "lk_sdresp",
	.prog = LK_SDRESP_PROG,
	.vers = LK_SDRESP_VERS,
	.table =  (struct ipcgen_table *) &lk_sdresp_prog_1_table,
	.nproc = lk_sdresp_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};

/* lk_sdresp_ioctl() */

static bool_t __lk_sdresp_detect_change_1_svc(struct lk_sdresp_arg *arg,
					      int *res, SVCXPRT *svcxprt)
{
#ifdef CONFIG_MMC_AMBARELLA
	extern void ambarella_sd_cd_ipc(int slot_id);
	ambarella_sd_cd_ipc(arg->slot);
#endif
	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t lk_sdresp_detect_change_1_svc(struct lk_sdresp_arg *arg, int *res, struct svc_req *rqstp)
{
	ipc_bh_queue((ipc_bh_f) __lk_sdresp_detect_change_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/*
 * Service initialization.
 */
static int __init lk_sdresp_init(void)
{
	return ipc_svc_prog_register(&lk_sdresp_prog);
}

/*
 * Service removal.
 */
static void __exit lk_sdresp_cleanup(void)
{
	ipc_svc_prog_unregister(&lk_sdresp_prog);
}


module_init(lk_sdresp_init);
module_exit(lk_sdresp_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Evan Chen <kfchen@ambarella.com>");
MODULE_DESCRIPTION("Ambarella IPC: lk_sdresp.x");

