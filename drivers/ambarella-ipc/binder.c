/*
 * drivers/ambarella-ipc/binder.c
 *
 * Ambarella IPC in Linux kernel-space.
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
#include <linux/freezer.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/sched.h>

#include <linux/aipc/aipc.h>
#include <linux/aipc/irq.h>
#include <linux/aipc/ipc_slock.h>
#include <linux/aipc/ipc_log.h>
#include <mach/boss.h>

#include "binder.h"

#define CONFIG_IPC_SIGNAL_LU_COMPL	0
#define CONFIG_IPC_INFINITE_WAIT	0

#define DEBUG_IPC_DATA			0
#define DEBUG_IPC_ALIGN_DATA		0
#define DEBUG_IPC_LK			0
#define DEBUG_IPC_LU			0
#define DEBUG_IPC_CHECK_DUMP		0
#define DEBUG_IPC_SLOW			0

#define CACHE_LINE_SIZE		32
#define CACHE_LINE_MASK		~(CACHE_LINE_SIZE - 1)

#define LOCK_POS_L_CLNT_OUT		48
#define LOCK_POS_L_CLNT_IN		49
#define LOCK_POS_L_SVC_IN		50
#define LOCK_POS_L_SVC_OUT		51
#define LOCK_POS_LU_CLNT_OUT		52
#define LOCK_POS_L_CLNT_CANCEL		53

int g_ipc_dump = 0;

#if DEBUG_IPC_LK
#define DEBUG_MSG_IPC_LK printk
#else
#define DEBUG_MSG_IPC_LK(...)
#endif

#if DEBUG_IPC_LU
#define DEBUG_MSG_IPC_LU printk
#else
#define DEBUG_MSG_IPC_LU(...)
#endif

#if DEBUG_IPC_SLOW
#define DEBUG_MSG_IPC_SLOW printk
#else
#define DEBUG_MSG_IPC_SLOW(...)
#endif

#if DEBUG_IPC_CHECK_DUMP
#define DEBUG_CHECK_DUMP \
do { \
	if (g_ipc_dump == 0) { \
		return; \
	} \
} while (0) 
#else
#define DEBUG_CHECK_DUMP
#endif

#if DEBUG_IPC_ALIGN_DATA
#define IPC_CACHE_ALIGN_DATA(data,size) \
do  { \
	u32	addr_start, addr_end; \
\
	addr_start = (u32) data & CACHE_LINE_MASK; \
	addr_end = ((u32) data + size + CACHE_LINE_SIZE - 1) & CACHE_LINE_MASK; \
	data = (unsigned char *) addr_start; \
	size = addr_end - addr_start; \
} while (0)

#else
#define IPC_CACHE_ALIGN_DATA(data,size)
#endif

#define BINDER_GET_SVCXPRT(i)   (binder->svcxprts[i])

#if DEBUG_IPC_DATA

static void ipc_dump_data (unsigned char *data, int size)
{
	DEBUG_CHECK_DUMP;

	if (data == NULL) {
		printk (KERN_NOTICE "[ipc] %08x %d\n", (unsigned int) data, size);
		return;
	}

	IPC_CACHE_ALIGN_DATA (data, size);

	while (size > 0) {
		int ssize = (size > 16)?16:size;

		printk ("[ipc] %08x: %02x %02x %02x %02x %02x %02x %02x %02x "
			"%02x %02x %02x %02x %02x %02x %02x %02x\n",
			(unsigned int) data,
			data[0], data[1], data[2], data[3],
			data[4], data[5], data[6], data[7],
			data[8], data[9], data[10], data[11],
			data[12], data[13], data[14], data[15]);
		data += ssize;
		size -= ssize;
	}
}

#else

#define ipc_dump_data(p1,p2) do {} while (0)

#endif

/* Global instance of binder */
static struct ipc_binder_s G_ipc_binder = {
	.init = 0,
};

struct ipc_binder_s *binder = &G_ipc_binder;
static struct ipc_buf_s *ipc_buf = NULL;

#define IPC_POLL_IRQ_INTERVAL (HZ/50)
static void ipc_poll_irq(unsigned long dummy);
static DEFINE_TIMER(ipc_poll_irq_timer, ipc_poll_irq, 0, 0);

void ipc_clnt_call_complete(SVCXPRT *svcxprt, enum clnt_stat rval);
inline static struct ipc_prog_s *ipc_lookup_prog(struct ipc_prog_s *head, unsigned int prog_id);

/*
 * Diff time stamps and the result.
 */
static inline u32 ipc_tick_diff(u32 start, u32 end)
{
	if (end < start)
		return 0xffffffff - start + end;
	else
		return end - start;
}

/*
 * Get time tick.
 */
u32 ipc_tick_get(void)
{
	return amba_readl(TIMER8_STATUS_REG);
}
EXPORT_SYMBOL(ipc_tick_get);

static u32 ipc_virt_to_phys (u32 virt)
{
	if (virt == 0) {
		return 0;
	}
	return ambarella_virt_to_phys (virt);
}

static u32 ipc_phys_to_virt (u32 phys)
{
	if (phys == 0) {
		return 0;
	}
	return ambarella_phys_to_virt (phys);
}

void ipc_flush_cache (void *addr, unsigned int size)
{
	DEBUG_MSG_IPC_LK ("[ipc] ipc_flush_cache (%08x, %d)\n", (unsigned int) addr, size);
	if (addr) {
		ambcache_flush_range (addr, size);
	}
}

void ipc_inv_cache (void *addr, unsigned int size)
{
	DEBUG_MSG_IPC_LK ("[ipc] ipc_inv_cache (%08x, %d)\n", (unsigned int) addr, size);
	if (addr) {
		ambcache_inv_range (addr, size);
	}
}

void ipc_clean_cache (void *addr, unsigned int size)
{
	DEBUG_MSG_IPC_LK ("[ipc] ipc_clean_cache (%08x, %d)\n", (unsigned int) addr, size);
	if (addr) {
		ambcache_clean_range (addr, size);
	}
}

void ipc_ptr_conv_virt_to_phys (u32 data, u32 num, u32 *offs)
{
	int i;

	if ((data == 0) || (num == 0) || (offs == NULL)) {
		return;
	}

	for (i = 0; i < num; i++) {
		u32 ptr = data + offs[i];
		*(u32 *) ptr = ipc_virt_to_phys (*(u32 *) ptr);
	}
}

void ipc_ptr_conv_phys_to_virt (u32 data, u32 num, u32 *offs)
{
	int i;

	if ((data == 0) || (num == 0) || (offs == NULL)) {
		return;
	}

	for (i = 0; i < num; i++) {
		u32 ptr = data + offs[i];
		*(u32 *) ptr = ipc_phys_to_virt (*(u32 *) ptr);
	}
}

int ipc_check_svcxprt(SVCXPRT *svcxprt)
{
	int rval;

	rval = (svcxprt->tag == IPC_TAG_BEGIN) && (svcxprt->tag_end == IPC_TAG_END);

	if (!rval) {
		ipc_log_print(IPC_LOG_LEVEL_WARNING, "%s: invalid svcxprt => %08x %08x %08x",
				(svcxprt->pid >= MIN_LUIPC_PROG_NUM)?"lu":"lk",
				(u32) svcxprt, svcxprt->tag, svcxprt->tag_end);
	}

	return rval;
}

