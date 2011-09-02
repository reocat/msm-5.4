/*
 * drivers/ambarella-ipc/client/i_gpio_client.c
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

#include <linux/module.h>
#include <linux/list.h>
#include <linux/slab.h>

#include <linux/aipc/aipc.h>
#include <linux/aipc/i_gpio.h>
#if !defined(__DEPENDENCY_GENERATION__)
#include "ipcgen/i_gpio.h"
#define __IPC_CLIENT_IMPL__
#include "ipcgen/i_gpio_tab.i"
#endif

static CLIENT *IPC_i_gpio = NULL;

static struct ipc_prog_s i_gpio_prog =
{
	.name = "i_gpio",
	.prog = I_GPIO_PROG,
	.vers = I_GPIO_VERS,
	.table = (struct ipcgen_table *) i_gpio_prog_1_table,
	.nproc = i_gpio_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};

/*
 * IPC: i_gpio.vgpio_config().
 */
int ipc_gpio_config(int line, int func)
{
	enum clnt_stat stat;
	struct vgpio_config_arg arg;

	arg.line=line;
	arg.func=func;

	stat = vgpio_config_1(&arg, NULL, IPC_i_gpio);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return 0;
}
EXPORT_SYMBOL(ipc_gpio_config);

/*
 * IPC: i_gpio.vgpio_set().
 */
int ipc_gpio_set(int line)
{
	enum clnt_stat stat;

	stat = vgpio_set_1(&line, NULL, IPC_i_gpio);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return 0;
}
EXPORT_SYMBOL(ipc_gpio_set);

/*
 * IPC: i_gpio.vgpio_clr().
 */
int ipc_gpio_clr(int line)
{
	enum clnt_stat stat;

	stat = vgpio_clr_1(&line, NULL, IPC_i_gpio);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return 0;
}
EXPORT_SYMBOL(ipc_gpio_clr);

/*
 * IPC: i_gpio.vgpio_get().
 */
int ipc_gpio_get(int line)
{
	enum clnt_stat stat;
	int res;

	stat = vgpio_get_1(&line, &res, IPC_i_gpio);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return res;
}
EXPORT_SYMBOL(ipc_gpio_get);

/*
 * IPC: i_gpio.vgpio_query_line().
 */
int ipc_gpio_query_line(int line, int *type, int *state, int *dir)
{
	enum clnt_stat stat;
	struct vgpio_line_stat res;

	stat = vgpio_query_line_1(&line, &res, IPC_i_gpio);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	*type = res.type;
	*state = res.state;
	*dir = res.dir;

	return 0;
}
EXPORT_SYMBOL(ipc_gpio_query_line);

struct gfi_event_handler_list
{
	struct list_head list;
	ipc_gfi_handler h;
	void *arg;
};

static LIST_HEAD(eh_head);

/*
 * Call back when interrupt of vgpio received.
 */
void ipc_gfi_got_event(int line)
{
	struct gfi_event_handler_list *eh;
	struct list_head *pos, *q;

	list_for_each_safe(pos, q, &eh_head) {
		eh = list_entry(pos, struct gfi_event_handler_list, list);
		eh->h(line, eh->arg);
	}
}

/*
 * IPC: i_gpio.vgfi_req().
 */
int ipc_gfi_req(int line, int time, ipc_gfi_handler h, void *cbarg)
{
	enum clnt_stat stat;
	struct vgfi_req_arg arg;
	struct gfi_event_handler_list *eh;

	arg.line = line;
	arg.time = time;
	stat = vgfi_req_1(&arg, NULL, IPC_i_gpio);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	eh = kmalloc(sizeof(*h), GFP_KERNEL);
	if (eh == NULL) {
		pr_err("%s(): out of memory\n", __func__);
		return -ENOMEM;
	}

	eh->h = h;
	eh->arg = cbarg;

	list_add_tail(&eh->list, &eh_head);

	return 0;
}
EXPORT_SYMBOL(ipc_gfi_req);

/*
 * IPC: i_gpio.vgfi_free().
 */
int ipc_gfi_free(int line, ipc_gfi_handler h)
{
	enum clnt_stat stat;
	struct gfi_event_handler_list *eh;
	struct list_head *pos, *q;

	list_for_each_safe(pos, q, &eh_head) {
		eh = list_entry(pos, struct gfi_event_handler_list, list);
		if (eh->h == h) {
			list_del(pos);
			kfree(eh);
		}
	}

	stat = vgfi_free_1(&line, NULL, IPC_i_gpio);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return 0;
}
EXPORT_SYMBOL(ipc_gfi_free);


int ipc_vfio_select_lock(int module, int lock)
{
	enum clnt_stat stat;
	struct vfio_lock_info lock_info;
	
	lock_info.module=module;
	lock_info.lock=lock;
	
	stat = vfio_select_lock_1(&lock_info, NULL, IPC_i_gpio);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return 0;
}
EXPORT_SYMBOL(ipc_vfio_select_lock);

int ipc_vfio_unlock(int lock)
{
	enum clnt_stat stat;

	stat = vfio_unlock_1(&lock, NULL, IPC_i_gpio);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}
	return 0;
}
EXPORT_SYMBOL(ipc_vfio_unlock);

/*
 * Client initialization.
 */
static int __init i_gpio_init(void)
{
	IPC_i_gpio = ipc_clnt_prog_register(&i_gpio_prog);
	if (IPC_i_gpio == NULL)
		return -EPERM;

	return 0;
}

/*
 * Client exit.
 */
static void __exit i_gpio_cleanup(void)
{
	if (IPC_i_gpio) {
		ipc_clnt_prog_unregister(&i_gpio_prog, IPC_i_gpio);
		IPC_i_gpio = NULL;
	}
}

subsys_initcall_sync(i_gpio_init);
module_exit(i_gpio_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Charles Chiou <cchiou@ambarella.com>");
MODULE_DESCRIPTION("Ambarella IPC: i_gpio.x");
