/*
 * drivers/ambarella-ipc/client/i_util_client.c
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
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>

#include <linux/aipc/aipc.h>
#include <linux/aipc/i_util.h>
#if !defined(__DEPENDENCY_GENERATION__)
#include "ipcgen/i_util.h"
#define __IPC_CLIENT_IMPL__
#include "ipcgen/i_util_tab.i"
#endif

static CLIENT *IPC_i_util = NULL;

static struct ipc_prog_s i_util_prog =
{
	.name = "i_util",
	.prog = I_UTIL_PROG,
	.vers = I_UTIL_VERS,
	.table = (struct ipcgen_table *) i_util_prog_1_table,
	.nproc = i_util_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};

/*
 * IPC: i_util.gettimeofday().
 */
int ipc_gettimeofday(struct timeval *timeval)
{
	enum clnt_stat stat;

	stat = itron_gettimeofday_1(NULL, timeval, IPC_i_util);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return 0;
}
EXPORT_SYMBOL(ipc_gettimeofday);

/*
 * IPC: i_util.printk().
 */
int ipc_printk(const char *s)
{
	enum clnt_stat stat;

	ambcache_clean_range ((void *) s, strlen (s) + 1);

	stat = itron_printk_1((char **) &s, NULL, IPC_i_util);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return 0;
}
EXPORT_SYMBOL(ipc_printk);

/*
 * IPC: i_util.fixed_printk().
 */
int ipc_fixed_printk(struct fixed_charbuf *charbuf)
{
	enum clnt_stat stat;

	stat = itron_fixed_printk_1(charbuf, NULL, IPC_i_util);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return 0;
}
EXPORT_SYMBOL(ipc_fixed_printk);

/*
 * IPC: i_util.async_ipc().
 */
void ipc_async_ipc_completion(void *arg, struct SVCXPRT *svcxprt)
{
	char *msg = (char *) arg;

	printk("ipc_async_ipc_completion: %08x %s\n", (unsigned int) svcxprt, msg);
}

void ipc_async_ipc(int svc_dly, int clnt_dly, int clnt_tmo, int clnt_retry)
{
	static int _svc_dly;
	static char *compl_msg = "I'm ipc_async_ipc.";
	enum clnt_stat stat;
	struct clnt_req req;
	int i;

	printk("ipc_async_ipc(%d, %d, %d, %d)\n", svc_dly, clnt_dly, clnt_tmo, clnt_retry);

	_svc_dly = svc_dly;

	memset(&req, 0, sizeof(req));
	req.compl_proc = ipc_async_ipc_completion;
	req.compl_arg = compl_msg;

	stat = itron_async_ipc_1(&_svc_dly, NULL, IPC_i_util, &req);
	printk("1. itron_async_ipc_1: %d (%s)\n", stat, ipc_strerror(stat));

	for (i = 0; i < clnt_retry; i++)
	{
		mdelay(clnt_dly);

		stat = ipc_clnt_wait_for_completion(IPC_i_util, &req, clnt_tmo);
		printk("2. ipc_clnt_wait_for_completion %d: %d (%s)\n", i, stat,
			ipc_strerror(stat));
		if (stat != IPC_TIMEDOUT) {
			break;
		}
	}
}
EXPORT_SYMBOL(ipc_async_ipc);

/*
 * IPC: i_util.itron_pm_suspend().
 */
int ipc_pm_suspend(void)
{
	enum clnt_stat stat;

	stat = itron_pm_suspend_1(NULL, NULL, IPC_i_util);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return 0;
}
EXPORT_SYMBOL(ipc_pm_suspend);

/*
 * IPC: i_util.itron_pm_resume().
 */
int ipc_pm_resume(void)
{
	enum clnt_stat stat;

	stat = itron_pm_resume_1(NULL, NULL, IPC_i_util);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return 0;
}
EXPORT_SYMBOL(ipc_pm_resume);

