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
#include <linux/seq_file.h>

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

#define PROC_SEQ_FILE_DEFINE(name) \
static void *seq_start_##name(struct seq_file *s, loff_t *pos); \
static void *seq_next_##name(struct seq_file *s, void *v, loff_t *pos); \
static void seq_stop_##name(struct seq_file *s, void *v); \
static int seq_show_##name(struct seq_file *s, void *v); \
\
static struct seq_operations proc_seq_ops_##name = { \
    .start = seq_start_##name, \
    .next  = seq_next_##name, \
    .stop  = seq_stop_##name, \
    .show  = seq_show_##name \
}; \
\
static int proc_open_##name(struct inode *inode, struct file *file) \
{ \
    return seq_open(file, &proc_seq_ops_##name); \
} \
\
static struct file_operations proc_ops_##name = { \
    .owner   = THIS_MODULE, \
    .open    = proc_open_##name, \
    .read    = seq_read, \
    .llseek  = seq_lseek, \
    .release = seq_release \
}; \
\
struct proc_dir_entry *proc_##name = NULL;

struct proc_dir_entry *proc_aipc = NULL;
EXPORT_SYMBOL(proc_aipc);
struct proc_dir_entry *proc_lu_prog_id = NULL;

/* procfs: proginfo */
PROC_SEQ_FILE_DEFINE(proginfo)

#if IPC_SPINLOCK_TEST
struct proc_dir_entry *proc_spin_test = NULL;
#endif

#if IPC_MUTEX_TEST
struct proc_dir_entry *proc_mutex = NULL;
static int G_mutex_count[256];
static unsigned int *G_mutex_shm_count = NULL;
#endif

static int print_ipcgen_table(struct seq_file *s,
			      struct ipcgen_table *table, int nproc)
{
	int i, len = 0;

	for (i = 1; i <= nproc; i++) {
		len += seq_printf(s, "0x%.8x %4d %4d %4d %4d %4d %4d   %s\n",
			       (unsigned int)table[i].proc,
			       table[i].fid,
			       table[i].len_arg,
			       table[i].arg_ptbl_num,
			       table[i].len_res,
			       table[i].res_ptbl_num,
			       table[i].timeout,
			       table[i].name);
	}

	return len;
}

static int print_lu_procinfo(struct seq_file *s,
			      struct aipc_nl_proc_info *proc_info,
			      int nproc)
{
	int i, len = 0;

	for (i = 0; i < nproc; i++) {
		len += seq_printf(s, "0x%.8x %4d %4d %4d   %s\n",
			       (unsigned int)proc_info->proc,
			       proc_info->fid,
			       proc_info->len_arg,
			       proc_info->len_res,
			       proc_info->name);
		proc_info++;
	}

	return len;
}

/*
 * seq_start() for /proc/aipc/proginfo
 */
static void *seq_start_proginfo(struct seq_file *s, loff_t *pos)
{
	uint32_t *seq;

	if (*pos > 0) {
		return NULL;
	}

	seq = kzalloc(sizeof(uint32_t), GFP_KERNEL);
	if (!seq) {
		return NULL;
	}

	*seq = *pos + 1;

	return seq;
}

/*
 * seq_next() for /proc/aipc/proginfo
 */
static void *seq_next_proginfo(struct seq_file *s, void *v, loff_t *pos)
{
	uint32_t *seq = v;

	*pos = ++(*seq);
	if (*pos > 0) {
		return NULL;
	}
	return seq;
}

/*
 * seq_stop() for /proc/aipc/proginfo
 */
static void seq_stop_proginfo(struct seq_file *s, void *v)
{
	kfree(v);
}

/*
 * seq_show() for /proc/aipc/proginfo
 */
static int seq_show_proginfo(struct seq_file *s, void *v)
{
	struct ipc_prog_s *prog;
	struct aipc_nl_prog *nl_prog;

	seq_printf(s, "=======================\n");
	seq_printf(s, "Registered IPC programs\n");
	seq_printf(s, "servers: ");
	for (prog = ipc_svc_progs(); prog != NULL; prog = prog->next) {
		seq_printf(s, "%s ", prog->name);
	}
	seq_printf(s, "\n");
	seq_printf(s, "clients: ");
	for (prog = ipc_clnt_progs(); prog != NULL; prog = prog->next) {
		seq_printf(s, "%s ", prog->name);
	}
	seq_printf(s, "\n");
	seq_printf(s, "=======================\n");
	seq_printf(s, "\n\n");

	for (prog = ipc_svc_progs(); prog != NULL; prog = prog->next) {
		seq_printf(s, "%s (S) P:0x%.8x, V:%d N:%d\n",
				prog->name, prog->prog, prog->vers, prog->nproc);
		print_ipcgen_table(s, prog->table, prog->nproc);
		seq_printf(s, "\n");
	}

	for (prog = ipc_clnt_progs(); prog != NULL; prog = prog->next) {
		seq_printf(s, "%s (C) P:0x%.8x, V:%d N:%d\n",
				prog->name, prog->prog, prog->vers, prog->nproc);
		print_ipcgen_table(s, prog->table, prog->nproc);
		seq_printf(s, "\n");
	}
	seq_printf(s, "=======================\n");
	seq_printf(s, "Registered IPC programs in user-space\n");
	list_for_each_entry(nl_prog, &binder->lu_prog_list, list) {
		seq_printf(s, "%s (S) P:0x%.8x, V:%d N:%d\n",
				nl_prog->prog_info.name,
				nl_prog->prog_info.pid,
				nl_prog->prog_info.vers,
				nl_prog->prog_info.nproc);
		print_lu_procinfo(s, nl_prog->proc_info, nl_prog->prog_info.nproc);
		seq_printf(s, "\n");
	}
	seq_printf(s, "\n");

	return 0;
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
        proc_proginfo->proc_fops = &proc_ops_proginfo;

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
	ipc_mutex_init();
	ipc_irq_init();		/* Setup IRQs */
	ipc_binder_init();	/* Setup the binder */
	ipc_bh_init();		/* Setup the bottom half */
	rval = aipc_nl_init();	/* Setup the chardev for user-space */
	if (rval < 0)
		goto done;

done:
	if (rval >= 0)
	{
		ipc_status_report();
	}
	else
	{
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

	ipc_status_report_ready(0);
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
