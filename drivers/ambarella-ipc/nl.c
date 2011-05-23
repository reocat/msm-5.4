/*
 * drivers/ambarella-ipc/nl.c
 *
 * Ambarella IPC in Linux kernel-space.
 *
 * Authors:
 *	Cao Rongrong <rrcao@ambarella.com>
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <net/sock.h>
#include <net/netlink.h>
#include <linux/spinlock.h>
#include <linux/list.h>

#include <linux/aipc/aipc.h>
#include "binder.h"

static inline int aipc_nl_rq_equal(SVCXPRT *svcxprt, struct aipc_nl_data *hdr)
{
	return ((svcxprt->pid  == hdr->pid)  &&
		(svcxprt->vers == hdr->vers) &&
		(svcxprt->fid  == hdr->fid)  &&
		(svcxprt->xid  == hdr->xid));
}

static inline void ipc_binder_add_donelist(SVCXPRT *svcxprt)
{
	unsigned long flags;

	spin_lock_irqsave(&binder->lu_done_lock, flags);
	list_add_tail(&svcxprt->u.l.list, &binder->lu_done_list);
	spin_unlock_irqrestore(&binder->lu_done_lock, flags);	
}

static inline void ipc_binder_del_donelist(SVCXPRT *svcxprt)
{
	unsigned long flags;

	spin_lock_irqsave(&binder->lu_done_lock, flags);
	list_del_init(&svcxprt->u.l.list);
	spin_unlock_irqrestore(&binder->lu_done_lock, flags);	
}

static inline SVCXPRT *ipc_binder_find_done(struct aipc_nl_data *aipc_hdr)
{
	SVCXPRT *svcxprt;
	int found = 0;
	unsigned long flags;

	spin_lock_irqsave(&binder->lu_done_lock, flags);
	list_for_each_entry(svcxprt, &binder->lu_done_list, u.l.list) {
		if (aipc_nl_rq_equal(svcxprt, aipc_hdr)) {
			found = 1;
			break;
		}
	}
	if (found)
		list_del(&svcxprt->u.l.list);
	spin_unlock_irqrestore(&binder->lu_done_lock, flags);	

	if (!found)
		svcxprt = NULL;

	return svcxprt;
}

/*
 * Transfer the fields from SVCXPRT (in kernel) to fields in
 * struct aipc_nl_data that goes into the user-space AIPC library.
 */
static inline void aipc_nl_set_header(struct aipc_nl_data *aipc_hdr,
				      SVCXPRT *svcxprt)
{
	aipc_hdr->xid = svcxprt->xid;
	aipc_hdr->pid = svcxprt->pid;
	aipc_hdr->fid = svcxprt->fid;
	aipc_hdr->vers = svcxprt->vers;
	aipc_hdr->len_arg = svcxprt->len_arg;
	aipc_hdr->len_res = svcxprt->len_res;
	aipc_hdr->rcode = svcxprt->rcode;
}

/*
 * Loate a registered program from user-space.
 */
static struct aipc_nl_prog *aipc_nl_search_prog(unsigned int prog)
{
	int found = 0;
	struct aipc_nl_prog *nl_prog;

	list_for_each_entry(nl_prog, &binder->lu_prog_list, list) {
		if (nl_prog->prog_info.pid == prog) {
			found = 1;
			break;
		}
	}

	if (found == 0)
		nl_prog = NULL;

	return nl_prog;
}

/*
 * Process a request destined for a user-space server. First transcode
 * from SVCXPRT to netlink message format and then unicast it to a
 * user-space process.
 */
