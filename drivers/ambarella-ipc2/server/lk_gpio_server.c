/*
 * drivers/ambarella-ipc/server/lk_gpio_server.c
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
#include "ipcgen/lk_gpio.h"
#define __IPC_SERVER_IMPL__
#include "ipcgen/lk_gpio_tab.i"
#endif

static struct ipc_prog_s lk_gpio_prog =
{
	.name = "lk_gpio",
	.prog = LK_GPIO_PROG,
	.vers = LK_GPIO_VERS,
	.table =  (struct ipcgen_table *) &lk_gpio_prog_1_table,
	.nproc = lk_gpio_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};

/* vgpio_interrupted() */

bool_t vgpio_interrupted_1_svc(int *line, void *res,
				struct svc_req *rqstp)
{
	extern void ipc_gfi_got_event(int line);

	ipc_gfi_got_event(*line);

	rqstp->svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(rqstp->svcxprt, NULL);

	return 1;
}

/*
 * Service initialization.
 */
static int __init lk_gpio_init(void)
{
	return ipc_svc_prog_register(&lk_gpio_prog);
}

/*
 * Service removal.
 */
static void __exit lk_gpio_cleanup(void)
{
	ipc_svc_prog_unregister(&lk_gpio_prog);
}


module_init(lk_gpio_init);
module_exit(lk_gpio_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Charles Chiou <cchiou@ambarella.com>");
MODULE_DESCRIPTION("Ambarella IPC: lk_gpio.x");
