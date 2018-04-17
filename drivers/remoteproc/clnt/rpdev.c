/**
 * system/src/rpclnt/rpdev.c
 *
 * History:
 *    2012/08/15 - [Tzu-Jung Lee] created file
 *
 * Copyright 2008-2015 Ambarella Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/rpmsg.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <plat/rpdev.h>

#include <plat/rpmsg_compat.h>
#include "rpclnt.h"
#include "vq.h"



#if 0
/* The feature bitmap for virtio rpmsg */
#define VIRTIO_RPMSG_F_NS	0 /* RP supports name service notifications */
/**
   + * struct rpmsg_hdr - common header for all rpmsg messages
   + * @src: source address
   + * @dst: destination address
   + * @reserved: reserved for future use
   + * @len: length of payload (in bytes)
   + * @flags: message flags
   + * @data: @len bytes of message payload data
   + *
   + * Every message sent(/received) on the rpmsg bus begins with this header.
   + */
struct rpmsg_hdr {
	u32 src;
	u32 dst;
	u32 reserved;
	u16 len;
	u16 flags;
	u8 data[0];
} __packed;
/**
 * struct rpmsg_ns_msg - dynamic name service announcement message
 * @name: name of remote service that is published
 * @addr: address of remote service that is published
 * @flags: indicates whether service is created or destroyed
 *
 * This message is sent across to publish a new service, or announce
 * about its removal. When we receive these messages, an appropriate
 * rpmsg channel (i.e device) is created/destroyed. In turn, the ->probe()
 * or ->remove() handler of the appropriate rpmsg driver will be invoked
 * (if/as-soon-as one is registered).
*/
struct rpmsg_ns_msg {
	char name[RPMSG_NAME_SIZE];
	u32 addr;
	u32 flags;
} __packed;
/**
 * enum rpmsg_ns_flags - dynamic name service announcement flags
 *
 * @RPMSG_NS_CREATE: a new remote service was just created
 * @RPMSG_NS_DESTROY: a known remote service was just destroyed
*/
enum rpmsg_ns_flags {
	RPMSG_NS_CREATE		= 0,
	RPMSG_NS_DESTROY	= 1,
};
#endif



static struct rpdev *g_registered_rpdev[64];
static int g_registered_cnt;
DEFINE_SPINLOCK(spinlock_rpdev_tbl);

static struct rpdev *rpdev_lookup(int src, int dst)
{

	struct rpdev *rpdev = NULL;
	unsigned long flag;
	int i;

	spin_lock_irqsave(&spinlock_rpdev_tbl, flag);

	for (i = 0; i < g_registered_cnt; i++) {

		rpdev = g_registered_rpdev[i];

		if (rpdev->src == src && rpdev->dst == dst) {
		    spin_unlock_irqrestore(&spinlock_rpdev_tbl, flag);
		    return rpdev;
		}

	}

	spin_unlock_irqrestore(&spinlock_rpdev_tbl, flag);


	return NULL;
}

void rpdev_rvq_cb(struct vq *vq)
{
	struct rpmsg_hdr	*hdr;
	struct rpdev		*rpdev;
	int			len;
	int			idx;

	while (1) {

		idx = vq_get_avail_buf(vq, (void**)&hdr, &len);
		if (idx < 0) {

			vq_enable_used_notify(vq);

			/*
			 * In case any message slipped in the window, check
			 * again.  Otherwise, it will be delayed until next
			 * interrupt.
			 */
			idx = vq_get_avail_buf(vq, (void**)&hdr, &len);
			
			if (idx < 0) {
		
				return; 
			}
	

			/* Disable the intruupt, and continue to poll */
			vq_disable_used_notify(vq);

		}


		rpdev = rpdev_lookup(hdr->dst, hdr->src);

		if (rpdev) {


			rpdev->cb(rpdev, &hdr->data, hdr->len, rpdev->priv, hdr->src);

			vq_add_used_buf(vq, idx, RPMSG_BUF_SIZE);


			continue;
		}


		rpdev = rpdev_lookup(hdr->dst, RPMSG_NS_ADDR);

		if (rpdev) {


		

			if (hdr->len == sizeof(struct rpmsg_ns_msg)) {  
		
				rpdev->dst = hdr->src;
				complete(&rpdev->comp);
				
				vq_add_used_buf(vq, idx, RPMSG_BUF_SIZE);
			
			}
		}
	}
	vq->kick(rpdev->rpclnt);

}

