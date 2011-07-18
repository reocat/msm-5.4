/*
 * drivers/ambarella-ipc/ipc.c
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>

#include <linux/aipc/aipc.h>
#include <linux/aipc/irq.h>
#include <mach/boss.h>
#if (CHIP_REV == I1)
#include <asm/spinlock.h>
#endif
#include <asm/uaccess.h>

#include <linux/aipc/ipc_slock.h>
#include <linux/aipc/ipc_shm.h>
#include <linux/aipc/ipc_mutex.h>
#include <linux/aipc/ipc_log.h>
#include <linux/aipc/i_status.h>

#include "binder.h"

#define IPC_SPINLOCK_TEST	0
#define IPC_MUTEX_TEST		1

extern void ipc_irq_init(void);
extern void ipc_irq_free(void);
extern int ipc_binder_init(void);
extern void ipc_binder_cleanup(void);
extern void ipc_bh_init(void);
extern void ipc_bh_cleanup(void);
extern void aipc_nl_cleanup(void);

#if defined(CONFIG_PROC_FS)

struct proc_dir_entry *proc_aipc = NULL;
EXPORT_SYMBOL(proc_aipc);
struct proc_dir_entry *proc_proginfo = NULL;
struct proc_dir_entry *proc_lu_prog_id = NULL;

#if IPC_SPINLOCK_TEST
struct proc_dir_entry *proc_spin_test = NULL;
#endif

#if IPC_MUTEX_TEST
struct proc_dir_entry *proc_mutex = NULL;
static int G_mutex_count[256];
static unsigned int *G_mutex_shm_count = NULL;
#endif

static int print_ipcgen_table(char *p, ssize_t size,
			      struct ipcgen_table *table, int nproc)
{
	int i, len = 0;

	for (i = 0; i < nproc; i++) {
		len += sprintf(p + len, "0x%.8x %4d %4d %4d   %s\n",
			       (unsigned int)table[i + 1].proc,
			       table[i + 1].fid,
			       table[i + 1].len_arg,
			       table[i + 1].len_res,
			       table[i + 1].name);
	}

	return len;
}

static int print_lu_procinfo(char *p, ssize_t size,
			      struct aipc_nl_proc_info *proc_info,
			      int nproc)
{
	int i, len = 0;

	for (i = 0; i < nproc; i++) {
		len += sprintf(p + len, "0x%.8x %4d %4d %4d   %s\n",
			       (unsigned int)proc_info->proc,
			       proc_info->fid,
			       proc_info->len_arg,
			       proc_info->len_res,
			       proc_info->name);
		proc_info++;
	}

	return len;
}

static int ipc_proginfo_proc_read(char *page, char **start,
				  off_t off, int count, int *eof, void *data)
{
	int len = 0;
	struct ipc_prog_s *prog;
	struct aipc_nl_prog *nl_prog;

	/*
	 * To avoid buffer overflow, we split all the information into two
	 * parts: kernel ipc programs, and user-space ipc programs
	 * This method looks somewhat odd, we may use seq_file instead
	 * of proc->read_proc.
	 */
	if (off != 0)
		goto lu_prog_info;

	*(unsigned long *)start = 1;

	len += snprintf(page + len, PAGE_SIZE - len,
			"=======================\n");
	len += snprintf(page + len, PAGE_SIZE - len,
			"Registered IPC programs\n");
	len += snprintf(page + len, PAGE_SIZE - len,
			"servers: ");
	for (prog = ipc_svc_progs(); prog != NULL; prog = prog->next) {
		len += snprintf(page + len, PAGE_SIZE - len,
				"%s ", prog->name);
	}
	len += snprintf(page + len, PAGE_SIZE - len,
			"\n");
	len += snprintf(page + len, PAGE_SIZE - len,
			"clients: ");
	for (prog = ipc_clnt_progs(); prog != NULL; prog = prog->next) {
		len += snprintf(page + len, PAGE_SIZE - len,
				"%s ", prog->name);
	}
	len += snprintf(page + len, PAGE_SIZE - len,
		      "\n");
	len += snprintf(page + len, PAGE_SIZE - len,
		      "=======================\n");
	len += snprintf(page + len, PAGE_SIZE - len,
		      "\n\n");

	for (prog = ipc_svc_progs(); prog != NULL; prog = prog->next) {
		len += snprintf(page + len, PAGE_SIZE - len,
			      "%s (S) P:0x%.8x, V:%d N:%d\n",
				prog->name, prog->prog,
				prog->vers, prog->nproc);
		len += print_ipcgen_table(page + len, PAGE_SIZE - len,
					  prog->table, prog->nproc);
		len += snprintf(page + len, PAGE_SIZE - len, "\n");
	}

	for (prog = ipc_clnt_progs(); prog != NULL; prog = prog->next) {
		len += snprintf(page + len, PAGE_SIZE - len,
				"%s (C) P:0x%.8x, V:%d N:%d\n",
				prog->name, prog->prog,
				prog->vers, prog->nproc);
		len += print_ipcgen_table(page + len, PAGE_SIZE - len,
					  prog->table, prog->nproc);
		len += snprintf(page + len, PAGE_SIZE - len,
				"\n");
	}

	return len;

