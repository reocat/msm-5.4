/*
 * drivers/ambarella-ipc/server/lk_touch_server.c
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
#include <linux/vmalloc.h>
#include <linux/time.h>
#include <linux/module.h>

#include <linux/aipc/aipc.h>
#include <linux/aipc/lk_touch.h>
#if !defined(__DEPENDENCY_GENERATION__)
#include "ipcgen/lk_touch.h"
#define __IPC_SERVER_IMPL__
#include "ipcgen/lk_touch_tab.i"
#endif

static struct ipc_prog_s lk_touch_prog =
{
	.name = "lk_touch",
	.prog = LK_TOUCH_PROG,
	.vers = LK_TOUCH_VERS,
	.table =  (struct ipcgen_table *) &lk_touch_prog_1_table,
	.nproc = lk_touch_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};

/* ------------------------------------------------------------------------- */

bool_t vtouch_press_abs_mt_sync_1_svc(struct vtouch_data *arg, void *res,
	struct svc_req *rqstp)
{
	struct amba_vtouch_data *data = (struct amba_vtouch_data *)arg;
	
	amba_vtouch_press_abs_mt_sync(data);
	
	rqstp->svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(rqstp->svcxprt, NULL);

	return 1;
}

bool_t vtouch_release_abs_mt_sync_1_svc(struct vtouch_data *arg, void *res,
	struct svc_req *rqstp)
{
	struct amba_vtouch_data *data = (struct amba_vtouch_data *)arg;
	
	amba_vtouch_release_abs_mt_sync(data);
	
	rqstp->svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(rqstp->svcxprt, NULL);

	return 1;
}

/* ------------------------------------------------------------------------- */

/*
 * Service initialization.
 */
static int __init lk_touch_init(void)
{
	return ipc_svc_prog_register(&lk_touch_prog);
}

/*
 * Service removal.
 */
static void __exit lk_touch_cleanup(void)
{
	ipc_svc_prog_unregister(&lk_touch_prog);
}


module_init(lk_touch_init);
module_exit(lk_touch_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Charles Chiou <cchiou@ambarella.com>");
MODULE_DESCRIPTION("Ambarella IPC: lk_touch.x");

