/*
 * drivers/ambarella-ipc/ipc_mutex.c
 *
 * Authors:
 *	Henry Lin <hllin@ambarella.com>
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <asm/cacheflush.h>

#include <linux/aipc/aipc.h>
#include <linux/aipc/irq.h>

#include <linux/aipc/ipc_mutex.h>
#include <linux/aipc/ipc_slock.h>
#include <linux/aipc/ipc_shm.h>
#include <linux/aipc/ipc_log.h>

#include <mach/boss.h>

/*
 * Definitions
 */
#define DEBUG_MUTEX_FLOW		0

#define IPC_MUTEX_OS_LOCAL		IPC_MUTEX_OS_LINUX
#define IPC_MUTEX_OS_REMOTE		IPC_MUTEX_OS_UITRON

#define IPC_MUTEX_GROUP_NUM		((IPC_MUTEX_ID_NUM + 31) >> 5)
#define IPC_MUTEX_GET_GROUP(id)		((id) >> 5)
#define IPC_MUTEX_GET_BIT(id)		(1 << ((id) & 0x1F))
#define IPC_MUTEX_GET_COMPL(id)		(&G_completions[id])
#define IPC_MUTEX_GET_TASK_ID(cid,pid)	((pid) + (cid << 30) + (IPC_MUTEX_OS_LOCAL << 31))
#define IPC_MUTEX_GET_LOCAL_TASK_ID(tid) ((tid) & 0x3fffffff)

#define K_ASSERT(x)			BUG_ON(!(x))

#if DEBUG_MUTEX_FLOW
#define DEBUG_MSG_MUTEX		pr_notice
#else
#define DEBUG_MSG_MUTEX(...)
#endif

/*
 * Structures
 */
typedef struct ipc_mutex_os_obj_s {
	int type;
	int irqno;
	int wakeup_lock;
	unsigned int wakeup_bitmap[IPC_MUTEX_GROUP_NUM];
} ipc_mutex_os_obj_t;

typedef struct ipc_mutex_obj_s {
	ipc_mutex_t *mutexs;
	int num;
	int next_mtx;

	int irqno_use_cortex_sw_int;

	ipc_mutex_os_obj_t os[IPC_MUTEX_OS_NUM];
} ipc_mutex_obj_t;

/*
 * Globals
 */
static ipc_mutex_obj_t *G_mutex = NULL;
static ipc_mutex_t *G_mutex_mutexs = NULL;
static struct completion G_completions[IPC_MUTEX_ID_NUM];

static char *G_mutex_owners[] = {"uITRON", "Linux", "None"};

/*
 * Forward Declaration
 */
static void ipc_mutex_irq_enable(int irqno, irqreturn_t (*handler)(int, void *));
static void ipc_mutex_irq_send(int irqno);
static void ipc_mutex_irq_clear(int irqno);

/*
 * Enable IRQ.
 */
static void ipc_mutex_irq_enable(int irqno, irqreturn_t (*handler)(int, void *))
{
	int rval;

	boss_set_irq_owner(irqno, BOSS_IRQ_OWNER_LINUX, 0);

	/* Sanitize the VIC line of the IRQ first */
	ipc_mutex_irq_clear(irqno);

	/* Register IRQ to system and enable it */
#if (CHIP_REV == I1)
	if (G_mutex->irqno_use_cortex_sw_int) {
		irqno += AXI_SOFT_IRQ(0);
	}
#endif
	rval = request_irq(irqno,
			   handler,
			   IRQF_TRIGGER_HIGH | IRQF_SHARED,
			   "IPC_Mutex",
			   G_mutex);
	K_ASSERT(rval == 0);
}

/*
 * Clear IRQ.
 */
