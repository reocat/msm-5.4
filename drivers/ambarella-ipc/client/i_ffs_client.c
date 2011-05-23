/*
 * drivers/ambarella-ipc/client/i_ffs_client.c
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

#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/list.h>

#include <linux/aipc/aipc.h>
#include <linux/aipc/i_ffs.h>
#if !defined(__DEPENDENCY_GENERATION__)
#include "ipcgen/i_ffs.h"
#define __IPC_CLIENT_IMPL__
#include "ipcgen/i_ffs_tab.i"
#endif

//#define ENABLE_DEBUG_MSG_IFFS
#ifdef ENABLE_DEBUG_MSG_IFFS
#define DEBUG_MSG printk
#else
#define DEBUG_MSG(...)
#endif

static CLIENT *IPC_i_ffs = NULL;

static struct ipc_prog_s i_ffs_prog =
{
	.name = "i_ffs",
	.prog = I_FFS_PROG,
	.vers = I_FFS_VERS,
	.table = (struct ipcgen_table *) i_ffs_prog_1_table,
	.nproc = i_ffs_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};

/*
 * IPC: i_ffs.vffs_fopen().
 */
int ipc_ffs_fopen(const char *file, const char *mode)
{
	enum clnt_stat stat;
	struct vffs_fopen_arg arg;
	int res;

	ambcache_clean_range ((void *) file, strlen (file) + 1);
	ambcache_clean_range ((void *) mode, strlen (mode) + 1);

	arg.file = (char *) file;
	arg.mode = (char *) mode;

	stat = vffs_fopen_1(&arg, &res, IPC_i_ffs);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return res;
}
EXPORT_SYMBOL(ipc_ffs_fopen);

/*
 * IPC: i_ffs.vffs_fclose().
 */
int ipc_ffs_fclose(int fd)
{
	enum clnt_stat stat;
	int res;

	stat = vffs_fclose_1(&fd, &res, IPC_i_ffs);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return res;
}
EXPORT_SYMBOL(ipc_ffs_fclose);

/*
 * IPC: i_ffs.vffs_fread().
 */
int ipc_ffs_fread(char *buf, unsigned int size, unsigned int nobj, int fd)
{
	enum clnt_stat stat;
	struct vffs_fread_arg arg;
	int res;

	arg.buf = buf;
	arg.size = size;
	arg.nobj = nobj;
	arg.fd = fd;

	stat = vffs_fread_1(&arg, &res, IPC_i_ffs);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	ambcache_inv_range (buf, size);

	return res;
}
EXPORT_SYMBOL(ipc_ffs_fread);

/*
 * IPC: i_ffs.vffs_fwrite().
 */
int ipc_ffs_fwrite(const char *buf, unsigned int size, unsigned int nobj, int fd)
{
	enum clnt_stat stat;
	struct vffs_fwrite_arg arg;
	int res;

	ambcache_clean_range ((void *) buf, size);

	arg.buf = (char *) buf;
	arg.size = size;
	arg.nobj = nobj;
	arg.fd = fd;

	stat = vffs_fwrite_1(&arg, &res, IPC_i_ffs);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return res;
}
EXPORT_SYMBOL(ipc_ffs_fwrite);

/*
 * IPC: i_ffs.vffs_fseek().
 */
int ipc_ffs_fseek(int fd, unsigned int offset, int origin)
{
	enum clnt_stat stat;
	struct vffs_fseek_arg arg;
	int res;

	arg.fd = fd;
	arg.offset = offset;
	arg.origin = origin;

	stat = vffs_fseek_1(&arg, &res, IPC_i_ffs);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return res;
}

/*
 * IPC: i_ffs.vffs_fsync().
 */
int ipc_ffs_fsync(int fd)
{
	enum clnt_stat stat;
	int res;

	stat = vffs_fsync_1(&fd, &res, IPC_i_ffs);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return res;
}
EXPORT_SYMBOL(ipc_ffs_fsync);

/*
 * IPC: i_ffs.vffs_ftell().
 */
u64 ipc_ffs_ftell(int fd)
{
	enum clnt_stat stat;
	u64 res;

	stat = vffs_ftell_1(&fd, &res, IPC_i_ffs);
	if (stat != IPC_SUCCESS)
		return 0;

	return res;
}
EXPORT_SYMBOL(ipc_ffs_ftell);

/*
 * IPC: i_ffs.vffs_feof().
 */
int ipc_ffs_feof(int fd)
{
	enum clnt_stat stat;
	int res;

	stat = vffs_feof_1(&fd, &res, IPC_i_ffs);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return res;
}
EXPORT_SYMBOL(ipc_ffs_feof);