void rpdev_svq_cb(struct vq *vq)
{
	vq_complete(vq);
	vq_enable_used_notify(vq);
}

int rpdev_send_offchannel(struct rpdev *rpdev, u32 src, u32 dst, void *data, int len)
{
	
	int idx				= 0;
	int buf_len			= 0;
	struct rpmsg_hdr *hdr		= NULL;
	
	struct vq *vq;

	vq = rpdev->rpclnt->svq;

	idx = vq_get_avail_buf(vq, (void**)&hdr, &buf_len);
	if (idx < 0)
		return -1;

	hdr->src	= src;
	hdr->dst	= dst;
	hdr->reserved	= 0;
	hdr->len	= len;
	hdr->flags	= 0;

	memcpy(hdr->data, data, len);

	vq_add_used_buf(vq, idx, buf_len);

	vq->kick(rpdev->rpclnt);

	return 0;
}
EXPORT_SYMBOL(rpdev_send_offchannel);

int rpdev_send(struct rpdev *rpdev, void *data, int len)
{
	struct vq *vq;

	
	while (rpdev_send_offchannel(rpdev, rpdev->src, rpdev->dst, data, len)) {

		vq = rpdev->rpclnt->svq;

		vq_enable_used_notify(vq);
		vq_wait_for_completion(vq);
		vq_disable_used_notify(vq);
	}
	

	return 0;
}
EXPORT_SYMBOL(rpdev_send);

int rpdev_trysend(struct rpdev *rpdev, void *data, int len)
{
	return rpdev_send_offchannel(rpdev, rpdev->src, rpdev->dst,
				     data, len);
}
EXPORT_SYMBOL(rpdev_trysend);

int rpdev_register(struct rpdev *rpdev, const char *bus_name)
{

	struct rpmsg_ns_msg nsm;


	struct rpclnt *rpclnt;


	rpclnt = rpclnt_sync(bus_name);
	
	rpdev->rpclnt = rpclnt;

	printk("[ %20s: %d ] ===============> to register rpdev->name= %s \n", __func__,__LINE__, rpdev->name);
	memcpy(&nsm.name, rpdev->name, RPMSG_NAME_LEN);
	////strncpy(chinfo.name, rpdev->name, RPMSG_NAME_LEN);
	nsm.addr = rpdev->src;
	////chinfo.src = rpdev->src;
	////chinfo.dst = RPMSG_NS_ADDR;
	nsm.flags = rpdev->flags;


	rpdev_send_offchannel(rpdev, rpdev->src, RPMSG_NS_ADDR,&nsm, sizeof(nsm));
	////rpdev_send_offchannel(rpdev, rpdev->src, RPMSG_NS_ADDR, &chinfo, sizeof(chinfo));

	wait_for_completion(&rpdev->comp);

	/*
	 * Supress the tx-complete interrupt by default.
	 *
	 * This interuupt will be re-enabled when necessary.
	 * When all available tx buffers are used up, users may
	 * be blocked if they were calling rpdev_send() API.
	 *
	 * In this case, we'd like to recieve interrupt when any
	 * tx-buffer is available so we can waken up the blocked
	 * users.
	 */
	vq_disable_used_notify(rpclnt->svq);
	

	return 0;
}
EXPORT_SYMBOL(rpdev_register);

struct rpdev *rpdev_alloc(const char *name, int flags, rpdev_cb cb, void *priv)
{
	
	struct rpdev *rpdev;
	unsigned long flag;

	rpdev = kmalloc(sizeof(*rpdev), GFP_KERNEL);

	strcpy(rpdev->name, name);

	spin_lock_irqsave(&spinlock_rpdev_tbl, flag);
	g_registered_rpdev[g_registered_cnt] = rpdev;

	rpdev->src	= RPMSG_RESERVED_ADDRESSES + g_registered_cnt++;
	spin_unlock_irqrestore(&spinlock_rpdev_tbl, flag);

	rpdev->dst	= RPMSG_NS_ADDR;
	rpdev->flags	= flags;
	rpdev->cb	= cb;
	rpdev->priv	= priv;

	init_completion(&rpdev->comp);


	return rpdev;
}
EXPORT_SYMBOL(rpdev_alloc);