SVCXPRT *ipc_binder_preproc_in (SVCXPRT *svcxprt, int is_svc)
{
	SVCXPRT *svcxprt_va;
	struct ipcgen_table *ftab;
	void *arg_va, *res_va;
	
	DEBUG_MSG_IPC_LK (KERN_NOTICE "[ipc] %s incoming: %08x\n", is_svc?"svc":"clnt",
		(unsigned int) svcxprt);

	svcxprt_va = (SVCXPRT *) ipc_phys_to_virt ((u32) svcxprt);

	binder->svcxprt_in = svcxprt_va;

	if (is_svc) {
		ipc_inv_cache (svcxprt_va, SVCXPRT_HEAD_SIZE);

		if (svcxprt_va->pid >= MIN_LUIPC_PROG_NUM) {
			svcxprt_va->ftab = NULL;
		} else {
			struct ipc_prog_s *prog;

			prog = ipc_lookup_prog(binder->prog_svc, svcxprt_va->pid);
			svcxprt_va->ftab = &prog->table[svcxprt_va->fid];
		}
	} else {
		ipc_flush_cache (svcxprt_va, SVCXPRT_HEAD_SIZE);
	}

	ipc_check_svcxprt (svcxprt_va);

	ftab = svcxprt_va->ftab;
	arg_va = (void *) ipc_phys_to_virt ((u32) svcxprt_va->arg);
	res_va = (void *) ipc_phys_to_virt ((u32) svcxprt_va->res);

	if (is_svc) {

		/* receive request */
		ipc_inv_cache (arg_va, svcxprt_va->len_arg);
		ipc_inv_cache (res_va, svcxprt_va->len_res);

		/* NOTE: ipc_inv_cache must be before any write to these buffers.
		         This is because these buffers may be overlapped. */

		if (ftab && ftab->arg_ptbl) {
			ipc_ptr_conv_phys_to_virt ((u32) arg_va,
					ftab->arg_ptbl_num, ftab->arg_ptbl);
			ipc_clean_cache (arg_va, svcxprt_va->len_arg);
		}
		if (ftab && ftab->res_ptbl) {
			ipc_ptr_conv_phys_to_virt ((u32) res_va,
					ftab->res_ptbl_num, ftab->res_ptbl);
			ipc_clean_cache (res_va, svcxprt_va->len_res);
		}

		DEBUG_MSG_IPC_LK (KERN_NOTICE "[ipc] arg: %08x [%08x] %d\n", 
			(u32) arg_va, (u32) svcxprt_va->arg, svcxprt_va->len_arg);
		if (g_ipc_dump) {
			printk  (KERN_NOTICE "[ipc] arg: %08x [%08x] %d\n", 
			(u32) arg_va, (u32) svcxprt_va->arg, svcxprt_va->len_arg);
		}
		ipc_dump_data (arg_va, svcxprt_va->len_arg);
		DEBUG_MSG_IPC_LK (KERN_NOTICE "[ipc] res: %08x [%08x] %d\n", 
			(u32) res_va, (u32) svcxprt_va->res, svcxprt_va->len_res);
		if (g_ipc_dump) {
			printk  (KERN_NOTICE "[ipc] res: %08x [%08x] %d\n", 
			(u32) res_va, (u32) svcxprt_va->res, svcxprt_va->len_res);
		}
		ipc_dump_data (res_va, svcxprt_va->len_res);

	} else {	
		/* receive response */
		ipc_flush_cache (arg_va, svcxprt_va->len_arg);
		ipc_flush_cache (res_va, svcxprt_va->len_res);
		if (ftab && ftab->res_ptbl) {
			ipc_ptr_conv_phys_to_virt ((u32) res_va,
					ftab->res_ptbl_num, ftab->res_ptbl);
		}
		
		DEBUG_MSG_IPC_LK (KERN_NOTICE "[ipc] res: %08x [%08x] %d\n", 
			(u32) res_va, (u32) svcxprt_va->res, svcxprt_va->len_res);
		ipc_dump_data (res_va, svcxprt_va->len_res);
	}

	binder->svcxprt_in_bak = *svcxprt_va;

	svcxprt_va->arg = arg_va;
	svcxprt_va->res = res_va;
	
	return svcxprt_va;
}

SVCXPRT *ipc_binder_preproc_out (SVCXPRT *svcxprt, int is_svc)
{
	SVCXPRT *svcxprt_pa;
	struct ipcgen_table *ftab = svcxprt->ftab;
	void *arg_pa, *res_pa;
	
	DEBUG_MSG_IPC_LK (KERN_NOTICE "[ipc] %s outgoing: %08x\n", is_svc?"svc":"clnt",
		(unsigned int) svcxprt);

	binder->svcxprt_out = svcxprt;
	binder->svcxprt_out_bak = *svcxprt;

	ipc_check_svcxprt (svcxprt);

	arg_pa = (void *) ipc_virt_to_phys ((u32) svcxprt->arg);
	res_pa = (void *) ipc_virt_to_phys ((u32) svcxprt->res);

	if (!is_svc) {
		/* send request */
		if (ftab && ftab->arg_ptbl) {
			ipc_ptr_conv_virt_to_phys ((u32) svcxprt->arg,
					ftab->arg_ptbl_num, ftab->arg_ptbl);
		}

		ipc_clean_cache (svcxprt->arg, svcxprt->len_arg);
		ipc_clean_cache (svcxprt->res, svcxprt->len_res);

		DEBUG_MSG_IPC_LK (KERN_NOTICE "[ipc] arg: %08x [%08x] %d\n", 
			(u32) svcxprt->arg, (u32) arg_pa, svcxprt->len_arg);
		ipc_dump_data (svcxprt->arg, svcxprt->len_arg);
	} else {
		/* send response */
		if (ftab && ftab->arg_ptbl) {
			ipc_ptr_conv_virt_to_phys ((u32) svcxprt->arg,
					ftab->arg_ptbl_num, ftab->arg_ptbl);
		}
		if (ftab && ftab->res_ptbl) {
			ipc_ptr_conv_virt_to_phys ((u32) svcxprt->res,
					ftab->res_ptbl_num, ftab->res_ptbl);
		}
		/* cache clean for arg and res must be after pointer translation */
		ipc_clean_cache (svcxprt->arg, svcxprt->len_arg);
		ipc_clean_cache (svcxprt->res, svcxprt->len_res);

		DEBUG_MSG_IPC_LK (KERN_NOTICE "[ipc] arg: %08x [%08x] %d\n", 
			(u32) svcxprt->arg, (u32) arg_pa, svcxprt->len_arg);
		if (g_ipc_dump) {
			printk (KERN_NOTICE "[ipc] arg: %08x [%08x] %d\n", 
			(u32) svcxprt->arg, (u32) arg_pa, svcxprt->len_arg);
		}
		ipc_dump_data (svcxprt->arg, svcxprt->len_arg + 0x1c);
		DEBUG_MSG_IPC_LK (KERN_NOTICE "[ipc] res: %08x [%08x] %d\n", 
			(u32) svcxprt->res, (u32) res_pa, svcxprt->len_res);
		if (g_ipc_dump) {
			printk (KERN_NOTICE "[ipc] res: %08x [%08x] %d\n", 
			(u32) svcxprt->res, (u32) res_pa, svcxprt->len_res);
		}
		ipc_dump_data (svcxprt->res, svcxprt->len_res);
	}

	svcxprt->arg = arg_pa;
	svcxprt->res = res_pa;
	
	ipc_clean_cache(svcxprt, SVCXPRT_HEAD_SIZE);
	svcxprt_pa = (SVCXPRT *) ipc_virt_to_phys ((u32) svcxprt);

	return svcxprt_pa;

}

