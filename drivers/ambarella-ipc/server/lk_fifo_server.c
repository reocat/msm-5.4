/*
 * drivers/ambarella-ipc/server/lk_fifo_server.c
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

#include <linux/aipc/aipc.h>
#define __VFIFO_IMPL__
#include <linux/aipc/vfifo.h>
#if !defined(__DEPENDENCY_GENERATION__)
#include "ipcgen/lk_fifo.h"
#define __IPC_SERVER_IMPL__
#include "ipcgen/lk_fifo_tab.i"
#endif
#undef __IPC_SERVER_IMPL__
#include "ipcgen/i_fifo.h"

extern CLIENT *IPC_i_fifo;

static struct vfifo lvfifo[MAX_VFIFO_OBJECTS];
static int lvfifo_count = 0;

/*
 * Maximum number of local vfifos.
 */
int local_vfifo_max(void)
{
	return MAX_VFIFO_OBJECTS;
}
EXPORT_SYMBOL(local_vfifo_max);

/*
 * Get the number of registered local vfifos.
 */
int local_vfifo_num(void)
{
	return lvfifo_count;
}
EXPORT_SYMBOL(local_vfifo_num);

/*
 * Search local vfifo by name.
 */
int local_vfifo_index_of(const char *name)
{
	int i;

	if (name == NULL)
		return -EINVAL;

	for (i = 0; i < MAX_VFIFO_OBJECTS; i++) {
		if (lvfifo[i].valid == 0)
			continue;

		if (strcmp(lvfifo[i].info.name, name) == 0)
			return i;
	}

	return -ENOENT;
}
EXPORT_SYMBOL(local_vfifo_index_of);

/*
 * Get the info of a local vfifo.
 */
const struct vfifo_info *local_vfifo_get_info(int index)
{
	if (index < 0 || index >= MAX_VFIFO_OBJECTS)
		return NULL;

	return &lvfifo[index].info;
}
EXPORT_SYMBOL(local_vfifo_get_info);

/*
 * Get the stat of a local vfifo.
 */
const struct vfifo_stat *local_vfifo_get_stat(int index)
{
	if (index < 0 || index >= MAX_VFIFO_OBJECTS)
		return NULL;

	return &lvfifo[index].stat;
}
EXPORT_SYMBOL(local_vfifo_get_stat);

/*
 * Register a new vfifo to the system.
 */
int local_vfifo_register(const char *name,
			 const char *description,
			 unsigned int base,
			 unsigned int size,
			 unsigned int waddr)
{
	int i;
	unsigned long flags;

	if (name == NULL || name[0] == '\0')
		return -EINVAL;
	if (size == 0)
		return -2;
	if (waddr < base || waddr > (base + size))
		return -EINVAL;
	if (lvfifo_count >= MAX_VFIFO_OBJECTS)
		return -EINVAL;

	local_irq_save(flags);

	for (i = 0; i < MAX_VFIFO_OBJECTS; i++) {
		if (lvfifo[i].valid == 0)
			break;
	}

	if (i >= MAX_VFIFO_OBJECTS) {
		local_irq_restore(flags);
		return -ENOENT;
	}

	lvfifo_count++;
	lvfifo[i].valid = 1;
	strncpy(lvfifo[i].info.name, name, sizeof(lvfifo[i].info.name));
	if (description == NULL || description[0] == '\0')
		lvfifo[i].info.description[0] = '\0';
	else
		strncpy(lvfifo[i].info.description, description,
			sizeof(lvfifo[i].info.description));
	lvfifo[i].info.base = base;
	lvfifo[i].info.size = size;
	lvfifo[i].stat.waddr = waddr;

	local_irq_restore(flags);

	return i;
}
EXPORT_SYMBOL(local_vfifo_register);

/*
 * Unregister a vfifo from system.
 */
int local_vfifo_unregister(int index)
{
	int rval = 0;
	unsigned long flags;

	if (index < 0 || index >= MAX_VFIFO_OBJECTS)
		return -EINVAL;

	local_irq_save(flags);

	if (lvfifo[index].valid == 0) {
		rval = -EINVAL;
	} else {
		lvfifo[index].valid = 0;
		lvfifo_count--;
	}

	local_irq_restore(flags);

	return rval;
}
EXPORT_SYMBOL(local_vfifo_unregister);

/*
 * Rebase a vfifo.
 */
