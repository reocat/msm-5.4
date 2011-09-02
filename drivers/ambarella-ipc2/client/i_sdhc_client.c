/*
 * drivers/ambarella-ipc/client/i_sdhc_client.c
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

#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/list.h>

#include <linux/aipc/aipc.h>
#include <linux/aipc/i_sdhc.h>
#if !defined(__DEPENDENCY_GENERATION__)
#include "ipcgen/i_sdhc.h"
#define __IPC_CLIENT_IMPL__
#include "ipcgen/i_sdhc_tab.i"
#endif

static CLIENT *IPC_i_sdhc = NULL;

static struct ipc_prog_s i_sdhc_prog =
{
	.name = "i_sdhc",
	.prog = I_SDHC_PROG,
	.vers = I_SDHC_VERS,
	.table = (struct ipcgen_table *) i_sdhc_prog_1_table,
	.nproc = i_sdhc_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};

/*
 * IPC: i_sdhc.vsdhc_num_host().
 */
int ipc_sdhc_num_host(void)
{
	enum clnt_stat stat;
	int rval;

	stat = vsdhc_num_host_1(NULL, &rval, IPC_i_sdhc);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return rval;
}
EXPORT_SYMBOL(ipc_sdhc_num_host);

/*
 * IPC: i_sdhc.vsdhc_num_slot().
 */
int ipc_sdhc_num_slot(int host)
{
	enum clnt_stat stat;
	int rval;

	stat = vsdhc_num_slot_1(&host, &rval, IPC_i_sdhc);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return rval;
}
EXPORT_SYMBOL(ipc_sdhc_num_slot);

/*
 * IPC: i_sdhc.vsdhc_card_in_slot().
 */
int ipc_sdhc_card_in_slot(int host, int card)
{
	enum clnt_stat stat;
	struct vsdhc_hc_arg arg;
	int rval;

	arg.host = host;
	arg.card = card;
	stat = vsdhc_card_in_slot_1(&arg, &rval, IPC_i_sdhc);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return rval;
}
EXPORT_SYMBOL(ipc_sdhc_card_in_slot);

/*
 * IPC: i_sdhc.vsdhc_write_protect().
 */
int ipc_sdhc_write_protect(int host, int card)
{
	enum clnt_stat stat;
	struct vsdhc_hc_arg arg;
	int rval;

	arg.host = host;
	arg.card = card;
	stat = vsdhc_write_protect_1(&arg, &rval, IPC_i_sdhc);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return rval;
}
EXPORT_SYMBOL(ipc_sdhc_write_protect);

/*
 * IPC: i_sdhc.vsdhc_has_iocard().
 */
int ipc_sdhc_has_iocard(int host, int card)
{
	enum clnt_stat stat;
	struct vsdhc_hc_arg arg;
	int rval;

	arg.host = host;
	arg.card = card;
	stat = vsdhc_has_iocard_1(&arg, &rval, IPC_i_sdhc);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return rval;
}
EXPORT_SYMBOL(ipc_sdhc_has_iocard);

/*
 * IPC: i_sdhc.vsdhc_req().
 */
int ipc_sdhc_req(int host, int card,
		 struct ipc_sdhc_req *req,
		 struct ipc_sdhc_reply *reply)
{
	enum clnt_stat stat;
	struct vsdhc_req_arg arg;
	struct vsdhc_req_res res;

	if (req == NULL || req->cmd == NULL)
		return -EINVAL;

	arg.hc.host = host;
	arg.hc.card = card;

	if (req->cmd) {
		arg.cmd.opcode = req->cmd->opcode;
		arg.cmd.arg = req->cmd->arg;
		arg.cmd.expect = req->cmd->expect;
		arg.cmd.retries = req->cmd->retries;
		arg.cmd.to = req->cmd->to;

		arg.has_cmd = 1;
	} else {
		arg.has_cmd = 0;
	}

	if (req->stop) {
		arg.stop.opcode = req->stop->opcode;
		arg.stop.arg = req->stop->arg;
		arg.stop.expect = req->stop->expect;
		arg.stop.retries = req->stop->retries;
		arg.stop.to = req->stop->to;

		arg.has_stop = 1;
	} else {
		arg.has_stop = 0;
	}
	if (req->data) {
		arg.data.buf = req->data->buf;
		arg.data.timeout_ns = req->data->timeout_ns;
		arg.data.timeout_clks = req->data->timeout_clks;
		arg.data.blksz = req->data->blksz;
		arg.data.blocks = req->data->blocks;
		arg.data.flags = req->data->flags;
		arg.has_data = 1;
	} else {
		arg.has_data = 0;
	}

