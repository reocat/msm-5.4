/*
 *
 * Copyright (C) 2012-2016, Ambarella, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/rpmsg.h>
#include <linux/err.h>
#include <linux/remoteproc.h>
#include <linux/aipc_msg.h>

#include <plat/rpdev.h>

#include "aipc_priv.h"

#define chnl_tx_name "aipc_rpc2"

static struct rpdev *rpdev;

/*
 * forward incoming packet from remote to router
 */
static void rpdev_rpc2_cb(struct rpdev *rpdev, void *data, int len,
				 void *priv, u32 src)
{
	DMSG("rpmsg_rpc recv %d bytes\n", len);
	aipc_router_send((struct aipc_pkt*)data, len);
}

/*
 * send out packet targeting Linux1
 */
static void rpdev_rpc2_send(struct aipc_pkt *pkt, int len, int port)
{
	while (rpdev_trysend(rpdev, (char*)pkt, len))
		;
}

static void rpdev_rpc2_init_work(struct work_struct *work)
{
	struct xprt_ops ops_tx = {
		.send = rpdev_rpc2_send,
	};
	aipc_router_register_xprt(AIPC_HOST_LINUX1, &ops_tx);

	rpdev = rpdev_alloc("aipc_rpc2", 0, rpdev_rpc2_cb, NULL);
	rpdev_register(rpdev, "ca9_a_and_b");
}

static struct work_struct work;
extern void aipc_nl_send(struct aipc_pkt *pkt, int len, int port);

static int __init rpdev_rpc2_init(void)
{
	struct xprt_ops ops_tx = {
		.send = aipc_nl_send,
	};
	aipc_router_register_xprt(AIPC_HOST_LINUX2, &ops_tx);
	INIT_WORK(&work, rpdev_rpc2_init_work);
	schedule_work(&work);
}

static void __exit rpdev_rpc2_fini(void)
{
}

module_init(rpdev_rpc2_init);
module_exit(rpdev_rpc2_fini);

MODULE_DESCRIPTION("RPMSG RPC2 Client");