int local_vfifo_rebase(int index,
		       unsigned int base,
		       unsigned int size,
		       unsigned int waddr)
{
	struct vfifo *vfifo;
	struct vfifo_notify *notify;
	unsigned long flags;
	enum clnt_stat stat;
	struct ififo_event event;

	if (index < 0 || index >= MAX_VFIFO_OBJECTS)
		return -EINVAL;

	/*
	 * Update local data structure
	 */
	local_irq_save(flags);

	vfifo = &lvfifo[index];
	vfifo->info.base = base;
	vfifo->info.size = size;
	vfifo->stat.waddr = waddr;
	vfifo->stat.raddr = waddr;
	notify = &vfifo->notify;

	local_irq_restore(flags);

	/*
	 * Send event to remote
	 */
	if (notify && (notify->evmask & VFIFO_MASK_REBASED)) {
		event.index = index;
		event.event = VFIFO_EVENT_UPDATED;
		event.saddr = 0;
		event.eaddr = 0;

		stat = ififo_remote_event_1(&event, NULL, IPC_i_fifo);
		if (stat != IPC_SUCCESS) {
			printk("ipc error: %d (%s)", stat, ipc_strerror(stat));
			return -EIO;
		}
	}

	return 0;
}
EXPORT_SYMBOL(local_vfifo_rebase);

/*
 * Update state of a vfifo.
 */
int local_vfifo_update(int index, unsigned int waddr)
{
	struct vfifo *vfifo;
	struct vfifo_notify *notify;
	unsigned long flags;
	enum clnt_stat stat;
	struct ififo_event event;
	unsigned int saddr, eaddr;

	if (index < 0 || index >= MAX_VFIFO_OBJECTS)
		return -EINVAL;

	/*
	 * Update local data structure
	 */

	local_irq_save(flags);

	vfifo = &lvfifo[index];
	saddr = vfifo->stat.waddr;
	vfifo->stat.waddr = waddr;
	eaddr = waddr;
	notify = &vfifo->notify;

	local_irq_restore(flags);

	/*
	 * Send event to remote
	 */
	if (notify && (notify->evmask & VFIFO_MASK_UPDATED)) {
		event.index = index;
		event.event = VFIFO_EVENT_UPDATED;
		event.saddr = saddr;
		event.eaddr = eaddr;

		stat = ififo_remote_event_1(&event, NULL, IPC_i_fifo);
		if (stat != IPC_SUCCESS) {
			printk("ipc error: %d (%s)", stat, ipc_strerror(stat));
			return -EIO;
		}
	}

	return 0;
}
EXPORT_SYMBOL(local_vfifo_update);

/*
 * Set 'notify' parameter to send to remote.
 */
int local_vfifo_set_notify(struct vfifo_notify *notify)
{
	int index;
	struct vfifo *vfifo;
	unsigned long flags;

	index = notify->index;
	if (index < 0 || index > MAX_VFIFO_OBJECTS)
		return -EINVAL;

	local_irq_save(flags);

	vfifo = &lvfifo[index];
	memcpy(&vfifo->notify, notify, sizeof(vfifo->notify));

	local_irq_restore(flags);

	return 0;
}
EXPORT_SYMBOL(local_vfifo_set_notify);

/* ------------------------------------------------------------------------- */

#define MAX_VPIPE_HANDLERS	MAX_VFIFO_OBJECTS

struct vpipe_h
{
	char name[8];
	vpipe_handler hdl;
	void *args;
};

static struct vpipe_h vpipe_h[MAX_VPIPE_HANDLERS];

/*
 * Add a vpipe handler.
 */
int vpipe_add_handler(const char *name, vpipe_handler hdl, void *args)
{
	int i;
	unsigned long flags;


	if (name == NULL || hdl == NULL)
		return -1;

	local_irq_save(flags);

	for (i = 0; i < MAX_VPIPE_HANDLERS; i++) {
		if (vpipe_h[i].name[0] == '\0') {
			strncpy(vpipe_h[i].name, name,
				sizeof(vpipe_h[i].name));
			vpipe_h[i].hdl = hdl;
			vpipe_h[i].args = args;
			break;
		}
	}

	local_irq_restore(flags);

	return (i < MAX_VPIPE_HANDLERS) ? i : -1;
}
EXPORT_SYMBOL(vpipe_add_handler);

/*
 * Delete a vpipe handler.
 */