lu_prog_info:

	len += snprintf(page + len, PAGE_SIZE - len,
			"=======================\n");
	len += snprintf(page + len, PAGE_SIZE - len,
			"Registered IPC programs in user-space\n");
	list_for_each_entry(nl_prog, &binder->lu_prog_list, list) {
		len += snprintf(page + len, PAGE_SIZE - len,
			      "%s (S) P:0x%.8x, V:%d N:%d\n",
				nl_prog->prog_info.name,
				nl_prog->prog_info.pid,
				nl_prog->prog_info.vers,
				nl_prog->prog_info.nproc);
		len += print_lu_procinfo(page + len, PAGE_SIZE - len,
					  nl_prog->proc_info,
					  nl_prog->prog_info.nproc);
		len += snprintf(page + len, PAGE_SIZE - len,
				"\n");
	}


	len += snprintf(page + len, PAGE_SIZE - len, "\n");

	*eof = 1;

	return len;
}

static int ipc_lu_prog_id_proc_read(char *page, char **start,
				  off_t off, int count, int *eof, void *data)
{
	int len = 0;
	unsigned int prog_id = ipc_lu_clnt_next_prog_id ();

	*(unsigned long *) start = (unsigned long) page;
	len += snprintf(page + len, PAGE_SIZE - len, "%08x\n", prog_id);
	*eof = 1;

	return len;
}

static int ipc_mutex_proc_write(struct file *file, const char __user *buffer,
				 unsigned long count, void *data)
{
	char cmd[64];
	char req[64];
	int argc;
	int mtxid;
	int cmd_id;

	if (G_mutex_shm_count == NULL) {
		G_mutex_shm_count = ipc_shm_get(IPC_SHM_AREA_GLOBAL, 
						IPC_SHM_GLOBAL_COUNT);
	}

	if (copy_from_user(req, buffer, count))
		return -EFAULT;

	argc = sscanf(req, "%s %d %d", cmd, &mtxid, &cmd_id);
	//pr_notice ("\nmtx: %s %d %d\n", cmd, mtxid, cmd_id);
	if (!strcmp (cmd, "lock")) {
		ipc_mutex_lock(mtxid);
	}
	else if (!strcmp (cmd, "unlock")) {
		ipc_mutex_unlock(mtxid);
	}
	else if (!strcmp (cmd, "inc")) {
		ipc_inc(G_mutex_shm_count);
		G_mutex_count[cmd_id]++;
	}
	else if (!strcmp (cmd, "test")) {
		ipc_mutex_lock(mtxid);

		ipc_inc(G_mutex_shm_count);
		G_mutex_count[cmd_id]++;

		ipc_mutex_unlock(mtxid);
	}
	else if (!strcmp (cmd, "reset")) {
		int i = 0;

		for (i = 0; i < sizeof(G_mutex_count)/sizeof(G_mutex_count[0]); i++) {
			G_mutex_count[i] = 0;
		}
	}

	return count;
}

#if IPC_SPINLOCK_TEST
static int ipc_spin_proc_lock(char *page, char **start,
				  off_t off, int count, int *eof, void *data)
{
	int len = 0;

	ipc_spin_test();

	*(unsigned long *)start = 1;
	*eof = 1;

	return len;
}
#endif

#endif  /* CONFIG_PROC_FS */

static int ipc_smem_init(void)
{
	u32 mem, size;

	mem = boss->smem_addr + BOSS_BOSS_MEM_SIZE;
	size = boss->smem_size - BOSS_BOSS_MEM_SIZE;

	/* Initialize shm buffer */
	if (size < IPC_SHM_MEM_SIZE) {
		return -1;
	}
	ipc_shm_init(mem, IPC_SHM_MEM_SIZE);
	mem += IPC_SHM_MEM_SIZE;
	size -= IPC_SHM_MEM_SIZE;

	/* Initialize spinlock buffer */
	if (size < IPC_SPINLOCK_MEM_SIZE) {
		return -1;
	}
	ipc_slock_init(mem, IPC_SPINLOCK_MEM_SIZE);
	mem += IPC_SPINLOCK_MEM_SIZE;
	size -= IPC_SPINLOCK_MEM_SIZE;
	
	/* Initialize mutex buffer */
	if (size < IPC_MUTEX_MEM_SIZE) {
		return -1;
	}
	ipc_mutex_init(mem, IPC_MUTEX_MEM_SIZE);
	mem += IPC_MUTEX_MEM_SIZE;
	size -= IPC_MUTEX_MEM_SIZE;

	/* Initialize log buffer */
	if (size < IPC_LOG_MEM_SIZE) {
		return -1;
	}
	ipc_log_init(mem, IPC_LOG_MEM_SIZE);
	mem += IPC_LOG_MEM_SIZE;
	size -= IPC_LOG_MEM_SIZE;

	return 0;
}

