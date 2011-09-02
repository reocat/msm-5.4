/*
 * drivers/ambarella-ipc/server/lk_alsa_fifo_server.c
 *
 * Authors:
 *	Eric Lee <cylee@ambarella.com>
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
 * Copyright (C) 2009-2011, Ambarella Inc.
 */

#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/aipc/aipc.h>
#if !defined(__DEPENDENCY_GENERATION__)
#include "ipcgen/lk_alsa_fifo.h"
#define __IPC_SERVER_IMPL__
#include "ipcgen/lk_alsa_fifo_tab.i"
#endif

static struct ipc_prog_s lk_alsa_fifo_prog =
{
	.name = "lk_alsa_fifo",
	.prog = LK_ALSA_FIFO_PROG,
	.vers = LK_ALSA_FIFO_VERS,
	.table =  (struct ipcgen_table *) &lk_alsa_fifo_prog_1_table,
	.nproc = lk_alsa_fifo_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};

/**
 * Struct for virtual DAI DMA manager.
 */
static struct vdaidma_s {
	struct vdma_chan_s {
		void (*handler1)(void *);
		void (*handler2)(void *);
		void *harg;
	} chan[2];
} G_vdaidma;

int vdma_request_callback(int chan, void (*handler1)(void *),
 void (*handler2)(void *), void *harg)
{
	int retval = 0;

	if (chan < 0 || chan >= 2) {
		retval = -EINVAL;
		goto ambarella_vdaidma_request_cb_exit_na;
	}

	if (handler1 == NULL || handler2 == NULL) {
		retval = -EINVAL;
		goto ambarella_vdaidma_request_cb_exit_na;
	}

	G_vdaidma.chan[chan].handler1 = handler1;
	G_vdaidma.chan[chan].handler2 = handler2;
	G_vdaidma.chan[chan].harg = harg;

ambarella_vdaidma_request_cb_exit_na:
	return retval;
}
EXPORT_SYMBOL(vdma_request_callback);

static void lk_update_alsa_vfifo_tx(void)
{
	void *harg = G_vdaidma.chan[0].harg;

	G_vdaidma.chan[0].handler1(harg);
}

static void lk_update_alsa_vfifo_rx(void)
{
	void *harg = G_vdaidma.chan[1].harg;

	G_vdaidma.chan[1].handler1(harg);
}

static void lk_alsa_op_finish_tx(void)
{
	void *harg = G_vdaidma.chan[0].harg;
	
	G_vdaidma.chan[0].handler2(harg);
}

static void lk_alsa_op_finish_rx(void)
{
	void *harg = G_vdaidma.chan[1].harg;

	G_vdaidma.chan[1].handler2(harg);
}

/* linux update alsa fifo read address */
static bool_t __lkalas_update_fifo_radr_1_svc(void *arg, void *res,
	SVCXPRT *svcxprt)
{
	lk_update_alsa_vfifo_tx();

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t lkalas_update_fifo_radr_1_svc(void *arg, void *res,
	struct svc_req *rqstp)
{
	ipc_bh_queue((ipc_bh_f) __lkalas_update_fifo_radr_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/* linux update alsa fifo write address */
static bool_t __lkalas_update_fifo_wadr_1_svc(void *arg, void *res,
  SVCXPRT *svcxprt)
{
	lk_update_alsa_vfifo_rx();

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);
	
	return 1;
}

bool_t lkalas_update_fifo_wadr_1_svc(void *arg, void *res,
	struct svc_req *rqstp)
{
	ipc_bh_queue((ipc_bh_f) __lkalas_update_fifo_wadr_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/* linux alsa tx op finish */
static bool_t __lkalas_tx_op_finish_1_svc(void *arg, void *res, SVCXPRT *svcxprt)
{
	lk_alsa_op_finish_tx();

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t lkalas_tx_op_finish_1_svc(void *arg, void *res, struct svc_req *rqstp)
{
	ipc_bh_queue((ipc_bh_f) __lkalas_tx_op_finish_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/* linux alsa rx op finish */
static bool_t __lkalas_rx_op_finish_1_svc(void *arg, void *res, SVCXPRT *svcxprt)
{
	lk_alsa_op_finish_rx();

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t lkalas_rx_op_finish_1_svc(void *arg, void *res, struct svc_req *rqstp)
{
	ipc_bh_queue((ipc_bh_f) __lkalas_rx_op_finish_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/* ------------------------------------------------------------------------- */

/*
 * Service initialization.
 */
static int __init lk_alsa_fifo_init(void)
{
	return ipc_svc_prog_register(&lk_alsa_fifo_prog);
}

/*
 * Service removal.
 */
static void __exit lk_alsa_fifo_cleanup(void)
{
	ipc_svc_prog_unregister(&lk_alsa_fifo_prog);
}

module_init(lk_alsa_fifo_init);
module_exit(lk_alsa_fifo_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eric Lee <cylee@ambarella.com>");
MODULE_DESCRIPTION("Ambarella IPC: lk_alsa_fifo.x");