int vpipe_del_handler(const char *name, vpipe_handler hdl)
{
	int i;
	unsigned long flags;


	if (name == NULL || hdl == NULL)
		return -1;

	local_irq_save(flags);

	for (i = 0; i < MAX_VPIPE_HANDLERS; i++) {
		if (strcmp(vpipe_h[i].name, name) == 0 &&
		    vpipe_h[i].hdl == hdl) {
			memset(&vpipe_h[i], 0x0, sizeof(struct vpipe_h));
			break;
		}
	}

	local_irq_restore(flags);

	return (i < MAX_VPIPE_HANDLERS) ? i : -1;
}
EXPORT_SYMBOL(vpipe_del_handler);

/*
 * Create a vpipe: lookup handler and delegate to it.
 */
int vpipe_create(const char *name, int inpipe)
{
	int i;

	if (name == NULL)
		return -1;

	for (i = 0; i < MAX_VPIPE_HANDLERS; i++)
		if (strcmp(vpipe_h[i].name, name) == 0)
			return vpipe_h[i].hdl(VPIPE_REQ_CREATE,
					      inpipe,
					      vpipe_h[i].args);

	return -1;
}
EXPORT_SYMBOL(vpipe_create);

/*
 * Destroy a vpipe: lookup handler and delegate to it.
 */
int vpipe_destroy(const char *name, int inpipe)
{
	int i;

	if (name == NULL)
		return -1;

	for (i = 0; i < MAX_VPIPE_HANDLERS; i++)
		if (strcmp(vpipe_h[i].name, name) == 0)
			return vpipe_h[i].hdl(VPIPE_REQ_DESTROY,
					      inpipe,
					      vpipe_h[i].args);

	return 0;
}
EXPORT_SYMBOL(vpipe_destroy);

/* ------------------------------------------------------------------------- */

#if defined(CONFIG_PROC_FS)

extern struct proc_dir_entry *proc_aipc;

static struct proc_dir_entry *proc_fifo_out = NULL;
static struct proc_dir_entry *proc_fifo_out_stat = NULL;

/*
 * /proc/aipc/fifo-out/stat read function.
 */
static int i_fifo_procfs_stat_read(char *page, char **start, off_t off,
				   int count, int *eof, void *data)
{
	int len = 0;
	const struct vfifo_info *info;
	int i, n, m;

	n = local_vfifo_num();
	m = local_vfifo_max();
	len += snprintf(page + len, PAGE_SIZE - len,
			"servers (%d/%d)\n", n, m);
	for (i = 0; i < m; i++) {
		info = local_vfifo_get_info(i);
		if (info == NULL || info->base == 0x0)
			continue;
		len += snprintf(page + len, PAGE_SIZE - len,
				"%3d %s\t(%s)\n",
				i, info->name, info->description);
	}

	*eof = 1;

	return len;
}

static void lk_fifo_procfs_init(void)
{
	proc_fifo_out = proc_mkdir("fifo-out", proc_aipc);
	if (proc_fifo_out == NULL) {
		pr_err("create fifo-in dir failed!\n");
		return;
	}

	proc_fifo_out_stat = create_proc_entry("stat", 0, proc_fifo_out);
	if (proc_fifo_out_stat) {
		proc_fifo_out_stat->data = NULL;
		proc_fifo_out_stat->read_proc = i_fifo_procfs_stat_read;
		proc_fifo_out_stat->write_proc = NULL;
	}
}

static void lk_fifo_procfs_cleanup(void)
{
	if (proc_fifo_out_stat) {
		remove_proc_entry("stat", proc_fifo_out);
		proc_fifo_out_stat = NULL;
	}

	remove_proc_entry("fifo-out", proc_aipc);
	proc_fifo_out = NULL;
}

#endif

/* ------------------------------------------------------------------------- */

static struct ipc_prog_s lk_fifo_prog =
{
	.name = "lk_fifo",
	.prog = LK_FIFO_PROG,
	.vers = LK_FIFO_VERS,
	.table =  (struct ipcgen_table *) &lk_fifo_prog_1_table,
	.nproc = lk_fifo_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};

/* lkfifo_max() */

bool_t lkfifo_max_1_svc(void *arg, int *res, struct svc_req *rqstp)
{
	*res = local_vfifo_max();

	rqstp->svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(rqstp->svcxprt, NULL);

	return 1;
}

/* lkfifo_num() */