/*
 * IPC: i_ffs.vffs_fstat().
 */
int ipc_ffs_fstat(const char *path, struct ipc_ffs_stat *ffs_stat)
{
	enum clnt_stat stat;
	struct vffs_fstat_res res;

	ambcache_clean_range ((void *) path, strlen (path) + 1);

	stat = vffs_fstat_1((char **) &path, &res, IPC_i_ffs);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	ffs_stat->fs_type	= res.fs_type;
	ffs_stat->fstfz		= res.fstfz;
	ffs_stat->fstact	= res.fstact;
	ffs_stat->fstad		= res.fstad;
	ffs_stat->fstautc	= res.fstautc;
	ffs_stat->fstut		= res.fstut;
	ffs_stat->fstuc		= res.fstuc;
	ffs_stat->fstud		= res.fstud;
	ffs_stat->fstuutc	= res.fstuutc;
	ffs_stat->fstct		= res.fstct;
	ffs_stat->fstcd		= res.fstcd;
	ffs_stat->fstcc		= res.fstcc;
	ffs_stat->fstcutc	= res.fstcutc;
	ffs_stat->fstat		= res.fstat;

	return res.rval;
}
EXPORT_SYMBOL(ipc_ffs_fstat);

/*
 * IPC: i_ffs.vffs_remove().
 */
int ipc_ffs_remove(const char *fname)
{
	enum clnt_stat stat;
	int res;

	ambcache_clean_range ((void *) fname, strlen (fname) + 1);

	stat = vffs_remove_1((char **)&fname, &res, IPC_i_ffs);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return res;
}
EXPORT_SYMBOL(ipc_ffs_remove);

/*
 * IPC: i_ffs.vffs_mkdir().
 */
int ipc_ffs_mkdir(const char *dname)
{
	enum clnt_stat stat;
	int res;

	ambcache_clean_range ((void *) dname, strlen (dname) + 1);

	stat = vffs_mkdir_1((char **)&dname, &res, IPC_i_ffs);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return res;
}
EXPORT_SYMBOL(ipc_ffs_mkdir);

/*
 * IPC: i_ffs.vffs_rmdir().
 */
int ipc_ffs_rmdir(const char *dname)
{
	enum clnt_stat stat;
	int res;

	ambcache_clean_range ((void *) dname, strlen (dname) + 1);

	stat = vffs_rmdir_1((char **)&dname, &res, IPC_i_ffs);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	return res;
}
EXPORT_SYMBOL(ipc_ffs_rmdir);

/*
 * IPC: i_ffs.vffs_fsfirst().
 */
int ipc_ffs_fsfirst(const char *path, unsigned char attr,
			struct ipc_ffs_fsfind *fsfind)
{
	enum clnt_stat stat;
	struct vffs_fsfirst_arg arg;
	struct vffs_fsfind_res res;

	ambcache_clean_range ((void *) path, strlen (path) + 1);
	arg.path = (char *) path;
	arg.attr = attr;

	stat = vffs_fsfirst_1(&arg, &res, IPC_i_ffs);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	fsfind->rval = res.rval;
	if (res.rval == 0) {
		fsfind->vdta = res.vdta;
		fsfind->Time = res.Time;
		fsfind->Date = res.Date;
		fsfind->FileSize = res.FileSize;
		fsfind->Attribute = res.Attribute;
		memcpy(fsfind->FileName, res.FileName, IPC_FFS_SHORT_NAME_LEN);
		memcpy(fsfind->LongName, res.LongName, IPC_FFS_LONG_NAME_LEN);
	} else {
		fsfind->vdta = fsfind->Time = 0;
		fsfind->Date = fsfind->FileSize = fsfind->Attribute = 0;
		memset(fsfind->FileName, 0x0, IPC_FFS_SHORT_NAME_LEN);
		memset(fsfind->LongName, 0x0, IPC_FFS_LONG_NAME_LEN);
	}

	return 0;
}
EXPORT_SYMBOL(ipc_ffs_fsfirst);

/*
 * IPC: i_ffs.vffs_fsnext().
 */
int ipc_ffs_fsnext(struct ipc_ffs_fsfind *fsfind)
{
	enum clnt_stat stat;
	struct vffs_fsfind_res res;

	stat = vffs_fsnext_1(&fsfind->vdta, &res, IPC_i_ffs);
	if (stat != IPC_SUCCESS) {
		pr_err("ipc error: %d (%s)\n", stat, ipc_strerror(stat));
		return -stat;
	}

	fsfind->rval = res.rval;
	if (res.rval == 0) {
		fsfind->vdta = res.vdta;
		fsfind->Time = res.Time;
		fsfind->Date = res.Date;
		fsfind->FileSize = res.FileSize;
		fsfind->Attribute = res.Attribute;
		memcpy(fsfind->FileName, res.FileName, IPC_FFS_SHORT_NAME_LEN);
		memcpy(fsfind->LongName, res.LongName, IPC_FFS_LONG_NAME_LEN);
	} else {
		fsfind->vdta = 0;
	}

	return 0;
}
EXPORT_SYMBOL(ipc_ffs_fsnext);