	if (arg.has_data) {
		ambcache_clean_range ((void *) ipc_phys_to_virt((u32)arg.data.buf), arg.data.blksz * arg.data.blocks);
	}


	stat = vsdhc_req_1(&arg, &res, IPC_i_sdhc);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	if (arg.has_data) {
		if ((arg.data.flags & IPC_SDHC_DATA_WRITE) == 0) { // read
			ambcache_flush_range ((void *) ipc_phys_to_virt((u32)arg.data.buf), arg.data.blksz * arg.data.blocks);
		}
	}

	if (reply) {
		reply->cmd_error = res.cmd_error;
		reply->cmd_resp[0] = res.cmd_resp[0];
		reply->cmd_resp[1] = res.cmd_resp[1];
		reply->cmd_resp[2] = res.cmd_resp[2];
		reply->cmd_resp[3] = res.cmd_resp[3];

		reply->data_error = res.data_error;
		reply->data_bytes_xfered = res.data_bytes_xfered;

		reply->stop_error = res.stop_error;
		reply->stop_resp[0] = res.stop_resp[0];
		reply->stop_resp[1] = res.stop_resp[1];
		reply->stop_resp[2] = res.stop_resp[2];
		reply->stop_resp[3] = res.stop_resp[3];
	}

	return 0;
}
EXPORT_SYMBOL(ipc_sdhc_req);

/*
 * IPC: i_sdhc.vsdhc_abort().
 */
int ipc_sdhc_abort(int host, int card)
{
	enum clnt_stat stat;
	struct vsdhc_hc_arg arg;
	int rval;

	arg.host = host;
	arg.card = card;
	stat = vsdhc_abort_1(&arg, &rval, IPC_i_sdhc);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return rval;
}
EXPORT_SYMBOL(ipc_sdhc_abort);

/*
 * IPC: i_sdhc.vsdhc_set_ios().
 */
int ipc_sdhc_set_ios(int host, int card, struct ipc_sdhc_ios *ios)
{
	enum clnt_stat stat;
	struct vsdhc_ios_arg arg;
	int rval;

	if (ios == NULL)
		return -EINVAL;

	arg.hc.host = host;
	arg.hc.card = card;
	arg.desired_clock = ios->desired_clock;
	arg.vdd = ios->vdd;
	arg.bus_mode = ios->bus_mode;
	arg.power_mode = ios->power_mode;

	stat = vsdhc_set_ios_1(&arg, &rval, IPC_i_sdhc);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return rval;
}
EXPORT_SYMBOL(ipc_sdhc_set_ios);

/*
 * IPC: i_sdhc.vsdhc_get_bus_status().
 */
int ipc_sdhc_get_bus_status(int host, int card)
{
	enum clnt_stat stat;
	struct vsdhc_hc_arg arg;
	int rval;

	arg.host = host;
	arg.card = card;
	stat = vsdhc_get_bus_status_1(&arg, &rval, IPC_i_sdhc);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return rval;
}
EXPORT_SYMBOL(ipc_sdhc_get_bus_status);

/*
 * IPC: i_sdhc.vsdhc_enable_event().
 */
static void ipc_sdhc_enable_event(int enable)
{
	vsdhc_enable_event_1(&enable, NULL, IPC_i_sdhc);
}

struct sdhc_event_handler_list
{
	struct list_head list;
	ipc_sdhc_event_handler h;
	void *arg;
};

static LIST_HEAD(eh_head);

/*
 * Callback when interrupt of vsdhc received.
 */
void ipc_vsdhc_got_event(int host, int card, int event, int type)
{
	struct sdhc_event_handler_list *eh;
	struct list_head *pos, *q;

	list_for_each_safe(pos, q, &eh_head) {
		eh = list_entry(pos, struct sdhc_event_handler_list, list);
		eh->h(host, card, event, type, eh->arg);
	}
}

/*
 * Register event handler for vsdhc.
 */