/*
 * IPC: i_util.lk_report_ready().
 */
int ipc_report_ready(void)
{
	enum clnt_stat stat;

	stat = lk_report_ready_1(NULL, NULL, IPC_i_util);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return 0;
}
EXPORT_SYMBOL(ipc_report_ready);

/*
 * IPC: i_util.lk_report_resume().
 */
int ipc_report_resume(void)
{
	enum clnt_stat stat;

	stat = lk_report_resume_1(NULL, NULL, IPC_i_util);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return 0;
}
EXPORT_SYMBOL(ipc_report_resume);

/*
 * IPC: i_util.lk_request_suspend().
 */
int ipc_request_suspend_me(void)
{
	enum clnt_stat stat;

	stat = lk_request_suspend_1(NULL, NULL, IPC_i_util);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return 0;
}
EXPORT_SYMBOL(ipc_request_suspend_me);

/*
 * IPC: i_util.lk_request_shutdown().
 */
int ipc_request_shutdown_me(void)
{
	enum clnt_stat stat;

	stat = lk_request_shutdown_1(NULL, NULL, IPC_i_util);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return 0;
}
EXPORT_SYMBOL(ipc_request_shutdown_me);

/*
 * IPC: i_util.lk_get_fb().
 */
int ipc_get_exfb(void **mem, unsigned int *size)
{
	enum clnt_stat stat;
	struct lk_exfb exfb;

	stat = lk_get_exfb_1(NULL, &exfb, IPC_i_util);
	if (stat != IPC_SUCCESS) {
		pr_err("%s: ipc error: %d (%s) (%p)\n",__FUNCTION__, stat, ipc_strerror(stat), IPC_i_util);
		return -stat;
	}

	*mem = exfb.mem;
	*size = exfb.size;

	return 0;
}
EXPORT_SYMBOL(ipc_get_exfb);

/*
 * IPC: i_util.lk_report_fb_owned().
 */
int ipc_report_fb_owned(void)
{
	enum clnt_stat stat;

	stat = lk_report_fb_owned_1(NULL, NULL, IPC_i_util);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return 0;
}
EXPORT_SYMBOL(ipc_report_fb_owned);

/*
 * IPC: i_util.lk_report_fb_released().
 */
int ipc_report_fb_released(void)
{
	enum clnt_stat stat;

	stat = lk_report_fb_released_1(NULL, NULL, IPC_i_util);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return 0;
}
EXPORT_SYMBOL(ipc_report_fb_released);

/*
 * IPC: i_util.lk_absuspend_prepare().
 */
int linux_absuspend_check(void *pinfo)
{
	enum clnt_stat stat;

	stat = lk_absuspend_check_1(NULL,
		(struct ambernation_check_info *)pinfo, IPC_i_util);
	if (stat != IPC_SUCCESS) {
		pr_err("%s: ipc error: %d (%s)\n",
			__func__, stat, ipc_strerror(stat));
		return -stat;
	}

	return 0;
}
EXPORT_SYMBOL(linux_absuspend_check);

/*
 * IPC: i_util.lk_absuspend_enter().
 */
int linux_absuspend_enter(void)
{
	enum clnt_stat stat;
	struct clnt_req req;

	stat = lk_absuspend_enter_1(NULL, NULL, IPC_i_util, &req);
	printk("%s: %d (%s)\n", __func__, stat, ipc_strerror(stat));

	return 0;
}
EXPORT_SYMBOL(linux_absuspend_enter);

/*
 * IPC: i_util.lk_absuspend_exit().
 */
int linux_absuspend_exit(void)
{
	enum clnt_stat stat;

	stat = lk_absuspend_exit_1(NULL, NULL, IPC_i_util);
	if (stat != IPC_SUCCESS) {
		pr_err("%s: ipc error: %d (%s)\n",
			__func__, stat, ipc_strerror(stat));
		return -stat;
	}

	return 0;
}
EXPORT_SYMBOL(linux_absuspend_exit);