#if defined(CONFIG_PROC_FS)

extern struct proc_dir_entry *proc_aipc;

static struct proc_dir_entry *proc_ffs = NULL;
static struct proc_dir_entry *proc_map = NULL;
static struct proc_dir_entry *proc_unmap = NULL;
static struct proc_dir_entry *proc_list = NULL;

static struct proc_dir_entry *proc_fcre = NULL;
static struct proc_dir_entry *proc_remove = NULL;
static struct proc_dir_entry *proc_seek = NULL;
static struct proc_dir_entry *proc_size = NULL;
static struct proc_dir_entry *proc_ffs_mkdir = NULL;
static struct proc_dir_entry *proc_rmdir = NULL;
static struct proc_dir_entry *proc_fsfirst = NULL;
static struct proc_dir_entry *proc_fsnext = NULL;

/*
 * Mapped entry to virtual ffs.
 */
struct proc_ffs_list
{
	struct list_head list;
	int vfd;			/* Virtual file descriptor */
	char name[16];			/* As presented on proc fs */
	char filename[256];		/* Actual file name on ffs */
	struct ipc_ffs_stat stat;	/* File stat */
	struct proc_dir_entry *proc;	/* proc entry created for this file */
};

static LIST_HEAD(pl_head);
static int last_mapped_vfd = -1;
static int last_unmapped_vfd = -1;
static struct ipc_ffs_stat last_ffs_stat;
static u64 last_seek_fpos;
static struct ipc_ffs_fsfind last_fsfind;

/*
 * Mapped file read.
 */
static int i_ffs_proc_fs_actual_read(char *page, char **start, off_t off,
				     int count, int *eof, void *data)
{
	struct proc_ffs_list *pl = (struct proc_ffs_list *) data;
	int rval;
	int len = 0;

	*start = page;
#if 0
	rval = ipc_ffs_fseek(pl->vfd, off, IPC_FFS_SEEK_SET);
	if (rval < 0)
		return -EFAULT;
#endif

	rval = ipc_ffs_fread(page, 1, count, pl->vfd);
	if (rval < 0) {
		DEBUG_MSG(KERN_WARNING "actual_read fail\n");
		return -EIO;
	} else
		len = rval;

	if (ipc_ffs_feof(pl->vfd))
		*eof = 1;
#if 0
	DEBUG_MSG(KERN_WARNING "actual_read, *start = %p, off = %d, count = %d, len = %d, eof = %d\n",
			*start, off, count, len, *eof);
#endif
	return len;
}

/*
 * Mapped file write.
 */
static int i_ffs_proc_fs_actual_write(struct file *file,
				      const char __user *buffer,
				      unsigned long count, void *data)
{
	struct proc_ffs_list *pl = (struct proc_ffs_list *) data;
	void *wbuf = NULL;
	int rval, wbuf_size, len;

	wbuf_size = 0x1000;
	wbuf = kmalloc(wbuf_size, GFP_KERNEL);
	if (!wbuf) {
		rval = -ENOMEM;
		goto done;
	}

	if (count > wbuf_size)
		len = wbuf_size;
	else
		len = count;

	if (copy_from_user(wbuf, buffer, len)) {
		rval = -EFAULT;
		goto done;
	}

	rval = ipc_ffs_fwrite(wbuf, 1, len, pl->vfd);
	if (rval < 0) {
		rval = -EIO;
		goto done;
	}

done:
	if (wbuf)
		kfree(wbuf);

	return rval;
}

/*
 * /proc/aipc/ffs/map read function.
 */
static int i_ffs_proc_fs_map_read(char *page, char **start, off_t off,
				  int count, int *eof, void *data)
{
	int len = 0;

	if (last_mapped_vfd < 0) {
		*eof = 1;
		return 0;
	}

	len += sprintf(page, "%.8x\n", last_mapped_vfd);
	*eof = 1;

	return len;
}

/*
 * /proc/aipc/ffs/map write function.
 */
