/*
 * drivers/ambarella-ipc/server/lkvfs_bh.c
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

#define NUM_LKVFS_BH_WORKERS	1
#define MAX_LKVFS_BH_QUEUE	32

#define LKVFS_BH_MAX_EXEC_TIME	0xffffffff

struct lkvfs_bh_queue_s
{
	ipc_bh_f func;
	void *arg;
	void *result;
	SVCXPRT *svcxprt;
};

struct lkvfs_bh_s
{
	struct lkvfs_bh_queue_s queue[MAX_LKVFS_BH_QUEUE];
	unsigned int r_index;
	unsigned int w_index;
	unsigned int max_exec_time;

	wait_queue_head_t wait;
	spinlock_t lock;

	struct workers_s {
		struct task_struct *task;
		unsigned int serviced;
	} workers[NUM_LKVFS_BH_WORKERS];
};

/* The global instance */
static struct lkvfs_bh_s *lkvfs_bh = NULL;

/*
 * Worker thread function.
 */
static int lkvfs_bh_worker_thread(void *data)
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

		rval = wait_event_interruptible(lkvfs_bh->wait,
						(lkvfs_bh->r_index !=
						 lkvfs_bh->w_index));
		if (rval != 0)
			break;	/* Received a signal */

		spin_lock_irqsave(&lkvfs_bh->lock, flags);

		if (lkvfs_bh->r_index == lkvfs_bh->w_index) {
			/* nothing to do ?! */
			spin_unlock_irqrestore(&lkvfs_bh->lock, flags);
			continue;
		}

		index = lkvfs_bh->r_index;
		func = lkvfs_bh->queue[index].func;
		arg = lkvfs_bh->queue[index].arg;
		result = lkvfs_bh->queue[index].result;
		svcxprt = lkvfs_bh->queue[index].svcxprt;
		lkvfs_bh->workers[id].serviced++;

		lkvfs_bh->r_index++;
		lkvfs_bh->r_index %= MAX_LKVFS_BH_QUEUE;

		spin_unlock_irqrestore(&lkvfs_bh->lock, flags);

		BUG_ON(func == NULL);
		BUG_ON(svcxprt == NULL);

		/* Service the request! */
		exec_begin = jiffies;
		func(arg, result, svcxprt);
		exec_end = jiffies;
		if (exec_end - exec_begin > lkvfs_bh->max_exec_time) {
			printk("lkvfs_bh %d: slow vfs service => xid = %u, pid = %u, fid = %d, %d ms\n",
				id, svcxprt->xid, svcxprt->pid, svcxprt->fid,
				jiffies_to_msecs(exec_end - exec_begin));
		}
	}

	return 0;
}

/*
 * Queue up one request!
 */
void lkvfs_bh_queue(ipc_bh_f func, void *arg, void *result, SVCXPRT *svcxprt)
{
	unsigned long flags;
	int index;

	spin_lock_irqsave(&lkvfs_bh->lock, flags);

	index = lkvfs_bh->w_index;
	lkvfs_bh->queue[index].func = func;
	lkvfs_bh->queue[index].arg = arg;
	lkvfs_bh->queue[index].result = result;
	lkvfs_bh->queue[index].svcxprt = svcxprt;
	lkvfs_bh->w_index++;
	lkvfs_bh->w_index %= MAX_LKVFS_BH_QUEUE;
	if (lkvfs_bh->w_index == lkvfs_bh->r_index)
		BUG();	/* overflow */

	wake_up_nr(&lkvfs_bh->wait, 1);

	spin_unlock_irqrestore(&lkvfs_bh->lock, flags);
}
EXPORT_SYMBOL(lkvfs_bh_queue);

/*
 * Initialize the IPC bottom half.
 */
void lkvfs_bh_init(void)
{
	int i;

	lkvfs_bh = kzalloc(sizeof(*lkvfs_bh), GFP_KERNEL);
	BUG_ON(lkvfs_bh == NULL);

	init_waitqueue_head(&lkvfs_bh->wait);
	spin_lock_init(&lkvfs_bh->lock);
	lkvfs_bh->max_exec_time = msecs_to_jiffies(LKVFS_BH_MAX_EXEC_TIME);

	for (i = 0; i < NUM_LKVFS_BH_WORKERS; i++) {
		lkvfs_bh->workers[i].task = kthread_run(
			lkvfs_bh_worker_thread, (void *) i, "lkvfs_bh %d", i);
	}
}

/*
 * Cleanup the IPC bottom half.
 */
void lkvfs_bh_cleanup(void)
{
	int i;

	if (lkvfs_bh) {
		kfree(lkvfs_bh);
		for (i = 0; i < NUM_LKVFS_BH_WORKERS; i++)
			if (lkvfs_bh->workers[i].task)
				kthread_stop(lkvfs_bh->workers[i].task);
		lkvfs_bh = NULL;
	}
}
