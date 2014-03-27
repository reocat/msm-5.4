/*
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 */

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/rpmsg.h>
#include <linux/err.h>
#include <linux/remoteproc.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>
#include <plat/ambalink_cfg.h>
#include "aipc_priv.h"

#define AIPC_RFS_CMD_OPEN        0
#define AIPC_RFS_CMD_CLOSE       1
#define AIPC_RFS_CMD_READ        2
#define AIPC_RFS_CMD_WRITE       3
#define AIPC_RFS_CMD_TELL        4
#define AIPC_RFS_CMD_SEEK        5
#define AIPC_RFS_CMD_REMOVE      6
#define AIPC_RFS_CMD_MKDIR       7
#define AIPC_RFS_CMD_RMDIR       8

#define AIPC_RFS_SEEK_SET        0
#define AIPC_RFS_SEEK_CUR        1
#define AIPC_RFS_SEEK_END        2

#define AIPC_RFS_MODE_RD         1
#define AIPC_RFS_MODE_WR         2

#define AIPC_RFS_REPLY_OK        0
#define AIPC_RFS_REPLY_NODEV     1

struct aipc_rfs_msg {
	unsigned char   msg_type;
	unsigned char   xprt;
	unsigned short  msg_len;
	void*           reserved;
	int             parameter[0];
};

struct aipc_rfs_open {
	int             flag;
	char            name[0];
};

struct aipc_rfs_close {
	struct file*    filp;
};

struct aipc_rfs_seek {
	struct file*    filp;
	int             origin;
	int             offset;
};

struct aipc_rfs_io {
	struct file*    filp;
	int             size;
	void*           data;
};

/*
 * process OPEN command
 */
static void rfs_open(struct aipc_rfs_msg *msg)
{
	struct aipc_rfs_open *param = (struct aipc_rfs_open*)msg->parameter;
	struct file *filp;
	int mode = -1;

	msg->msg_type = AIPC_RFS_REPLY_NODEV;

	switch (param->flag) {
	case AIPC_RFS_MODE_RD:
		mode = O_RDONLY;
		break;
	case AIPC_RFS_MODE_WR:
		mode = O_CREAT | O_WRONLY;
		break;
	}
	//DMSG("rfs_open mode %d %X\n", param->flag, mode);

	if (mode != -1) {
		filp = filp_open(param->name, mode, 0644);
		//DMSG("rfs_open %s %p\n", param->name, filp);

		if (!IS_ERR(filp) && filp != NULL) {
			msg->msg_type = AIPC_RFS_REPLY_OK;
			msg->parameter[0] = (int)filp;
		}
	}

	msg->msg_len = sizeof(struct aipc_rfs_msg) + sizeof(int);
}

/*
 * process READ command
 */
static void rfs_read(struct aipc_rfs_msg *msg)
{
	struct aipc_rfs_io *param = (struct aipc_rfs_io*)msg->parameter;
	mm_segment_t oldfs = get_fs();
	struct file* filp = param->filp;
	void *data = (void *)ambalink_phys_to_virt((u32)param->data);
	int ret = 0;

	if (param->size) {
		set_fs(KERNEL_DS);
		ret = vfs_read(filp, data, param->size, &filp->f_pos);
		set_fs(oldfs);
		if (ret < 0)
			EMSG("rfs_read error %d\n", ret);
	}

	msg->msg_type = AIPC_RFS_REPLY_OK;
	msg->msg_len = sizeof(struct aipc_rfs_msg) + sizeof(int);
	msg->parameter[0] = ret;
}
	
/*
 * process WRITE command
 */
static void rfs_write(struct aipc_rfs_msg *msg)
{
	struct aipc_rfs_io *param = (struct aipc_rfs_io*)msg->parameter;
	mm_segment_t oldfs = get_fs();
	struct file* filp = param->filp;
	void *data = (void *)ambalink_phys_to_virt((u32)param->data);
	int ret = 0;

	if (param->size) {
		set_fs(KERNEL_DS);
		ret = vfs_write(filp, data, param->size, &filp->f_pos);
		set_fs(oldfs);
		if (ret < 0)
			EMSG("rfs_write error %d\n", ret);
	}

	msg->msg_type = AIPC_RFS_REPLY_OK;
	msg->msg_len = sizeof(struct aipc_rfs_msg) + sizeof(int);
	msg->parameter[0] = ret;
}

/*
 * process CLOSE command
 */
static void rfs_close(struct aipc_rfs_msg *msg)
{
	struct aipc_rfs_close *param = (struct aipc_rfs_close*)msg->parameter;
	
	if (param->filp) {
		//DMSG("rfs_close %p\n", param->filp);
		filp_close(param->filp, NULL);
	}
	msg->msg_type = AIPC_RFS_REPLY_OK;
	msg->msg_len = sizeof(struct aipc_rfs_msg);
}

/*
 * process TELL command
 */