void  ipc_svc_get_stat(struct ipcstat_s *ipcstat)
{
	unsigned long flags;

	spin_lock_irqsave(&binder->lock, flags);
	*ipcstat = binder->ipcstat;
	spin_unlock_irqrestore(&binder->lock, flags);
}
EXPORT_SYMBOL(ipc_svc_get_stat);

/*
 * Get registered serivce programs.
 */
struct ipc_prog_s *ipc_svc_progs(void)
{
	return binder->prog_svc;
}
EXPORT_SYMBOL(ipc_svc_progs);

/*
 * Get registered client programs.
 */
struct ipc_prog_s *ipc_clnt_progs(void)
{
	return binder->prog_clnt;
}
EXPORT_SYMBOL(ipc_clnt_progs);

#if defined(STATIC_SVC)
/*
 *  * Count trailing zero
 *   */
static inline unsigned int ipc_ctz(unsigned int val)
{
	register unsigned int tmp = 0;

	__asm__ __volatile__(
			"rsbs	%0, %1, #0;\n"
			"and	%1, %1, %0;\n"
			"clz	%1, %1;\n"
			"rsbne	%1, %1, #0x1f"
			:"=r"(tmp)
			:"r"(val)
			);

	return val;
}

/*
 * Allocate a SVCXPRT
 */
static void ipc_init_svcxprts(void)
{
	int i;
	unsigned char *ptr;
	int id;

	/* Initialize SVCXPRT pools */
	binder->svcxprt_mask = 0xFFFFFFFF;
	binder->svcxprt_num = IPC_BINDER_MSG_BUFSIZE - 1;
	binder->svcxprt_in_queue = 0;
	binder->svcxprt_in_queue_max = 0;

	/* Create svcxprt spinlock */
	spin_lock_init(&binder->svcxprt_lock);
#if 0
	printk("ipc: SVCXPRT: size = %d, num = %d, mtx = %d",
	SVCXPRT_ALIGNED_SIZE, binder->svcxprt_num,
	binder->svcxprt_mtxid);
#endif
	binder->svcxprt_buf = (char*)ipc_malloc(SVCXPRT_ALIGNED_SIZE * (IPC_BINDER_MSG_BUFSIZE - 1));
	if(binder->svcxprt_buf == NULL)
	{
		pr_err("binder->svcxprt_buf alloc fail\n");
		K_ASSERT(0);
	}
	memset(binder->svcxprt_buf, 0, sizeof(binder->svcxprt_buf));

	/* Create svcxprt flag */
	ptr = binder->svcxprt_buf;
	id = boss->svc_lock_start_id;
	for (i = 0; i < binder->svcxprt_num; i++)
	{
		SVCXPRT *svcxprt = (SVCXPRT *) ptr;

		binder->svcxprts[i] = svcxprt;
		svcxprt->svcxprt_lock = id;
		id++;

		ptr += SVCXPRT_ALIGNED_SIZE;
	}
}
#endif

/*
 *  * Allocate a SVCXPRT
 *   */
static SVCXPRT *ipc_alloc_svcxprt(void)
{
	SVCXPRT *svcxprt = NULL;
#if defined(STATIC_SVC)
	unsigned long flags;
	int id;

	spin_lock_irqsave(&binder->svcxprt_lock, flags);

	id = (int)ipc_ctz(binder->svcxprt_mask);
	if ((id >= binder->svcxprt_num) || (id < 0)) {
		goto exit;
	}

	svcxprt = BINDER_GET_SVCXPRT(id);
	memset (svcxprt, 0, SVCXPRT_HEAD_SIZE);
	svcxprt->u.l.priv_id = id;
	binder->svcxprt_mask &= ~(1 << id);
	binder->svcxprt_in_queue++;
	if (binder->svcxprt_in_queue > binder->svcxprt_in_queue_max) {
		binder->svcxprt_in_queue_max = binder->svcxprt_in_queue;
	}
exit:
	spin_unlock_irqrestore(&binder->svcxprt_lock, flags);
#else
	svcxprt = ipc_malloc(sizeof(SVCXPRT));

	if(svcxprt)
		memset (svcxprt, 0, sizeof(SVCXPRT));
#endif

	return svcxprt;
}

/*
 *  * Free a SVCXPRT
 *   */
static void ipc_free_svcxprt(SVCXPRT *svcxprt)
{
#if defined(STATIC_SVC)
	unsigned long flags;
	int id = svcxprt->u.l.priv_id;

	spin_lock_irqsave(&binder->svcxprt_lock, flags);

	if (binder->svcxprt_mask & (1 << id)) {
		printk("ipc: try to free unused SVCXPRT %d, %08x",
			id, (int)svcxprt);
		K_ASSERT(0);
	}

	K_ASSERT(svcxprt == BINDER_GET_SVCXPRT(id));
	binder->svcxprt_mask |= (1 << id);
	binder->svcxprt_in_queue--;

	spin_unlock_irqrestore(&binder->svcxprt_lock, flags);
#else
	ipc_free(svcxprt);
#endif
}

/*
 * Initialize the binder.
 */
int ipc_binder_init(void)
{
	memset(binder, 0x0, sizeof(*binder));

	binder->init = 1;
	binder->lu_prog_id = 1;

	/* Set up the correct pointer to the streaming buffers */
	ipc_buf = &boss->ipc_buf;

	binder->ipcstat.timescale = boss->ipcstat_timescale;
	binder->ipcstat.ticks_in_1ms = boss->ipcstat_timescale / 1000;
	binder->tick_irq_interval = boss->ipcstat_timescale / 50;

	binder->ipcstat.min_req_lat = 0xFFFFFFFF;
	binder->ipcstat.min_rsp_lat = 0xFFFFFFFF;
	binder->ipcstat.min_call_exec = 0xFFFFFFFF;

	printk (KERN_NOTICE "aipc: boss = 0x%08x, ipc_buf = %08x\n", (u32) boss, (u32) ipc_buf);
	printk (KERN_NOTICE "aipc: binder = 0x%08x %08x\n",
		(u32) binder, ipc_virt_to_phys ((u32) binder));

	printk (KERN_NOTICE "aipc: svc outgoing = %08x, [%d, %d) %d, lock: %d\n",
		(u32) ipc_buf->svc_outgoing,
		ipc_buf->svc_out_head, ipc_buf->svc_out_tail,
		IPC_BINDER_MSG_BUFSIZE, ipc_buf->svc_out_lock);

	printk (KERN_NOTICE "aipc: svc incoming = %08x, [%d, %d) %d, lock: %d\n",
		(u32) ipc_buf->svc_incoming,
		ipc_buf->svc_in_head, ipc_buf->svc_in_tail,
		IPC_BINDER_MSG_BUFSIZE, ipc_buf->svc_in_lock);

	printk (KERN_NOTICE "aipc: clnt incoming = %08x, [%d, %d) %d, lock: %d\n",
		(u32) ipc_buf->clnt_incoming,
		ipc_buf->clnt_in_head, ipc_buf->clnt_in_tail,
		IPC_BINDER_MSG_BUFSIZE, ipc_buf->clnt_in_lock);

	printk (KERN_NOTICE "aipc: clnt outgoing = %08x, [%d, %d) %d, lock: %d\n",
		(u32) ipc_buf->clnt_outgoing,
		ipc_buf->clnt_out_head, ipc_buf->clnt_out_tail,
		IPC_BINDER_MSG_BUFSIZE, ipc_buf->clnt_out_lock);

	/* Initialize mutex, lock, and ...*/
	spin_lock_init(&binder->lock);
	spin_lock_init(&binder->lu_prog_lock);
	spin_lock_init(&binder->lu_done_lock);

	mod_timer(&ipc_poll_irq_timer,
		  jiffies + IPC_POLL_IRQ_INTERVAL);

#if defined(STATIC_SVC)
	/* Create all SVCXPRTs */
	ipc_init_svcxprts();
#endif

	return 0;
}

