/*
 * drivers/ambarella-ipc/client/i_fifo_client.c
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
#include "ipcgen/i_fifo.h"
#define __IPC_CLIENT_IMPL__
#include "ipcgen/i_fifo_tab.i"
#endif

CLIENT *IPC_i_fifo = NULL;

static struct vfifo rvfifo[MAX_VFIFO_OBJECTS];
static int rvfifo_count = 0;
static struct vfifo_cb rvfifo_cb[MAX_VFIFO_OBJECTS];

/*
 * Maximum number of remote vfifos.
 */
int remote_vfifo_max(void)
{
	return MAX_VFIFO_OBJECTS;
}
EXPORT_SYMBOL(remote_vfifo_max);

/*
 * Get the number of registered remote vfifos.
 */
int remote_vfifo_num(void)
{
	return rvfifo_count;
}
EXPORT_SYMBOL(remote_vfifo_num);

/*
 * Search remote vfifo by name.
 */
int remote_vfifo_index_of(const char *name)
{
	int i;

	if (name == NULL)
		return -EINVAL;

	for (i = 0; i < MAX_VFIFO_OBJECTS; i++) {
		if (rvfifo[i].valid == 0)
			continue;

		if (strcmp(rvfifo[i].info.name, name) == 0)
			return i;
	}

	return -ENOENT;
}
EXPORT_SYMBOL(remote_vfifo_index_of);

/*
 * Get the info of a remote vfifo.
 */
const struct vfifo_info *remote_vfifo_get_info(int index)
{
	if (index < 0 || index >= MAX_VFIFO_OBJECTS)
		return NULL;

	return &rvfifo[index].info;
}
EXPORT_SYMBOL(remote_vfifo_get_info);

/*
 * Get the stat of a remote vfifo.
 */
const struct vfifo_stat *remote_vfifo_get_stat(int index)
{
	if (index < 0 || index >= MAX_VFIFO_OBJECTS)
		return NULL;

	return &rvfifo[index].stat;
}
EXPORT_SYMBOL(remote_vfifo_get_stat);

/*
 * Subscribe to a remote vfifo producer.
 */
int remote_vfifo_subscribe(int index,
			   struct vfifo_notify *clnt_param,
			   struct vfifo_event *event,
			   vfifo_event_cb cb,
			   void *args)
{
	struct vfifo *vfifo = NULL;
	struct vfifo_cb *scb = NULL;
	struct vfifo_notify *notify = NULL;
	unsigned long flags;
	int rval = 0;
	enum clnt_stat stat;

	if (index < 0 || index >= MAX_VFIFO_OBJECTS)
		return -1;

	vfifo = &rvfifo[index];
	scb = vfifo->cb;

	/*
	 * Update local data structure to set up call back.
	 */
	local_irq_save(flags);

	if (vfifo->info.base == 0 || vfifo->info.size == 0) {
		rval = -1;	/* Invalid vfifo */ 
	} else if (scb->func != NULL) {
		rval = -2;	/* Already subscribed */
	} else if (cb == NULL ||
		   clnt_param == NULL ||
		   clnt_param->index != index ||
		   clnt_param->evmask == 0x0) {
		rval = -3;	/* Invalid parameters */
		   
	} else {
		scb->func = cb;
		scb->args = args;

		notify = &rvfifo->notify;
		notify->index = clnt_param->index;
		notify->evmask = clnt_param->evmask;
		notify->uptype = clnt_param->uptype;
		notify->count = clnt_param->count;
		notify->threshold = clnt_param->threshold;
	}

	local_irq_restore(flags);

	if (rval != 0)
		goto done;

	/*
	 * Update to the server and setup notification parameters.
	 */
	stat = ififo_set_notify_1((struct ififo_notify *) notify, &rval,
				  IPC_i_fifo);
	if (stat != IPC_SUCCESS) {
		rval = -1;
	}


	/* On any IPC or server-side failures, roll back the changes. */
	if (rval < 0) {
		local_irq_save(flags);

		if (scb)
			memset(scb, 0x0, sizeof(*scb));
		if (notify)
			memset(notify, 0x0, sizeof(*notify));

		local_irq_restore(flags);
	}


done:

	return rval;
}
EXPORT_SYMBOL(remote_vfifo_subscribe);

/*
 * Unsubscribe from a remote vfifo producer.
 */
