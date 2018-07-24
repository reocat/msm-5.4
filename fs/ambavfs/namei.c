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

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/pagemap.h>
#include <linux/writeback.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/namei.h>
#include <asm/page.h>
#include <plat/ambalink_cfg.h>
#include "ambafs.h"

#define VALIDATE_TIME_NS 20000
unsigned long *qstat_buf;
DEFINE_MUTEX(qstat_mutex);

/*
 * init qstat_mutex, which protects qstat_buf
 */
void qstat_init (void)
{
	mutex_init(&qstat_mutex);
}

/*
 * helper function to trigger/wait a remote command excution
 */
static void exec_cmd(struct dentry *dentry,
		void *buf, int size, int cmd)
{
	struct ambafs_msg  *msg  = (struct ambafs_msg  *)buf;
	char *path = (char*)msg->parameter;
	int len;

	//dir = d_find_any_alias(inode);
	len = ambafs_get_full_path(dentry, path, (char*)buf + size - path);
	//path[len] = '/';
	//strcpy(path+len+1, dentry->d_name.name);
	//dput(dir);

	msg->cmd = cmd;
	ambafs_rpmsg_exec(msg, strlen(path) + 1);
}

/*
 * couple a dentry with inode
 */
static void ambafs_attach_inode(struct dentry *dentry, struct inode *inode)
{
	inode->i_private = dentry;
	d_instantiate(dentry, inode);
	if (d_unhashed(dentry))
		d_rehash(dentry);
}

/*
 * lookup for a specific @dentry under dir @inode
 */
static struct dentry *ambafs_lookup(struct inode *inode,
	            struct dentry *dentry, unsigned int flags)
{
	int buf[128];
	struct ambafs_msg  *msg  = (struct ambafs_msg  *)buf;
	struct ambafs_stat *stat = (struct ambafs_stat*)msg->parameter;

	AMBAFS_DMSG("%s:  %s \r\n", __func__, dentry->d_name.name);

	exec_cmd(dentry, msg, sizeof(buf), AMBAFS_CMD_STAT);
	if (msg->flag) {
		inode = ambafs_new_inode(dentry->d_sb, stat);
		if (inode) {
			struct dentry *new = d_splice_alias(inode, dentry);
			inode->i_private = dentry;
			ambafs_update_inode(inode, stat);
			return new;
		}
	}

	return NULL;
}

/*
 * create a new file @dentry under dir @inode
 */
static int ambafs_create(struct inode *inode, struct dentry *dentry, umode_t mode,
				bool excl)
{
	int buf[128];
	struct ambafs_msg  *msg  = (struct ambafs_msg *)buf;
	struct ambafs_stat *stat = (struct ambafs_stat*)msg->parameter;

	AMBAFS_DMSG("%s:  %s \r\n", __func__, dentry->d_name.name);

	exec_cmd(dentry, msg, sizeof(buf), AMBAFS_CMD_CREATE);
	if (stat->type == AMBAFS_STAT_FILE) {
		struct inode *child = ambafs_new_inode(dentry->d_sb, stat);
		if (child) {
			ambafs_attach_inode(dentry, child);
			ambafs_update_inode(child, stat);
			return 0;
		}
	}

	return -ENODEV;
}

/*
 * remove the file @dentry under dir @inode
 */
static int ambafs_unlink(struct inode *inode, struct dentry *dentry)
{
	int buf[128];
	struct ambafs_msg  *msg  = (struct ambafs_msg *)buf;

	AMBAFS_DMSG("%s:  %s \r\n", __func__, dentry->d_name.name);
	exec_cmd(dentry, msg, sizeof(buf), AMBAFS_CMD_DELETE);
	if (msg->flag == 0) {
		drop_nlink(dentry->d_inode);
		return 0;
	}
	return -EBUSY;
}

/*
 * make a new dir @dentry under dir @inode
 */
static int ambafs_mkdir(struct inode *inode,
			struct dentry *dentry, umode_t mode)
{
	int buf[128];
	struct ambafs_msg  *msg  = (struct ambafs_msg *)buf;
	struct ambafs_stat *stat = (struct ambafs_stat*)msg->parameter;

