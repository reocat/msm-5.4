/*
 * drivers/ambarella-ipc/ipc_slock.c
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

#include <ambhw/chip.h>
#include <linux/sched.h>
#include <linux/aipc/aipc.h>
#include <linux/aipc/ipc_slock.h>
#include <linux/aipc/ipc_shm.h>

/*
 * Debug
 */

/*
 * Definitions
 */
#define IPC_SLOCK_OS_UITRON		0
#define IPC_SLOCK_OS_LINUX		1
#define IPC_SLOCK_OS_LOCAL		IPC_SLOCK_OS_LINUX
#define IPC_SLOCK_OS_REMOTE		IPC_SLOCK_OS_UITRON

#define IPC_SLOCK_GET_TASK_ID(cid,pid)	((pid) + (cid << 30) + (IPC_SLOCK_OS_LOCAL << 31))
#define IPC_SLOCK_GET_LOCAL_TASK_ID(tid) ((tid) & 0x3fffffff)

#define IPC_SLOCK_MAX_NUM		64

/*
 * Global structure
 */
typedef struct ipc_slock_obj_s {
	ipc_slock_t	*locks;
	int		num;
	spinlock_t	spinlocks[IPC_SLOCK_MAX_NUM];
} ipc_slock_obj_t;

static ipc_slock_obj_t G_lock_obj;


/*
 * Forward Declarations
 */
#define ipc_readl(a)	(*(volatile unsigned int *)(a))

/*
 * Initialize spinlock system.
 */
void ipc_slock_init(unsigned int addr, unsigned int size)
{
	int i;

	memset ((void *) addr, 0, size);

	G_lock_obj.locks = (ipc_slock_t *) addr;
	G_lock_obj.num = size / sizeof (ipc_slock_t);

	for (i = 0; i < IPC_SLOCK_MAX_NUM; i++) {
		spin_lock_init(&G_lock_obj.spinlocks[i]);
	}
}
EXPORT_SYMBOL(ipc_slock_init);

/*
 * Lock a spinlock.
 */
void ipc_spin_lock(int i, int pos)
{
	ipc_slock_t *lock = &G_lock_obj.locks[i];
	unsigned long flags;
	unsigned long tmp;
#if DEBUG_LOCK_TIME
	unsigned int time;
#endif
#if DEBUG_SPINLOCK
	int cpu_id, pid;

	K_ASSERT(i < IPC_SLOCK_MAX_NUM);

	cpu_id = smp_processor_id ();
	pid = current->pid;
#endif
#if DEBUG_LOCK_TIME
	time = jiffies;
#endif
	spin_lock_irqsave(&G_lock_obj.spinlocks[i], flags);

#if (CHIP_REV == I1)
	__asm__ __volatile__(
"1:	ldrex	%0, [%1]\n"
"	teq	%0, #0\n"
"	strexeq	%0, %2, [%1]\n"
"	teqeq	%0, #0\n"
"	bne	1b"
	: "=&r" (tmp)
	: "r" (&lock->lock), "r" (1)
	: "cc");
#endif /* CHIP_REV == I1 */

#if DEBUG_LOCK_TIME
	lock->cortex_lock_time = jiffies;
	if (lock->cortex_lock_time - time > DEBUG_LOCK_MAX_MS) {
		lock->cortex_long_wait++;
	}
#endif
	lock->flags = flags;
	lock->count++;
	lock->cortex++;

#if DEBUG_SPINLOCK
	K_ASSERT(lock->lock_count == lock->unlock_count);
	lock->cpu = IPC_SPINLOCK_CPU_CORTEX;
	lock->lock_count++;
#if DEBUG_LOG_SPINLOCK
	lock->lock_task_id[lock->lock_log_idx] = IPC_SLOCK_GET_TASK_ID(cpu_id, pid);
	lock->lock_pos[lock->lock_log_idx] = pos;
	if (++lock->lock_log_idx == DEBUG_LOG_SPINLOCK_NUM) {
		lock->lock_log_idx = 0;
	}
#endif /* DEBUG_LOG_SPINLOCK */
#endif /* DEBUG_SPINLOCK */

	smp_mb();
}
EXPORT_SYMBOL(ipc_spin_lock);

/*
 * Unlock a spinlock.
 */
void ipc_spin_unlock(int i, int pos)
{
	ipc_slock_t *lock = &G_lock_obj.locks[i];
	unsigned int time;

	K_ASSERT(i < IPC_SLOCK_MAX_NUM);

	smp_mb();

#if DEBUG_SPINLOCK
	K_ASSERT(lock->lock_count == (lock->unlock_count + 1));
	K_ASSERT(lock->cpu == IPC_SPINLOCK_CPU_CORTEX);
#if DEBUG_LOG_SPINLOCK
	K_ASSERT(lock->lock_pos[lock->unlock_log_idx] == pos);
	lock->unlock_pos[lock->unlock_log_idx] = pos;
	if (++lock->unlock_log_idx == DEBUG_LOG_SPINLOCK_NUM) {
		lock->unlock_log_idx = 0;
	}
#endif /* DEBUG_LOG_SPINLOCK */
	lock->unlock_count++;
#endif /* DEBUG_SPINLOCK */

#if DEBUG_LOCK_TIME
	time = jiffies;
	if (time - lock->cortex_lock_time > DEBUG_LOCK_MAX_MS) {
		lock->cortex_long_lock++;
	}
#endif

#if (CHIP_REV == I1)
	__asm__ __volatile__(
"	str	%1, [%0]\n"
	:
	: "r" (&lock->lock), "r" (0)
	: "cc");
#endif /* CHIP_REV == I1 */

	spin_unlock_irqrestore(&G_lock_obj.spinlocks[i], lock->flags);
}
EXPORT_SYMBOL(ipc_spin_unlock);

/*
 * Try to lock a spinlock
 */
int ipc_spin_trylock(int i)
{
	ipc_slock_t *lock = &G_lock_obj.locks[i];
	unsigned long tmp;

	__asm__ __volatile__(
"	ldrex	%0, [%1]\n"
"	teq	%0, #0\n"
"	strexeq	%0, %2, [%1]"
	: "=&r" (tmp)
	: "r" (&lock->lock), "r" (1)
	: "cc");

	if (tmp == 0) {
		smp_mb();
		return 1;
	} else {
		return 0;
	}
}
EXPORT_SYMBOL(ipc_spin_trylock);

/*
 * Test a spinlock
 */
void ipc_spin_test(void)
{
	int cpu_id;
	volatile unsigned int *global_count, *global_stop;
	volatile unsigned int *cortex_count;

	cpu_id = smp_processor_id();
	pr_notice("[ipc] ipc_spin_test: %d\n", cpu_id);

	global_count = ipc_shm_get(IPC_SHM_AREA_GLOBAL, IPC_SHM_GLOBAL_COUNT);
	global_stop = ipc_shm_get(IPC_SHM_AREA_GLOBAL, IPC_SHM_GLOBAL_STOP);
	cortex_count = ipc_shm_get(IPC_SHM_AREA_CORTEX0 + cpu_id, IPC_SHM_CORTEX_COUNT); 

	do {
		ipc_spin_lock (0, IPC_SLOCK_POS_TEST);
		ipc_inc (global_count);
		ipc_inc (cortex_count);
		ipc_spin_unlock (0, IPC_SLOCK_POS_TEST);
	}
	while (ipc_readl (global_stop) == 0);

}
EXPORT_SYMBOL(ipc_spin_test);

