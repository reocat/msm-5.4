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

#include "aipc_priv.h"
/*#include <plat/rpmsg_compat.h>*/

#define chnl_tx_name "aipc_rpc2"

//static struct rpmsg_channel *chnl_tx;
static struct rpmsg_device *chnl_tx;

/*
 * forward incoming packet from remote to router
 */
static int rpmsg_rpc2_recv(struct rpmsg_device *rpdev, void *data, int len,
                           void *priv, u32 src)
{
	DMSG("rpmsg_rpc recv %d bytes\n", len);
	aipc_router_send((struct aipc_pkt*)data, len);
}

/*
 * send out packet targeting ThreadX
 */
static void rpmsg_rpc2_send_tx(struct aipc_pkt *pkt, int len, int port)
{
	if (chnl_tx) {
		rpmsg_send(chnl_tx->ept, pkt, len);
	}
}

static int rpmsg_rpc2_probe(struct rpmsg_device *rpdev)
{
	int ret = 0;
	//struct rpmsg_ns_msg nsm;
	struct rpmsg_channel_info chinfo;

	if (!strcmp(rpdev->id.name, chnl_tx_name))
		chnl_tx = rpdev;

	strncpy(chinfo.name, rpdev->id.name, sizeof(chinfo.name));
	chinfo.src = RPMSG_ADDR_ANY;
	chinfo.dst = rpdev->dst;
	//nsm.addr = rpdev->dst;
	//memcpy(nsm.name, rpdev->id.name, RPMSG_NAME_SIZE);
	//nsm.flags = 0;

	rpmsg_send(rpdev->ept, &chinfo, sizeof(chinfo));

	return ret;
}

static void rpmsg_rpc2_remove(struct rpmsg_device *rpdev)
{
}

static struct rpmsg_device_id rpmsg_rpc2_id_table[] = {
	{ .name = chnl_tx_name, },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, rpmsg_rpc2_id_table);

static struct rpmsg_driver rpmsg_rpc2_host_driver = {
	.drv.name   = KBUILD_MODNAME,
	.drv.owner  = THIS_MODULE,
	.id_table   = rpmsg_rpc2_id_table,
	.probe      = rpmsg_rpc2_probe,
	.callback   = rpmsg_rpc2_recv,
	.remove     = rpmsg_rpc2_remove,
};

extern void aipc_nl_send(struct aipc_pkt *pkt, int len, int port);
static int __init rpmsg_rpc2_host_init(void)
{
	struct xprt_ops ops_tx = {
		.send = rpmsg_rpc2_send_tx,
	};
	struct xprt_ops ops_tx1 = {
		.send = aipc_nl_send,
	};
	aipc_router_register_xprt(AIPC_HOST_LINUX2, &ops_tx);
	aipc_router_register_xprt(AIPC_HOST_LINUX1, &ops_tx1);

	return register_rpmsg_driver(&rpmsg_rpc2_host_driver);
}

static void __exit rpmsg_rpc2_host_fini(void)
{
	unregister_rpmsg_driver(&rpmsg_rpc2_host_driver);
}

module_init(rpmsg_rpc2_host_init);
module_exit(rpmsg_rpc2_host_fini);

MODULE_DESCRIPTION("RPMSG RPC2 Server");