/*
 * Cleanup binder.
 */
void ipc_binder_cleanup(void)
{
#if defined(STATIC_SVC)
	if(binder->svcxprt_buf)
		ipc_free(binder->svcxprt_buf);
#endif
}

/*
 * Register a service program.
 */
int ipc_svc_prog_register(struct ipc_prog_s *prog)
{
	unsigned long flags;

	if (prog == NULL)
		return -1;

	spin_lock_irqsave(&binder->lock, flags);

	if (binder->prog_svc == NULL) {
		prog->next = NULL;
		prog->prev = NULL;
		binder->prog_svc = binder->prog_svc_tail = prog;
	} else {
		binder->prog_svc_tail->next = prog;
		prog->prev = binder->prog_svc_tail;
		prog->next = NULL;
		binder->prog_svc_tail = prog;
	}

	spin_unlock_irqrestore(&binder->lock, flags);

	return 0;
}
EXPORT_SYMBOL(ipc_svc_prog_register);

/*
 * Unregister a service program.
 */
int ipc_svc_prog_unregister(struct ipc_prog_s *prog)
{
	unsigned long flags;

	if (prog == NULL)
		return -1;

	spin_lock_irqsave(&binder->lock, flags);

	if (binder->prog_svc == prog)
		binder->prog_svc = prog->next;
	if (binder->prog_svc_tail == prog)
		binder->prog_svc_tail = prog->prev;
	if (prog->prev != NULL)
		prog->prev->next = prog->next;
	if (prog->next != NULL)
		prog->next->prev = prog->prev;

	spin_unlock_irqrestore(&binder->lock, flags);

	return 0;
}
EXPORT_SYMBOL(ipc_svc_prog_unregister);

/*
 * Register a client program.
 */
CLIENT *ipc_clnt_prog_register(struct ipc_prog_s *prog)
{
	CLIENT *clnt = NULL;
	unsigned long flags;

	printk("aipc: register: %s %08x %d\n", prog->name, prog->prog, prog->vers);

	if (prog == NULL)
		return NULL;

	/* Allocate new object */
	clnt = ipc_malloc(sizeof(*clnt));
	if (clnt == NULL)
		return NULL;
	memset(clnt, 0x0, sizeof(*clnt));

	clnt->prog = prog;	/* Set the prog */

	spin_lock_irqsave(&binder->lock, flags);

	/* Append to list */
	if (binder->prog_clnt == NULL) {
		prog->next = NULL;
		prog->prev = NULL;
		binder->prog_clnt = binder->prog_clnt_tail = prog;
	} else {
		binder->prog_clnt_tail->next = prog;
		prog->prev = binder->prog_clnt_tail;
		prog->next = NULL;
		binder->prog_clnt_tail = prog;
	}

	/* Append new client to binder */
	if (binder->clients == NULL) {
		clnt->next = clnt->prev = NULL;
		binder->clients = binder->clients_tail = clnt;
	} else {
		binder->clients_tail->next = clnt;
		clnt->prev = binder->clients_tail;
		binder->clients_tail = clnt;
	}

	spin_unlock_irqrestore(&binder->lock, flags);

	return clnt;
}
EXPORT_SYMBOL(ipc_clnt_prog_register);

/*
 * Unregister a client program.
 */
extern int ipc_clnt_prog_unregister(struct ipc_prog_s *prog, CLIENT *client)
{
	unsigned long flags;

	if (prog == NULL)
		return -1;
	if (client == NULL)
		return -1;
	if (client->prog != prog)
		return -1;

	spin_lock_irqsave(&binder->lock, flags);

	/* Remove prog */
	if (binder->prog_clnt == prog)
		binder->prog_clnt = prog->next;
	if (binder->prog_clnt_tail == prog)
		binder->prog_clnt_tail = prog->prev;
	if (prog->prev != NULL)
		prog->prev->next = prog->next;
	if (prog->next != NULL)
		prog->next->prev = prog->prev;

	/* Remove client */
	if (binder->clients == client)
		binder->clients = client->next;
	if (binder->clients_tail == client)
		binder->clients_tail = client->prev;
	if (client->prev != NULL)
		client->prev->next = client->next;
	if (client->next != NULL)
		client->next->prev = client->prev;

	ipc_free(client);

	spin_unlock_irqrestore(&binder->lock, flags);

	return 0;
}
EXPORT_SYMBOL(ipc_clnt_prog_unregister);

/*
 * IPC IRQ polling timer function
 */
static void ipc_poll_irq(unsigned long dummy)
{
	unsigned int elapsed;

	if (binder->tick_irq_last != 0) {
		/* Statistics: exec time */
		elapsed = ipc_tick_diff (ipc_tick_get (), binder->tick_irq_last);
		if (elapsed > binder->tick_irq_interval) {
			ipc_fake_irq ();
		}
	}

	mod_timer(&ipc_poll_irq_timer,
		  jiffies + IPC_POLL_IRQ_INTERVAL);
}

/*
 * Profiling
 */
static inline void ipc_clnt_call_statistic (SVCXPRT *svcxprt)
{
	unsigned long flags;
	unsigned int cur, elapsed;

	spin_lock_irqsave(&binder->lock, flags);

	cur = ipc_tick_get ();

	/* Statistics: exec time */
	elapsed = ipc_tick_diff (cur, svcxprt->time[IPC_TIME_SEND_REQEST]);
	if (elapsed > binder->ipcstat.max_call_exec) {
		binder->ipcstat.max_call_exec = elapsed;
	}
	if (elapsed > binder->ipcstat.ticks_in_1ms) {
		binder->ipcstat.slow_call_count++;
		DEBUG_MSG_IPC_SLOW("ipcstat %d: slow exec: %08x %d, %d ticks\n",
			svcxprt->xid, svcxprt->pid, svcxprt->fid, elapsed);
	}
	if (elapsed < binder->ipcstat.min_call_exec) {
		binder->ipcstat.min_call_exec = elapsed;
	}
	binder->ipcstat.total_call_exec += elapsed;

	/* Statistics: request latency */
	elapsed = ipc_tick_diff (svcxprt->time[IPC_TIME_GOT_REQEST],
			svcxprt->time[IPC_TIME_SEND_REQEST]);
	if (elapsed > binder->ipcstat.max_req_lat) {
		binder->ipcstat.max_req_lat = elapsed;
	}
	if (elapsed > binder->ipcstat.ticks_in_1ms) {
		binder->ipcstat.slow_req_count++;
		DEBUG_MSG_IPC_SLOW("ipcstat %d: slow req latency: %08x %d, %d ticks\n",
			svcxprt->xid, svcxprt->pid, svcxprt->fid, elapsed);
	}
	if (elapsed < binder->ipcstat.min_req_lat) {
		binder->ipcstat.min_req_lat = elapsed;
	}
	binder->ipcstat.total_req_lat += elapsed;

	/* Statistics: response latency */
	elapsed = ipc_tick_diff (svcxprt->time[IPC_TIME_GOT_REPLY],
			svcxprt->time[IPC_TIME_SEND_REPLY]);
	if (elapsed > binder->ipcstat.max_rsp_lat) {
		binder->ipcstat.max_rsp_lat = elapsed;
	}
	if (elapsed > binder->ipcstat.ticks_in_1ms) {
		binder->ipcstat.slow_rsp_count++;
		DEBUG_MSG_IPC_SLOW("ipcstat %d: slow rsp latency: %08x %d, %d ticks\n",
			svcxprt->xid, svcxprt->pid, svcxprt->fid, elapsed);
	}
	if (elapsed < binder->ipcstat.min_rsp_lat) {
		binder->ipcstat.min_rsp_lat = elapsed;
	}
	binder->ipcstat.total_rsp_lat += elapsed;

	/* Statistics: wakeup latency */
	elapsed = ipc_tick_diff (cur, svcxprt->time[IPC_TIME_GOT_REPLY]);
	if (elapsed > binder->ipcstat.max_wakeup_lat) {
		binder->ipcstat.max_wakeup_lat = elapsed;
	}
	if (elapsed > binder->ipcstat.ticks_in_1ms) {
		binder->ipcstat.slow_wakeup_count++;
		DEBUG_MSG_IPC_SLOW("ipcstat %d: slow wakeup latency: %08x %d, %d ticks\n",
			svcxprt->xid, svcxprt->pid, svcxprt->fid, elapsed);
	}
	if (elapsed < binder->ipcstat.min_wakeup_lat) {
		binder->ipcstat.min_wakeup_lat = elapsed;
	}
	binder->ipcstat.total_wakeup_lat += elapsed;

	spin_unlock_irqrestore(&binder->lock, flags);
}