int ipc_set_device_owner(int device, int owner)
{
	enum clnt_stat stat;
	struct device_owner dev_owner;

	dev_owner.device = device;
	dev_owner.owner = owner;

	stat = lk_set_device_owner_1(&dev_owner, NULL, IPC_i_util);
	if (stat != IPC_SUCCESS) {
		pr_err("%s: ipc error: %d (%s)\n",
			__func__, stat, ipc_strerror(stat));
		return -stat;
	}

	return 0;
}
EXPORT_SYMBOL(ipc_set_device_owner);

#if defined(CONFIG_PROC_FS)

extern struct proc_dir_entry *proc_aipc;

struct proc_dir_entry *proc_gettimeofday = NULL;
struct proc_dir_entry *proc_printk = NULL;
struct proc_dir_entry *proc_fixed_printk = NULL;
struct proc_dir_entry *proc_async_ipc = NULL;
struct proc_dir_entry *proc_ambernation = NULL;

static int i_util_proc_fs_gettimeofday(char *page, char **start, off_t off,
				       int count, int *eof, void *data)
{
	int rval;
	int len = 0;
	struct timeval timeval;

	if (off != 0)
		return 0;

	rval = ipc_gettimeofday(&timeval);
	if (rval < 0)
		return -EFAULT;

	len += snprintf(page + len, PAGE_SIZE - len,
			"%lu.%.6lu\n",
			timeval.tv_sec, timeval.tv_usec);
	*eof = 1;

	return len;
}

static int i_util_proc_fs_printk(struct file *file, const char __user *buffer,
				 unsigned long count, void *data)
{
	int rval;
	char *buf;

	buf = kmalloc(GFP_KERNEL, count);
	if (buf == NULL)
		return -ENOMEM;

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	rval = ipc_printk(buf);
	kfree(buf);

	if (rval < 0)
		return -EFAULT;
	return count;
}

static int i_util_proc_fs_fixed_printk(struct file *file,
				       const char __user *buffer,
				       unsigned long count, void *data)
{
	int rval;
	struct fixed_charbuf charbuf;

	if (count > sizeof(charbuf.buf))
		count = sizeof(charbuf.buf);

	memset(&charbuf, 0x0, sizeof(charbuf));
	if (copy_from_user(charbuf.buf, buffer, count))
		return -EFAULT;

	rval = ipc_fixed_printk(&charbuf);
	if (rval < 0)
		return -EFAULT;

	return count;
}

static int i_util_proc_fs_async_ipc(struct file *file,
				       const char __user *buffer,
				       unsigned long count, void *data)
{
	int argc;
	char buf[512];
	int svc_dly, clnt_dly, clnt_tmo, clnt_retry;

	if (count > sizeof(buf))
		count = sizeof(buf);

	memset(buf, 0x0, sizeof(buf));
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	argc = sscanf(buf, "%d, %d, %d, %d", &svc_dly, &clnt_dly, &clnt_tmo, &clnt_retry);
	if (argc < 1) {
		svc_dly = 1000;
	}
	if (argc < 2) {
		clnt_dly = 100;
	}
	if (argc < 3) {
		clnt_tmo = 1000;
	}
	if (argc < 4) {
		clnt_retry = 10;
	}

	ipc_async_ipc(svc_dly, clnt_dly, clnt_tmo, clnt_retry);

	return count;
}

