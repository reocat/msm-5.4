/*
 *
 * Copyright (C) 2012-2016, Ambarella, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <linux/ioport.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/rpmsg.h>
#include <linux/err.h>
#include <linux/kthread.h>
#include <linux/remoteproc.h>
#include <linux/suspend.h>
#include <trace/events/power.h>

#include <plat/ambalink_cfg.h>

/*----------------------------------------------------------------------------*/
#if 1 // debug
    #define dbg_trace(str, args...)     printk("@@@ %s[#%d]: " str, __func__, __LINE__, ## args)
#else
    #define dbg_trace(str, args...)
#endif


typedef u32 UINT32;
typedef u64 UINT64;

typedef enum _AMBA_RPDEV_LINK_CTRL_CMD_e_ {
	LINK_CTRL_CMD_HIBER_PREPARE_FROM_LINUX = 0,
	LINK_CTRL_CMD_HIBER_ENTER_FROM_LINUX,
	LINK_CTRL_CMD_HIBER_EXIT_FROM_LINUX,
	LINK_CTRL_CMD_HIBER_ACK,
	LINK_CTRL_CMD_SUSPEND,
	LINK_CTRL_CMD_GPIO_LINUX_ONLY_LIST,
	LINK_CTRL_CMD_GET_MEM_INFO,
	LINK_CTRL_CMD_SET_RTOS_MEM
} AMBA_RPDEV_LINK_CTRL_CMD_e;

/**
 * LinkCtrlSuspendLinux related data structure.
 */
typedef enum _AMBA_RPDEV_LINK_CTRL_SUSPEND_TARGET_e_ {
    LINK_CTRL_CMD_SUSPEND_TO_DISK = 0,
    LINK_CTRL_CMD_SUSPEND_TO_DRAM
} AMBA_RPDEV_LINK_CTRL_SUSPEND_TARGET_e;

typedef struct _AMBA_RPDEV_LINK_CTRL_CMD_s_ {
	UINT32  Cmd;
	UINT64  Param1;
	UINT64  Param2;
} AMBA_RPDEV_LINK_CTRL_CMD_s;

typedef struct _LINK_CTRL_MEMINFO_CMD_s_ {
	UINT32	Cmd;
        void*   RtosStart;
        void*   RtosEnd;
        void*   RtosSystemStart;
        void*   RtosSystemEnd;
        void*   CachedHeapStart;
        void*   CachedHeapEnd;
        void*   NonCachedHeapStart;
        void*   NonCachedHeapEnd;
} LINK_CTRL_MEMINFO_CMD_s;

typedef enum _AMBA_RPDEV_LINK_CTRL_MEMTYPE_e_ {
	LINK_CTRL_MEMTYPE_HEAP = 0,
	LINK_CTRL_MEMTYPE_DSP
} AMBA_RPDEV_LINK_CTRL_MEMTYPE_e;

struct _AMBA_RPDEV_LINK_CTRL_MEMINFO_s_ {
    UINT64 base_addr;
	UINT64 phys_addr;
	UINT32 size;
	UINT32 padding;
	UINT32 padding1;
	UINT32 padding2;
	UINT32 padding3;
	UINT32 padding4;
} __attribute__((aligned(32), packed));
typedef struct _AMBA_RPDEV_LINK_CTRL_MEMINFO_s_ AMBA_RPDEV_LINK_CTRL_MEMINFO_t;

DECLARE_COMPLETION(linkctrl_comp);
struct rpmsg_channel *rpdev_linkctrl;
int hibernation_start = 0;
static AMBA_RPDEV_LINK_CTRL_MEMINFO_t rpdev_meminfo;

#if 0
static struct resource amba_rtosmem = {
       .name   = "RTOS memory region",
       .start  = 0x0,
       .end    = 0x0,
       .flags  = IORESOURCE_MEM | IORESOURCE_BUSY,
};

/*
 * Standard memory resources
 */
