/*
 * drivers/ambarella-ipc/server/lk_vfs_server.c
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <asm/uaccess.h>
#include <linux/statfs.h>

#include <linux/aipc/aipc.h>
#if !defined(__DEPENDENCY_GENERATION__)
#include "ipcgen/lk_vfs.h"
#define __IPC_SERVER_IMPL__
#include "ipcgen/lk_vfs_tab.i"
#endif

//#define ENABLE_DEBUG_MSG_LKVFS
#ifdef ENABLE_DEBUG_MSG_LKVFS
#define DEBUG_MSG printk
#else
#define DEBUG_MSG(...)
#endif

//#define ENABLE_DEBUG_MSG_LKVFS_CACHE
#ifdef ENABLE_DEBUG_MSG_LKVFS_CACHE
#define DEBUG_CACHE printk
#else
#define DEBUG_CACHE(...)
#endif

#define LKVFS_PATH_LEN	256

extern void lkvfs_bh_init(void);
extern void lkvfs_bh_cleanup(void);
extern void lkvfs_bh_queue(ipc_bh_f func, void *arg, void *result, SVCXPRT *svcxprt);

static struct ipc_prog_s lk_vfs_prog =
{
	.name = "lk_vfs",
	.prog = LK_VFS_PROG,
	.vers = LK_VFS_VERS,
	.table =  (struct ipcgen_table *) &lk_vfs_prog_1_table,
	.nproc = lk_vfs_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};

/* lk_filp_open() */

static bool_t __lk_filp_open_1_svc(struct lk_filp_open_arg *arg, int *res,
				   SVCXPRT *svcxprt)
{
	struct file *filp;

	DEBUG_CACHE("ambcache_inv_range(0x%p, %d)\n", arg->path, LKVFS_PATH_LEN);
	ambcache_inv_range (arg->path, LKVFS_PATH_LEN);
	BUG_ON(strlen(arg->path) >= LKVFS_PATH_LEN);

	DEBUG_MSG("__lk_filp_open_1_svc: path = %s, flag = %d, mode = %d\n",
		arg->path, arg->flag, arg->mode);

	filp = filp_open(arg->path, arg->flag, arg->mode);
	if (IS_ERR(filp) || (filp == NULL))
		*res = -1;
	else
		*res = (int) filp;

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	DEBUG_MSG("__lk_filp_open_1_svc done, flip = 0x%p\n", filp);

	return 1;
}