static int i_util_proc_fs_ambernation(struct file *file,
				       const char __user *buffer,
				       unsigned long count, void *data)
{
	char cmd[32];
	struct ambernation_check_info check_info;

	if (count > 32)
		count = 32;

	memset(cmd, 0, sizeof(cmd));

	if (copy_from_user(cmd, buffer, count)) {
		return -EFAULT;
	}

	if (strncmp(cmd, "suspend", 7) == 0) {
		linux_absuspend_enter();
	} else if (strncmp(cmd, "resume", 6) == 0) {
		linux_absuspend_exit();
	} else if (strncmp(cmd, "check", 5) == 0) {
		linux_absuspend_check((void *)&check_info);
		printk("aoss = 0x%p\n", check_info.aoss_info);
		printk("fn_pri[0] = 0x%08x\n", check_info.aoss_info->fn_pri[0]);
		printk("fn_pri[1] = 0x%08x\n", check_info.aoss_info->fn_pri[1]);
		printk("magic = 0x%08x\n", check_info.aoss_info->magic);
		printk("total_pages = 0x%08x\n",
			check_info.aoss_info->total_pages);
		printk("copy_pages = 0x%08x\n",
			check_info.aoss_info->copy_pages);
		printk("page_info = 0x%p\n", check_info.aoss_info->page_info);
	} else {
		printk("ambernation: unknow command = %s\n", cmd);
	}

	return count;
}

#endif

/*
 * Client initialization.
 */
static int __init i_util_init(void)
{
	IPC_i_util = ipc_clnt_prog_register(&i_util_prog);
	if (IPC_i_util == NULL)
		return -EPERM;

#if defined(CONFIG_PROC_FS)
	proc_gettimeofday = create_proc_entry("gettimeofday", 0, proc_aipc);
	if (proc_gettimeofday) {
		proc_gettimeofday->data = NULL;
		proc_gettimeofday->read_proc = i_util_proc_fs_gettimeofday;
		proc_gettimeofday->write_proc = NULL;
	}

	proc_printk = create_proc_entry("printk", 0, proc_aipc);
	if (proc_printk) {
		proc_printk->data = NULL;
		proc_printk->read_proc = NULL;
		proc_printk->write_proc = i_util_proc_fs_printk;
	}

	proc_fixed_printk = create_proc_entry("fixed_printk", 0, proc_aipc);
	if (proc_fixed_printk) {
		proc_fixed_printk->data = NULL;
		proc_fixed_printk->read_proc = NULL;
		proc_fixed_printk->write_proc = i_util_proc_fs_fixed_printk;
	}
	proc_async_ipc = create_proc_entry("async_ipc", 0, proc_aipc);
	if (proc_async_ipc) {
		proc_async_ipc->data = NULL;
		proc_async_ipc->read_proc = NULL;
		proc_async_ipc->write_proc = i_util_proc_fs_async_ipc;
	}
	proc_ambernation = create_proc_entry("ambernation", 0, proc_aipc);
	if (proc_ambernation) {
		proc_ambernation->data = NULL;
		proc_ambernation->read_proc = NULL;
		proc_ambernation->write_proc = i_util_proc_fs_ambernation;
	}
#endif

	return 0;
}

/*
 * Client exit.
 */
static void __exit i_util_cleanup(void)
{
#if defined(CONFIG_PROC_FS)
	if (proc_gettimeofday) {
		remove_proc_entry("gettimeofday", proc_aipc);
		proc_gettimeofday = NULL;
	}
	if (proc_printk) {
		remove_proc_entry("printk", proc_aipc);
		proc_printk = NULL;
	}
	if (proc_fixed_printk) {
		remove_proc_entry("fixed_printk", proc_aipc);
		proc_fixed_printk = NULL;
	}
	if (proc_async_ipc) {
		remove_proc_entry("async_ipc", proc_aipc);
		proc_async_ipc = NULL;
	}
	if (proc_ambernation) {
		remove_proc_entry("ambernation", proc_aipc);
		proc_ambernation = NULL;
	}
#endif
	if (IPC_i_util) {
		ipc_clnt_prog_unregister(&i_util_prog, IPC_i_util);
		IPC_i_util = NULL;
	}
}

subsys_initcall_sync(i_util_init);
module_exit(i_util_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Charles Chiou <cchiou@ambarella.com>");
MODULE_DESCRIPTION("Ambarella IPC: i_util.x");
