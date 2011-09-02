/*
 * drivers/ambarella-ipc/server/lk_sdhc_server.c
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

#include <linux/time.h>
#include <linux/module.h>

#include <linux/aipc/aipc.h>
#if !defined(__DEPENDENCY_GENERATION__)
#include "ipcgen/lk_sdhc.h"
#define __IPC_SERVER_IMPL__
#include "ipcgen/lk_sdhc_tab.i"
#endif

static struct ipc_prog_s lk_sdhc_prog =
{
	.name = "lk_sdhc",
	.prog = LK_SDHC_PROG,
	.vers = LK_SDHC_VERS,
	.table =  (struct ipcgen_table *) &lk_sdhc_prog_1_table,
	.nproc = lk_sdhc_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};

/* vsdhc_event() */

bool_t vsdhc_event_1_svc(struct vsdhc_event_arg *arg, void *res,
			 struct svc_req *rqstp)
{
	extern void ipc_vsdhc_got_event(int, int, int, int);

	ipc_vsdhc_got_event(arg->host, arg->card, arg->event, arg->type);

	rqstp->svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(rqstp->svcxprt, NULL);

	return 1;
}

/* vsdhc_sdio_interrupt() */

bool_t vsdhc_sdio_interrupt_1_svc(int *arg, void *res, struct svc_req *rqstp)
{

	extern void ipc_vsdhc_got_sdio_interrupt(int);

	ipc_vsdhc_got_sdio_interrupt(*arg);

	rqstp->svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(rqstp->svcxprt, NULL);

	return 1;
}

/*
 * Service initialization.
 */
static int __init lk_sdhc_init(void)
{
	return ipc_svc_prog_register(&lk_sdhc_prog);
}

/*
 * Service removal.
 */
static void __exit lk_sdhc_cleanup(void)
{
	ipc_svc_prog_unregister(&lk_sdhc_prog);
}


module_init(lk_sdhc_init);
module_exit(lk_sdhc_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Charles Chiou <cchiou@ambarella.com>");
MODULE_DESCRIPTION("Ambarella IPC: lk_sdhc.x");