static struct resource amba_heapmem[] = {
	{
		.name = "RTOS System",
		.start = 0,
		.end = 0,
		.flags = IORESOURCE_MEM | IORESOURCE_EXCLUSIVE | IORESOURCE_BUSY
	},
	{
		.name = "Cached heap",
		.start = 0,
		.end = 0,
		.flags = IORESOURCE_MEM
	},
	{
		.name = "Non-Cached heap",
		.start = 0,
		.end = 0,
		.flags = IORESOURCE_MEM
	},
};
#endif

/*----------------------------------------------------------------------------*/
static int rpmsg_linkctrl_ack(void *data)
{
	complete(&linkctrl_comp);

	return 0;
}

/*----------------------------------------------------------------------------*/
static int rpmsg_linkctrl_suspend(void *data)
{
#if 0
	extern int amba_state_store(void *suspend_to);
	AMBA_RPDEV_LINK_CTRL_CMD_s *ctrl_cmd = (AMBA_RPDEV_LINK_CTRL_CMD_s *) data;
	struct task_struct *task;

	task = kthread_run(amba_state_store, (void *) ctrl_cmd->Param1,
	                   "linkctrl_suspend");
	if (IS_ERR(task))
		return PTR_ERR(task);
#else
    dbg_trace("To Do feature !!!!!!!\n");
#endif
	return 0;
}