static inline void ipc_timeout_init(SVCXPRT *svcxprt)
{
	svcxprt->u.l.timeout = 0;
#ifdef STATIC_SVC
	svcxprt->svcxprt_cancel = 0;
#endif
}

static inline enum clnt_stat ipc_timeout_cancel(SVCXPRT *svcxprt, unsigned int *flags)
{
#ifdef STATIC_SVC
	ipc_spin_lock(svcxprt->svcxprt_lock, flags, LOCK_POS_L_CLNT_CANCEL);
	svcxprt->svcxprt_cancel = 1;
	ipc_spin_unlock(svcxprt->svcxprt_lock, *flags, LOCK_POS_L_CLNT_CANCEL);
#endif
	svcxprt->u.l.timeout = 1;
	return IPC_TIMEDOUT;
}

static inline void ipc_timeout_wait(SVCXPRT *svcxprt, enum clnt_stat stat)
{
#ifdef STATIC_SVC
	if(stat == IPC_TIMEDOUT) {
		wait_for_completion(&svcxprt->u.l.cmpl);
/*        printk("%s: %d (%s)\n", __func__, svcxprt->rcode, ipc_strerror(svcxprt->rcode));*/
		ipc_free_svcxprt(svcxprt);
	}
#endif
}

/*
 * Execute an RPC call. Request comes from LU
 */
enum clnt_stat ipc_lu_clnt_call(unsigned int clnt_pid,
				struct aipc_nl_data *aipc_hdr,
				char *in, char *out)
{
	SVCXPRT *svcxprt = NULL;
	SVCXPRT *svcxprt_pa;
	enum clnt_stat rval = IPC_SUCCESS;
	unsigned int flags;
	unsigned int time_send_req = ipc_tick_get ();
	unsigned int xid;

	ipc_log_print(IPC_LOG_LEVEL_DEBUG, "lu: call_v => %d %08x %08x %08x %s %d",
		clnt_pid, (unsigned int) aipc_hdr, (unsigned int) in,
		(unsigned int) out, current->comm, current->pid);

	/* Check client handle */
	if (aipc_hdr == NULL) {
		ipc_log_print(IPC_LOG_LEVEL_ERROR, "lu: %s", ipc_strerror(IPC_BADCLIENT));
		return IPC_BADCLIENT;
	}

	/*
	 * Create a transaction object
	 */
	svcxprt = ipc_alloc_svcxprt();
	if (svcxprt == NULL) {
		/* Out of memory.. So free object and return error */
		ipc_log_print(IPC_LOG_LEVEL_ERROR, "lu: %s", ipc_strerror(IPC_CANTSEND));
		return IPC_CANTSEND;
	}

	svcxprt->tag = IPC_TAG_BEGIN;
	svcxprt->tag_end = IPC_TAG_END;
	svcxprt->pid = aipc_hdr->pid;
	svcxprt->fid = aipc_hdr->fid;
	svcxprt->vers = aipc_hdr->vers;
	svcxprt->arg = in;
	svcxprt->len_arg = aipc_hdr->len_arg;
	svcxprt->res = out;
	svcxprt->len_res = aipc_hdr->len_res;
	svcxprt->rcode = IPC_PROCESSING;
	svcxprt->raw_ptr = svcxprt;
	svcxprt->proc_type = aipc_hdr->proc_type;
	svcxprt->lu_xid = aipc_hdr->lu_xid;
	svcxprt->u.l.clnt_pid = clnt_pid;
	svcxprt->compl_arg = current;
	svcxprt->time[IPC_TIME_SEND_REQEST] = time_send_req;
	ipc_timeout_init(svcxprt);
	init_completion(&svcxprt->u.l.cmpl);

	ipc_spin_lock(ipc_buf->lock, &flags, LOCK_POS_LU_CLNT_OUT);
	svcxprt->xid = ipc_buf->ipc_xid++;
	ipc_spin_unlock(ipc_buf->lock, flags, LOCK_POS_LU_CLNT_OUT);

	xid = svcxprt->xid;
	svcxprt_pa = ipc_binder_preproc_out (svcxprt, 0);

	ipc_log_print(IPC_LOG_LEVEL_INFO, "lu: call %08x %08x %d %u",
			xid, svcxprt->pid, svcxprt->fid, time_send_req);

	/* Put the SVCXPRT into outgoing queue */
	ipc_spin_lock(ipc_buf->clnt_out_lock, &flags, LOCK_POS_LU_CLNT_OUT);

	if (IPC_CMD_QUEUE_NOT_FULL(ipc_buf->clnt_out_head, ipc_buf->clnt_out_tail)) 
	{
		ipc_buf->clnt_outgoing[ipc_buf->clnt_out_tail] = svcxprt_pa;
		ipc_buf->clnt_out_tail = IPC_CMD_QUEUE_PTR_NEXT(ipc_buf->clnt_out_tail);
		binder->pending_req++;

		/* Send interrupt to remote OS. */
		ipc_send_irq();
	} else {
		rval = IPC_CMD_QUEUE_FULL;
		ipc_spin_unlock(ipc_buf->clnt_out_lock, flags, LOCK_POS_LU_CLNT_OUT);
		ipc_clnt_call_complete(svcxprt, rval);
		goto exit;
	}

	ipc_spin_unlock(ipc_buf->clnt_out_lock, flags, LOCK_POS_LU_CLNT_OUT);

	if ((aipc_hdr->proc_type & IPC_CALL_ASYNC) == 0) {

#if CONFIG_IPC_INFINITE_WAIT
		aipc_hdr->timeout = 0xFFFFFFFF;
#endif
		ipc_log_print(IPC_LOG_LEVEL_INFO, "lu: call wait %08x", xid);

		/* Wait for the remote procedure to complete */
		if (wait_for_completion_timeout(&svcxprt->u.l.cmpl, 
				msecs_to_jiffies(aipc_hdr->timeout)) > 0) {
 			aipc_nl_send_result_to_lu(svcxprt);
			rval = IPC_SUCCESS;
		} else {
			rval = ipc_timeout_cancel(svcxprt, &flags);
		}

		if(rval != IPC_TIMEDOUT) {
			ipc_clnt_call_complete(svcxprt, rval);
		}
	} else {
		svcxprt->proc_type |= IPC_CALL_ASYNC_AUTO_DEL;
	}

exit:
	K_ASSERT(rval != IPC_CMD_QUEUE_FULL);

	ipc_timeout_wait(svcxprt, rval);
	
	if ((rval != IPC_SUCCESS) && (rval != IPC_PROCESSING)) {
		ipc_log_print(IPC_LOG_LEVEL_ERROR, "lu: %s => %08x",
				ipc_strerror(rval), xid);
	}

	ipc_log_print(IPC_LOG_LEVEL_INFO, "lu: call end %08x", xid);

	return rval;
}