static void rfs_tell(struct aipc_rfs_msg *msg)
{
        struct aipc_rfs_seek *param = (struct aipc_rfs_seek*)msg->parameter;
	*(long long*)msg->parameter = (long long)(param->filp->f_pos);
}

/*
 * process SEEK command
 */
static void rfs_seek(struct aipc_rfs_msg *msg)
{
        struct aipc_rfs_seek *param = (struct aipc_rfs_seek*)msg->parameter;
	loff_t offset;
	int origin = 0;

	switch (param->origin) {
	case AIPC_RFS_SEEK_SET:
		origin = SEEK_SET;
		break;
	case AIPC_RFS_SEEK_CUR:
		origin = SEEK_CUR;
		break;
	case AIPC_RFS_SEEK_END:
		origin = SEEK_END;
		break;
	}

	offset = (loff_t)param->offset;
	offset = vfs_llseek(param->filp, offset, origin);
	msg->msg_type = AIPC_RFS_REPLY_OK;
	msg->msg_len  = sizeof(struct aipc_rfs_msg) + sizeof(long long);
	*(long long*)msg->parameter = (long long)offset;
}

/*
 * process REMOVE command
 */
static void rfs_remove(struct aipc_rfs_msg *msg)
{
	struct aipc_rfs_open *param = (struct aipc_rfs_open*)msg->parameter;
	int    ret;

	ret = sys_unlink(param->name);
	DMSG("rfs_remove %s %d\n", param->name, ret);
	msg->msg_len = sizeof(struct aipc_rfs_msg) + sizeof(int);
	msg->parameter[0] = ret;
}

/*
 * process MKDIR command
 */
static void rfs_mkdir(struct aipc_rfs_msg *msg)
{
	struct aipc_rfs_open *param = (struct aipc_rfs_open*)msg->parameter;
	int    ret;

	ret = sys_mkdir(param->name, 0644);
	DMSG("rfs_mkdir %s %d\n", param->name, ret);
	msg->msg_len = sizeof(struct aipc_rfs_msg) + sizeof(int);
	msg->parameter[0] = ret;
}

/*
 * process RMDIR command
 */
static void rfs_rmdir(struct aipc_rfs_msg *msg)
{
	struct aipc_rfs_open *param = (struct aipc_rfs_open*)msg->parameter;
	int    ret;

	ret = sys_rmdir(param->name);
	DMSG("rfs_rmdir %s %d\n", param->name, ret);
	msg->msg_len = sizeof(struct aipc_rfs_msg) + sizeof(int);
	msg->parameter[0] = ret;
}

static void (*msg_handler[])(struct aipc_rfs_msg*) = {
	rfs_open,
	rfs_close,
	rfs_read,
	rfs_write,
	rfs_tell,
	rfs_seek,
	rfs_remove,
	rfs_mkdir,
	rfs_rmdir,
};

/*
 * process requests from remote core
 */
static void rpmsg_rfs_recv(struct rpmsg_channel *rpdev, void *data, int len,
			void *priv, u32 src)
{
	struct aipc_rfs_msg *msg = (struct aipc_rfs_msg*)data;
	int cmd = msg->msg_type;

	if (cmd < 0 || cmd >= ARRAY_SIZE(msg_handler)) {
		EMSG("unknown rfs command %d\n", cmd);
		return;
	}

	msg_handler[cmd](msg);
	rpmsg_send(rpdev, msg, msg->msg_len);
}

static int rpmsg_rfs_probe(struct rpmsg_channel *rpdev)
{
	int ret = 0;
	struct rpmsg_ns_msg nsm;

	nsm.addr = rpdev->dst;
	memcpy(nsm.name, rpdev->id.name, RPMSG_NAME_SIZE);
	nsm.flags = 0;

	rpmsg_send(rpdev, &nsm, sizeof(nsm));

	return ret;
}

static void rpmsg_rfs_remove(struct rpmsg_channel *rpdev)
{
}

static struct rpmsg_device_id rpmsg_rfs_id_table[] = {
	{ .name	= "aipc_rfs", },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, rpmsg_rfs_id_table);

static struct rpmsg_driver rpmsg_rfs_driver = {
	.drv.name	= KBUILD_MODNAME,
	.drv.owner	= THIS_MODULE,
	.id_table	= rpmsg_rfs_id_table,
	.probe		= rpmsg_rfs_probe,
	.callback	= rpmsg_rfs_recv,
	.remove		= rpmsg_rfs_remove,
};

static int __init rpmsg_rfs_init(void)
{
	return register_rpmsg_driver(&rpmsg_rfs_driver);
}

static void __exit rpmsg_rfs_fini(void)
{
	unregister_rpmsg_driver(&rpmsg_rfs_driver);
}

module_init(rpmsg_rfs_init);
module_exit(rpmsg_rfs_fini);

MODULE_DESCRIPTION("RPMSG RFS Server");