	AMBAFS_DMSG("%s:  %s \r\n", __func__, dentry->d_name.name);

	exec_cmd(dentry, msg, sizeof(buf), AMBAFS_CMD_MKDIR);
	if (stat->type == AMBAFS_STAT_DIR) {
		struct inode *child = ambafs_new_inode(dentry->d_sb, stat);
		if (child) {
			ambafs_attach_inode(dentry, child);
			ambafs_update_inode(child, stat);
			inc_nlink(child);
			inc_nlink(inode);
			return 0;
		}
	}

	return -ENODEV;
}

/*
 * remove the dir @dentry under dir @inode
 */
static int ambafs_rmdir(struct inode *inode, struct dentry *dentry)
{
	int buf[128];
	struct ambafs_msg  *msg  = (struct ambafs_msg *)buf;

	AMBAFS_DMSG("%s: %s\n", __func__, dentry->d_name.name);

	exec_cmd(dentry, msg, sizeof(buf), AMBAFS_CMD_RMDIR);
	if (msg->flag == 0) {
		clear_nlink(dentry->d_inode);
		if (inode->i_nlink > 1) {
			// i_nlink of parent dir should > 1
			// if dir in SD card is not updated, it'lll be 1
			drop_nlink(inode);
		}
		return 0;
	}
	return -EBUSY;
}

/*
 * move a file or directory
 */
static int ambafs_rename(struct inode *old_dir, struct dentry *old_dentry,
			 struct inode *new_dir, struct dentry *new_dentry,
			 unsigned int flags)
{
	int buf[128];
	struct ambafs_msg  *msg  = (struct ambafs_msg  *)buf;
	struct ambafs_stat *stat = (struct ambafs_stat*)msg->parameter;
	char *path, *end = (char*)buf + sizeof(buf);
	int len;

	AMBAFS_DMSG("ambafs_rename %s-->%s\n",
		old_dentry->d_name.name, new_dentry->d_name.name);

	if (flags & ~(RENAME_NOREPLACE | RENAME_WHITEOUT | RENAME_EXCHANGE))
		return -EINVAL;

	/* get new path */
	path = (char*)msg->parameter;
	len = ambafs_get_full_path(new_dentry, path, end - path);

        /* get old path */
	path += strlen(path) + 1;
	len = ambafs_get_full_path(old_dentry, path, end - path);
	len = strchr(path, 0) + 1 - (char*)msg->parameter;

	msg->cmd = AMBAFS_CMD_RENAME;
	path = (char*)msg->parameter;
	ambafs_rpmsg_exec(msg, len);

	if (msg->flag != 0) {
		int is_dir = (stat->type == AMBAFS_STAT_DIR) ? 1 : 0;
		if (new_dentry->d_inode) {
			drop_nlink(new_dentry->d_inode);
			if (is_dir) {
				drop_nlink(old_dir);
				drop_nlink(new_dentry->d_inode);
			}
		} else if (is_dir) {
			drop_nlink(old_dir);
			inc_nlink(new_dir);
		}
		return 0;
	}

	return -EBUSY;
}

const struct inode_operations ambafs_dir_inode_ops = {
	.lookup = ambafs_lookup,
	.create = ambafs_create,
	.unlink = ambafs_unlink,
	.mkdir  = ambafs_mkdir,
	.rmdir  = ambafs_rmdir,
	.rename = ambafs_rename,
};

/*
 * ambafs quick stat.
 */
int ambafs_get_qstat(struct dentry *dentry, void *buf, int size)
{
	struct ambafs_msg *msg = (struct ambafs_msg*)buf;
	volatile struct ambafs_qstat *stat = (struct ambafs_qstat*)msg->parameter;
	char *path = (char*)&(msg->parameter[1]);
	int len, i, type = AMBAFS_STAT_NULL;

	if (dentry == NULL) {
		AMBAFS_EMSG("%s: dentry == NULL \r\n", __func__);
		return AMBAFS_STAT_NULL;
	}

	if (0 == mutex_trylock(&qstat_mutex)) {
		return AMBAFS_STAT_DIR;
	}

	len = ambafs_get_full_path(dentry, path, (char*)buf + size - path);

	msg->parameter[0] = (u64) ambalink_virt_to_phys((uintptr_t)(void *) stat);

	AMBAFS_DMSG("%s: path = %s, quick_stat result phy address = 0x%x \r\n", __func__, path, msg->parameter[0]);

	msg->cmd = AMBAFS_CMD_QUICK_STAT;
	ambafs_rpmsg_send(msg, len + 1 + 8, NULL, NULL);

	for (i = 0; i < (65536*5); i++) {
		if (stat->magic == AMBAFS_QSTAT_MAGIC) {
			stat->magic = 0x0;
			type = stat->type;
			goto exit;
		}
	}

exit:
	mutex_unlock(&qstat_mutex);
	return type;
}