/*
 * Get next program id for a LU client program
 */
unsigned int ipc_lu_clnt_next_prog_id (void)
{
	unsigned int prog_id;
	unsigned long flags;

	spin_lock_irqsave(&binder->lock, flags);
	prog_id = binder->lu_prog_id++;
	spin_unlock_irqrestore(&binder->lock, flags);

	return prog_id;
}
EXPORT_SYMBOL(ipc_lu_clnt_next_prog_id);

/*
 * Lookup IPC program
 */
inline static struct ipc_prog_s *
ipc_lookup_prog(struct ipc_prog_s *head, unsigned int prog_id)
{
	struct ipc_prog_s *prog;

	for (prog = head; prog != NULL; prog = prog->next) {
		if (prog->prog == prog_id)
			break;
	}

	return prog;
}

/*
 * Lookup IPC call table
 */
inline static int 
ipc_lookup_ftab(struct ipc_prog_s *prog, unsigned long procnum, struct ipcgen_table **pftab)
{
	int i;
	int fid = -1;
	struct ipcgen_table *ftab;

	/*
	 * Now find the procedure.
	 * Firstly, do a quick test by using index into the array.
	 * Note that this should always succeed IFF the designer of the X file
	 * kept the procedure in-order (good design).
	 * However, if the quick test fails, then we walk through the array
	 * looking for a match.
	 */
	ftab = prog->table;
	if (procnum < prog->nproc &&
	    ftab[procnum].fid == procnum) {
		fid = procnum;
		ftab = &ftab[fid];
	} else {
		for (i = 0; i < prog->nproc; i++) {
			if (ftab[i + 1].fid == procnum) {
				fid = procnum;
				ftab = &ftab[fid];
				break;
			}
		}
	}
	if (fid != -1) {
		*pftab = ftab;
	}

	return fid;
}

void ipc_clnt_call_signal_lu(SVCXPRT *svcxprt)
{
	int rval;
	struct siginfo info;

	memset(&info, 0, sizeof(struct siginfo));
	info.si_signo = SIGRTMAX;
	info.si_code = SI_QUEUE;
	info._sifields._rt._sigval.sival_int = svcxprt->lu_xid;

	rval = send_sig_info(SIGRTMAX, &info, (struct task_struct *) svcxprt->compl_arg);
}

/*
 * Complete an RPC call.
 */
void ipc_clnt_call_complete(SVCXPRT *svcxprt, enum clnt_stat rval)
{
	ipc_log_print(IPC_LOG_LEVEL_INFO, "%s: complete %08x",
			(svcxprt->pid >= MIN_LUIPC_PROG_NUM)?"lu":"lk", svcxprt->xid);

	if (rval == IPC_SUCCESS) {
		ipc_clnt_call_statistic(svcxprt);
	}

	/* call completion callback */
	if (svcxprt->compl_proc) {
		svcxprt->compl_proc(svcxprt->compl_arg, svcxprt);
	}

#if !defined(STATIC_SVC)
	if (svcxprt->u.l.timeout) {
		svcxprt->u.l.timeout = 0;
	}
#endif

	/* Free transaction object */
	ipc_free_svcxprt(svcxprt);
}

/*
 * Execute an RPC call. Request comes from LK
 */
enum clnt_stat ipc_clnt_call(CLIENT *clnt,
			     unsigned long procnum,
			     char *in, char *out,
			     struct clnt_req *req)
{
	struct ipc_prog_s *prog;
	struct ipcgen_table *ftab = NULL;
	SVCXPRT *svcxprt = NULL;
	SVCXPRT *svcxprt_pa;
	int fid = -1;
	enum clnt_stat rval = IPC_PROCESSING;
	unsigned int flags;
	unsigned int time_send_req = ipc_tick_get ();
	unsigned int xid;

	K_ASSERT(binder->init);

	ipc_log_print(IPC_LOG_LEVEL_DEBUG, "lk: call_v => %08x %d %08x %08x",
		(unsigned int ) clnt, procnum, (unsigned int ) in, (unsigned int ) out);

	/* Check client handle */
	if (clnt == NULL) {
		ipc_log_print(IPC_LOG_LEVEL_ERROR, "lk: %s", ipc_strerror(IPC_BADCLIENT));
		return IPC_BADCLIENT;
	}

#if 1
	/* Paranoid check... Maybe remove this later to reduce loading? */
	for (prog = binder->prog_clnt; prog != NULL; prog = prog->next) {
		if (prog == clnt->prog)
			break;
	}
	if (prog == NULL) {
		ipc_log_print(IPC_LOG_LEVEL_ERROR, "lk: %s => %08x %d",
				ipc_strerror(IPC_NOPROG), prog->prog, fid);
		return IPC_NOPROG;
	}
#endif

	prog = clnt->prog;
	fid = ipc_lookup_ftab(prog, procnum, &ftab);
	if ((fid == -1) || (ftab == NULL)) {
		ipc_log_print(IPC_LOG_LEVEL_ERROR, "lk: %s => %08x %d",
				ipc_strerror(IPC_NOPROC), prog->prog, fid);
		return -1;
	}

	/*
	 * Create a transaction object
	 * Allocate the size of SVCXPRT plus the size of the "result", so we can
	 * assign the pointer ->res to the end of this structure without having to
	 * call a separate malloc.
	 */
	svcxprt = ipc_alloc_svcxprt();
	if (svcxprt == NULL) {
		/* Out of memory.. So free object and return error */
		ipc_log_print(IPC_LOG_LEVEL_ERROR, "lk: %s => %08x %d",
				ipc_strerror(IPC_CANTSEND), prog->prog, fid);
		return IPC_CANTSEND;
	}

	svcxprt->tag = IPC_TAG_BEGIN;
	svcxprt->tag_end = IPC_TAG_END;
	svcxprt->pid = prog->prog;
	svcxprt->fid = fid;
	svcxprt->vers = prog->vers;
	svcxprt->arg = in;
	svcxprt->len_arg = ftab->len_arg;
	svcxprt->res = out;
	svcxprt->len_res = ftab->len_res;
	svcxprt->rcode = IPC_PROCESSING;
	svcxprt->raw_ptr = svcxprt;
	svcxprt->ftab = ftab;
	svcxprt->proc_type = ftab->type;
	svcxprt->time[IPC_TIME_SEND_REQEST] = time_send_req;
	ipc_timeout_init(svcxprt);
	init_completion(&svcxprt->u.l.cmpl);

