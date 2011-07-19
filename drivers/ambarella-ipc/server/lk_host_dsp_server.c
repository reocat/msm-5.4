/*
 * drivers/ambarella-ipc/server/lk_host_dsp_server.c
 *
 * Authors:
 *	Chris Lin <tflin@ambarella.com>
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
 * Copyright (C) 2010-2011, Ambarella Inc.
 */

#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/time.h>
#include <linux/module.h>

#include <linux/aipc/aipc.h>
#if !defined(__DEPENDENCY_GENERATION__)
#include "ipcgen/lk_host_dsp.h"
#define __IPC_SERVER_IMPL__
#include "ipcgen/lk_host_dsp_tab.i"
#endif

static struct ipc_prog_s lk_host_dsp_prog =
{
	.name = "lk_host_dsp",
	.prog = LK_HOST_DSP_PROG,
	.vers = LK_HOST_DSP_VERS,
	.table =  (struct ipcgen_table *) &lk_host_dsp_prog_1_table,
	.nproc = lk_host_dsp_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};

bool_t a2c_dsp_host_1_svc(void *arg, void *res, 
			  struct svc_req *rqstp)
{
  
  printk("a2c_dsp_host!\n");
  rqstp->svcxprt->rcode = IPC_SUCCESS;
  ipc_svc_sendreply(rqstp->svcxprt, NULL);

  return 1;
}


/*
 * Service initialization.
 */
static int __init lk_host_dsp_init(void)
{
	return ipc_svc_prog_register(&lk_host_dsp_prog);
}

/*
 * Service removal.
 */
static void __exit lk_host_dsp_cleanup(void)
{
	ipc_svc_prog_unregister(&lk_host_dsp_prog);
}


module_init(lk_host_dsp_init);
module_exit(lk_host_dsp_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chris Lin <tflin@ambarella.com>");
MODULE_DESCRIPTION("Ambarella IPC: lk_host_dsp.x");