static void ipc_mutex_irq_clear(int irqno)
{
	if (G_mutex->irqno_use_cortex_sw_int) {
#if (CHIP_REV == I1)
		amba_writel(AHB_SCRATCHPAD_REG(0x14), 1 << irqno);
#endif
	}
	else
	{
		if (irqno < 32) {
			amba_writel(VIC_SOFTEN_CLR_REG, 1 << irqno);
		}
#if (VIC_INSTANCES >= 2)
		else if (irqno < 64) {
			amba_writel(VIC2_SOFTEN_CLR_REG, 1 << (irqno - 32));
		}
#endif
#if (VIC_INSTANCES >= 3)
		else if (irqno < 96) {
			amba_writel(VIC3_SOFTEN_CLR_REG, 1 << (irqno - 64));
		}
#endif
		else {
			K_ASSERT(0);
		}
	}
}

/*
 * Issue IRQ.
 */
static void ipc_mutex_irq_send(int irqno)
{
	DEBUG_MSG_MUTEX ("ipc: mutex: issue wakeup irq\n");

	if (irqno < 32) {
		amba_writel(VIC_SOFTEN_REG, 1 << irqno);
	}
#if (VIC_INSTANCES >= 2)
	else if (irqno < 64) {
		amba_writel(VIC2_SOFTEN_REG, 1 << (irqno - 32));
	}
#endif
#if (VIC_INSTANCES >= 3)
	else if (irqno < 96) {
		amba_writel(VIC3_SOFTEN_REG, 1 << (irqno - 64));
	}
#endif
	else {
		K_ASSERT(0);
	}
}

/*
 * Wake up a waiting task in remote OS
 *
 * Set a bit that represents requested mutex and issue an interrupt
 */
static void
ipc_mutex_wakeup_remote(ipc_mutex_t *mutex)
{
	ipc_mutex_os_obj_t *os = &G_mutex->os[IPC_MUTEX_OS_REMOTE];
	int mtxid = mutex->id;

	ipc_spin_lock(os->wakeup_lock, IPC_SLOCK_POS_WAKEUP_ADD);

	os->wakeup_bitmap[IPC_MUTEX_GET_GROUP(mtxid)] |= IPC_MUTEX_GET_BIT(mtxid);
	ipc_mutex_irq_send(os->irqno);

	ipc_spin_unlock(os->wakeup_lock, IPC_SLOCK_POS_WAKEUP_ADD);
}

/*
 * Look up the mutex to be awaken
 */
static ipc_mutex_t *
ipc_mutex_wakeup_lookup(ipc_mutex_os_obj_t *os)
{
	int i, mtxid = -1;

	ipc_spin_lock(os->wakeup_lock, IPC_SLOCK_POS_WAKEUP_REMOVE);

	for (i = 0; i < IPC_MUTEX_GROUP_NUM; i++) {
		unsigned int bits;

		bits = os->wakeup_bitmap[i];
		if (bits) {
			mtxid = ipc_ctz(bits);
			os->wakeup_bitmap[i] &= ~(1 << mtxid);
			mtxid += (i << 5);
			break;
		}
	}

	ipc_spin_unlock(os->wakeup_lock, IPC_SLOCK_POS_WAKEUP_REMOVE);

	return 	(mtxid >= 0)?&G_mutex_mutexs[mtxid]:NULL;
}

/*
 * Wake up a waiting task in local OS
 */
static void
ipc_mutex_wakeup_local(ipc_mutex_t *mutex)
{
	ipc_mutex_os_t *os = &mutex->os[IPC_MUTEX_OS_LOCAL];

	DEBUG_MSG_MUTEX ("ipc: mutex %d: help to wakeup a local waiting task",
			mutex->id);

	ipc_spin_lock(mutex->lock, IPC_SLOCK_POS_WAKEUP_LOCAL);
	K_ASSERT(mutex->token == IPC_MUTEX_OS_LOCAL);
	K_ASSERT(os->count > 0);
	os->count--;
	if (os->count == 0) {
		os->priority = 0;
	}
	ipc_spin_unlock(mutex->lock, IPC_SLOCK_POS_WAKEUP_LOCAL);

	complete(IPC_MUTEX_GET_COMPL(mutex->id));
}