int aipc_nl_get_request_from_itron(SVCXPRT *svcxprt)
{
	int len, rval;
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	struct aipc_nl_data *aipc_hdr;
	struct aipc_nl_prog *nl_prog;
	unsigned long flags;

	spin_lock_irqsave(&binder->lu_prog_lock, flags);
	nl_prog = aipc_nl_search_prog(svcxprt->pid);
	if (!nl_prog) {
		svcxprt->rcode = IPC_PROGUNAVAIL;
		spin_unlock_irqrestore(&binder->lu_prog_lock, flags);
		ipc_svc_sendreply(svcxprt, NULL);
		return -1;
	}
	spin_unlock_irqrestore(&binder->lu_prog_lock, flags);

	len = NLMSG_SPACE(NL_AIPC_HDRLEN + svcxprt->len_arg);
	skb = alloc_skb(len, GFP_KERNEL);
	if (!skb){
		pr_err("%s: no memory.\n", __func__);
		return -1;
	}

	nlh = nlmsg_put(skb, 0, 0, 0, NL_AIPC_HDRLEN + svcxprt->len_arg, 0);
	NETLINK_CB(skb).pid = 0;	/* from kernel */
	NETLINK_CB(skb).dst_group = 0;

	aipc_hdr = (struct aipc_nl_data *) NLMSG_DATA(nlh);
	aipc_nl_set_header(aipc_hdr, svcxprt);
	memcpy(NL_AIPC_DATA(aipc_hdr), svcxprt->arg, svcxprt->len_arg);

	ipc_binder_add_donelist(svcxprt);

	rval = netlink_unicast(binder->nlsock, skb,
			       nl_prog->prog_info.uid, MSG_DONTWAIT);
	if (rval < 0) {
		pr_err("can't unicast skb (%d)\n", rval);
		ipc_binder_del_donelist(svcxprt);
		kfree_skb(skb);
		/* Maybe the nl_prog have been unregistered,
		 * so we need to search it again */
		spin_lock_irqsave(&binder->lu_prog_lock, flags);
		nl_prog = aipc_nl_search_prog(svcxprt->pid);
		if (nl_prog) {
			list_del(&nl_prog->list);
			kfree(nl_prog);
		}
		spin_unlock_irqrestore(&binder->lu_prog_lock, flags);
		return -1;
	}

	return 0;
}

/*
 * Send a reply to a request to uITRON.
 */
static int aipc_nl_send_result_to_itron(struct nlmsghdr *nlh)
{
	struct aipc_nl_data *aipc_hdr;
	SVCXPRT *svcxprt;
	char *out;
	int rval = 0;

	aipc_hdr = NLMSG_DATA(nlh);
	svcxprt = ipc_binder_find_done(aipc_hdr);
	if (svcxprt) {
		if (aipc_hdr->rcode == IPC_SUCCESS)
			out = NL_AIPC_DATA(aipc_hdr);
		else
			out = NULL;
		svcxprt->rcode = aipc_hdr->rcode;
		rval = ipc_svc_sendreply(svcxprt, out);
	} else {
		pr_err("no request is available!\n");
		rval = -1;
	}

	return rval;
}

/*
 * Pull request from netlink, decode it and make an actual call to the remote
 * OS.
 */
static int aipc_nl_get_request_from_lu(struct nlmsghdr *nlh)
{
	enum clnt_stat rval;
	struct aipc_nl_data *aipc_hdr;
	unsigned int clnt_pid;
	char *buf;

	buf = ipc_malloc(NL_MAX_BUFLEN); /* It will be freed after
					    send result to lu */
	clnt_pid = nlh->nlmsg_pid;
	aipc_hdr = NLMSG_DATA(nlh);

	rval = ipc_lu_clnt_call(clnt_pid, aipc_hdr,
				NL_AIPC_DATA(aipc_hdr), buf);
	if (rval != IPC_SUCCESS) {
		pr_err("%s(): ipc_call failed (%d)\n", __func__, rval);
		ipc_free(buf);
	}

	return 0;
}

/*
 * Send the result of a request to linux user-space process.
 */
int aipc_nl_send_result_to_lu(SVCXPRT *svcxprt)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	struct aipc_nl_data *aipc_hdr;
	int len, rval = 0;

	len = NLMSG_SPACE(NL_AIPC_HDRLEN + svcxprt->len_res);
	skb = alloc_skb(len, GFP_ATOMIC);
	if (!skb){
		pr_err("%s: no memory.\n", __func__);
		ipc_free(svcxprt->res);
		return -1;
	}
	nlh = nlmsg_put(skb, 0, 0, 0, NL_AIPC_HDRLEN + svcxprt->len_res, 0);
	NETLINK_CB(skb).pid = 0;	/* from kernel */
	NETLINK_CB(skb).dst_group = 0;

	aipc_hdr = (struct aipc_nl_data *) NLMSG_DATA(nlh);
	aipc_nl_set_header(aipc_hdr, svcxprt);
	memcpy(NL_AIPC_DATA(aipc_hdr), svcxprt->res, svcxprt->len_res);

	rval = netlink_unicast(binder->nlsock, skb, svcxprt->u.l.clnt_pid,
			       MSG_DONTWAIT);
	if (rval < 0) {
		pr_err("can not unicast skb (%d)\n", rval);
		kfree_skb(skb);
		rval = -1;
	}

	ipc_free(svcxprt->res);

	return rval;
}