static int ambafs_d_revalidate(struct dentry *dentry, unsigned int flags)
{
	struct inode *inode;
	struct dentry *parent;
	void *align_buf;
	struct timespec64 ts;
	int ret,type;

	inode = d_inode_rcu(dentry);
	if (inode && is_bad_inode(inode))
		goto invalid;
	else if (time_before64(dentry->d_time, get_jiffies_64()) ||
		 (flags & LOOKUP_REVAL)) {

		/* For negative dentries, always do a fresh lookup */
		if (!inode)
			goto invalid;

		ret = -ECHILD;
		if (flags & LOOKUP_RCU)
			goto out;

		ret = 1;
		if(!S_ISDIR(inode->i_mode))
			goto out;
		align_buf = (void *)((((unsigned long) qstat_buf) & (~0x3f)) + 0x40);

		parent = dget_parent(dentry);
		type = ambafs_get_qstat(dentry, align_buf, QSTAT_BUFF_SIZE);
		dput(parent);

		if (type != AMBAFS_STAT_DIR) {
			AMBAFS_DMSG("%s fail: dentry = %p name = %s type = %d \r\n", __func__,dentry,dentry->d_name.name, type);
			goto out;
		} else {
			ts.tv_sec = 0;
			ts.tv_nsec = VALIDATE_TIME_NS;
			dentry->d_time = jiffies + timespec64_to_jiffies(&ts);
		}
	}

	ret = 1;
out:
	return ret;

invalid:
	ret = 0;
	goto out;
}

const struct dentry_operations ambafs_dentry_ops = {
	.d_revalidate	= ambafs_d_revalidate,
};

/*
 * get inode stat
 */
struct ambafs_stat* ambafs_get_stat(struct dentry *dentry, void *buf, int size)
{
	struct ambafs_msg *msg = (struct ambafs_msg*)buf;
	struct ambafs_stat *stat = (struct ambafs_stat*)msg->parameter;
	char *path = (char*)msg->parameter;
	int len;

	if (dentry == NULL) {
		AMBAFS_EMSG("%s: dentry == NULL \r\n", __func__);
		stat->type = AMBAFS_STAT_NULL;
		goto exit;
	}

	len = ambafs_get_full_path(dentry, path, (char*)buf + size - path);

	AMBAFS_DMSG("%s:  %s \r\n", __func__, path);

	msg->cmd = AMBAFS_CMD_STAT;
	ambafs_rpmsg_exec(msg, len+1);
	if (!msg->flag)
		stat->type = AMBAFS_STAT_NULL;

exit:
	return stat;
}

/*
 * get file attriubte, for cache consistency
 */
static int ambafs_getattr(const struct path *path, struct kstat *stat,
		   u32 request_mask, unsigned int query_flags)
{
	struct dentry *dentry = path->dentry;
	AMBAFS_DMSG("%s:  \r\n", __func__);

	if (dentry->d_inode && (dentry->d_inode->i_opflags & AMBAFS_IOP_SKIP_GET_STAT)) {
		dentry->d_inode->i_opflags &= ~AMBAFS_IOP_SKIP_GET_STAT;
	} else {
		int buf[128];
		struct ambafs_stat *astat;

		astat = ambafs_get_stat(dentry, buf, sizeof(buf));
		if (astat->type == AMBAFS_STAT_NULL) {
			return -ENOENT;
		}

		if (astat->type == AMBAFS_STAT_FILE) {
			ambafs_update_inode(dentry->d_inode, astat);
		}
	}

	return simple_getattr(path, stat, request_mask, query_flags);
}