/*
 * IRQ handler for interrupt sent from the remote OS.
 */
static irqreturn_t ipc_mutex_irq_handler(int irqno, void *args)
{
	ipc_mutex_t *mutex;
	ipc_mutex_os_obj_t *os = &G_mutex->os[IPC_MUTEX_OS_LOCAL];

	DEBUG_MSG_MUTEX ("ipc: mutex isr");

	ipc_mutex_irq_clear(os->irqno);

	mutex = ipc_mutex_wakeup_lookup(os);
	if (mutex) {
		ipc_mutex_wakeup_local(mutex);
	} else {
		K_ASSERT(0);
	}

	return IRQ_HANDLED;
}

/*
 * Initialize OS specific part in mutex subsystem
 */
static int ipc_mutex_os_init(ipc_mutex_os_obj_t *os, int type)
{
	if (type == IPC_MUTEX_OS_LOCAL) {
		ipc_mutex_irq_enable(os->irqno, ipc_mutex_irq_handler);
	}

	return 0;
}

/*
 * Initialize mutex memory map
 */
int ipc_mutex_init_map(u32 addr, u32 size)
{
	/* Initialize global mutex object */
	G_mutex = (ipc_mutex_obj_t *) addr;
	G_mutex_mutexs = (ipc_mutex_t *) ipc_phys_to_virt(G_mutex->mutexs);

	pr_notice ("ipc: mutex = %08x, %08x, %d\n",
		(unsigned int) G_mutex, (unsigned int) G_mutex_mutexs, G_mutex->num);

	return 0;
}
EXPORT_SYMBOL(ipc_mutex_init_map);

/*
 * Initialize mutex subsystem
 */
int ipc_mutex_init(void)
{
	u32 i;

	/* Initialize OS specific part */
	for (i = 0; i < IPC_MUTEX_OS_NUM; i++) {
		ipc_mutex_os_init(&G_mutex->os[i], i);
	}

	for (i = 0; i < IPC_MUTEX_ID_NUM; i++) {
		init_completion(IPC_MUTEX_GET_COMPL(i));
	}

	pr_notice ("ipc: mutex = %d, %d\n",
		G_mutex->os[IPC_MUTEX_OS_UITRON].wakeup_lock,
		G_mutex->os[IPC_MUTEX_OS_LINUX].wakeup_lock);

	return 0;
}
EXPORT_SYMBOL(ipc_mutex_init);

/*
 * Lock a mutex.
 *
 * If the mutex has already been locked, put current task to wait queue.
 */