	ipc_spin_lock(ipc_buf->lock, &flags, LOCK_POS_L_CLNT_OUT);
	svcxprt->xid = ipc_buf->ipc_xid++;
	ipc_spin_unlock(ipc_buf->lock, flags, LOCK_POS_L_CLNT_OUT);

	xid = svcxprt->xid;
	svcxprt_pa = ipc_binder_preproc_out (svcxprt, 0);

	ipc_log_print(IPC_LOG_LEVEL_INFO, "lk: call %08x %08x %d %u",
			xid, svcxprt->pid, svcxprt->fid, time_send_req);

	/* Put the SVCXPRT into outgoing queue */
	ipc_spin_lock(ipc_buf->clnt_out_lock, &flags, LOCK_POS_L_CLNT_OUT);

	if (IPC_CMD_QUEUE_NOT_FULL(ipc_buf->clnt_out_head, ipc_buf->clnt_out_tail)) 
	{
		ipc_buf->clnt_outgoing[ipc_buf->clnt_out_tail] = svcxprt_pa;
		ipc_buf->clnt_out_tail = IPC_CMD_QUEUE_PTR_NEXT(ipc_buf->clnt_out_tail);
		binder->pending_req++;

		/* Send interrupt to remote OS. */
		ipc_send_irq();
	} else {
		rval = IPC_CMD_QUEUE_FULL;
		ipc_spin_unlock(ipc_buf->clnt_out_lock, flags, LOCK_POS_L_CLNT_OUT);
		ipc_clnt_call_complete(svcxprt, rval);
		goto exit;
	}

	ipc_spin_unlock(ipc_buf->clnt_out_lock, flags, LOCK_POS_L_CLNT_OUT);

#if CONFIG_IPC_INFINITE_WAIT
	ftab->timeout = 0xFFFFFFFF;
#endif

	/* Wait for completion or just pass through */
	if ((ftab->type & IPC_CALL_ASYNC) == 0) {
		unsigned long success;

		ipc_log_print(IPC_LOG_LEVEL_INFO, "lk: call wait %08x", xid);

		/* Wait for the remote procedure to complete */
		success = wait_for_completion_timeout(&svcxprt->u.l.cmpl,
						   msecs_to_jiffies(ftab->timeout));
		if (success) {
			/* At the stage, the handshake is complete, grab the rcode and be */
			/* ready to return upstack! */
			rval = (enum clnt_stat) svcxprt->rcode;
		} else {
			rval = ipc_timeout_cancel(svcxprt, &flags);
		}

		if(rval != IPC_TIMEDOUT) {
			ipc_clnt_call_complete(svcxprt, rval);
		}
	} else {
		if (req) {
			req->svcxprt = svcxprt;
			svcxprt->compl_proc = req->compl_proc;
			svcxprt->compl_arg = req->compl_arg;
		}
	}
exit:
	K_ASSERT(rval != IPC_CMD_QUEUE_FULL);

	ipc_timeout_wait(svcxprt, rval);

	if ((rval != IPC_SUCCESS) && (rval != IPC_PROCESSING)) {
		ipc_log_print(IPC_LOG_LEVEL_ERROR, "lk: %s => %08x",
				ipc_strerror(rval), xid);
	}

	ipc_log_print(IPC_LOG_LEVEL_INFO, "lk: call end %08x", xid);

	return rval;
}
EXPORT_SYMBOL(ipc_clnt_call);

/*
 * Wait util the IPC call is complete.
 */
enum clnt_stat ipc_clnt_wait_for_completion(CLIENT *clnt,
					struct clnt_req *req, int timeout)
{
	enum clnt_stat rval = IPC_SUCCESS;
	SVCXPRT *svcxprt;

	K_ASSERT(clnt && req);

	svcxprt = req->svcxprt;

	if ((svcxprt->proc_type & IPC_CALL_ASYNC_MASK) != IPC_CALL_ASYNC) {
		return IPC_EINVAL;
	}

	if (svcxprt->rcode == IPC_PROCESSING) {
		unsigned long success;

		/* Wait for the remote procedure to complete */
		if(timeout != 0) {
			success = wait_for_completion_timeout(&svcxprt->u.l.cmpl,
							   msecs_to_jiffies(timeout));
		} else {
			success = try_wait_for_completion(&svcxprt->u.l.cmpl);
		}
		if (success) {
			rval = (enum clnt_stat) svcxprt->rcode;
		} else {
			rval = IPC_TIMEDOUT;
		}
	} else {
		rval = (enum clnt_stat) svcxprt->rcode;
	}

	if (rval != IPC_TIMEDOUT) {
		ipc_clnt_call_complete(svcxprt, rval);
	}

	return rval;
}
EXPORT_SYMBOL(ipc_clnt_wait_for_completion);

/*
 * Set timeout all IPC call for the CLIENT.
 */
void ipc_clnt_set_timeout(CLIENT *clnt, struct timeval tout)
{
	struct ipc_prog_s *prog;
	struct ipcgen_table *ftab;
	int i, timeout;

	timeout = tout.tv_sec * 1000 + tout.tv_usec / 1000;
	prog = clnt->prog;
	ftab = prog->table;

	for (i = 0; i < prog->nproc; i++) {
		ftab[i + 1].timeout = timeout;
	}
}
EXPORT_SYMBOL(ipc_clnt_set_timeout);

/*
 * Set timeout of an IPC call.
 */
void ipc_clnt_set_call_timeout(CLIENT *clnt, int fid, struct timeval tout)
{
	struct ipc_prog_s *prog;
	struct ipcgen_table *ftab;
	int i, timeout;

	timeout = tout.tv_sec * 1000 + tout.tv_usec / 1000;
	prog = clnt->prog;
	ftab = prog->table;

	for (i = 0; i < prog->nproc; i++) {
		if (ftab[i + 1].fid == fid) {
			ftab[i + 1].timeout = timeout;
			break;
		}
	}
}
EXPORT_SYMBOL(ipc_clnt_set_call_timeout);

/*
 * Send reply back to client.
 */
bool_t ipc_svc_sendreply(SVCXPRT *svcxprt, char *out)
{
	bool_t rval = 1;
	unsigned int flags;
	SVCXPRT *svcxprt_pa;

	K_ASSERT(svcxprt);

	svcxprt->time[IPC_TIME_SEND_REPLY] = ipc_tick_get ();

	if (out) {
		memcpy(svcxprt->res, out, svcxprt->len_res);
	}

	svcxprt_pa = ipc_binder_preproc_out (svcxprt, 1);

	/* Now put the transaction into outgoing queue */
	ipc_spin_lock(ipc_buf->svc_out_lock, &flags, LOCK_POS_L_SVC_OUT);

	if (IPC_CMD_QUEUE_NOT_FULL(ipc_buf->svc_out_head, ipc_buf->svc_out_tail))
	{
		ipc_buf->svc_outgoing[ipc_buf->svc_out_tail] = svcxprt_pa;
		ipc_buf->svc_out_tail = IPC_CMD_QUEUE_PTR_NEXT(ipc_buf->svc_out_tail);
		binder->pending_rsp--;

		/* Send an IRQ to remote OS */
		ipc_send_irq();
	} else {
		rval = 0;
	}

	ipc_spin_unlock(ipc_buf->svc_out_lock, flags, LOCK_POS_L_SVC_OUT);

	K_ASSERT(rval == 1);

	return rval;
}
EXPORT_SYMBOL(ipc_svc_sendreply);