static int i_ffs_proc_fs_map_write(struct file *file,
				   const char __user *buffer,
				   unsigned long count, void *data)
{
	struct proc_ffs_list *pl;
	struct proc_dir_entry *proc;
	char name[16];
	char filename[260];
	char mode[3];
	int vfd;
	int i;

	if (count > sizeof(filename))
		return -EINVAL;

	memset(&filename, 0x0, sizeof(filename));
	if (copy_from_user(filename, buffer, count))
		return -EFAULT;

	/* Strip trailing \r or \n */
	for (i = count - 1 ; i >= 0; i--) {
		if (filename[i] == '\r' || filename[i] == '\n')
			filename[i] = '\0';
		else
			break;
	}

	/* Get the parameters */
	for (; i >= 0; i--) {
		if (filename[i] == ',') {
			strncpy(mode, &filename[i + 1], sizeof(mode));
			filename[i] = '\0';
			break;
		}
	}

	if (i < 0)
		strcpy(mode, "r");

	DEBUG_MSG(KERN_WARNING "map_write: %s, %s\n", filename, mode);

	/* Try to obtain a file descriptor */
	vfd = ipc_ffs_fopen(filename, mode);
	if (vfd < 0)
		return -EIO;

	snprintf(name, sizeof(name), "%.8x", vfd);
	last_mapped_vfd = vfd;

	/* Create a new node */
	pl = kzalloc(sizeof(*pl), GFP_KERNEL);
	if (pl) {
		pl->vfd = vfd;
		strncpy(pl->name, name, sizeof(pl->name));
		strncpy(pl->filename, filename, sizeof(pl->filename));
		ipc_ffs_fstat(filename, &pl->stat);
		pl->proc = NULL;
		list_add_tail(&pl->list, &pl_head);
	} else
		return -ENOMEM;

	/* Create a new proc entry */
	proc = create_proc_entry(pl->name, 0, proc_ffs);
	if (proc) {
		pl->proc = proc;	/* Save pointer to pl */
		proc->data = (void *) pl;
		proc->read_proc = i_ffs_proc_fs_actual_read;
		proc->write_proc = i_ffs_proc_fs_actual_write;
		proc->size = pl->stat.fstfz;
	}

	return count;
}

/*
 * /proc/aipc/ffs/unmap read function.
 */
static int i_ffs_proc_fs_unmap_read(char *page, char **start, off_t off,
				    int count, int *eof, void *data)
{
	int len = 0;

	if (last_unmapped_vfd < 0) {
		*eof = 1;
		return 0;
	}

	len += sprintf(page, "%.8x\n", last_unmapped_vfd);
	*eof = 1;

	return len;
}

/*
 * /proc/aipc/ffs/unmap write function.
 */
static int i_ffs_proc_fs_unmap_write(struct file *file,
				     const char __user *buffer,
				     unsigned long count, void *data)
{
	struct proc_ffs_list *pl;
	struct list_head *pos, *q;
	char name[16];
	int all = 0;
	int vfd = 0;
	int i, name_len;

	if (count > sizeof(name))
		return -EINVAL;

	memset(&name, 0x0, sizeof(name));
	if (copy_from_user(name, buffer, count))
		return -EFAULT;

	/* Strip trailing \r or \n */
	for (i = count - 1 ; i >= 0; i--) {
		if (name[i] == '\r' || name[i] == '\n')
			name[i] = '\0';
		else
			break;
	}
	name_len = i + 1;

	if (strcasecmp(name, "all") == 0) {
		all = 1;
	} else {
		/* Scan the name for exact 8-digit hex character match */
		if (name_len != 8)
			goto done;
		for (i = 0; i < 8; i++) {
			int x;

			if (name[i] >= '0' && name[i] <= '9')
				x = name[i] - '0';
			else if (name[i] >= 'a' && name[i] <= 'f')
				x = name[i] - 'a' + 10;
			else if (name[i] >= 'A' && name[i] <= 'F')
				x = name[i] - 'A' + 10;
			else
				goto done;

			vfd |= (x << ((7 - i) * 4));
		}
	}

	list_for_each_safe(pos, q, &pl_head) {
		pl = list_entry(pos, struct proc_ffs_list, list);
		if (all || pl->vfd == vfd) {
			DEBUG_MSG(KERN_WARNING "unmap_write: %s, %x\n",
					pl->filename, pl->vfd);
			last_unmapped_vfd = pl->vfd;
			ipc_ffs_fclose(pl->vfd);
			remove_proc_entry(pl->name, proc_ffs);
			list_del(pos);
			kfree(pl);
		}
	}

done:

	return count;
}

/*
 * /proc/aipc/ffs/list read function.
 */