/*----------------------------------------------------------------------------*/
static int rpmsg_linkctrl_gpio_linux_only_list(void *data)
{
	extern void ambarella_gpio_create_linux_only_mask(u32 gpio);
	AMBA_RPDEV_LINK_CTRL_CMD_s *ctrl_cmd = (AMBA_RPDEV_LINK_CTRL_CMD_s *) data;
	u8 *p;
	int ret;
	u32 gpio;
        u64 virt;

	virt = (u64) ambalink_phys_to_virt(ctrl_cmd->Param1);

	while((p = (u8 *) strsep((char **) &virt, ", "))) {
		ret = kstrtouint(p, 0, &gpio);
		if (ret < 0) {
			continue;
		}

		//ambarella_gpio_create_linux_only_mask(gpio);
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
static int rpmsg_linkctrl_set_rtos_mem(void *data)
{
#if 0
	int rval = 0;
	LINK_CTRL_MEMINFO_CMD_s *meminfo_cmd = (LINK_CTRL_MEMINFO_CMD_s *) data;

	amba_rtosmem.start = meminfo_cmd->RtosStart;
	amba_rtosmem.end = meminfo_cmd->RtosEnd;

	rval = request_resource(&iomem_resource, &amba_rtosmem);
	if (rval < 0 && rval != -EBUSY) {
		printk("Ambarella RTOS memory resource %pR cannot be added\n", &amba_rtosmem);
	}

	amba_heapmem[0].start	= meminfo_cmd->RtosSystemStart;
	amba_heapmem[0].end	= meminfo_cmd->RtosSystemEnd - 1;
	amba_heapmem[1].start	= meminfo_cmd->CachedHeapStart;
	amba_heapmem[1].end	= meminfo_cmd->CachedHeapEnd - 1;
	amba_heapmem[2].start	= meminfo_cmd->NonCachedHeapStart;
	amba_heapmem[2].end	= meminfo_cmd->NonCachedHeapEnd - 1;

	rval = request_resource(&amba_rtosmem, &amba_heapmem[0]);
	if (rval < 0 && rval != -EBUSY) {
		printk("Ambarella RTOS system memory resource %pR cannot be added\n",
			&amba_heapmem[0]);
	}

	rval = request_resource(&amba_rtosmem, &amba_heapmem[1]);
	if (rval < 0 && rval != -EBUSY) {
		printk("Ambarella RTOS cached heap memory resource %pR cannot be added\n",
			&amba_heapmem[1]);
	}

	rval = request_resource(&amba_rtosmem, &amba_heapmem[2]);
	if (rval < 0 && rval != -EBUSY) {
		printk("Ambarella RTOS non-cached heap memory resource %pR cannot be added\n",
			&amba_heapmem[2]);
	}
#else
    dbg_trace("To Do feature !!!!!!!\n");
#endif
	return 0;
}

/*----------------------------------------------------------------------------*/
int rpmsg_linkctrl_cmd_get_mem_info(u8 type, void **base, void **phys, u32 *size)
{
	AMBA_RPDEV_LINK_CTRL_CMD_s ctrl_cmd;
	u64 phy_addr;

	dbg_trace("type=%d\n", type);

	phy_addr = (u64) ambalink_virt_to_phys((unsigned long) &rpdev_meminfo);

	memset(&ctrl_cmd, 0x0, sizeof(ctrl_cmd));
	ctrl_cmd.Cmd = LINK_CTRL_CMD_GET_MEM_INFO;
	ctrl_cmd.Param1 = (u64) type;
	ctrl_cmd.Param2 = phy_addr;

	rpmsg_send(rpdev_linkctrl, &ctrl_cmd, sizeof(ctrl_cmd));

	wait_for_completion(&linkctrl_comp);

	*base = rpdev_meminfo.base_addr;
	*phys = rpdev_meminfo.phys_addr;
	*size = rpdev_meminfo.size;

	dbg_trace("type= %u, base= x%llx, phys= x%llx, size= x%x\n",
	    type, rpdev_meminfo.base_addr, rpdev_meminfo.phys_addr, rpdev_meminfo.size);

	return 0;
}
EXPORT_SYMBOL(rpmsg_linkctrl_cmd_get_mem_info);

int rpmsg_linkctrl_cmd_suspend_prepare(u32 info)
{
	AMBA_RPDEV_LINK_CTRL_CMD_s ctrl_cmd;

	dbg_trace("0x%08x\n", info);

	memset(&ctrl_cmd, 0x0, sizeof(ctrl_cmd));
	ctrl_cmd.Cmd = LINK_CTRL_CMD_HIBER_PREPARE_FROM_LINUX;
	ctrl_cmd.Param1 = info;

	rpmsg_send(rpdev_linkctrl, &ctrl_cmd, sizeof(ctrl_cmd));

	//wait_for_completion(&linkctrl_comp);

	hibernation_start = 1;

	return 0;
}
EXPORT_SYMBOL(rpmsg_linkctrl_cmd_suspend_prepare);

int rpmsg_linkctrl_cmd_suspend_enter(int flag)
{
	AMBA_RPDEV_LINK_CTRL_CMD_s ctrl_cmd;

	dbg_trace("\n");

	memset(&ctrl_cmd, 0x0, sizeof(ctrl_cmd));
	ctrl_cmd.Cmd = LINK_CTRL_CMD_HIBER_ENTER_FROM_LINUX;
	ctrl_cmd.Param1 = flag;

	rpmsg_send(rpdev_linkctrl, &ctrl_cmd, sizeof(ctrl_cmd));

	return 0;
}
EXPORT_SYMBOL(rpmsg_linkctrl_cmd_suspend_enter);

int rpmsg_linkctrl_cmd_suspend_exit(int flag)
{
	AMBA_RPDEV_LINK_CTRL_CMD_s ctrl_cmd;

	memset(&ctrl_cmd, 0x0, sizeof(ctrl_cmd));
	ctrl_cmd.Cmd = LINK_CTRL_CMD_HIBER_EXIT_FROM_LINUX;
	ctrl_cmd.Param1 = flag;

	rpmsg_send(rpdev_linkctrl, &ctrl_cmd, sizeof(ctrl_cmd));

	hibernation_start = 0;

	return 0;
}
EXPORT_SYMBOL(rpmsg_linkctrl_cmd_suspend_exit);

/*----------------------------------------------------------------------------*/
typedef int (*PROC_FUNC)(void *data);
static PROC_FUNC linkctrl_proc_list[] = {
	rpmsg_linkctrl_ack,
	rpmsg_linkctrl_suspend,
	rpmsg_linkctrl_gpio_linux_only_list,
	rpmsg_linkctrl_set_rtos_mem,
};

static void rpmsg_linkctrl_cb(struct rpmsg_channel *rpdev, void *data, int len,
                              void *priv, u32 src)
{
	AMBA_RPDEV_LINK_CTRL_CMD_s *msg = (AMBA_RPDEV_LINK_CTRL_CMD_s *) data;

    dbg_trace("recv: cmd= %d, param1= x%llx, param2= x%llx\n",
        msg->Cmd, msg->Param1, msg->Param2);

	switch (msg->Cmd) {
	case LINK_CTRL_CMD_HIBER_ACK:
		linkctrl_proc_list[0](data);
		break;
	case LINK_CTRL_CMD_SUSPEND:
		linkctrl_proc_list[1](data);
		break;
	case LINK_CTRL_CMD_GPIO_LINUX_ONLY_LIST:
		linkctrl_proc_list[2](data);
		break;
	case LINK_CTRL_CMD_SET_RTOS_MEM:
		linkctrl_proc_list[3](data);
		break;
	default:
		break;
	}
}

/*
 * This synchronization is implemented by mutually excluding regular CPU
 * hotplug and Suspend/Hibernate call paths by hooking onto the Suspend/
 * Hibernate notifications.
 */
static int rpmsg_linkctrl_pm_callback(struct notifier_block *nb,
			unsigned long action, void *ptr)
{
	switch (action) {
	case PM_SUSPEND_PREPARE:
		rpmsg_linkctrl_cmd_suspend_prepare(LINK_CTRL_CMD_SUSPEND_TO_DRAM);
		break;
	case PM_HIBERNATION_PREPARE:
		rpmsg_linkctrl_cmd_suspend_prepare(LINK_CTRL_CMD_SUSPEND_TO_DISK);
		break;
	case PM_POST_SUSPEND:
		rpmsg_linkctrl_cmd_suspend_exit(LINK_CTRL_CMD_SUSPEND_TO_DRAM);
		break;
	case PM_POST_HIBERNATION:
		rpmsg_linkctrl_cmd_suspend_exit(LINK_CTRL_CMD_SUSPEND_TO_DISK);
		break;
	default:
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

static int rpmsg_linkctrl_probe(struct rpmsg_channel *rpdev)
{
	int ret = 0;
	struct rpmsg_ns_msg nsm;

	rpdev_linkctrl = rpdev;
	nsm.addr = rpdev->dst;
	memcpy(nsm.name, rpdev->id.name, RPMSG_NAME_SIZE);
	nsm.flags = 0;

	rpmsg_send(rpdev, &nsm, sizeof(nsm));

	pm_notifier(rpmsg_linkctrl_pm_callback, 0);

	return ret;
}

static void rpmsg_linkctrl_remove(struct rpmsg_channel *rpdev)
{
}

static struct rpmsg_device_id rpmsg_linkctrl_id_table[] = {
	{ .name = "link_ctrl", },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, rpmsg_linkctrl_id_table);

static struct rpmsg_driver rpmsg_linkctrl_driver = {
	.drv.name   = KBUILD_MODNAME,
	.drv.owner  = THIS_MODULE,
	.id_table   = rpmsg_linkctrl_id_table,
	.probe      = rpmsg_linkctrl_probe,
	.callback   = rpmsg_linkctrl_cb,
	.remove     = rpmsg_linkctrl_remove,
};

static int __init rpmsg_linkctrl_init(void)
{
	return register_rpmsg_driver(&rpmsg_linkctrl_driver);
}

static void __exit rpmsg_linkctrl_fini(void)
{
	unregister_rpmsg_driver(&rpmsg_linkctrl_driver);
}

module_init(rpmsg_linkctrl_init);
module_exit(rpmsg_linkctrl_fini);

MODULE_DESCRIPTION("RPMSG AmbaRpdev_LinkCtrl Server");