int ipc_mutex_lock(int mtxid)
{
	int rval = IPC_MUTEX_ERR_OK;
	ipc_mutex_t *mutex;
	int locked;
	int cpu_id = smp_processor_id();
	int pid = current->pid;
#if DEBUG_LOG_MUTEX
	unsigned int tskid = IPC_MUTEX_GET_TASK_ID(cpu_id, pid);
#endif
	int next_id, prev_token;
#if DEBUG_LOG_LOCK_TIME
	unsigned long wtime;
	ipc_mutex_lock_time_t *time;
#endif

	mutex = &G_mutex_mutexs[mtxid];
#if DEBUG_LOG_LOCK_TIME
	wtime = jiffies;
#endif

	DEBUG_MSG_MUTEX ("ipc: mutex %d: %d@%d lock", mtxid, pid, cpu_id);

	ipc_log_print(IPC_LOG_LEVEL_DEBUG, "lmtx: lock %d %d %d", mtxid, pid, cpu_id);

	if (in_interrupt()) {
		printk("ipc: ipc_mutex_lock(%d) cannot be called in interrupt context!!\n", mtxid);
		K_ASSERT(0);
	}

	ipc_spin_lock(mutex->lock, IPC_SLOCK_POS_LOCK);

	next_id = mutex->next_id;
	prev_token = mutex->token;

	locked = (mutex->token != IPC_MUTEX_TOKEN_NONE);
	if (locked) {
		ipc_mutex_os_t *os = &mutex->os[IPC_MUTEX_OS_LOCAL];

		os->count++;
		if (os->priority == 0) {
			os->priority = mutex->next_id;
		}
	} else {
		mutex->token = IPC_MUTEX_OS_LOCAL;
	}
	mutex->next_id++;

	ipc_spin_unlock(mutex->lock, IPC_SLOCK_POS_LOCK);

	if (locked) {
		int err;

		err = wait_for_completion_timeout(&G_completions[mtxid],
					msecs_to_jiffies(mutex->timeout));
		if (err == 0) {
			rval = IPC_MUTEX_ERR_TIMEOUT;
			K_ASSERT(0);
		}
	}

	K_ASSERT(mutex->token == IPC_MUTEX_OS_LOCAL);

	ipc_spin_lock(mutex->lock, IPC_SLOCK_POS_LOCK_COUNT);

	K_ASSERT(mutex->lock_count == mutex->unlock_count);
	K_ASSERT(mutex->arm_lock_count == mutex->arm_unlock_count);
	K_ASSERT(mutex->cortex_lock_count == mutex->cortex_unlock_count);

#if DEBUG_LOG_LOCK_TIME
	mutex->lock_time = jiffies;
	wtime = jiffies - wtime;

	mutex->linux_lock_wait_time += wtime;
	if (wtime > mutex->max_lock_wait_time) {
		mutex->max_lock_wait_time = wtime;
		mutex->max_lock_wait_os = IPC_MUTEX_OS_LINUX;
		mutex->max_lock_wait_id = mutex->lock_count;
	}

	time = &mutex->lock_times[mutex->lock_time_idx];
	time->lock_id = mutex->lock_count;
	time->lock_os = IPC_MUTEX_OS_LINUX;
	time->lock_wait = wtime;
#endif
	mutex->lock_count++;
	mutex->cortex_lock_count++;
#if DEBUG_LOG_MUTEX
	mutex->lock_task_id[mutex->lock_log_idx] = tskid;
	mutex->lock_prev_token[mutex->lock_log_idx] = prev_token;
	mutex->lock_next_id[mutex->lock_log_idx] = next_id;
	if (++mutex->lock_log_idx == DEBUG_LOG_MUTEX_NUM) {
		mutex->lock_log_idx = 0;
	}
#endif
	ipc_spin_unlock(mutex->lock, IPC_SLOCK_POS_LOCK_COUNT);

	DEBUG_MSG_MUTEX ("ipc: mutex %d: %d@%d lock_OK", mtxid, pid, cpu_id);

	ipc_log_print(IPC_LOG_LEVEL_DEBUG, "lmtx: lock_ok %d %d %d", mtxid, pid, cpu_id);

	return rval;
}
EXPORT_SYMBOL(ipc_mutex_lock);

/*
 * Unlock a mutex
 *
 * Unlock the mutex and choose one of its waiting tasks to wake up.
 */