static int i_ffs_proc_fs_list_read(char *page, char **start, off_t off,
				   int count, int *eof, void *data)
{
	struct proc_ffs_list *pl;
	struct list_head *pos, *q;
	int len = 0;

	list_for_each_safe(pos, q, &pl_head) {
		pl = list_entry(pos, struct proc_ffs_list, list);
		len += snprintf(page + len, PAGE_SIZE - len,
				"%s%12llu\t%s\n",
				pl->name, pl->stat.fstfz, pl->filename);
		if (len >= PAGE_SIZE)
			break;
	}

	*eof = 1;

	return len;
}

/*
 * /proc/aipc/ffs/create read function.
 */
static int i_ffs_proc_fs_create_read(char *page, char **start, off_t off,
				   int count, int *eof, void *data)
{
	return 0;
}

/*
 * /proc/aipc/ffs/create write function.
 */
static int i_ffs_proc_fs_create_write(struct file *file,
				     const char __user *buffer,
				     unsigned long count, void *data)
{
	return 0;
}

/*
 * /proc/aipc/ffs/remove read function.
 */
static int i_ffs_proc_fs_remove_read(char *page, char **start, off_t off,
				   int count, int *eof, void *data)
{
	return 0;
}

/*
 * /proc/aipc/ffs/remove write function.
 */
static int i_ffs_proc_fs_remove_write(struct file *file,
				     const char __user *buffer,
				     unsigned long count, void *data)
{
	char filename[256];
	int rval, i;

	if (count > sizeof(filename))
		return -EINVAL;

	memset(&filename, 0x0, sizeof(filename));
	if (copy_from_user(filename, buffer, count))
		return -EFAULT;

	/* Strip trailing \r or \n */
	for (i = count - 1 ; i >= 0; i--) {
		if (filename[i] == '\r' || filename[i] == '\n')
			filename[i] = '\0';
		else
			break;
	}

	DEBUG_MSG(KERN_WARNING "remove_write: %s\n", filename);

	rval = ipc_ffs_remove(filename);
	if (rval < 0)
		return -EIO;

	return count;
}

/*
 * /proc/aipc/ffs/seek read function.
 */
static int i_ffs_proc_fs_seek_read(char *page, char **start, off_t off,
				   int count, int *eof, void *data)
{
	int len;

	len = sprintf(page, "0x%llx\n", last_seek_fpos);
	*eof = 1;

	return len;
}

/*
 * /proc/aipc/ffs/seek write function.
 */
 /* FIXME: non-thread safe, add mutex to protect. */
static int i_ffs_proc_fs_seek_write(struct file *file,
				     const char __user *buffer,
				     unsigned long count, void *data)
{
	struct proc_ffs_list *pl;
	struct list_head *pos, *q;
	char param[40];
	char off_str[16];
	char ori_str[8];
	int vfd = 0, offset = 0, origin = 0, rval = 0, len = 0, y = 0;
	int i, x;

	if (count > sizeof(param))
		return -EINVAL;

	memset(&param, 0x0, sizeof(param));
	if (copy_from_user(param, buffer, count))
		return -EFAULT;

	/* Strip trailing \r or \n */
	for (i = count - 1 ; i >= 0; i--) {
		if (param[i] == '\r' || param[i] == '\n')
			param[i] = '\0';
		else
			break;
	}

	/* Get origin paramter */
	for (; i >= 0; i--) {
		if (param[i] == ',') {
			strncpy(ori_str, &param[i + 1], sizeof(ori_str));
			param[i] = '\0';
			break;
		}
	}

	/* Get offset paramter */
	for (; i >= 0; i--) {
		if (param[i] == ',') {
			strncpy(off_str, &param[i + 1], sizeof(off_str));
			param[i] = '\0';
			len = strlen(off_str);
			break;
		}
	}

	DEBUG_MSG(KERN_WARNING "seek_write: %s, %s, %s\n", param, off_str, ori_str);

	/* Translate parameters */
	if (strcmp(ori_str, "set") == 0) {
		origin = IPC_FFS_SEEK_SET;
	} else if (strcmp(ori_str, "cur") == 0) {
		origin = IPC_FFS_SEEK_CUR;
	} else if (strcmp(ori_str, "end") == 0) {
		origin = IPC_FFS_SEEK_END;
	} else {
		return -EINVAL;
	}

	for (i = len - 1; i >= 0; i--) {
		if (off_str[i] >= '0' && off_str[i] <= '9')
			x = off_str[i] - '0';
		else if (off_str[i] >= 'a' && off_str[i] <= 'f')
			x = off_str[i] - 'a' + 10;
		else if (off_str[i] >= 'A' && off_str[i] <= 'F')
			x = off_str[i] - 'A' + 10;
		else
			return -EINVAL;

		offset |= (x << (y++ * 4));
	}

	for (i = 0; i < 8; i++) {
		if (param[i] >= '0' && param[i] <= '9')
			x = param[i] - '0';
		else if (param[i] >= 'a' && param[i] <= 'f')
			x = param[i] - 'a' + 10;
		else if (param[i] >= 'A' && param[i] <= 'F')
			x = param[i] - 'A' + 10;
		else
			return -EINVAL;

		vfd |= (x << ((7 - i) * 4));
	}

	list_for_each_safe(pos, q, &pl_head) {
		pl = list_entry(pos, struct proc_ffs_list, list);
		if (pl->vfd == vfd) {
			rval = 1;
		}
	}

	if (!rval)
		return -EINVAL;

	rval = ipc_ffs_fseek(vfd, offset, origin);
	if (rval < 0)
		return -EIO;

	last_seek_fpos = ipc_ffs_ftell(vfd);

	return count;
}