bool_t lk_filp_open_1_svc(struct lk_filp_open_arg *arg, int *res,
			  struct svc_req *rqstp)
{
	lkvfs_bh_queue((ipc_bh_f) __lk_filp_open_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/* lk_fput() */

static bool_t __lk_fput_1_svc(int *arg, void *res, SVCXPRT *svcxprt)
{
	struct file *filp;

	filp = (struct file *) *arg;

	DEBUG_MSG("__lk_fput_1_svc, arg = 0x%p, *arg = 0x%x\n", arg, *arg);

	fput(filp);

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t lk_fput_1_svc(int *arg, void *res, struct svc_req *rqstp)
{
	lkvfs_bh_queue((ipc_bh_f) __lk_fput_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/* lk_f_op_read() */

static bool_t __lk_f_op_read_1_svc(struct lk_f_op_read_arg *arg, int *res,
				   SVCXPRT *svcxprt)
{
	struct file *filp;
	mm_segment_t oldfs;

	filp = (struct file *) arg->filp;

	if (filp->f_op->read == NULL) {
		*res = -ENODEV;
	} else {
		oldfs = get_fs();
		set_fs(KERNEL_DS);

		DEBUG_CACHE("ambcache_inv_range(0x%p, %d)\n", arg->buf, arg->count);
		ambcache_inv_range (arg->buf, arg->count);

		DEBUG_MSG("__lk_f_op_read_1_svc: 0x%p, 0x%p, 0x%x\n",
				filp, arg->buf, arg->count);

		*res = filp->f_op->read(filp, arg->buf, arg->count,
					&filp->f_pos);

		DEBUG_CACHE("ambcache_clean_range(0x%p, %d)\n", arg->buf, *res);
		ambcache_clean_range (arg->buf, *res);

		DEBUG_MSG("read bytes: %d, filp->f_pos: %lld\n",
			*res, filp->f_pos);

		set_fs(oldfs);
	}

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t lk_f_op_read_1_svc(struct lk_f_op_read_arg *arg, int *res,
			  struct svc_req *rqstp)
{
	lkvfs_bh_queue((ipc_bh_f) __lk_f_op_read_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/* lk_f_op_write() */

static bool_t __lk_f_op_write_1_svc(struct lk_f_op_write_arg *arg, int *res,
				    SVCXPRT *svcxprt)
{
	struct file *filp;
	mm_segment_t oldfs;

	filp = (struct file *) arg->filp;

	if (filp->f_op->write == NULL) {
		*res = -ENODEV;
	} else {
		oldfs = get_fs();
		set_fs(KERNEL_DS);

		DEBUG_CACHE("ambcache_inv_range(0x%p, %d)\n", arg->buf, arg->count);
		ambcache_inv_range (arg->buf, arg->count);

		DEBUG_MSG("__lk_f_op_write_1_svc: 0x%p, 0x%p, 0x%x\n",
				filp, arg->buf, arg->count);

		*res = filp->f_op->write(filp, arg->buf, arg->count,
					 &filp->f_pos);

		DEBUG_MSG("write bytes: %d, filp->f_pos: %lld\n",
					*res, filp->f_pos);

		set_fs(oldfs);
	}

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t lk_f_op_write_1_svc(struct lk_f_op_write_arg *arg, int *res,
			   struct svc_req *rqstp)
{
	lkvfs_bh_queue((ipc_bh_f) __lk_f_op_write_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/* lk_vfs_llseek() */

static bool_t __lk_vfs_llseek_1_svc(struct lk_vfs_llseek_arg *arg, int *res,
				    SVCXPRT *svcxprt)
{
	off_t retval;

	retval = -EINVAL;
	if (arg->origin <= SEEK_MAX) {
		loff_t res = vfs_llseek((struct file *) arg->filp,
				(loff_t) arg->offset, arg->origin);
		retval = res;
		if (res != (loff_t)retval)
			retval = -EOVERFLOW;	/* LFS: should only happen on 32 bit platforms */
	}

	if (retval < 0)
		*res = -1;
	else
		*res = 0;

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t lk_vfs_llseek_1_svc(struct lk_vfs_llseek_arg *arg, int *res,
			struct svc_req *rqstp)
{
	lkvfs_bh_queue((ipc_bh_f) __lk_vfs_llseek_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/* file_pos_read() */

static  bool_t __lk_file_pos_read_1_svc(int *arg, s64 *res, SVCXPRT *svcxprt)
{
	struct file *filp = (struct file *) *arg;

	*res = (s64) filp->f_pos;

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t lk_file_pos_read_1_svc(int *arg, s64 *res, struct svc_req *rqstp)
{
	lkvfs_bh_queue((ipc_bh_f) __lk_file_pos_read_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/* lk_sys_mkdir() */

static  bool_t __lk_sys_mkdir_1_svc(lk_sys_mkdir_arg *arg, int *res, SVCXPRT *svcxprt)
{
	extern long sys_mkdir(const char __user * pathname, int mode);
	mm_segment_t oldfs;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	DEBUG_CACHE("ambcache_inv_range(0x%p, %d)\n", arg->path, LKVFS_PATH_LEN);
	ambcache_inv_range (arg->path, LKVFS_PATH_LEN);
	BUG_ON(strlen(arg->path) >= LKVFS_PATH_LEN);

	DEBUG_MSG("__lk_sys_mkdir_1_svc: path = %s, mode = %d\n",
				arg->path, arg->mode);

	*res = sys_mkdir((const char*) arg->path, arg->mode);

	set_fs(oldfs);

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t lk_sys_mkdir_1_svc(lk_sys_mkdir_arg *arg, int *res, struct svc_req *rqstp)
{
	lkvfs_bh_queue((ipc_bh_f) __lk_sys_mkdir_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/* lk_sys_rmdir() */

static  bool_t __lk_sys_rmdir_1_svc(char **arg, int *res, SVCXPRT *svcxprt)
{
	extern long sys_rmdir(const char __user * pathname);
	mm_segment_t oldfs;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	DEBUG_CACHE("ambcache_inv_range(0x%p, %d)\n", *arg, LKVFS_PATH_LEN);
	ambcache_inv_range (*arg, LKVFS_PATH_LEN);
	BUG_ON(strlen(*arg) >= LKVFS_PATH_LEN);

	DEBUG_MSG("__lk_sys_rmdir_1_svc: path = %s\n", *arg);

	*res = sys_rmdir((const char*) *arg);

	set_fs(oldfs);

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t lk_sys_rmdir_1_svc(char **arg, int *res, struct svc_req *rqstp)
{
	lkvfs_bh_queue((ipc_bh_f) __lk_sys_rmdir_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/* lk_sys_unlink() */

static  bool_t __lk_sys_unlink_1_svc(char **arg, int *res, SVCXPRT *svcxprt)
{
	extern long sys_unlink(const char __user * pathname);
	mm_segment_t oldfs;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	DEBUG_CACHE("ambcache_inv_range(0x%p, %d)\n", *arg, LKVFS_PATH_LEN);
	ambcache_inv_range (*arg, LKVFS_PATH_LEN);
	BUG_ON(strlen(*arg) >= LKVFS_PATH_LEN);

	DEBUG_MSG("__lk_sys_unlink_1_svc: path = %s\n", *arg);

	*res = sys_unlink((const char*) *arg);

	set_fs(oldfs);

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t lk_sys_unlink_1_svc(char **arg, int *res, struct svc_req *rqstp)
{
	lkvfs_bh_queue((ipc_bh_f) __lk_sys_unlink_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/* lk_sys_rename() */

static  bool_t __lk_sys_rename_1_svc(lk_sys_rename_arg *arg, int *res, SVCXPRT *svcxprt)
{
	extern long sys_rename(const char __user * oldname,const char __user * newname);
	mm_segment_t oldfs;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	DEBUG_CACHE("ambcache_inv_range(0x%p, %d)\n", arg->oldname, LKVFS_PATH_LEN);
	DEBUG_CACHE("ambcache_inv_range(0x%p, %d)\n", arg->newname, LKVFS_PATH_LEN);
	ambcache_inv_range (arg->oldname, LKVFS_PATH_LEN);
	ambcache_inv_range (arg->newname, LKVFS_PATH_LEN);
	BUG_ON(strlen(arg->oldname) >= LKVFS_PATH_LEN);
	BUG_ON(strlen(arg->newname) >= LKVFS_PATH_LEN);

	DEBUG_MSG("__lk_sys_rename_1_svc: oldname = %s, newname = %s\n",
				arg->oldname, arg->newname);

	*res = sys_rename((const char*) arg->oldname,
			(const char*) arg->newname);

	set_fs(oldfs);

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t lk_sys_rename_1_svc(lk_sys_rename_arg *arg, int *res, struct svc_req *rqstp)
{
	lkvfs_bh_queue((ipc_bh_f) __lk_sys_rename_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/* lk_getdents() */

struct lk_getdents_callback {
	struct lk_dirent *current_dir;
	struct lk_dirent *previous;
	int count;
	int error;
};

static  bool_t __lk_getdents_1_svc(lk_getdents_arg *arg, int *res, SVCXPRT *svcxprt)
{
	mm_segment_t oldfs;
	struct lk_dirent *lastdirent;
	struct lk_getdents_callback buf;
	int error;
	struct file *file;
	extern int lk_filldir(void * __buf, const char * name, int namlen, loff_t offset,
			   u64 ino, unsigned int d_type);

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	file = (struct file *) arg->filp;

	DEBUG_MSG("__lk_getdents_1_svc: 0x%p, 0x%p, 0x%x\n",
			file, arg->dirent, arg->count);

	error = -EFAULT;
	if (!access_ok(VERIFY_WRITE, arg->dirent, arg->count))
		goto out;

	buf.current_dir = arg->dirent;
	buf.previous = NULL;
	buf.count = arg->count;
	buf.error = 0;

	DEBUG_CACHE("ambcache_inv_range(0x%p, %d)\n", arg->dirent, arg->count);
	ambcache_inv_range (arg->dirent, arg->count);

	error = vfs_readdir(file, lk_filldir, &buf);
	if (error >= 0) {
		error = buf.error;
	}

	lastdirent = buf.previous;
	if (lastdirent) {
		if (put_user(file->f_pos, &lastdirent->d_off))
			error = -EFAULT;
		else
			error = arg->count - buf.count;
	}

	/* Clean cache here to avoid one cache-line corrutption to uITORN. */
	DEBUG_CACHE("ambcache_clean_range(0x%p, %d)\n", arg->dirent, arg->count);
	ambcache_clean_range (arg->dirent, arg->count);

out:
	*res = error;
	set_fs(oldfs);

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t lk_getdents_1_svc(lk_getdents_arg *arg, int *res, struct svc_req *rqstp)
{
	lkvfs_bh_queue((ipc_bh_f) __lk_getdents_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/* lk_sys_stat() */

static  bool_t __lk_sys_stat_1_svc(lk_sys_stat_arg *arg, int *res, SVCXPRT *svcxprt)
{
	extern long sys_newstat(const char __user * filename,struct stat __user * statbuf);
	mm_segment_t oldfs;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	DEBUG_CACHE("ambcache_inv_range(0x%p, %d)\n", arg->pathname, LKVFS_PATH_LEN);
	DEBUG_CACHE("ambcache_inv_range(0x%p, %d)\n", arg->stat, sizeof(struct stat));
	ambcache_inv_range (arg->pathname, LKVFS_PATH_LEN);
	ambcache_inv_range (arg->stat, sizeof(struct stat));

	BUG_ON(strlen(arg->pathname) >= LKVFS_PATH_LEN);

	DEBUG_MSG("__lk_sys_stat_1_svc: path = %s,  stat = 0x%p\n",
				arg->pathname, arg->stat);

	*res = sys_newstat((const char *) arg->pathname,
			(struct stat *) arg->stat);

	DEBUG_CACHE("ambcache_clean_range(0x%p, %d)\n", arg->stat, sizeof(struct stat));
	ambcache_clean_range (arg->stat, sizeof(struct stat));

	set_fs(oldfs);

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t lk_sys_stat_1_svc(lk_sys_stat_arg *arg, int *res, struct svc_req *rqstp)
{
	lkvfs_bh_queue((ipc_bh_f) __lk_sys_stat_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/* lk_sys_lstat() */

static  bool_t __lk_sys_lstat_1_svc(lk_sys_stat_arg *arg, int *res, SVCXPRT *svcxprt)
{
	extern long sys_newlstat(const char __user * filename,struct stat __user * statbuf);
	mm_segment_t oldfs;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	DEBUG_CACHE("ambcache_inv_range(0x%p, %d)\n", arg->pathname, LKVFS_PATH_LEN);
	DEBUG_CACHE("ambcache_inv_range(0x%p, %d)\n", arg->stat, sizeof(struct stat));
	ambcache_inv_range (arg->pathname, LKVFS_PATH_LEN);
	ambcache_inv_range (arg->stat, sizeof(struct stat));

	BUG_ON(strlen(arg->pathname) >= LKVFS_PATH_LEN);

	DEBUG_MSG("__lk_sys_lstat_1_svc: path = %s,  stat = 0x%p\n",
				arg->pathname, arg->stat);

	*res = sys_newlstat((const char *) arg->pathname,
			(struct stat *) arg->stat);

	DEBUG_CACHE("ambcache_clean_range(0x%p, %d)\n", arg->stat, sizeof(struct stat));
	ambcache_clean_range (arg->stat, sizeof(struct stat));

	set_fs(oldfs);

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t lk_sys_lstat_1_svc(lk_sys_stat_arg *arg, int *res, struct svc_req *rqstp)
{
	lkvfs_bh_queue((ipc_bh_f) __lk_sys_lstat_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

static int fpstat(struct file *f, struct stat __user *statbuf)
{
	struct kstat stat;
	int error;
	extern int lk_cp_new_stat(struct kstat *stat, struct stat __user *statbuf);

	BUG_ON(f == NULL);

	if (f) {
		error = vfs_getattr(f->f_path.mnt, f->f_path.dentry, &stat);
	}

	if (!error)
		error = lk_cp_new_stat(&stat, statbuf);

	return error;
}

/* lk_fpstat() */

static  bool_t __lk_fpstat_1_svc(lk_fpstat_arg *arg, int *res, SVCXPRT *svcxprt)
{
	mm_segment_t oldfs;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	DEBUG_CACHE("ambcache_inv_range(0x%p, %d)\n", arg->stat, sizeof(struct stat));
	ambcache_inv_range (arg->stat, sizeof(struct stat));

	DEBUG_MSG("__lk_fpstat_1_svc: filp = 0x%x,  stat = 0x%p\n",
				arg->filp, arg->stat);

	*res = fpstat((struct file *) arg->filp, (struct stat *) arg->stat);

	DEBUG_CACHE("ambcache_clean_range(0x%p, %d)\n", arg->stat, sizeof(struct stat));
	ambcache_clean_range (arg->stat, sizeof(struct stat));

	set_fs(oldfs);

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t lk_fpstat_1_svc(lk_fpstat_arg *arg, int *res, struct svc_req *rqstp)
{
	lkvfs_bh_queue((ipc_bh_f) __lk_fpstat_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/* lk_sys_chdir() */

static  bool_t __lk_sys_chdir_1_svc(char **arg, int *res, SVCXPRT *svcxprt)
{
	extern long sys_chdir(const char __user * filename);
	mm_segment_t oldfs;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	DEBUG_CACHE("ambcache_inv_range(0x%p, %d)\n", *arg, LKVFS_PATH_LEN);
	ambcache_inv_range (*arg, LKVFS_PATH_LEN);
	BUG_ON(strlen(*arg) >= LKVFS_PATH_LEN);

	DEBUG_MSG("__lk_sys_chdir_1_svc: path = %s\n", *arg);

	*res = sys_chdir((const char *) *arg);
	set_fs(oldfs);

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t lk_sys_chdir_1_svc(char **arg, int *res, struct svc_req *rqstp)
{
	lkvfs_bh_queue((ipc_bh_f) __lk_sys_chdir_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/* lk_sys_statfs() */

static  bool_t __lk_sys_statfs_1_svc(lk_sys_statfs_arg *arg, int *res, SVCXPRT *svcxprt)
{
	extern  long sys_statfs(const char __user * path,
					struct statfs __user *buf);
	mm_segment_t oldfs;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	DEBUG_CACHE("ambcache_inv_range(0x%p, %d)\n", arg->pathname, LKVFS_PATH_LEN);
	DEBUG_CACHE("ambcache_inv_range(0x%p, %d)\n", arg->statfs, sizeof(struct statfs));
	ambcache_inv_range (arg->pathname, LKVFS_PATH_LEN);
	ambcache_inv_range (arg->statfs, sizeof(struct statfs));
	BUG_ON(strlen(arg->pathname) >= LKVFS_PATH_LEN);

	DEBUG_MSG("__lk_sys_statfs_1_svc: path = %s,  statfs = 0x%p\n",
				arg->pathname, arg->statfs);

	*res = sys_statfs((const char *) arg->pathname,
			(struct statfs *) arg->statfs);

	DEBUG_CACHE("ambcache_clean_range(0x%p, %d)\n", arg->statfs, sizeof(struct statfs));
	ambcache_clean_range (arg->statfs, sizeof(struct statfs));

	set_fs(oldfs);

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t lk_sys_statfs_1_svc(lk_sys_statfs_arg *arg, int *res, struct svc_req *rqstp)
{
	lkvfs_bh_queue((ipc_bh_f) __lk_sys_statfs_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/* lk_sys_sync() */

static bool_t __lk_sys_sync_1_svc(void *arg, int *res, SVCXPRT *svcxprt)
{
	extern long sys_sync(void);

	DEBUG_MSG("__lk_sys_sync_1_svc()\n");

	*res = sys_sync();

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t lk_sys_sync_1_svc(void *arg, int *res, struct svc_req *rqstp)
{
	lkvfs_bh_queue((ipc_bh_f) __lk_sys_sync_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/* lk_sys_fsync() */

static bool_t __lk_sys_fsync_1_svc(int *arg, int *res, SVCXPRT *svcxprt)
{
	struct file *filp;

	filp = (struct file *) *arg;

	DEBUG_MSG("__lk_sys_fsync_1_svc, arg = 0x%p, *arg = 0x%x\n", arg, *arg);

	if (filp) {
		*res = vfs_fsync(filp, 0);
	} else {
		*res = -1;
	}

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t lk_sys_fsync_1_svc(int *arg, int *res, struct svc_req *rqstp)
{
	lkvfs_bh_queue((ipc_bh_f) __lk_sys_fsync_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/* lk_sys_mount() */

static bool_t __lk_sys_mount_1_svc(struct lk_sys_mount_arg *arg, int *res, SVCXPRT *svcxprt)
{
	extern long sys_mount(char __user *dev_name, char __user *dir_name,
					char __user *type, unsigned long flags,
					void __user *data);
	mm_segment_t oldfs;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	DEBUG_CACHE("ambcache_inv_range(0x%p, %d)\n", arg->dev_name, LKVFS_PATH_LEN);
	DEBUG_CACHE("ambcache_inv_range(0x%p, %d)\n", arg->dir_name, LKVFS_PATH_LEN);
	ambcache_inv_range (arg->dev_name, LKVFS_PATH_LEN);
	ambcache_inv_range (arg->dir_name, LKVFS_PATH_LEN);
	BUG_ON(strlen(arg->dev_name) >= LKVFS_PATH_LEN);
	BUG_ON(strlen(arg->dir_name) >= LKVFS_PATH_LEN);

	if (arg->type_size > 0) {
		DEBUG_CACHE("ambcache_inv_range(0x%p, %d)\n", arg->type, arg->type_size);
		ambcache_inv_range (arg->type, arg->type_size);
	}

	if (arg->data_size > 0) {
		DEBUG_CACHE("ambcache_inv_range(0x%x, %d)\n", (unsigned int) arg->data, arg->data_size);
		ambcache_inv_range ((void *) arg->data, arg->data_size);
	}

	DEBUG_MSG("__lk_sys_mount_1_svc, dev_name = %s, dir_name = %s, "
			"type = %s, flags = 0x%x, data = 0x%x\n",
			arg->dev_name, arg->dir_name,
			arg->type, (unsigned int) arg->flags,
			(unsigned int) arg->data);

	*res = sys_mount(arg->dev_name, arg->dir_name, arg->type,
			arg->flags, (void *)arg->data);

	set_fs(oldfs);

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t lk_sys_mount_1_svc(struct lk_sys_mount_arg *arg, int *res, struct svc_req *rqstp)
{
	lkvfs_bh_queue((ipc_bh_f) __lk_sys_mount_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}


/* lk_sys_umount() */

static bool_t __lk_sys_umount_1_svc(struct lk_sys_umount_arg *arg, int *res, SVCXPRT *svcxprt)
{
	extern long sys_umount(char __user *name, int flags);
	mm_segment_t oldfs;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	DEBUG_CACHE("ambcache_inv_range(0x%p, %d)\n", arg->name, LKVFS_PATH_LEN);
	ambcache_inv_range (arg->name, LKVFS_PATH_LEN);
	BUG_ON(strlen(arg->name) >= LKVFS_PATH_LEN);

	DEBUG_MSG("__lk_sys_umount_1_svc, name = %s, flags = 0x%x\n",
			arg->name, arg->flags);

	*res = sys_umount(arg->name, arg->flags);

	set_fs(oldfs);

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t lk_sys_umount_1_svc(struct lk_sys_umount_arg *arg, int *res, struct svc_req *rqstp)
{
	lkvfs_bh_queue((ipc_bh_f) __lk_sys_umount_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/* lk_vfs_ioctl() */

static bool_t __lk_vfs_ioctl_1_svc(struct lk_vfs_ioctl_arg *arg, int *res, SVCXPRT *svcxprt)
{
	extern int do_vfs_ioctl(struct file *filp, unsigned int fd, unsigned int cmd,
		     unsigned long arg);
	mm_segment_t oldfs;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	/* FIXME: the arg->arg should be do cache operation depends on cmd. */

	DEBUG_MSG("__lk_vfs_ioctl_1_svc, filp = 0x%x, fd = %d, cmd = %d, arg = 0x%x\n",
			arg->filp, arg->fd, arg->cmd, arg->arg);

	*res = do_vfs_ioctl((struct file *)arg->filp, arg->fd,
				arg->cmd, arg->arg);

	set_fs(oldfs);

	svcxprt->rcode = IPC_SUCCESS;
	ipc_svc_sendreply(svcxprt, NULL);

	return 1;
}

bool_t lk_vfs_ioctl_1_svc(struct lk_vfs_ioctl_arg *arg, int *res, struct svc_req *rqstp)
{
	lkvfs_bh_queue((ipc_bh_f) __lk_vfs_ioctl_1_svc,
		     arg, res, rqstp->svcxprt);
	return 1;
}

/*
 * Service initialization.
 */
static int __init lk_vfs_init(void)
{
	lkvfs_bh_init();
	return ipc_svc_prog_register(&lk_vfs_prog);
}

/*
 * Service removal.
 */
static void __exit lk_vfs_cleanup(void)
{
	lkvfs_bh_cleanup();
	ipc_svc_prog_unregister(&lk_vfs_prog);
}


module_init(lk_vfs_init);
module_exit(lk_vfs_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Charles Chiou <cchiou@ambarella.com>");
MODULE_DESCRIPTION("Ambarella IPC: lk_vfs.x");