int ipc_mutex_unlock(int mtxid)
{
	ipc_mutex_t *mutex;
	int token = IPC_MUTEX_TOKEN_NONE;
	ipc_mutex_os_t *local, *remote;
	int cpu_id = smp_processor_id();
	int pid = current->pid;
#if DEBUG_LOG_MUTEX
	unsigned int tskid = IPC_MUTEX_GET_TASK_ID(cpu_id, pid);
#endif
#if DEBUG_LOG_LOCK_TIME
	ipc_mutex_lock_time_t *time;
#endif

	mutex = &G_mutex_mutexs[mtxid];
	local = &mutex->os[IPC_MUTEX_OS_LOCAL];
	remote = &mutex->os[IPC_MUTEX_OS_REMOTE];

	ipc_log_print(IPC_LOG_LEVEL_DEBUG, "lmtx: unlock %d %d %d", mtxid, pid, cpu_id);

	DEBUG_MSG_MUTEX ("ipc: mutex %d: %d@%d unlock", mtxid, pid, cpu_id);

#if DEBUG_LOG_LOCK_TIME
	time = &mutex->lock_times[mutex->lock_time_idx];
	if (jiffies < mutex->lock_time) {
		time->lock_time = 0xffffffff - mutex->lock_time + jiffies;
	} else {
		time->lock_time = jiffies - mutex->lock_time;
	}

	if (++mutex->lock_time_idx == DEBUG_LOG_LOCK_TIME_NUM) {
		mutex->lock_time_idx = 0;
	}

	if (time->lock_time > mutex->max_lock_time) {
		mutex->max_lock_time = time->lock_time;
		mutex->max_lock_id = time->lock_id;
	}
#endif

	ipc_spin_lock(mutex->lock, IPC_SLOCK_POS_UNLOCK);

	if (local->count + remote->count > 0)
	{
		/* Choose the os to be woken up */
		if (local->count == 0) {
			token = IPC_MUTEX_OS_REMOTE;
		}
		else if (remote->count == 0) {
			token = IPC_MUTEX_OS_LOCAL;
		}
		else {
			if (mutex->policy == IPC_MUTEX_POLICY_FIFO) {
				if (local->priority <= remote->priority) {
					token = IPC_MUTEX_OS_LOCAL;
				} else {
					token = IPC_MUTEX_OS_REMOTE;
				}
			}
			else if (mutex->policy == IPC_MUTEX_POLICY_ROUND_ROBIN) {
				token = IPC_MUTEX_OS_REMOTE;
			}
			else if (mutex->policy == IPC_MUTEX_POLICY_WAIT_COUNT) {
				if (local->count >= remote->count) {
					token = IPC_MUTEX_OS_LOCAL;
				} else {
					token = IPC_MUTEX_OS_REMOTE;
				}
			}
			else {
				K_ASSERT(0);
			}
		}

		/* If local OS's turn, prepare for local task's wakeup */
		if (token == IPC_MUTEX_OS_LOCAL) {
			ipc_mutex_os_t *os = &mutex->os[IPC_MUTEX_OS_LOCAL];

			os->count--;
			if (os->count == 0) {
				os->priority = 0;
			}
		}
	}

#if DEBUG_LOG_MUTEX
	K_ASSERT(IPC_MUTEX_GET_LOCAL_TASK_ID(mutex->lock_task_id[mutex->unlock_log_idx]) ==
		IPC_MUTEX_GET_LOCAL_TASK_ID(tskid));
	mutex->unlock_task_id[mutex->unlock_log_idx] = tskid;
	if (++mutex->unlock_log_idx == DEBUG_LOG_MUTEX_NUM) {
		mutex->unlock_log_idx = 0;
	}
#endif
	mutex->unlock_count++;
	mutex->cortex_unlock_count++;
	mutex->token = token;

	ipc_spin_unlock(mutex->lock, IPC_SLOCK_POS_UNLOCK);

	DEBUG_MSG_MUTEX ("ipc: mutex %d: %d@%d unlock_OK => next: %s (l: %d, r: %d)",
			mtxid, pid, cpu_id, G_mutex_owners[token],
			local->count, remote->count);

	ipc_log_print(IPC_LOG_LEVEL_DEBUG, "lmtx: unlock_ok %d %d %d, next: %s (l: %d, r: %d)",
			mtxid, pid, cpu_id, G_mutex_owners[token], local->count, remote->count);

	if (token == IPC_MUTEX_OS_LOCAL) {
		complete(IPC_MUTEX_GET_COMPL(mtxid));
	}
	if (token == IPC_MUTEX_OS_REMOTE) {
		ipc_mutex_wakeup_remote(mutex);
	}

	return IPC_MUTEX_ERR_OK;
}
EXPORT_SYMBOL(ipc_mutex_unlock);