/*
 * /proc/aipc/ffs/size read function.
 */
static int i_ffs_proc_fs_size_read(char *page, char **start, off_t off,
				   int count, int *eof, void *data)
{
	int len;

	len = sprintf(page, "0x%llx\n", last_ffs_stat.fstfz);
	*eof = 1;

	return len;
}

/*
 * /proc/aipc/ffs/size write function.
 */
static int i_ffs_proc_fs_size_write(struct file *file,
				     const char __user *buffer,
				     unsigned long count, void *data)
{
	char filename[256];
	int rval, i;

	if (count > sizeof(filename))
		return -EINVAL;

	memset(&filename, 0x0, sizeof(filename));
	if (copy_from_user(filename, buffer, count))
		return -EFAULT;

	/* Strip trailing \r or \n */
	for (i = count - 1 ; i >= 0; i--) {
		if (filename[i] == '\r' || filename[i] == '\n')
			filename[i] = '\0';
		else
			break;
	}

	DEBUG_MSG(KERN_WARNING "size_write: %s\n", filename);

	rval = ipc_ffs_fstat(filename, &last_ffs_stat);
	if (rval < 0)
		return -EIO;

	return count;
}

/*
 * /proc/aipc/ffs/mkdir read function.
 */
static int i_ffs_proc_fs_mkdir_read(char *page, char **start, off_t off,
				   int count, int *eof, void *data)
{
	return 0;
}

/*
 * /proc/aipc/ffs/mkdir write function.
 */
static int i_ffs_proc_fs_mkdir_write(struct file *file,
				     const char __user *buffer,
				     unsigned long count, void *data)
{
	char pathname[256];
	int rval, i;

	if (count > sizeof(pathname))
		return -EINVAL;

	memset(&pathname, 0x0, sizeof(pathname));
	if (copy_from_user(pathname, buffer, count))
		return -EFAULT;

	/* Strip trailing \r or \n */
	for (i = count - 1 ; i >= 0; i--) {
		if (pathname[i] == '\r' || pathname[i] == '\n')
			pathname[i] = '\0';
		else
			break;
	}

	DEBUG_MSG(KERN_WARNING "mkdir_write: %s\n", pathname);

	/* Try to obtain a file descriptor */
	rval = ipc_ffs_mkdir(pathname);
	if (rval < 0)
		return -EIO;

	return count;
}

/*
 * /proc/aipc/ffs/rmdir read function.
 */
static int i_ffs_proc_fs_rmdir_read(char *page, char **start, off_t off,
				   int count, int *eof, void *data)
{
	return 0;
}

/*
 * /proc/aipc/ffs/rmdir write function.
 */
static int i_ffs_proc_fs_rmdir_write(struct file *file,
				     const char __user *buffer,
				     unsigned long count, void *data)
{
	char pathname[256];
	int rval, i;

	if (count > sizeof(pathname))
		return -EINVAL;

	memset(&pathname, 0x0, sizeof(pathname));
	if (copy_from_user(pathname, buffer, count))
		return -EFAULT;

	/* Strip trailing \r or \n */
	for (i = count - 1 ; i >= 0; i--) {
		if (pathname[i] == '\r' || pathname[i] == '\n')
			pathname[i] = '\0';
		else
			break;
	}

	DEBUG_MSG(KERN_WARNING "rmdir_write: %s\n", pathname);

	/* Try to obtain a file descriptor */
	rval = ipc_ffs_rmdir(pathname);
	if (rval < 0)
		return -EIO;

	return count;
}

/*
 * /proc/aipc/ffs/fsfirst read function.
 */
