/*
 * drivers/ambarella-ipc/ipc_bh.h
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

#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/aipc/aipc.h>

#define NUM_IPC_BH_WORKERS	2
#define MAX_IPC_BH_QUEUE	64

struct ipc_bh_queue_s
{
	ipc_bh_f func;
	void *arg;
	void *result;
	SVCXPRT *svcxprt;
};

struct ipc_bh_s
{
	struct ipc_bh_queue_s queue[MAX_IPC_BH_QUEUE];
	unsigned int r_index;
	unsigned int w_index;

	wait_queue_head_t wait;
	struct semaphore sem;

	struct workers_s {
		struct task_struct *task;
		unsigned int serviced;
	} workers[NUM_IPC_BH_WORKERS];
};

/* The global instance */
static struct ipc_bh_s *ipc_bh = NULL;

/*
 * Worker thread function.
 */
static int ipc_bh_worker_thread(void *data)
{
	int id = (int) data;
	int rval;
	int index;
	ipc_bh_f func;
	void *arg;
	void *result;
	SVCXPRT *svcxprt;

	for (;;) {
		rval = wait_event_interruptible(ipc_bh->wait,
						(ipc_bh->r_index !=
						 ipc_bh->w_index));
		if (rval != 0)
			break;	/* Received a signal */

		down(&ipc_bh->sem);

		if (ipc_bh->r_index == ipc_bh->w_index) {
			/* nothing to do ?! */
			up(&ipc_bh->sem);
			continue;
		}

		index = ipc_bh->r_index;
		func = ipc_bh->queue[index].func;
		arg = ipc_bh->queue[index].arg;
		result = ipc_bh->queue[index].result;
		svcxprt = ipc_bh->queue[index].svcxprt;
		ipc_bh->workers[id].serviced++;

#ifdef STATIC_SVC
		if(ipc_cancel_check(svcxprt)) {
			up(&ipc_bh->sem);
			continue;
		}
#endif

		ipc_bh->r_index++;
		ipc_bh->r_index %= MAX_IPC_BH_QUEUE;

		up(&ipc_bh->sem);

		BUG_ON(func == NULL);
		BUG_ON(svcxprt == NULL);

		/* Service the request! */
		func(arg, result, svcxprt);
	}

	return 0;
}

/*
 * Queue up one request!
 */
void ipc_bh_queue(ipc_bh_f func, void *arg, void *result, SVCXPRT *svcxprt)
{
	unsigned long flags;
	int index;

	local_irq_save(flags);

	index = ipc_bh->w_index;
	ipc_bh->queue[index].func = func;
	ipc_bh->queue[index].arg = arg;
	ipc_bh->queue[index].result = result;
	ipc_bh->queue[index].svcxprt = svcxprt;
	ipc_bh->w_index++;
	ipc_bh->w_index %= MAX_IPC_BH_QUEUE;
	if (ipc_bh->w_index == ipc_bh->r_index)
		BUG();	/* overflow */

	wake_up_nr(&ipc_bh->wait, 1);

	local_irq_restore(flags);
}
EXPORT_SYMBOL(ipc_bh_queue);

/*
 * Initialize the IPC bottom half.
 */
void ipc_bh_init(void)
{
	int i;

	ipc_bh = kzalloc(sizeof(*ipc_bh), GFP_KERNEL);
	BUG_ON(ipc_bh == NULL);

	init_waitqueue_head(&ipc_bh->wait);
	sema_init(&ipc_bh->sem, 1);

	for (i = 0; i < NUM_IPC_BH_WORKERS; i++) {
		ipc_bh->workers[i].task = kthread_run(
			ipc_bh_worker_thread, (void *) i, "ipc_bh %d", i);
	}
}

/*
 * Cleanup the IPC bottom half.
 */
void ipc_bh_cleanup(void)
{
	int i;

	if (ipc_bh) {
		kfree(ipc_bh);
		for (i = 0; i < NUM_IPC_BH_WORKERS; i++)
			if (ipc_bh->workers[i].task)
				kthread_stop(ipc_bh->workers[i].task);
		ipc_bh = NULL;
	}
}
