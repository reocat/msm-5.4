/*
 * drivers/ambarella-ipc/server/lk_msg_reply_server.c
 *
 * @Author: Hanbo Xiao
 * @Email : hbxiao@ambarell.com
 * @Time  : 08/09/2011
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
#include <linux/delay.h>

#include <linux/aipc/aipc.h>
#if !defined(__DEPENDENCY_GENERATION__)
#include "ipcgen/lk_msg_reply.h"
#define __IPC_SERVER_IMPL__
#include "ipcgen/lk_msg_reply_tab.i"
#endif

static struct ipc_prog_s lk_msg_reply_prog =
{
	.name = "lk_msg_reply",
	.prog = LK_MSG_REPLY_PROG,
	.vers = LK_MSG_REPLY_VERS,
	.table =  (struct ipcgen_table *) &lk_msg_reply_prog_1_table,
	.nproc = lk_msg_reply_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};

/* lk_msg_reply() */
static bool_t __lk_msg_reply_1_svc(struct lk_msg_reply_s *arg, int *res,
				    SVCXPRT *svcxprt)
{
	printk (KERN_NOTICE "Message replied from ITRON: %s", arg->msg_content);
	
	/* Some logical operations can be handled here. */

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t lk_msg_reply_1_svc(struct lk_msg_reply_s *arg, int *res,
			   struct svc_req *rqstp)
{
	ipc_bh_queue((ipc_bh_f) __lk_msg_reply_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}


/* ------------------------------------------------------------------------- */

/*
 * Service initialization.
 */
static int __init lk_msg_reply_init(void)
{
	return ipc_svc_prog_register(&lk_msg_reply_prog);
}

/*
 * Service removal.
 */
static void __exit lk_msg_reply_cleanup(void)
{
	ipc_svc_prog_unregister(&lk_msg_reply_prog);
}

module_init(lk_msg_reply_init);
module_exit(lk_msg_reply_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hanbo Xiao <hbxiao@ambarella.com>");
MODULE_DESCRIPTION("Ambarella IPC: lk_msg_reply.x");
