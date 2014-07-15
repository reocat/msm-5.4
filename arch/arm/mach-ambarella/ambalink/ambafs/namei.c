#include <linux/kernel.h>
#include <linux/pagemap.h>
#include <linux/writeback.h>
#include <asm/page.h>
#include "ambafs.h"

#define CACHE_VALIDITY_INTERVAL (2*HZ)

/*
 * helper function to trigger/wait a remote command excution
 */
static void exec_cmd(struct inode *inode, struct dentry *dentry,
		void *buf, int size, int cmd)
{
	struct ambafs_msg  *msg  = (struct ambafs_msg  *)buf;
	struct dentry *dir = (struct dentry *)inode->i_private;
	char *path = (char*)msg->parameter;
	int len;

	//dir = d_find_any_alias(inode);
	len = ambafs_get_full_path(dir, path, (char*)buf + size - path);
	path[len] = '/';
	strcpy(path+len+1, dentry->d_name.name);
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

	//AMBAFS_DMSG("ambafs_lookup %s\n", dentry->d_name.name);
	exec_cmd(inode, dentry, msg, sizeof(buf), AMBAFS_CMD_STAT);
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

	//AMBAFS_DMSG("ambafs_create %s %p\n", dentry->d_name.name, dentry);
	exec_cmd(inode, dentry, msg, sizeof(buf), AMBAFS_CMD_CREATE);
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

	//AMBAFS_DMSG113("ambafs_unlink %s\n", dentry->d_name.name);
	exec_cmd(inode, dentry, msg, sizeof(buf), AMBAFS_CMD_DELETE);
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

	AMBAFS_DMSG("ambafs_create %s\n", dentry->d_name.name);
	exec_cmd(inode, dentry, msg, sizeof(buf), AMBAFS_CMD_MKDIR);
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

	AMBAFS_DMSG("ambafs_unlink %s\n", dentry->d_name.name);
	exec_cmd(inode, dentry, msg, sizeof(buf), AMBAFS_CMD_RMDIR);
	if (msg->flag == 0) {
		clear_nlink(dentry->d_inode);
		drop_nlink(inode);
		return 0;
	}
	return -EBUSY;
}

/*
 * move a file or directory
 */
static int ambafs_rename(struct inode *old_dir, struct dentry *old_dentry,
		struct inode *new_dir, struct dentry *new_dentry)
{
	int buf[128];
	struct ambafs_msg  *msg  = (struct ambafs_msg  *)buf;
	struct ambafs_stat *stat = (struct ambafs_stat*)msg->parameter;
	char *path, *end = (char*)buf + sizeof(buf);
	int len;

	AMBAFS_DMSG("ambafs_rename %s-->%s\n",
		old_dentry->d_name.name, new_dentry->d_name.name);

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
 * get inode stat
 */
struct ambafs_stat* ambafs_get_stat(struct inode *inode, void *buf, int size)
{
	struct ambafs_msg *msg = (struct ambafs_msg*)buf;
	struct dentry *dir = (struct dentry*) inode->i_private;
	char *path = (char*)msg->parameter;
	int len;

	len = ambafs_get_full_path(dir, path, (char*)buf + size - path);
	AMBAFS_DMSG("ambafs_getattr %s\n", path);
	msg->cmd = AMBAFS_CMD_STAT;
	ambafs_rpmsg_exec(msg, len+1);

	return (struct ambafs_stat*)msg->parameter;
}

/*
 * get file attriubte, for cache consistency
 */
static int ambafs_getattr(struct vfsmount *mnt, struct dentry *dentry,
		struct kstat *stat)
{
	if (jiffies - dentry->d_time >= CACHE_VALIDITY_INTERVAL) {
		int buf[128];
		struct ambafs_stat *astat;

		astat = ambafs_get_stat(dentry->d_inode, buf, sizeof(buf));
		if (astat->type == AMBAFS_STAT_FILE) {
			ambafs_update_inode(dentry->d_inode, astat);
		}
	}
	return simple_getattr(mnt, dentry, stat);
}


const struct inode_operations ambafs_file_inode_ops = {
	.getattr = ambafs_getattr,
};
