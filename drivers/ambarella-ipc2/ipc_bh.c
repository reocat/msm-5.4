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
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/aipc/aipc.h>

#define NUM_IPC_BH_WORKERS	2
#define MAX_IPC_BH_QUEUE	64

#define IPC_BH_MAX_EXEC_TIME	500

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
	unsigned int max_exec_time;
	unsigned int count;

	wait_queue_head_t wait;
	spinlock_t lock;

	struct workers_s {
		struct task_struct *task;
		unsigned int serviced;
		unsigned int count;
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
	unsigned long flags;

	for (;;) {
		unsigned long exec_begin, exec_end;

		rval = wait_event_interruptible(ipc_bh->wait,
						(ipc_bh->r_index !=
						 ipc_bh->w_index));
		if (rval != 0)
			break;	/* Received a signal */

		spin_lock_irqsave(&ipc_bh->lock, flags);

		if (ipc_bh->r_index == ipc_bh->w_index) {
			/* nothing to do ?! */
			spin_unlock_irqrestore(&ipc_bh->lock, flags);
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
			spin_unlock_irqrestore(&ipc_bh->lock, flags);
			continue;
		}
#endif

		ipc_bh->r_index++;
		ipc_bh->r_index %= MAX_IPC_BH_QUEUE;

		spin_unlock_irqrestore(&ipc_bh->lock, flags);

		BUG_ON(func == NULL);
		BUG_ON(svcxprt == NULL);

		/* Service the request! */
		exec_begin = jiffies;
		func(arg, result, svcxprt);
		exec_end = jiffies;
		if (exec_end - exec_begin > ipc_bh->max_exec_time) {
			printk("ipc_bh %d: slow ipc service => xid = %u, pid = %08x, fid = %d, %d ms\n",
				id, svcxprt->xid, svcxprt->pid, svcxprt->fid,
				jiffies_to_msecs(exec_end - exec_begin));
		}
		ipc_bh->workers[id].count++;
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

	spin_lock_irqsave(&ipc_bh->lock, flags);

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

	spin_unlock_irqrestore(&ipc_bh->lock, flags);
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
	spin_lock_init(&ipc_bh->lock);
	ipc_bh->max_exec_time = msecs_to_jiffies(IPC_BH_MAX_EXEC_TIME);

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