/*
 * fill the timestamp info.
 */
static void fill_time_info(struct ambafs_timestmp *stamp, struct tm *time)
{
	stamp->year = time->tm_year;
	stamp->month = time->tm_mon;
	stamp->day = time->tm_mday;
	stamp->hour = time->tm_hour;
	stamp->min = time->tm_min;
	stamp->sec = time->tm_sec;
}
/*
 * set file attriubte, here we only implement timestamp change.
 */
#define AMBAFS_TIMES_SET_FLAGS (ATTR_MTIME | ATTR_ATIME | ATTR_CTIME)

int ambafs_remote_set_timestamp(struct dentry *dentry, struct iattr *attr)
{
	int buf[128];
	struct ambafs_msg *msg = (struct ambafs_msg *)buf;
	struct ambafs_stat_timestmp *stat = (struct ambafs_stat_timestmp *)msg->parameter;
	char *path = (char *)stat->name;
	int len;
	struct tm time;

	AMBAFS_DMSG("%s:  %s\r\n", __func__, path);
	len = ambafs_get_full_path(dentry, path, (char *)buf + sizeof(buf) - path);
	msg->cmd = AMBAFS_CMD_SET_TIME;
	time_to_tm(attr->ia_atime.tv_sec, 0, &time);
	fill_time_info(&stat->atime, &time);
	time_to_tm(attr->ia_mtime.tv_sec, 0, &time);
	fill_time_info(&stat->mtime, &time);
	time_to_tm(attr->ia_ctime.tv_sec, 0, &time);
	fill_time_info(&stat->ctime, &time);
	ambafs_rpmsg_exec(msg, sizeof(struct ambafs_stat_timestmp) + len + 1);

	if(msg->parameter[0]) {
		return -EIO;
	}

	return 0;
}

static int SupportRemoteSize = 1;

int ambafs_remote_set_size(struct dentry *dentry, u64 size)
{
	int buf[128];
	struct ambafs_msg *msg = (struct ambafs_msg *)buf;
	struct ambafs_stat_size *stat = (struct ambafs_stat_size *)msg->parameter;
	char *path = (char *)stat->name;
	int len, ret;

	AMBAFS_DMSG("%s file %s set size %d\n", __func__, path, size);
	if(SupportRemoteSize == 0) {
		return 0;
	}
	len = ambafs_get_full_path(dentry, path, (char *)buf + sizeof(buf) - path);
	msg->cmd = AMBAFS_CMD_SET_SIZE;
	stat->size = (u64)size;
	ret = ambafs_rpmsg_exec_timeout(msg, sizeof(struct ambafs_stat_size) + len + 1,250);

	if(ret) {
		SupportRemoteSize = 0;
		AMBAFS_EMSG("%s not support remote size\n", __func__);
		return 0;
	}

	if(msg->parameter[0]) {
		return -EIO;
	}

	return 0;
}

int ambafs_setattr(struct dentry *dentry, struct iattr *attr)
{
	struct inode *inode = dentry->d_inode;
	int error = 0;

	error = setattr_prepare(dentry, attr);
	if (error) {
		return error;
	}
	AMBAFS_DMSG("%s: \r\n", __func__);
	// Notify rtos to change the size
	if (attr->ia_valid & ATTR_SIZE) {
		truncate_setsize(inode, attr->ia_size);
		error = ambafs_remote_set_size(dentry, (u64)attr->ia_size);
	}

	// Notify rtos to change the timestamp
	if (attr->ia_valid & AMBAFS_TIMES_SET_FLAGS) {
		if(inode->i_opflags & AMBAFS_IOP_CREATE_FOR_WRITE){
			inode->i_opflags |= AMBAFS_IOP_SET_TIMESTAMP;
		} else {
			error = ambafs_remote_set_timestamp(dentry, attr);
		}
	}

	// Notice that we don't notify rtos to chmod the file.
	// chmod will be invalid when inode flushing.
	setattr_copy(inode, attr);
	return error;
}

const struct inode_operations ambafs_file_inode_ops = {
	.getattr = ambafs_getattr,
	.setattr = ambafs_setattr,
};
