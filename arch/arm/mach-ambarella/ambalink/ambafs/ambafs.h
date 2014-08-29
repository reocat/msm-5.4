#ifndef __AMBAFS_H__
#define __AMBAFS_H__

#include <linux/fs.h>
#include <linux/types.h>
#include <linux/backing-dev.h>

#define AMBAFS_CMD_LS_INIT       0
#define AMBAFS_CMD_LS_NEXT       1
#define AMBAFS_CMD_LS_EXIT       2
#define AMBAFS_CMD_STAT          3
#define AMBAFS_CMD_OPEN          4
#define AMBAFS_CMD_CLOSE         5
#define AMBAFS_CMD_READ          6
#define AMBAFS_CMD_WRITE         7
#define AMBAFS_CMD_CREATE        8
#define AMBAFS_CMD_DELETE        9
#define AMBAFS_CMD_MKDIR         10
#define AMBAFS_CMD_RMDIR         11
#define AMBAFS_CMD_RENAME        12
#define AMBAFS_CMD_MOUNT         13
#define AMBAFS_CMD_UMOUNT        14
#define AMBAFS_CMD_RESERVED0     15
#define AMBAFS_CMD_RESERVED1     16
#define AMBAFS_CMD_VOLSIZE       17

#define AMBAFS_STAT_NULL         0
#define AMBAFS_STAT_FILE         1
#define AMBAFS_STAT_DIR          2

#define AMBAFS_INO_MAX_RESERVED  256

/* This is used in i_opflags to skip ambafs_get_stat() while doing ambafs_getattr().
   ambafs_dir_readdir() just get ambafs_stat information, thus no need ambafs_get_stat() again.
   This improves a lots while "ls" 9999 files in one dir. */
#define AMBAFS_IOP_SKIP_GET_STAT	0x0100


#define AMBAFS_DMSG(...)         printk(__VA_ARGS__)
#define AMBAFS_EMSG(...)         printk(KERN_ERR __VA_ARGS__)

struct ambafs_msg {
	unsigned char   cmd;
	unsigned char   flag;
	unsigned short  len;
	void*           xfr;
	int             parameter[0];
};

struct ambafs_stat {
	void*           statp;
	int             type;
	int64_t         size;
	unsigned long   atime;
	unsigned long   mtime;
	unsigned long   ctime;
	char            name[0];
};

struct ambafs_bh {
	int64_t      offset;
	char*        addr;
	int          len;
};

struct ambafs_io {
	void*                   fp;
	int                     total;
	struct ambafs_bh        bh[0];
};

extern int  ambafs_rpmsg_init(void);
extern void ambafs_rpmsg_exit(void);
extern int  ambafs_rpmsg_exec(struct ambafs_msg *, int);
extern int  ambafs_rpmsg_send(struct ambafs_msg *msg, int len,
			void (*cb)(void*, struct ambafs_msg*, int), void*);

extern struct inode* ambafs_new_inode(struct super_block *, struct ambafs_stat*);
extern void   ambafs_update_inode(struct inode *inode, struct ambafs_stat *stat);
extern struct ambafs_stat* ambafs_get_stat(struct dentry *dentry, struct inode *inode, void *buf, int size);

extern int   ambafs_get_full_path(struct dentry *dir, char *buf, int len);
extern void* ambafs_remote_open(struct dentry *dentry, int mode);
extern void  ambafs_remote_close(void *fp);

extern const struct file_operations  ambafs_dir_ops;
extern const struct file_operations  ambafs_file_ops;
extern const struct inode_operations ambafs_dir_inode_ops;
extern const struct inode_operations ambafs_file_inode_ops;
extern const struct address_space_operations ambafs_aops;
extern const struct dentry_operations ambafs_dentry_ops;
extern       struct backing_dev_info ambafs_bdi;
#endif  //__AMBAFS_H__