/*
 * IPC call completion BH.
 */
static bool_t ipc_clnt_call_complete_bh(void *arg, void *res, SVCXPRT *svcxprt)
{
#if CONFIG_IPC_SIGNAL_LU_COMPL
	if ((svcxprt->pid >= MIN_IIPC_PROG_NUM) && (svcxprt->pid < MIN_LKIPC_PROG_NUM)) {
		ipc_clnt_call_signal_lu(svcxprt);
	}
#endif
	ipc_clnt_call_complete(svcxprt, (enum clnt_stat) svcxprt->rcode);

	return 1;
}

/*
 * Process incoming server reply to client requests.
 */
static void ipc_binder_got_reply(SVCXPRT *svcxprt)
{
	svcxprt->time[IPC_TIME_GOT_REPLY] = ipc_tick_get ();

#ifdef STATIC_SVC
	if ((svcxprt->proc_type & IPC_CALL_ASYNC_AUTO_DEL)) {
#else
	if ((svcxprt->proc_type & IPC_CALL_ASYNC_AUTO_DEL) || svcxprt->u.l.timeout) {
#endif
		ipc_bh_queue((ipc_bh_f) ipc_clnt_call_complete_bh,
			     NULL, NULL, svcxprt);
	} else {
		/* Just wake up the caller who is blocked by the event flag. */
		/* Note that we're in ISR */
#ifdef STATIC_SVC
		if(svcxprt->u.l.timeout) {
			svcxprt->u.l.timeout = 0;
			svcxprt->rcode = IPC_CANCEL;
		}
		else
#endif
			svcxprt->rcode = IPC_SUCCESS;
		complete(&svcxprt->u.l.cmpl);
	}
}

#ifdef STATIC_SVC
int ipc_cancel_check(SVCXPRT *svcxprt)
{
	unsigned int flags;

	ipc_spin_lock(svcxprt->svcxprt_lock, &flags, LOCK_POS_L_CLNT_CANCEL);
	if(svcxprt->svcxprt_cancel) {
		svcxprt->rcode = IPC_CANCEL;
		svcxprt->svcxprt_cancel = 0;
		ipc_spin_unlock(svcxprt->svcxprt_lock, flags, LOCK_POS_L_CLNT_CANCEL);
		ipc_svc_sendreply(svcxprt, NULL);
		return 1;
	}
	ipc_spin_unlock(svcxprt->svcxprt_lock, flags, LOCK_POS_L_CLNT_CANCEL);
	return 0;
}
#endif

/*
 * Piggy-backed on one of ipc_bh task to perform user-space pass-over
 * request
 */
static bool_t __ipc_binder_userspace_got_request(void *arg, void *res,
						 SVCXPRT *svcxprt)
{
	int rval;

	rval = aipc_nl_get_request_from_itron(svcxprt);
	if (rval != 0) {
		ipc_log_print(IPC_LOG_LEVEL_ERROR, "lu: req fail => %08x %08x %d",
				svcxprt->xid, svcxprt->pid, svcxprt->fid);
	}

	return 1;
}

/*
 * Process incoming request from remote client.
 */
void ipc_binder_got_request(SVCXPRT *svcxprt)
{
	struct ipc_prog_s *prog;
	struct ipcgen_table *table;
	bool_t (*proc)(void *argp, void *result, struct svc_req *);
	struct svc_req svc_req;
	bool_t result_code;

	svcxprt->time[IPC_TIME_GOT_REQEST] = ipc_tick_get ();

	if (svcxprt->pid >= MIN_LUIPC_PROG_NUM) {
		ipc_bh_queue(__ipc_binder_userspace_got_request,
			     NULL, NULL, svcxprt);
		return;
	}

	/* Locate the program */
	for (prog = binder->prog_svc; prog != NULL; prog = prog->next) {
		if (prog->prog == svcxprt->pid) {
			table = prog->table;
			break;
		}
	}

	/* Unable to find a server for the transaction */
	if (prog == NULL) {
		svcxprt->rcode = IPC_PROGUNAVAIL;
		ipc_svc_sendreply(svcxprt, NULL);
		return;
	}

	/* Check function index sanity and proc availability */
	if (svcxprt->fid > prog->nproc ||
	    table[svcxprt->fid].proc == NULL) {
		svcxprt->rcode = IPC_PROCUNAVAIL;
		ipc_svc_sendreply(svcxprt, NULL);
		return;
	} else {
		proc = (bool_t (*)(void *, void *, struct svc_req *))
			table[svcxprt->fid].proc;
	}

	/* Check for consistency in version */
	if (svcxprt->vers != prog->vers) {
		svcxprt->rcode = IPC_VERSMISMATCH;
		ipc_svc_sendreply(svcxprt, NULL);
		return;
	}

#ifdef STATIC_SVC
	if (ipc_cancel_check(svcxprt))
		return;
#endif

	/* Now invoke the function! */
	svc_req.svcxprt = svcxprt;
	result_code = proc(svcxprt->arg, svcxprt->res, &svc_req);
}

/*
 * Interrupt handler.
 */
void ipc_binder_dispatch(void)
{
	SVCXPRT *svcxprt;
	unsigned int flags;
	unsigned int incoming = 0;

	/* Check incoming replies (complete call upstack to client) */
	DEBUG_MSG_IPC_LK ("[ipc] check incoming replies\n");
	ipc_spin_lock(ipc_buf->clnt_in_lock, &flags, LOCK_POS_L_CLNT_IN);
	if (IPC_CMD_QUEUE_NOT_EMPTY(ipc_buf->clnt_in_head, ipc_buf->clnt_in_tail)) {
		svcxprt = ipc_buf->clnt_incoming[ipc_buf->clnt_in_head];
		ipc_buf->clnt_in_head = IPC_CMD_QUEUE_PTR_NEXT(ipc_buf->clnt_in_head);
		binder->pending_req--;
		incoming++;
	} else {
		svcxprt = NULL;
	}
	ipc_spin_unlock(ipc_buf->clnt_in_lock, flags, LOCK_POS_L_CLNT_IN);

	if (svcxprt) {
		svcxprt = ipc_binder_preproc_in (svcxprt, 0);
		ipc_binder_got_reply(svcxprt);				
	}

	/* Check incoming requests */
	DEBUG_MSG_IPC_LK ("[ipc] check incoming requests\n");
	ipc_spin_lock(ipc_buf->svc_in_lock, &flags, LOCK_POS_L_SVC_IN);
	if (IPC_CMD_QUEUE_NOT_EMPTY(ipc_buf->svc_in_head, ipc_buf->svc_in_tail)) {
		svcxprt = ipc_buf->svc_incoming[ipc_buf->svc_in_head];
		ipc_buf->svc_in_head = IPC_CMD_QUEUE_PTR_NEXT(ipc_buf->svc_in_head);
		binder->pending_rsp++;
		incoming++;
	} else {
		svcxprt = NULL;
	}
	ipc_spin_unlock(ipc_buf->svc_in_lock, flags, LOCK_POS_L_SVC_IN);

	if (svcxprt) {
		svcxprt = ipc_binder_preproc_in (svcxprt, 1);
		ipc_binder_got_request(svcxprt);				
	}

	binder->tick_irq_last = ipc_tick_get ();

	DEBUG_MSG_IPC_LK ("[ipc] ipc_binder_dispatch END\n");
}