static void ipc_status_report(void)
{
	i_status_init();
	ipc_status_report_ready(1);
	ipc_boss_ver_report(BOSS_LINUX_VERSION);
}

static int __init ambarella_ipc_init(void)
{
	int rval = 0;

	printk (KERN_NOTICE "aipc: ambarella ipc init\n");

#if defined(CONFIG_PROC_FS)
	proc_aipc = proc_mkdir("aipc", NULL);
	if (!proc_aipc) {
		printk(KERN_ERR "create /proc/aipc failed\n");
		rval = -ENOMEM;
		goto done;
	}

	proc_proginfo = create_proc_entry("proginfo", S_IRUGO, proc_aipc);
	if (proc_proginfo == NULL) {
		printk(KERN_ERR "create /procfs/aipc/proginfo failed\n");
		rval = -ENOMEM;
		goto done;
	}
	proc_proginfo->read_proc = ipc_proginfo_proc_read;

	proc_lu_prog_id = create_proc_entry("lu_prog_id", S_IRUGO, proc_aipc);
	if (proc_lu_prog_id == NULL) {
		printk(KERN_ERR "create /procfs/aipc/lu_prog_id failed\n");
		rval = -ENOMEM;
		goto done;
	}
	proc_lu_prog_id->read_proc = ipc_lu_prog_id_proc_read;

#if IPC_MUTEX_TEST
	proc_mutex = create_proc_entry("mutex", S_IRUGO, proc_aipc);
	if (proc_mutex == NULL) {
		printk(KERN_ERR "create /procfs/aipc/mutex failed\n");
		rval = -ENOMEM;
		goto done;
	}
	proc_mutex->write_proc = ipc_mutex_proc_write;
#endif

#if IPC_SPINLOCK_TEST
	proc_spin_test = create_proc_entry("spin_test", S_IRUGO, proc_aipc);
	if (proc_spin_test == NULL) {
		printk(KERN_ERR "create /procfs/aipc/spin_test failed\n");
		rval = -ENOMEM;
		goto done;
	}
	proc_spin_test->read_proc = ipc_spin_proc_lock;
#endif

#endif
	rval = ipc_smem_init();	/* Setup shared memory */
	if (rval < 0)
		goto done;

	ipc_irq_init();		/* Setup IRQs */
	ipc_binder_init();	/* Setup the binder */
	ipc_bh_init();		/* Setup the bottom half */
	rval = aipc_nl_init();	/* Setup the chardev for user-space */
	if (rval < 0)
		goto done;

done:
	if (rval < 0) {
#if defined(CONFIG_PROC_FS)
#if IPC_SPINLOCK_TEST
		if (proc_spin_test) {
			remove_proc_entry("spin_test", proc_aipc);
			proc_spin_test = NULL;
		}
#endif
#if IPC_MUTEX_TEST
		if (proc_mutex) {
			remove_proc_entry("mutex", proc_aipc);
			proc_mutex = NULL;
		}
#endif
		if (proc_lu_prog_id) {
			remove_proc_entry("lu_prog_id", proc_aipc);
			proc_lu_prog_id = NULL;
		}

		if (proc_proginfo) {
			remove_proc_entry("proginfo", proc_aipc);
			proc_proginfo = NULL;
		}

		if (proc_aipc) {
			remove_proc_entry("aipc", NULL);
			proc_aipc = NULL;
		}
#endif
		ipc_binder_cleanup();
		ipc_bh_cleanup();
		ipc_irq_free();
	}
	else
		ipc_status_report();

	return rval;
}

static void __exit ambarella_ipc_cleanup(void)
{
	printk (KERN_NOTICE "[ipc] ambarella_ipc_cleanup\n");

#if defined(CONFIG_PROC_FS)
#if IPC_SPINLOCK_TEST
	if (proc_spin_test) {
		remove_proc_entry("spin_test", proc_aipc);
		proc_spin_test = NULL;
	}
#endif
#if IPC_MUTEX_TEST
	if (proc_mutex) {
		remove_proc_entry("mutex", proc_aipc);
		proc_mutex = NULL;
	}
#endif
	if (proc_lu_prog_id) {
		remove_proc_entry("lu_prog_id", proc_aipc);
		proc_lu_prog_id = NULL;
	}

	if (proc_proginfo) {
		remove_proc_entry("proginfo", proc_aipc);
		proc_proginfo = NULL;
	}

	if (proc_aipc) {
		remove_proc_entry("aipc", NULL);
		proc_aipc = NULL;
	}
#endif

	i_status_cleanup();
	ipc_binder_cleanup();
	ipc_bh_cleanup();
	ipc_irq_free();
	aipc_nl_cleanup();
}

subsys_initcall(ambarella_ipc_init);
module_exit(ambarella_ipc_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Charles Chiou <cchiou@ambarella.com>");
MODULE_DESCRIPTION("Ambarella IPC");
