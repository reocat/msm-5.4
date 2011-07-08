/*
 * drivers/ambarella-ipc/client/i_pmic_client.c
 *
 * Authors:
 *	Keny Huang <skhuang@ambarella.com>
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

#include <asm/uaccess.h>
#include <linux/module.h>

#include <linux/aipc/aipc.h>
#include <linux/aipc/i_pmic.h>
#if !defined(__DEPENDENCY_GENERATION__)
#include "ipcgen/i_pmic.h"
#define __IPC_CLIENT_IMPL__
#include "ipcgen/i_pmic_tab.i"
#endif

static CLIENT *IPC_i_pmic = NULL;

static struct ipc_prog_s i_pmic_prog =
{
	.name = "i_pmic",
	.prog = I_PMIC_PROG,
	.vers = I_PMIC_VERS,
	.table = (struct ipcgen_table *) i_pmic_prog_1_table,
	.nproc = i_pmic_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};


unsigned int ipc_ipmic_get_modules(unsigned int pmic_id)
{
	enum clnt_stat stat;
	unsigned int res;

	stat = i_pmic_get_modules_1(&pmic_id, &res, IPC_i_pmic);
	if (stat != IPC_SUCCESS) {
		pr_err("%s: ipc error - %d (%s)\n", __func__, stat, ipc_strerror(stat));
		return -stat;
	}

	return res;
}
EXPORT_SYMBOL(ipc_ipmic_get_modules);


int ipc_ipmic_init_module(unsigned int module_id)
{
	enum clnt_stat stat;
	int res;

	stat = i_pmic_init_module_1(&module_id, &res, IPC_i_pmic);
	if (stat != IPC_SUCCESS) {
		pr_err("%s: ipc error - %d (%s)\n", __func__, stat, ipc_strerror(stat));
		return -stat;
	}

	return res;
}
EXPORT_SYMBOL(ipc_ipmic_init_module);


int ipc_ipmic_get_prop_int(unsigned int module_id, unsigned int prop_id)
{
	enum clnt_stat stat;
	struct i_pmic_prop_id_s ppid;
	struct i_pmic_ps_props_s prop_val;

	ppid.module_id = module_id;
	ppid.prop_id = prop_id;
	
	stat = i_pmic_get_prop_1(&ppid, &prop_val, IPC_i_pmic);
	if (stat != IPC_SUCCESS) {
		pr_err("%s: ipc error - %d (%s)\n", __func__, stat, ipc_strerror(stat));
		return -stat;
	}

	return prop_val.intval;
}
EXPORT_SYMBOL(ipc_ipmic_get_prop_int);


int ipc_ipmic_set_prop_int(unsigned int module_id, unsigned int prop_id, int prop_value)
{
	enum clnt_stat stat;
	int res;
	struct i_pmic_setprop_s prop_val;

	memset(&prop_val,0,sizeof(struct i_pmic_setprop_s));
	
	prop_val.prop.module_id = module_id;
	prop_val.prop.prop_id = prop_id;
	prop_val.set_prop_val.intval = prop_value;

	stat = i_pmic_set_prop_1(&prop_val, &res, IPC_i_pmic);
	if (stat != IPC_SUCCESS) {
		pr_err("%s: ipc error - %d (%s)\n", __func__, stat, ipc_strerror(stat));
		return -stat;
	}

	return res;
}
EXPORT_SYMBOL(ipc_ipmic_set_prop_int);


int ipc_ipmic_get_prop_str(unsigned int module_id, unsigned int prop_id, char *prop_value)
{
	enum clnt_stat stat;
	struct i_pmic_prop_id_s ppid;
	struct i_pmic_ps_props_s prop_val;

	ppid.module_id = module_id;
	ppid.prop_id = prop_id;

	stat = i_pmic_get_prop_1(&ppid, &prop_val, IPC_i_pmic);
	if (stat != IPC_SUCCESS) {
		pr_err("%s: ipc error - %d (%s)\n", __func__, stat, ipc_strerror(stat));
		return -stat;
	}

	memcpy(prop_value,prop_val.strval,64);
	
	return 0;
}
EXPORT_SYMBOL(ipc_ipmic_get_prop_str);


int ipc_ipmic_set_prop_str(unsigned int module_id, unsigned int prop_id, char *prop_value)
{
	enum clnt_stat stat;
	int res;
	struct i_pmic_setprop_s prop_val;

	memset(&prop_val,0,sizeof(struct i_pmic_setprop_s));
	
	prop_val.prop.module_id = module_id;
	prop_val.prop.prop_id = prop_id;
	memcpy(prop_val.set_prop_val.strval,prop_value,64);

	stat = i_pmic_set_prop_1(&prop_val, &res, IPC_i_pmic);
	if (stat != IPC_SUCCESS) {
		pr_err("%s: ipc error - %d (%s)\n", __func__, stat, ipc_strerror(stat));
		return -stat;
	}

	return res;
}
EXPORT_SYMBOL(ipc_ipmic_set_prop_str);


/*
 * Client initialization.
 */
static int __init i_pmic_init(void)
{
	IPC_i_pmic = ipc_clnt_prog_register(&i_pmic_prog);
	if (IPC_i_pmic == NULL)
		return -EPERM;

	return 0;
}

/*
 * Client exit.
 */
static void __exit i_pmic_cleanup(void)
{
	if (IPC_i_pmic) {
		ipc_clnt_prog_unregister(&i_pmic_prog, IPC_i_pmic);
		IPC_i_pmic = NULL;
	}
}

subsys_initcall_sync(i_pmic_init);
module_exit(i_pmic_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Keny Huang <skhuang@ambarella.com>");
MODULE_DESCRIPTION("Ambarella IPC: i_pmic.x");