static int i_ffs_proc_fs_fsfirst_read(char *page, char **start, off_t off,
				   int count, int *eof, void *data)
{
	int len = 0;

	/* FIXME: how to handle unicode case? */
	/* Assume ASCII now */
	if (last_fsfind.rval == 0) {
		len += sprintf(page, "%s\n", last_fsfind.FileName);
		//len += sprintf(page, "%s\n", last_fsfind.LongName);
	} else {
		len = 0;
	}

	*eof = 1;

	return len;
}

/*
 * /proc/aipc/ffs/fsfirst write function. ipc_ffs_fsfirst
 */
#define ATTR_ALL                (0x007FuL)          /* for fsfirst function */
static int i_ffs_proc_fs_fsfirst_write(struct file *file,
				     const char __user *buffer,
				     unsigned long count, void *data)
{
	char pathname[256];
	int rval, i;

	if (count > sizeof(pathname))
		return -EINVAL;

	memset(&pathname, 0x0, sizeof(pathname));
	if (copy_from_user(pathname, buffer, count))
		return -EFAULT;

	/* Strip trailing \r or \n */
	for (i = count - 1 ; i >= 0; i--) {
		if (pathname[i] == '\r' || pathname[i] == '\n')
			pathname[i] = '\0';
		else
			break;
	}

	DEBUG_MSG(KERN_WARNING "fsfirst_write: %s\n", pathname);

	/* Try to obtain a file descriptor */
	rval = ipc_ffs_fsfirst(pathname, ATTR_ALL, &last_fsfind);
	if (rval < 0)
		return -EIO;

	return count;
}

/*
 * /proc/aipc/ffs/fsnext read function.
 */
static int i_ffs_proc_fs_fsnext_read(char *page, char **start, off_t off,
				   int count, int *eof, void *data)
{
	int len = 0, rval;

	if (off > 0)
		goto done;

	/* FIXME: how to handle unicode case? */
	rval = ipc_ffs_fsnext(&last_fsfind);
	if (rval < 0)
		goto done;

	if (last_fsfind.rval == 0) {
		len += sprintf(page, "%s\n", last_fsfind.FileName);
		//len += sprintf(page, "%s\n", last_fsfind.LongName);
	} else {
		len = 0;
	}

done:
	*eof = 1;
#if 0
	DEBUG_MSG(KERN_WARNING "fsnext_read, *start = %p, off = %d, count = %d,"
		" len = %d, eof = %d\n", *start, off, count, len, *eof);
#endif

	return len;
}

/*
 * /proc/aipc/ffs/fsnext write function.
 */
static int i_ffs_proc_fs_fsnext_write(struct file *file,
				     const char __user *buffer,
				     unsigned long count, void *data)
{
	return 0;
}

/*
 * Install /procfs/aipc/ffs
 */
static void i_ffs_procfs_init(void)
{
	proc_ffs = proc_mkdir("ffs", proc_aipc);
	if (proc_ffs == NULL) {
		pr_err("create ffs dir failed!\n");
		return;
	}

	proc_map = create_proc_entry("map", 0, proc_ffs);
	if (proc_map) {
		proc_map->data = NULL;
		proc_map->read_proc = i_ffs_proc_fs_map_read;
		proc_map->write_proc = i_ffs_proc_fs_map_write;
	}

	proc_unmap = create_proc_entry("unmap", 0, proc_ffs);
	if (proc_unmap) {
		proc_unmap->data = NULL;
		proc_unmap->read_proc = i_ffs_proc_fs_unmap_read;
		proc_unmap->write_proc = i_ffs_proc_fs_unmap_write;
	}

	proc_list = create_proc_entry("list", 0, proc_ffs);
	if (proc_list) {
		proc_list->data = NULL;
		proc_list->read_proc = i_ffs_proc_fs_list_read;
		proc_list->write_proc = NULL;
	}

	proc_fcre = create_proc_entry("create", 0, proc_ffs);
	if (proc_fcre) {
		proc_fcre->data = NULL;
		proc_fcre->read_proc = i_ffs_proc_fs_create_read;
		proc_fcre->write_proc = i_ffs_proc_fs_create_write;
	}

	proc_remove = create_proc_entry("remove", 0, proc_ffs);
	if (proc_remove) {
		proc_remove->data = NULL;
		proc_remove->read_proc = i_ffs_proc_fs_remove_read;
		proc_remove->write_proc = i_ffs_proc_fs_remove_write;
	}

	proc_seek = create_proc_entry("seek", 0, proc_ffs);
	if (proc_seek) {
		proc_seek->data = NULL;
		proc_seek->read_proc = i_ffs_proc_fs_seek_read;
		proc_seek->write_proc = i_ffs_proc_fs_seek_write;
	}

	proc_size = create_proc_entry("size", 0, proc_ffs);
	if (proc_size) {
		proc_size->data = NULL;
		proc_size->read_proc = i_ffs_proc_fs_size_read;
		proc_size->write_proc = i_ffs_proc_fs_size_write;
	}

	proc_ffs_mkdir = create_proc_entry("mkdir", 0, proc_ffs);
	if (proc_ffs_mkdir) {
		proc_ffs_mkdir->data = NULL;
		proc_ffs_mkdir->read_proc = i_ffs_proc_fs_mkdir_read;
		proc_ffs_mkdir->write_proc = i_ffs_proc_fs_mkdir_write;
	}


	proc_rmdir = create_proc_entry("rmdir", 0, proc_ffs);
	if (proc_rmdir) {
		proc_rmdir->data = NULL;
		proc_rmdir->read_proc = i_ffs_proc_fs_rmdir_read;
		proc_rmdir->write_proc = i_ffs_proc_fs_rmdir_write;
	}

	proc_fsfirst = create_proc_entry("fsfirst", 0, proc_ffs);
	if (proc_fsfirst) {
		proc_fsfirst->data = NULL;
		proc_fsfirst->read_proc = i_ffs_proc_fs_fsfirst_read;
		proc_fsfirst->write_proc = i_ffs_proc_fs_fsfirst_write;
	}


	proc_fsnext = create_proc_entry("fsnext", 0, proc_ffs);
	if (proc_fsnext) {
		proc_fsnext->data = NULL;
		proc_fsnext->read_proc = i_ffs_proc_fs_fsnext_read;
		proc_fsnext->write_proc = i_ffs_proc_fs_fsnext_write;
	}
}