bool_t lkfifo_num_1_svc(void *arg, int *res, struct svc_req *rqstp)
{
	*res = local_vfifo_num();

	rqstp->svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(rqstp->svcxprt, NULL);

	return 1;
}

/* lkfifo_query() */

bool_t lkfifo_query_1_svc(int *arg, struct lkfifo_info *res,
			  struct svc_req *rqstp)
{
	const struct vfifo_info *info;

	info = local_vfifo_get_info(*arg);
	if (info == NULL) {
		memset(res, 0x0, sizeof(*res));
	} else {
		strncpy(res->name, info->name, sizeof(res->name));
		strncpy(res->description, info->description,
			sizeof(res->description));
		res->base = info->base;
		res->size = info->size;
	}

	rqstp->svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(rqstp->svcxprt, NULL);

	return 1;
}

/* lkfifo_set_notify() */

bool_t lkfifo_set_notify_1_svc(struct lkfifo_notify *arg, int *res,
			       struct svc_req *rqstp)
{
	struct vfifo_notify notify;

	notify.index = arg->index;
	notify.evmask = arg->evmask;
	notify.uptype = arg->uptype;
	notify.count = arg->count;
	notify.threshold = arg->threshold;

	*res = local_vfifo_set_notify(&notify);

	rqstp->svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(rqstp->svcxprt, NULL);

	return 1;
}

/* lkfifo_remote_changed() */

static bool_t __lkfifo_remote_changed_1_svc(void *arg, void *res,
					    SVCXPRT *svcxprt)
{
	remote_vfifo_changed();

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t lkfifo_remote_changed_1_svc(void *arg, void *res, struct svc_req *rqstp)
{
	ipc_bh_queue((ipc_bh_f) __lkfifo_remote_changed_1_svc,
		     arg, res, rqstp->svcxprt);

	return 1;
}

/* lkfifo_remote_event() */

static bool_t __lkfifo_remote_event_1_svc(struct lkfifo_event *arg, void *res,
					  SVCXPRT *svcxprt)
{
	remote_vfifo_pushed_event((struct vfifo_event *) arg);

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t lkfifo_remote_event_1_svc(struct lkfifo_event *arg, void *res,
				 struct svc_req *rqstp)
{
	ipc_bh_queue((ipc_bh_f) __lkfifo_remote_event_1_svc,
		     arg, res, rqstp->svcxprt);

	return 1;
}

/* --- lkfifo_create() --- */

bool_t __lkfifo_create_1_svc(struct lkfifo_pipe_arg *arg, int *res,
			     SVCXPRT *svcxprt)
{
	*res = vpipe_create(arg->name, arg->inpipe);
	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t lkfifo_create_1_svc(struct lkfifo_pipe_arg *arg, int *res,
			   struct svc_req *rqstp)
{
	ipc_bh_queue((ipc_bh_f) __lkfifo_create_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/* --- lkfifo_destroy() --- */

bool_t __lkfifo_destroy_1_svc(struct lkfifo_pipe_arg *arg, int *res,
			      SVCXPRT *svcxprt)
{
	*res = vpipe_destroy(arg->name, arg->inpipe);
	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t lkfifo_destroy_1_svc(struct lkfifo_pipe_arg *arg, int *res,
			    struct svc_req *rqstp)
{
	ipc_bh_queue((ipc_bh_f) __lkfifo_destroy_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/*
 * Service initialization.
 */
static int __init lk_fifo_init(void)
{
	int rval;
	int i;

	memset(lvfifo, 0x0, sizeof(lvfifo));
	for (i = 0; i < MAX_VFIFO_OBJECTS; i++) {
		lvfifo[i].stat.notify = &lvfifo[i].notify;
		lvfifo[i].stat.event = &lvfifo[i].event;
		lvfifo[i].index = i;
	}
	lvfifo_count = 0;

	rval = ipc_svc_prog_register(&lk_fifo_prog);

#if defined(CONFIG_PROC_FS)
	lk_fifo_procfs_init();
#endif

	return rval;
}

/*
 * Service removal.
 */
static void __exit lk_fifo_cleanup(void)
{
	ipc_svc_prog_unregister(&lk_fifo_prog);

#if defined(CONFIG_PROC_FS)
	lk_fifo_procfs_cleanup();
#endif
}

module_init(lk_fifo_init);
module_exit(lk_fifo_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Charles Chiou <cchiou@ambarella.com>");
MODULE_DESCRIPTION("Ambarella IPC: lk_fifo.x");