/*
 * Register a program/server from user-space to the AIPC subsystem.
 */
static enum aipc_nl_status aipc_nl_register_prog(struct nlmsghdr *nlh)
{
	enum aipc_nl_status status = AIPC_NLSTATUS_REGISTERED;
	struct aipc_nl_prog *nl_prog;
	struct aipc_nl_prog_info *prog_info;
	struct aipc_nl_proc_info *proc_info;
	char *data = NLMSG_DATA(nlh);
	unsigned int prog, len, i;
	unsigned long flags;

	spin_lock_irqsave(&binder->lu_prog_lock, flags);

	if (nlh->nlmsg_len < NLMSG_SPACE(sizeof(struct aipc_nl_prog_info)))
		return AIPC_NLSTATUS_ERR_EINVAL;

	prog_info = (struct aipc_nl_prog_info *)data;
	proc_info = (struct aipc_nl_proc_info *)(data + sizeof(*prog_info));

	prog = prog_info->pid;
	if (prog < MIN_LUIPC_PROG_NUM) {
		status = AIPC_NLSTATUS_ERR_OTHERS;
		goto done;
	}

	/* Check if the prog has been registered or not */
	nl_prog = aipc_nl_search_prog(prog);
	if (nl_prog != NULL) {
		status = AIPC_NLSTATUS_ERR_EXIST;
		goto done;
	}

	len = sizeof(*nl_prog) + prog_info->nproc * sizeof(*proc_info);
	nl_prog = kzalloc(len, GFP_KERNEL);
	if (nl_prog == NULL) {
		pr_err("no memory for aipc_nl_prog\n");
		status = AIPC_NLSTATUS_ERR_OTHERS;
		goto done;
	}

	memcpy(&nl_prog->prog_info, prog_info, sizeof(*prog_info));
	for (i = 0; i < prog_info->nproc; i++)
		memcpy(nl_prog->proc_info + i, proc_info++, sizeof(*proc_info));
	nl_prog->status = status;
	list_add_tail(&nl_prog->list, &binder->lu_prog_list);

done:
	spin_unlock_irqrestore(&binder->lu_prog_lock, flags);

	return status;

}

/*
 * Unregister a program/server from the AIPC subsystem.
 */
static enum aipc_nl_status aipc_nl_unregister_prog(struct nlmsghdr *nlh)
{
	enum aipc_nl_status status = AIPC_NLSTATUS_UNREGISTERED;
	struct aipc_nl_prog *nl_prog;
	unsigned int prog;
	unsigned long flags;

	spin_lock_irqsave(&binder->lu_prog_lock, flags);

	prog = *((unsigned int *) NLMSG_DATA(nlh));
	if (prog < MIN_LUIPC_PROG_NUM) {
		status = AIPC_NLSTATUS_ERR_OTHERS;
		goto done;
	}

	nl_prog = aipc_nl_search_prog(prog);
	if (nl_prog == NULL) {	/* The prog has never been registered.  */
		status = AIPC_NLSTATUS_ERR_NOENT;
		goto done;
	}

	list_del(&nl_prog->list);
	kfree(nl_prog);

done:
	spin_unlock_irqrestore(&binder->lu_prog_lock, flags);

	return status;

}


/*
 * Send STATUS back to user.
 * We reuse the skb, so the prog number is still stored in it.
 */
static int aipc_nl_ack(struct sk_buff *skb,
		       int nlmsg_pid,
		       enum aipc_nl_status status)
{
	int rval = 0;
	unsigned int prog;
	struct aipc_nl_prog *nl_prog;
	struct nlmsghdr *nlh;

	/* Sanity check */
	if (skb == NULL || skb->len <= NLMSG_HDRLEN || nlmsg_pid == 0) {
		rval = -EINVAL;
		goto done;
	}

	nlh = nlmsg_hdr(skb);
	if (!nlh || nlh->nlmsg_len <= NLMSG_HDRLEN) {
		rval = -EINVAL;
		goto done;
	}

	NETLINK_CB(skb).pid = 0;		/* from kernel */
	NETLINK_CB(skb).dst_group = 0;		/* unicast */