/*
 * Uninstall /proc/aipc/ffs
 */
static void i_ffs_procfs_cleanup(void)
{
	struct proc_ffs_list *pl;
	struct list_head *pos, *q;

	if (proc_map) {
		remove_proc_entry("map", proc_ffs);
		proc_map = NULL;
	}

	if (proc_unmap) {
		remove_proc_entry("unmap", proc_ffs);
		proc_unmap = NULL;
	}

	if (proc_list) {
		remove_proc_entry("list", proc_ffs);
		proc_list = NULL;
	}

	if (proc_fcre) {
		remove_proc_entry("create", proc_ffs);
		proc_fcre = NULL;
	}

	if (proc_remove) {
		remove_proc_entry("remove", proc_ffs);
		proc_remove = NULL;
	}

	if (proc_seek) {
		remove_proc_entry("seek", proc_ffs);
		proc_seek = NULL;
	}

	if (proc_seek) {
		remove_proc_entry("seek", proc_ffs);
		proc_seek = NULL;
	}

	if (proc_size) {
		remove_proc_entry("size", proc_ffs);
		proc_size = NULL;
	}

	if (proc_ffs_mkdir) {
		remove_proc_entry("mkdir", proc_ffs);
		proc_ffs_mkdir = NULL;
	}

	if (proc_rmdir) {
		remove_proc_entry("rmdir", proc_ffs);
		proc_rmdir = NULL;
	}

	if (proc_fsfirst) {
		remove_proc_entry("fsfirst", proc_ffs);
		proc_fsfirst = NULL;
	}

	if (proc_fsnext) {
		remove_proc_entry("fsnext", proc_ffs);
		proc_fsnext = NULL;
	}

	list_for_each_safe(pos, q, &pl_head) {
		pl = list_entry(pos, struct proc_ffs_list, list);
		ipc_ffs_fclose(pl->vfd);
		remove_proc_entry(pl->name, proc_ffs);
		list_del(pos);
		kfree(pl);
	}

	remove_proc_entry("ffs", proc_aipc);
	proc_ffs = NULL;
}

#endif  /* CONFIG_PROC_FS */

/*
 * Client initialization.
 */
static int __init i_ffs_init(void)
{
	IPC_i_ffs = ipc_clnt_prog_register(&i_ffs_prog);
	if (IPC_i_ffs == NULL)
		return -EPERM;

#if defined(CONFIG_PROC_FS)
	i_ffs_procfs_init();
#endif

	return 0;
}

/*
 * Client exit.
 */
static void __exit i_ffs_cleanup(void)
{
	if (IPC_i_ffs) {
		ipc_clnt_prog_unregister(&i_ffs_prog, IPC_i_ffs);
		IPC_i_ffs = NULL;
	}

#if defined(CONFIG_PROC_FS)
	i_ffs_procfs_cleanup();
#endif
}

module_init(i_ffs_init);
module_exit(i_ffs_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Charles Chiou <cchiou@ambarella.com>");
MODULE_DESCRIPTION("Ambarella IPC: i_ffs.x");