int remote_vfifo_unsubscribe(int index, vfifo_event_cb cb)
{
	struct vfifo *vfifo = NULL;
	struct vfifo_cb *scb = NULL;
	struct vfifo_notify *notify = NULL;
	unsigned long flags;
	int rval = 0;
	enum clnt_stat stat;

	if (index < 0 || index >= MAX_VFIFO_OBJECTS)
		return -1;

	vfifo = &rvfifo[index];
	scb = vfifo->cb;

	/*
	 * Update local data structure.
	 */
	local_irq_save(flags);

	if (vfifo->info.base == 0 || vfifo->info.size == 0) {
		rval = -1;	/* Invalid vfifo */ 
	} else if (scb->func == NULL) {
		rval = -2;	/* Not yet subscribed */
	} else if (cb == NULL ||
		   scb->func != cb) {
		rval = -3;	/* Invalid parameters */
		   
	} else {
		memset(scb, 0x0, sizeof(*scb));
		notify = &rvfifo->notify;
		memset(notify, 0x0, sizeof(*notify));
	}

	local_irq_restore(flags);

	if (rval != 0)
		goto done;

	/*
	 * Update to the server with zero'ed out notification parameters.
	 */
	stat = ififo_set_notify_1((struct ififo_notify *) notify, &rval,
				  IPC_i_fifo);
	if (stat != IPC_SUCCESS) {
		rval = -1;
	}

done:

	return rval;
}
EXPORT_SYMBOL(remote_vfifo_unsubscribe);

/*
 * Update on the vfifo read pointer to remote vfifo.
 */
int remote_vfifo_update(int index, unsigned int raddr)
{
	return -1;
}
EXPORT_SYMBOL(remote_vfifo_update);

/*
 * Remote vfifo changed.
 */
int remote_vfifo_changed(void)
{
	return -1;
}
EXPORT_SYMBOL(remote_vfifo_changed);

/*
 * Remote vfifo pushed an event to us.
 */
int remote_vfifo_pushed_event(struct vfifo_event *event)
{
	return -1;
}
EXPORT_SYMBOL(remote_vfifo_pushed_event);

/* ------------------------------------------------------------------------- */

#if defined(CONFIG_PROC_FS)

extern struct proc_dir_entry *proc_aipc;

static struct proc_dir_entry *proc_fifo_in = NULL;
static struct proc_dir_entry *proc_fifo_in_stat = NULL;

/*
 * /proc/aipc/fifo-in/stat read function.
 */
static int i_fifo_procfs_stat_read(char *page, char **start, off_t off,
				   int count, int *eof, void *data)
{
	int len = 0;
	const struct vfifo_info *info;
	int i, n, m;

	n = remote_vfifo_num();
	m = remote_vfifo_max();
	len += snprintf(page + len, PAGE_SIZE - len,
			"clients (%d/%d)\n", n, m);
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

static void i_fifo_procfs_init(void)
{
	proc_fifo_in = proc_mkdir("fifo-in", proc_aipc);
	if (proc_fifo_in == NULL) {
		pr_err("create fifo-in dir failed!\n");
		return;
	}

	proc_fifo_in_stat = create_proc_entry("stat", 0, proc_fifo_in);
	if (proc_fifo_in_stat) {
		proc_fifo_in_stat->data = NULL;
		proc_fifo_in_stat->read_proc = i_fifo_procfs_stat_read;
		proc_fifo_in_stat->write_proc = NULL;
	}
}

static void i_fifo_procfs_cleanup(void)
{
	if (proc_fifo_in_stat) {
		remove_proc_entry("stat", proc_fifo_in);
		proc_fifo_in_stat = NULL;
	}

	remove_proc_entry("fifo-in", proc_aipc);
	proc_fifo_in = NULL;
}

#endif

/* ------------------------------------------------------------------------- */

static struct ipc_prog_s i_fifo_prog =
{
	.name = "i_fifo",
	.prog = I_FIFO_PROG,
	.vers = I_FIFO_VERS,
	.table = (struct ipcgen_table *) i_fifo_prog_1_table,
	.nproc = i_fifo_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};

/*
 * Client initialization.
 */
static int __init i_fifo_init(void)
{
	int i;

	memset(rvfifo, 0x0, sizeof(rvfifo));
	for (i = 0; i < MAX_VFIFO_OBJECTS; i++) {
		rvfifo[i].stat.notify = &rvfifo[i].notify;
		rvfifo[i].stat.event = &rvfifo[i].event;
		rvfifo[i].index = i;
		rvfifo[i].cb = &rvfifo_cb[i];
		rvfifo[i].cb->func = NULL;
		rvfifo[i].cb->args = NULL;
	}
	rvfifo_count = 0;

	IPC_i_fifo = ipc_clnt_prog_register(&i_fifo_prog);
	if (IPC_i_fifo == NULL)
		return -EPERM;

#if defined(CONFIG_PROC_FS)
	i_fifo_procfs_init();
#endif

	return 0;
}

static void __exit i_fifo_cleanup(void)
{
	if (IPC_i_fifo) {
		ipc_clnt_prog_unregister(&i_fifo_prog, IPC_i_fifo);
		IPC_i_fifo = NULL;
	}

#if defined(CONFIG_PROC_FS)
	i_fifo_procfs_cleanup();
#endif
}

module_init(i_fifo_init);
module_exit(i_fifo_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Charles Chiou <cchiou@ambarella.com>");
MODULE_DESCRIPTION("Ambarella IPC: i_fifo.x");