	prog = *((unsigned int *) NLMSG_DATA(nlh));
	*((enum aipc_nl_status *) NLMSG_DATA(nlh)) = status;

	rval = netlink_unicast(binder->nlsock, skb, nlmsg_pid, MSG_DONTWAIT);
	if (rval < 0) {
		nl_prog = aipc_nl_search_prog(prog);
		if (nl_prog) {
			list_del_init(&nl_prog->list);
			kfree(nl_prog);
		}

		kfree_skb(skb);
	}
done:
	return 0;
}

/*
 * Called when data is available from netlink; the request is pulled out
 * and processed based on the type of message we get.
 */
static void aipc_nl_data_ready(struct sk_buff *__skb)
{
	int ack = 0, nlmsg_pid = 0;
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	enum aipc_nl_status status;
	int rval;

	skb = skb_get(__skb);

	if (skb->len >= NLMSG_SPACE(0)) {
		nlh = nlmsg_hdr(skb);
		if (skb->len != nlh->nlmsg_len) {
			pr_info("%s(): nlmsg_len = %d\n", __func__,
				nlh->nlmsg_len);
		}

		nlmsg_pid = nlh->nlmsg_pid;

		switch(nlh->nlmsg_type)	{
		case AIPC_NLTYPE_RESULT:
			aipc_nl_send_result_to_itron(nlh);
			break;
		case AIPC_NLTYPE_REQUEST:
			aipc_nl_get_request_from_lu(nlh);
			break;
		case AIPC_NLTYPE_REGISTER:
			status = aipc_nl_register_prog(nlh);
			ack = 1;
			break;
		case AIPC_NLTYPE_UNREGISTER:
			status = aipc_nl_unregister_prog(nlh);
			ack = 1;
			break;

		default:
			pr_err("invalid aipc netlink type: %d\n",
			       nlh->nlmsg_type);
			status = AIPC_NLSTATUS_ERR_OTHERS;
			ack = 1;
			break;
		}
	}

	if (ack) {
		rval = aipc_nl_ack(skb, nlmsg_pid, status);
		if (rval < 0)
			pr_err("can't ack to lu (%d)\n", rval);
	} else {
		kfree_skb(skb);
	}

	return;
}

/*
 * Netlink notify event.
 */
static int ipc_nl_event(struct notifier_block *this,
			unsigned long event, void *ptr)
{
	struct netlink_notify *notify;
	struct aipc_nl_prog *nl_prog;
	unsigned long flags;

	if (!ptr || event != NETLINK_URELEASE)
		goto done;

	notify = (struct netlink_notify *) ptr;

	if (notify->protocol != NETLINK_AIPC ||
	    notify->pid == 0 ||
	    notify->net != &init_net)
		goto done;

	/* Find out the nl_prog registered with this pid (program id), */
	/* and delete it from lu_prog_list */
	spin_lock_irqsave(&binder->lu_prog_lock, flags);
	nl_prog = aipc_nl_search_prog(notify->pid);
	if (nl_prog != NULL) {
		list_del(&nl_prog->list);
		kfree(nl_prog);
	}
	spin_unlock_irqrestore(&binder->lu_prog_lock, flags);

done:

	return NOTIFY_DONE;
}

static struct notifier_block ipc_nl_notifier =
{
	.notifier_call	= ipc_nl_event,
};

/*
 * Initialize the AIPC netlink module.
 */
int aipc_nl_init(void)
{
	INIT_LIST_HEAD(&binder->lu_prog_list);
	INIT_LIST_HEAD(&binder->lu_req_list);
	INIT_LIST_HEAD(&binder->lu_done_list);

	netlink_register_notifier(&ipc_nl_notifier);
	binder->nlsock = netlink_kernel_create(&init_net, NETLINK_AIPC, 0,
					       aipc_nl_data_ready, NULL,
					       THIS_MODULE);
	if (binder->nlsock == NULL) {
		pr_err("aipc_net_link: cannot create netlink socket.\n");
		netlink_unregister_notifier(&ipc_nl_notifier);
		return -EIO;
	}

	return 0;
}

/*
 * Cleanup the AIPC netlink module.
 */
int aipc_nl_cleanup(void)
{
	if (binder->nlsock) {
		netlink_kernel_release(binder->nlsock);
		binder->nlsock = NULL;
	}

	netlink_unregister_notifier(&ipc_nl_notifier);

	return 0;
}

