#include "ambafs.h"
#include <linux/aio.h>

/*
 * check inode stats again remote
 */
static int check_stat(struct inode *inode)
{
	int buf[128];
	struct ambafs_stat *stat;
	struct dentry *dentry = (struct dentry*)inode->i_private;

	stat = ambafs_get_stat(dentry, NULL, buf, sizeof(buf));
	if (stat->type != AMBAFS_STAT_FILE) {
		//AMBAFS_EMSG("failed to get stat\n");
		return -ENOENT;
	}

	ambafs_update_inode(inode, stat);
	return 0;
}

/*
 * request remote core to open a file, returns remote filp
 */
void* ambafs_remote_open(struct dentry *dentry, int mode)
{
	int buf[128];
	struct ambafs_msg *msg  = (struct ambafs_msg*) buf;
	char *path = (char*)&msg->parameter[1];

	ambafs_get_full_path(dentry, path, (char*)buf + sizeof(buf) - path);
	//AMBAFS_DMSG("%s %s %X\n", __FUNCTION__, path, mode);
	msg->cmd = AMBAFS_CMD_OPEN;
	msg->parameter[0] = (int)mode;
	ambafs_rpmsg_exec(msg, 4 + strlen(path) + 1);
	return (void*)msg->parameter[0];
}

/*
 * request remote core to close a file
 */
void ambafs_remote_close(void *fp)
{
	int buf[16];
	struct ambafs_msg  *msg  = (struct ambafs_msg  *)buf;

	//AMBAFS_DMSG("%s %p\n", __FUNCTION__, fp);
	msg->cmd = AMBAFS_CMD_CLOSE;
        msg->parameter[0] = (int)fp;
	ambafs_rpmsg_exec(msg, 4);
}

static int ambafs_file_open(struct inode *inode, struct file *filp)
{
	int ret;

	ret = check_stat(inode);
	if ((ret < 0) && (filp->f_mode & FMODE_READ)) {
		/*
		 *  We are here for the case, RTOS delete the file without notify linux to drop inode.
		 *  For read, just return failure.
		 *  For write, just reuse the inode to write the file.
		 */
		return ret;
	}

	filp->private_data = ambafs_remote_open(filp->f_dentry, filp->f_mode);
	return 0;
}

static int ambafs_file_release(struct inode *inode, struct file *filp)
{
	struct address_space *mapping = &inode->i_data;
	void *fp = filp->private_data;

	//AMBAFS_DMSG("%s %p\n", __FUNCTION__, fp);
	mapping->private_data = fp;
	filemap_write_and_wait(inode->i_mapping);
	mapping->private_data = NULL;
	ambafs_remote_close(fp);
	check_stat(inode);
	return 0;
}

/*
 * Sync the file to backing device
 *   The @file is just a place-holder and very likely a newly-created file.
 *   Since the remote FS only allows one file for write, we basically can't
 *   do fsync at all with @file.
 *   We can call generic_file_fsync since it fools the VFS that the file
 *   is synced.
 *   We can't return an error either, otherwise many user-space application
 *   will simply fail.
 */
static int ambafs_file_fsync(struct file *file, loff_t start,  loff_t end,
			int datasync)
{
	/*AMBAFS_DMSG("ambafs_file_fsync start=%d end=%d sync=%d\n",
	    (int)start, (int)end, datasync);*/
	//return generic_file_fsync(file, start, end, datasync);
	return 0;
}

const struct file_operations ambafs_file_ops = {
	.open         = ambafs_file_open,
	.release      = ambafs_file_release,
	.fsync        = ambafs_file_fsync,
	.llseek       = generic_file_llseek,
	.read         = do_sync_read,
        .write        = do_sync_write,
	.aio_read     = generic_file_aio_read,
	.aio_write    = generic_file_aio_write,
	.mmap         = generic_file_mmap,
	.splice_read  = generic_file_splice_read,
};