void ipc_sdhc_event_register(ipc_sdhc_event_handler h, void *cbarg)
{
	struct sdhc_event_handler_list *eh;

	eh = kmalloc(sizeof(*h), GFP_KERNEL);
	if (eh == NULL) {
		pr_err("%s(): out of memory\n", __func__);
		return;
	}

	eh->h = h;
	eh->arg = cbarg;

	if (list_empty(&eh_head))
		ipc_sdhc_enable_event(1);
	list_add_tail(&eh->list, &eh_head);
}
EXPORT_SYMBOL(ipc_sdhc_event_register);

/*
 * Unregister event handler for vsdhc.
 */
void ipc_sdhc_event_unregister(ipc_sdhc_event_handler h)
{
	struct sdhc_event_handler_list *eh;
	struct list_head *pos, *q;

	list_for_each_safe(pos, q, &eh_head) {
		eh = list_entry(pos, struct sdhc_event_handler_list, list);
		if (eh->h == h) {
			list_del(pos);
			kfree(eh);
		}
	}

	if (list_empty(&eh_head))
		ipc_sdhc_enable_event(0);
}
EXPORT_SYMBOL(ipc_sdhc_event_unregister);

struct sdhc_sdio_irq_handler_list
{
	struct list_head list;
	ipc_sdhc_sdio_irq_handler h;
	void *arg;
};

static LIST_HEAD(qh_head);

/*
 * IPC: i_sdhc.vsdhc_enable_sdio_irq().
 */
void ipc_sdhc_enable_sdio_irq(int host, int enable)
{
	struct vsdhc_intr_arg arg;
	arg.host = host;
	arg.enable = enable;
	vsdhc_enable_sdio_irq_1(&arg, NULL, IPC_i_sdhc);
}
EXPORT_SYMBOL(ipc_sdhc_enable_sdio_irq);

/*
 * Callback when interrupt of vsdhc received.
 */
void ipc_vsdhc_got_sdio_interrupt(int host, int card, int sdio_irq, int type)
{
	struct sdhc_sdio_irq_handler_list *qh;
	struct list_head *pos, *q;

	list_for_each_safe(pos, q, &qh_head) {
		qh = list_entry(pos, struct sdhc_sdio_irq_handler_list, list);
		qh->h(host, qh->arg);
	}
}

/*
 * Register sdio_irq handler for vsdhc.
 */
void ipc_sdhc_sdio_irq_register(ipc_sdhc_sdio_irq_handler h, void *cbarg)
{
	struct sdhc_sdio_irq_handler_list *qh;

	qh = kmalloc(sizeof(*h), GFP_KERNEL);
	if (qh == NULL) {
		pr_err("%s(): out of memory\n", __func__);
		return;
	}

	qh->h = h;
	qh->arg = cbarg;

	list_add_tail(&qh->list, &qh_head);
}
EXPORT_SYMBOL(ipc_sdhc_sdio_irq_register);

/*
 * Unregister sdio_irq handler for vsdhc.
 */
void ipc_sdhc_sdio_irq_unregister(ipc_sdhc_sdio_irq_handler h)
{
	struct sdhc_sdio_irq_handler_list *qh;
	struct list_head *pos, *q;

	list_for_each_safe(pos, q, &qh_head) {
		qh = list_entry(pos, struct sdhc_sdio_irq_handler_list, list);
		if (qh->h == h) {
			list_del(pos);
			kfree(qh);
		}
	}
}
EXPORT_SYMBOL(ipc_sdhc_sdio_irq_unregister);

/*
 * Client initialization.
 */
static int __init i_sdhc_init(void)
{
	IPC_i_sdhc = ipc_clnt_prog_register(&i_sdhc_prog);
	if (IPC_i_sdhc == NULL)
		return -EPERM;

	return 0;
}

/*
 * Client exit.
 */
static void __exit i_sdhc_cleanup(void)
{
	if (IPC_i_sdhc) {
		ipc_sdhc_enable_event(0);
		ipc_clnt_prog_unregister(&i_sdhc_prog, IPC_i_sdhc);
		IPC_i_sdhc = NULL;
	}
}

subsys_initcall_sync(i_sdhc_init);
module_exit(i_sdhc_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Charles Chiou <cchiou@ambarella.com>");
MODULE_DESCRIPTION("Ambarella IPC: i_sdhc.x");
