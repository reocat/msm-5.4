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

	AMBAFS_DMSG("%s \r\n", __func__);
	stat = ambafs_get_stat(dentry, NULL, buf, sizeof(buf));
	if (stat->type != AMBAFS_STAT_FILE) {
		AMBAFS_DMSG("%s: ambafs_get_stat() failed\n", __func__);
		return -ENOENT;
	}

	ambafs_update_inode(inode, stat);
	return 0;
}

/*
 * request remote core to open a file, returns remote filp
 */
void* ambafs_remote_open(struct dentry *dentry, int flags)
{
	int buf[128];
	struct ambafs_msg *msg  = (struct ambafs_msg*) buf;
	char *path = (char*)&msg->parameter[1];
	char *mode;

	ambafs_get_full_path(dentry, path, (char*)buf + sizeof(buf) - path);

	if ((flags & O_WRONLY) && (flags & O_APPEND)) {
		/* read, write, error if file not exist. We have read-for-write in ambafs, use r+ instead of a */
		mode = "r+";
	} else if ((flags & O_WRONLY) || (flags & O_TRUNC) || (flags & O_CREAT)) {
		/* write, create if file not exist,  destory file content if file exist */
		mode = "w";
	} else if (flags & O_RDWR) {
		if ((flags & O_TRUNC) || (flags & O_CREAT)) {
			/* read, write, create if file not exist,  destory file content if file exist */
			mode = "w+";
		} else {
			/* read, write, error if file not exist, */
			mode = "r+";
		}
	} else {
		/* read, error if file not exist, */
		mode = "r";
	}

	msg->cmd = AMBAFS_CMD_OPEN;
	strncpy((char *)&msg->parameter[0], mode, 4);
	ambafs_rpmsg_exec(msg, 4 + strlen(path) + 1);
	AMBAFS_DMSG("%s %s %s 0x%x\n", __FUNCTION__, path, mode, msg->parameter[0]);
	return (void*)msg->parameter[0];
}

/*
 * request remote core to close a file
 */
void ambafs_remote_close(void *fp)
{
	int buf[16];
	struct ambafs_msg  *msg  = (struct ambafs_msg  *)buf;

	AMBAFS_DMSG("%s %p\n", __FUNCTION__, fp);
	msg->cmd = AMBAFS_CMD_CLOSE;
        msg->parameter[0] = (int)fp;
	ambafs_rpmsg_exec(msg, 4);
}

static int ambafs_file_open(struct inode *inode, struct file *filp)
{
	int ret;

	AMBAFS_DMSG("%s f_mode = 0x%x, f_flags = 0x%x\r\n", __func__, filp->f_mode, filp->f_flags);
	AMBAFS_DMSG("%s O_APPEND = 0x%x O_TRUNC = 0x%x O_CREAT = 0x%x  O_RDWR = 0x%x O_WRONLY=0x%x\r\n",
		__func__, O_APPEND, O_TRUNC, O_CREAT, O_RDWR, O_WRONLY);

	ret = check_stat(inode);
	if ((ret < 0) && (filp->f_mode & FMODE_READ)) {
		/*
		 *  We are here for the case, RTOS delete the file without notify linux to drop inode.
		 *  For read, just return failure.
		 *  For write, just reuse the inode to write the file.
		 */
		return ret;
	}

	if (filp->f_flags & (O_CREAT | O_TRUNC))
		inode->i_opflags |= AMBAFS_IOP_CREATE_FOR_WRITE;

	filp->private_data = ambafs_remote_open(filp->f_dentry, filp->f_flags);
	return 0;
}

static int ambafs_file_release(struct inode *inode, struct file *filp)
{
	struct address_space *mapping = &inode->i_data;
	void *fp = filp->private_data;

	AMBAFS_DMSG("%s %p\n", __FUNCTION__, fp);
	mapping->private_data = fp;
	AMBAFS_DMSG("%s: before filemap_write_and_wait\n", __FUNCTION__);
	filemap_write_and_wait(inode->i_mapping);
	AMBAFS_DMSG("%s after filemap_write_and_wait\n", __FUNCTION__);
	mapping->private_data = NULL;
	ambafs_remote_close(fp);
	inode->i_opflags &= ~AMBAFS_IOP_CREATE_FOR_WRITE;
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

	AMBAFS_DMSG("%s: start=%d end=%d sync=%d\r\n", __func__, (int)start, (int)end, datasync);

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
