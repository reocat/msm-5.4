/*
 * drivers/ambarella-ipc/server/lk_button_server.c
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
#include <linux/aipc/lk_button.h>
#if !defined(__DEPENDENCY_GENERATION__)
#include "ipcgen/lk_button.h"
#define __IPC_SERVER_IMPL__
#include "ipcgen/lk_button_tab.i"
#endif

static struct ipc_prog_s lk_button_prog =
{
	.name = "lk_button",
	.prog = LK_BUTTON_PROG,
	.vers = LK_BUTTON_VERS,
	.table =  (struct ipcgen_table *) &lk_button_prog_1_table,
	.nproc = lk_button_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};

/* ------------------------------------------------------------------------- */

bool_t vbutton_key_pressed_1_svc(int *arg, void *res,
	struct svc_req *rqstp)
{
	//printk("%s, %d\n",__func__,*arg);

	amba_vbutton_key_pressed(*arg);
	
	rqstp->svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(rqstp->svcxprt, NULL);

	return 1;
}

bool_t vbutton_key_released_1_svc(int *arg, void *res,
	struct svc_req *rqstp)
{
	//printk("%s, %d\n",__func__,*arg);

	amba_vbutton_key_released(*arg);
	
	rqstp->svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(rqstp->svcxprt, NULL);

	return 1;
}

/* ------------------------------------------------------------------------- */

/*
 * Service initialization.
 */
static int __init lk_button_init(void)
{
	return ipc_svc_prog_register(&lk_button_prog);
}

/*
 * Service removal.
 */
static void __exit lk_button_cleanup(void)
{
	ipc_svc_prog_unregister(&lk_button_prog);
}


module_init(lk_button_init);
module_exit(lk_button_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Charles Chiou <cchiou@ambarella.com>");
MODULE_DESCRIPTION("Ambarella IPC: lk_button.x");

