/*
 * drivers/ambarella-ipc/server/lk_util_server.c
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

#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/time.h>
#include <linux/module.h>
#include <linux/delay.h>

#include <linux/aipc/aipc.h>
#include <linux/aipc/lk_util.h>
#if !defined(__DEPENDENCY_GENERATION__)
#include "ipcgen/lk_util.h"
#define __IPC_SERVER_IMPL__
#include "ipcgen/lk_util_tab.i"
#endif

static struct ipc_prog_s lk_util_prog =
{
	.name = "lk_util",
	.prog = LK_UTIL_PROG,
	.vers = LK_UTIL_VERS,
	.table =  (struct ipcgen_table *) &lk_util_prog_1_table,
	.nproc = lk_util_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};

/* linux_gettimeofday() */

bool_t linux_gettimeofday_1_svc(void *arg, struct timeval *res,
				struct svc_req *rqstp)
{
	do_gettimeofday(res);

	rqstp->svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(rqstp->svcxprt, NULL);

	return 1;
}

/* linux_printk() */

bool_t linux_printk_1_svc(char **arg, void *res, struct svc_req *rqstp)
{
	printk(*arg);

	rqstp->svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(rqstp->svcxprt, NULL);

	return 1;
}

/* linux_async_ipc() */
static bool_t __linux_async_ipc_1_svc(unsigned int *arg, vptr *res,
				    SVCXPRT *svcxprt)
{
	printk("async_ipc: %08x\n", *arg);
	mdelay(*arg);
	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t linux_async_ipc_1_svc(unsigned int *arg, void *res,
			   struct svc_req *rqstp)
{
	ipc_bh_queue((ipc_bh_f) __linux_async_ipc_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/* linux_kmalloc() */

static bool_t __linux_kmalloc_1_svc(unsigned int *arg, vptr *res,
				    SVCXPRT *svcxprt)
{
	*res = kmalloc(*arg, GFP_KERNEL);
	if (*res != NULL)
		*res = __virt_to_phys(*res);
	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t linux_kmalloc_1_svc(unsigned int *arg, vptr *res,
			   struct svc_req *rqstp)
{
	ipc_bh_queue((ipc_bh_f) __linux_kmalloc_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/* linux_kfree() */

static bool_t __linux_kfree_1_svc(vptr *arg, void *res, SVCXPRT *svcxprt)
{
	vptr ptr = *arg;

	if (ptr != NULL)
		ptr = __phys_to_virt(ptr);
	kfree(ptr);

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t linux_kfree_1_svc(vptr *arg, void *res, struct svc_req *rqstp)
{
	ipc_bh_queue((ipc_bh_f) __linux_kfree_1_svc, arg, res, rqstp->svcxprt);
	return 1;
}

/* linux_vmalloc() */

static bool_t __linux_vmalloc_1_svc(unsigned int *arg, vptr *res,
				    SVCXPRT *svcxprt)
{
	*res = vmalloc(*arg);
	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t linux_vmalloc_1_svc(unsigned int *arg, vptr *res,
			   struct svc_req *rqstp)
{
	ipc_bh_queue((ipc_bh_f) __linux_vmalloc_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/* linux_vfree() */

static bool_t __linux_vfree_1_svc(vptr *arg, void *res, SVCXPRT *svcxprt)
{

	vfree(*arg);
	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t linux_vfree_1_svc(vptr *arg, void *res, struct svc_req *rqstp)
{
	ipc_bh_queue((ipc_bh_f) __linux_vfree_1_svc, arg, res, rqstp->svcxprt);
	return 1;
}

/* linux_pm_suspend() */

static bool_t __linux_pm_suspend_1_svc(void *arg, void *res, SVCXPRT *svcxprt)
{
	/* Not yet implemented! */

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t linux_pm_suspend_1_svc(void *arg, void *res, struct svc_req *rqstp)
{
	ipc_bh_queue((ipc_bh_f) __linux_pm_suspend_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/* linux_pm_resume() */

static bool_t __linux_pm_resume_1_svc(void *arg, void *res, SVCXPRT *svcxprt)
{
	/* Not yet implemented! */

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t linux_pm_resume_1_svc(void *arg, void *res, struct svc_req *rqstp)
{
	ipc_bh_queue((ipc_bh_f) __linux_pm_resume_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/* linux_sync() */

static bool_t __linux_sync_1_svc(void *arg, void *res, SVCXPRT *svcxprt)
{
	/* Not yet implemented! */

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t linux_sync_1_svc(void *arg, void *res, struct svc_req *rqstp)
{
	ipc_bh_queue((ipc_bh_f) __linux_sync_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

struct fbreq_handler fbreq_handler =
{
	.takeover_req = NULL,
	.release_req = NULL,
};
EXPORT_SYMBOL(fbreq_handler);

/* linux_takeover_fb() */

static bool_t __linux_takeover_fb_1_svc(struct osd_fb_arg *arg, int *res, SVCXPRT *svcxprt)
{
	if (fbreq_handler.takeover_req)
		*res = fbreq_handler.takeover_req(arg->id);
	else
		*res = -1;

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t linux_takeover_fb_1_svc(struct osd_fb_arg *arg, int *res, struct svc_req *rqstp)
{
	ipc_bh_queue((ipc_bh_f) __linux_takeover_fb_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/* linux_release_fb() */

static bool_t __linux_release_fb_1_svc(struct osd_fb_arg *arg, int *res, SVCXPRT *svcxprt)
{
	if (fbreq_handler.release_req)
		*res = fbreq_handler.release_req(arg->id);
	else
		*res = -1;

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t linux_release_fb_1_svc(struct osd_fb_arg *arg, int *res, struct svc_req *rqstp)
{
	ipc_bh_queue((ipc_bh_f) __linux_release_fb_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

extern int ambarella_vi_send_key(int key, int pressed);
bool_t linux_send_key_1_svc(struct linux_key_status *arg, int *res, struct svc_req *rqstp)
{
	*res = ambarella_vi_send_key(arg->key, arg->pressed);
	
	rqstp->svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(rqstp->svcxprt, NULL);

	return 1;
}

/* linux_get_ipcstat() */
static bool_t __linux_get_ipcstat_1_svc(void *arg, struct linux_ipcstat_res *res, SVCXPRT *svcxprt)
{
	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t linux_get_ipcstat_1_svc(void *arg, struct linux_ipcstat_res *res, struct svc_req *rqstp)
{
	ipc_bh_queue((ipc_bh_f) __linux_get_ipcstat_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

static bool_t __linux_usb_gadget_enable_1_svc(int *arg, int *res, SVCXPRT *svcxprt)
{
#ifdef CONFIG_USB_AMBARELLA
	int reval;
	reval = ambarella_udc_connect((*arg & 1));

	if(reval < 0)
		svcxprt->rcode = IPC_PROGUNAVAIL;
	else
		svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);
#else
	svcxprt->rcode = IPC_PROGUNAVAIL;
	ipc_svc_sendreply(svcxprt, NULL);
#endif

	return 1;
}

bool_t linux_usb_gadget_enable_1_svc(u_int *arg, void *res, struct svc_req *rqstp)
{
	ipc_bh_queue((ipc_bh_f) __linux_usb_gadget_enable_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/* ------------------------------------------------------------------------- */

/*
 * Service initialization.
 */
static int __init lk_util_init(void)
{
	return ipc_svc_prog_register(&lk_util_prog);
}

/*
 * Service removal.
 */
static void __exit lk_util_cleanup(void)
{
	ipc_svc_prog_unregister(&lk_util_prog);
}


module_init(lk_util_init);
module_exit(lk_util_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Charles Chiou <cchiou@ambarella.com>");
MODULE_DESCRIPTION("Ambarella IPC: lk_util.x");
