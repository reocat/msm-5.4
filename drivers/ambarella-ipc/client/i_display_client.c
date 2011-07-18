/*
 * drivers/ambarella-ipc/client/i_dsp_client.c
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
#include <linux/aipc/i_display.h>
#if !defined(__DEPENDENCY_GENERATION__)
#include "ipcgen/i_display.h"
#define __IPC_CLIENT_IMPL__
#include "ipcgen/i_display_tab.i"
#endif

#define DEBUG_VDSP_CLINT	0

#if DEBUG_VDSP_CLINT
#define DEBUG_MSG_VDSP pr_notice
#else
#define DEBUG_MSG_VDSP(...)
#endif

static CLIENT *IPC_i_display_dsp = NULL;
static CLIENT *IPC_i_display_osd = NULL;

static struct ipc_prog_s i_display_dsp_prog =
{
	.name = "i_display_dsp",
	.prog = I_DISPLAY_DSP_PROG,
	.vers = I_DISPLAY_DSP_VERS,
	.table = (struct ipcgen_table *) i_display_dsp_prog_1_table,
	.nproc = i_display_dsp_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};

static struct ipc_prog_s i_display_osd_prog =
{
	.name = "i_display_osd",
	.prog = I_DISPLAY_OSD_PROG,
	.vers = I_DISPLAY_OSD_VERS,
	.table = (struct ipcgen_table *) i_display_osd_prog_1_table,
	.nproc = i_display_osd_prog_1_nproc,
	.next = NULL,
	.prev = NULL,
};

struct myvdspdrv
{
	struct vdspdrv vdspdrv;
	struct vdspdrv_osd osd[2];
};

static struct myvdspdrv G_vdspdrv;
struct vdspdrv *vdspdrv;
EXPORT_SYMBOL(vdspdrv);
struct vdspdrv_osd *vdspdrv_osd[2];
EXPORT_SYMBOL(vdspdrv_osd);

/*
 * Refresh osd state.
 */
int vdspdrv_refresh_osd(int id)
{
	enum clnt_stat stat;
	int rval = 0;
	struct i_display_osd_info osdinfo;

	DEBUG_MSG_VDSP ("[ipc] vdspdrv_refresh_osd (%d)\n", id);

	stat = i_display_osd_get_info_1(&id, &osdinfo, IPC_i_display_osd);
	if (stat == IPC_SUCCESS) {
		struct vdspdrv_osd *osd = vdspdrv_osd[id];
		int i;

		osd->enabled = osdinfo.enabled;

		osd->interlaced = osdinfo.interlaced;
		osd->colorformat = osdinfo.colorformat;
		osd->bitsperpixel = osdinfo.bitsperpixel;

		osd->flip = osdinfo.flip;

		osd->offset_x = osdinfo.offset_x;
		osd->offset_y = osdinfo.offset_y;
		osd->width = osdinfo.width;
		osd->height = osdinfo.height;
		osd->pitch = osdinfo.pitch;

		osd->repeat_field = osdinfo.repeat_field;

		osd->rescaler_en = osdinfo.rescaler_en;
		osd->premultiplied = osdinfo.premultiplied;
		osd->input_width = osdinfo.input_width;
		osd->input_height = osdinfo.input_height;

		osd->global_blend = osdinfo.global_blend;
		osd->transparent_color = osdinfo.transparent_color;
		osd->transparent_color_en = osdinfo.transparent_color_en;

		osd->csc_en = osdinfo.csc_en;
		memcpy (osd->csc_parms, osdinfo.csc_parms, sizeof (osd->csc_parms));

		osd->zbuf0 = osdinfo.zbuf0;
		osd->zbuf1 = osdinfo.zbuf1;
		osd->zbuf2 = osdinfo.zbuf2;
		osd->zbuf3 = osdinfo.zbuf3;
		osd->osdupdptr = osdinfo.osdupdptr;

		DEBUG_MSG_VDSP ("[ipc] i_display_osd_get_info_1 (%d, %08x) => \n",
			   id, osd);
		DEBUG_MSG_VDSP ("enabled = %d\n", osd->enabled);
		DEBUG_MSG_VDSP ("interlaced = %d\n", osd->interlaced);
		DEBUG_MSG_VDSP ("colorformat = %d\n", osd->colorformat);
		DEBUG_MSG_VDSP ("bitsperpixel = %d\n", osd->bitsperpixel);
		DEBUG_MSG_VDSP ("flip = %d\n", osd->flip);
		DEBUG_MSG_VDSP ("offset_x = %d\n", osd->offset_x);
		DEBUG_MSG_VDSP ("offset_y = %d\n", osd->offset_y);
		DEBUG_MSG_VDSP ("width = %d\n", osd->width);
		DEBUG_MSG_VDSP ("height = %d\n", osd->height);
		DEBUG_MSG_VDSP ("pitch = %d\n", osd->pitch);
		DEBUG_MSG_VDSP ("repeat_field = %d\n", osd->repeat_field);
		DEBUG_MSG_VDSP ("rescaler_en = %d\n", osd->rescaler_en);
		DEBUG_MSG_VDSP ("premultiplied = %d\n", osd->premultiplied);
		DEBUG_MSG_VDSP ("input_width = %d\n", osd->input_width);
		DEBUG_MSG_VDSP ("input_height = %d\n", osd->input_height);
		DEBUG_MSG_VDSP ("csc_en = %d\n", osd->csc_en);
		for (i = 0; i < 9; i++) {
			DEBUG_MSG_VDSP ("csc_parms[%d] = %08x\n", i, osd->csc_parms[i]);
		}
		DEBUG_MSG_VDSP ("global_blend = %d\n", osd->global_blend);
		DEBUG_MSG_VDSP ("transparent_color = %d\n", osd->transparent_color);
		DEBUG_MSG_VDSP ("transparent_color_en = %d\n", osd->transparent_color_en);
		DEBUG_MSG_VDSP ("zbuf0 = %08x\n", (u32) osd->zbuf0);
		DEBUG_MSG_VDSP ("zbuf1 = %08x\n", (u32) osd->zbuf1);
		DEBUG_MSG_VDSP ("zbuf2 = %08x\n", (u32) osd->zbuf2);
		DEBUG_MSG_VDSP ("zbuf3 = %08x\n", (u32) osd->zbuf3);
		DEBUG_MSG_VDSP ("osdupdptr = %08x\n", (u32) osd->osdupdptr);	
	} else {
		rval = -EIO;
		goto done;
	}
	
	rval = 0;

done:
	DEBUG_MSG_VDSP ("[ipc] vdspdrv_refresh_osd END: %d\n", rval);

	return rval;
}
EXPORT_SYMBOL(vdspdrv_refresh_osd);

/*
 * Refresh vdspdrv state.
 */
int vdspdrv_refresh(void)
{
	enum clnt_stat stat;
	int rval = 0;
	struct i_display_dsp_drvinfo drvinfo;
	struct i_display_dsp_opinfo opinfo;
	struct i_display_dsp_histinfo histinfo;

	DEBUG_MSG_VDSP ("[ipc] vdspdrv_refresh\n");

	stat = i_display_dsp_active_1(NULL, &vdspdrv->active, IPC_i_display_dsp);
	if (stat != IPC_SUCCESS) {
		rval = -EIO;
		goto done;
	}
	DEBUG_MSG_VDSP ("[ipc] i_display_dsp_active_1 => %d\n", vdspdrv->active);

	stat = i_display_dsp_get_drvinfo_1(NULL, &drvinfo, IPC_i_display_dsp);
	if (stat != IPC_SUCCESS) {
		rval = -EIO;
		goto done;
	}
	vdspdrv->initdata = (unsigned int) drvinfo.initdata;
	vdspdrv->cmdsize = drvinfo.cmdsize;
	DEBUG_MSG_VDSP ("[ipc] i_display_dsp_get_drvinfo_1 => %08x %d\n",
		vdspdrv->initdata, vdspdrv->cmdsize);

	stat = i_display_dsp_get_opinfo_1(NULL, &opinfo, IPC_i_display_dsp);
	if (stat != IPC_SUCCESS) {
		rval = -EIO;
		goto done;
	}
	vdspdrv->opmode = opinfo.opmode;
	vdspdrv->bufaddr = opinfo.bufaddr;
	vdspdrv->bufsize = opinfo.bufsize;
	DEBUG_MSG_VDSP ("[ipc] i_display_dsp_get_opinfo_1 => %d %08x %d\n",
		vdspdrv->opmode, vdspdrv->bufaddr, vdspdrv->bufsize);

	stat = i_display_dsp_get_histinfo_1(NULL, &histinfo, IPC_i_display_dsp);
	if (stat != IPC_SUCCESS) {
		rval = -EIO;
		goto done;
	}
	vdspdrv->maxcapcmds = histinfo.maxcapcmds;
	vdspdrv->ncapcmds = histinfo.ncapcmds;
	vdspdrv->capcmds = (unsigned int *) histinfo.capcmds;
	vdspdrv->maxhistory = histinfo.maxhistory;
	vdspdrv->nhistory = histinfo.nhistory;
	vdspdrv->history = histinfo.history;
	DEBUG_MSG_VDSP ("[ipc] i_display_dsp_get_histinfo_1 => %08x %d %d %08x %d %d\n",
		vdspdrv->capcmds, vdspdrv->ncapcmds, vdspdrv->maxcapcmds,
		vdspdrv->history, vdspdrv->nhistory, vdspdrv->maxhistory);
	
	rval = 0;

done:
	DEBUG_MSG_VDSP ("[ipc] display_refresh END: %d\n", rval);

	return rval;
}
EXPORT_SYMBOL(vdspdrv_refresh);

/*
 * Submit a dsp command to vdspdrv for execution.
 */
int vdspdrv_put_cmd(void *dspcmd)
{
	enum clnt_stat stat;
	int rval = 0;

	DEBUG_MSG_VDSP ("[ipc] vdspdrv_put_cmd (%08x)\n", dspcmd);

	stat = i_display_dsp_put_cmd_1(dspcmd, &rval, IPC_i_display_dsp);
	if (stat != IPC_SUCCESS)
		rval = -EIO;

	return rval;
}
EXPORT_SYMBOL(vdspdrv_put_cmd);

/*
 * Add a command to be captured.
 */
int vdspdrv_add_capcmd(unsigned int cmd)
{
	enum clnt_stat stat;
	int rval = 0;

	DEBUG_MSG_VDSP ("[ipc] vdspdrv_add_capcmd (%08x)\n", cmd);

	stat = i_display_dsp_add_capcmd_1(&cmd, &rval,IPC_i_display_dsp);
	if (stat != IPC_SUCCESS)
		rval = -EIO;

	return rval;
}
EXPORT_SYMBOL(vdspdrv_add_capcmd);

/*
 * Delete a command from being captured.
 */
int vdspdrv_del_capcmd(unsigned int cmd)
{
	enum clnt_stat stat;
	int rval = 0;

	DEBUG_MSG_VDSP ("[ipc] vdspdrv_del_capcmd (%08x)\n", cmd);

	stat = i_display_dsp_del_capcmd_1(&cmd, &rval, IPC_i_display_dsp);
	if (stat != IPC_SUCCESS)
		rval = -EIO;

	return 0;
}
EXPORT_SYMBOL(vdspdrv_del_capcmd);

/*
 * DSP command history recall.
 */
void *vdspdrv_recall_cmd(unsigned int cmd, int voutid)
{
	enum clnt_stat stat;
	struct i_display_dsp_recall_cmd_arg arg;
	struct i_display_dsp_recall_cmd_res res;

	DEBUG_MSG_VDSP ("[ipc] vdspdrv_recall_cmd (%08x, %d)\n", cmd, voutid);

	arg.cmd = cmd;
	arg.voutid = voutid;
	res.dspcmd = NULL;
	stat = i_display_dsp_recall_cmd_1(&arg, &res, IPC_i_display_dsp);

	return res.dspcmd;
}
EXPORT_SYMBOL(vdspdrv_recall_cmd);

/*
 * Take over control from vdspdrv.
 */
int vdspdrv_takeover(void)
{
	enum clnt_stat stat;
	int rval;

	DEBUG_MSG_VDSP ("[ipc] vdspdrv_takeover\n");

	stat = i_display_dsp_takeover_1(NULL, &rval, IPC_i_display_dsp);
	if (stat != IPC_SUCCESS)
		return -EIO;

	return rval;
}
EXPORT_SYMBOL(vdspdrv_takeover);

/*
 * Hand back control to vdspdrv.
 */
int vdspdrv_handback(void)
{
	enum clnt_stat stat;
	int rval;

	DEBUG_MSG_VDSP ("[ipc] vdspdrv_handback\n");

	stat = i_display_dsp_handback_1(NULL, &rval, IPC_i_display_dsp);
	if (stat != IPC_SUCCESS)
		return -EIO;

	return rval;
}
EXPORT_SYMBOL(vdspdrv_handback);

/*
 * Get status of video plane.
 */
int vdspdrv_video_plane_enabled(int voutid)
{
	enum clnt_stat stat;
	int rval;

	DEBUG_MSG_VDSP ("[ipc] vdspdrv_video_plane_enabled (%d)\n", voutid);

	stat = i_display_osd_video_plane_enabled_1(&voutid, &rval, IPC_i_display_osd);
	if (stat != IPC_SUCCESS)
		return -EIO;

	DEBUG_MSG_VDSP ("[ipc] => %d\n", rval);

	return rval;
}
EXPORT_SYMBOL(vdspdrv_video_plane_enabled);


/*
 * Enable or disable the video plane.
 */
int vdspdrv_en_video_plane(int voutid, int enable)
{
	enum clnt_stat stat;
	struct i_display_osd_enable_arg arg;
	int rval;

	DEBUG_MSG_VDSP ("[ipc] vdspdrv_en_video_plane (%d, %d)\n", voutid, enable);

	arg.voutid = voutid;
	arg.enable = enable;
	stat = i_display_osd_en_video_plane_1(&arg, &rval, IPC_i_display_osd);
	if (stat != IPC_SUCCESS)
		return -EIO;

	DEBUG_MSG_VDSP ("[ipc] => %d\n", rval);

	return rval;
}
EXPORT_SYMBOL(vdspdrv_en_video_plane);

/*
 * Request OSD activities be suspended.
 */
int vdspdrv_osd_suspend(void)
{
	enum clnt_stat stat;
	int rval;

	DEBUG_MSG_VDSP ("[ipc] vdspdrv_osd_suspend ()\n");

	stat = i_display_osd_suspend_activities_1(NULL, &rval, IPC_i_display_osd);
	if (stat != IPC_SUCCESS)
		return -EIO;

	return rval;
}
EXPORT_SYMBOL(vdspdrv_osd_suspend);

/*
 * Request OSD activities be resumed.
 */
int vdspdrv_osd_resume(void)
{
	enum clnt_stat stat;
	int rval;

	DEBUG_MSG_VDSP ("[ipc] vdspdrv_osd_resume ()\n");

	stat = i_display_osd_resume_activities_1(NULL, &rval, IPC_i_display_osd);
	if (stat != IPC_SUCCESS)
		return -EIO;

	return rval;
}
EXPORT_SYMBOL(vdspdrv_osd_resume);

void *vdspdrv_osd_get_clut(int voutid)
{
	enum clnt_stat stat;
	struct i_display_osd_clut res;

	DEBUG_MSG_VDSP ("[ipc] vdspdrv_osd_get_clut (%d)\n", voutid);

	stat = i_display_osd_get_clut_1(&voutid, &res, IPC_i_display_osd);
	if (stat != IPC_SUCCESS)
		return NULL;

	DEBUG_MSG_VDSP ("[ipc] => %d, %08x\n", res.voutid, res.clut);

	return res.clut;

}
EXPORT_SYMBOL(vdspdrv_osd_get_clut);

int vdspdrv_osd_set_clut(int voutid, void *clut)
{
	enum clnt_stat stat;
	struct i_display_osd_clut arg;
	int rval;

	DEBUG_MSG_VDSP ("[ipc] vdspdrv_osd_set_clut (%d, %08x)\n", voutid, clut);

	arg.voutid = voutid;
	arg.clut = clut;
	stat = i_display_osd_set_clut_1(&arg, &rval, IPC_i_display_osd);
	if (stat != IPC_SUCCESS)
		return -EIO;

	DEBUG_MSG_VDSP ("[ipc] => %d\n", rval);

	return rval;
}
EXPORT_SYMBOL(vdspdrv_osd_set_clut);

int vdspdrv_osd_enable(int voutid, int enable)
{
	enum clnt_stat stat;
	struct i_display_osd_enable_arg arg;
	int rval;

	DEBUG_MSG_VDSP ("[ipc] vdspdrv_osd_enable (%d, %d)\n", voutid, enable);

	arg.voutid = voutid;
	arg.enable = enable;
	stat = i_display_osd_enable_1(&arg, &rval, IPC_i_display_osd);
	if (stat != IPC_SUCCESS)
		return -EIO;

	return rval;
}
EXPORT_SYMBOL(vdspdrv_osd_enable);

int vdspdrv_osd_flip(int voutid, int flip)
{
	enum clnt_stat stat;
	struct i_display_osd_flip_arg arg;
	int rval;

	DEBUG_MSG_VDSP ("[ipc] vdspdrv_osd_flip (%d, %d)\n", voutid, flip);

	arg.voutid = voutid;
	arg.flip = flip;
	stat = i_display_osd_flip_1(&arg, &rval, IPC_i_display_osd);
	if (stat != IPC_SUCCESS)
		return -EIO;

	return rval;
}
EXPORT_SYMBOL(vdspdrv_osd_flip);

int display_osd_switch(int voutid)
{
	enum clnt_stat stat;
	//struct i_display_osd_setbuf_arg arg;
	int rval;

	//DEBUG_MSG_VDSP ("[ipc] display_osd_switch (%d, %08x)\n", voutid, ptr);

	//arg.voutid = voutid;
	//arg.ptr = ptr;
	stat = i_display_osd_switch_1(&voutid, &rval, IPC_i_display_osd);
	if (stat != IPC_SUCCESS)
		return -EIO;

	return rval;
}
EXPORT_SYMBOL(display_osd_switch);

int display_osd_setbuf(int voutid, void *ptr1, void *ptr2, int width, int height, int pitch)
{
	enum clnt_stat stat;
	struct i_display_osd_setbuf_arg arg;
	int rval;

	//DEBUG_MSG_VDSP ("[ipc] dispaly_osd_setbuf (%d, %08x)\n", voutid, ptr);

	arg.voutid = voutid;
	arg.ptr1 = ptr1;
	arg.ptr2 = ptr2;
	arg.width = width;
	arg.height = height;
	arg.pitch = pitch;
	stat = i_display_osd_setbuf_1(&arg, &rval, IPC_i_display_osd);
	if (stat != IPC_SUCCESS)
		return -EIO;

	return rval;
}
EXPORT_SYMBOL(display_osd_setbuf);

int display_osd_apply(int voutid, struct vdspdrv_osd *osd)
{
	enum clnt_stat stat;
	struct i_display_osd_apply_arg arg;
	int rval;

	DEBUG_MSG_VDSP ("[ipc] display_osd_apply (%d, %08x)\n", voutid, osd);
	DEBUG_MSG_VDSP ("enabled = %d\n", osd->enabled);
	DEBUG_MSG_VDSP ("interlaced = %d\n", osd->interlaced);
	DEBUG_MSG_VDSP ("colorformat = %d\n", osd->colorformat);
	DEBUG_MSG_VDSP ("bitsperpixel = %d\n", osd->bitsperpixel);
	DEBUG_MSG_VDSP ("flip = %d\n", osd->flip);
	DEBUG_MSG_VDSP ("offset_x = %d\n", osd->offset_x);
	DEBUG_MSG_VDSP ("offset_y = %d\n", osd->offset_y);
	DEBUG_MSG_VDSP ("width = %d\n", osd->width);
	DEBUG_MSG_VDSP ("height = %d\n", osd->height);
	DEBUG_MSG_VDSP ("pitch = %d\n", osd->pitch);
	DEBUG_MSG_VDSP ("repeat_field = %d\n", osd->repeat_field);
	DEBUG_MSG_VDSP ("rescaler_en = %d\n", osd->rescaler_en);
	DEBUG_MSG_VDSP ("premultiplied = %d\n", osd->premultiplied);
	DEBUG_MSG_VDSP ("input_width = %d\n", osd->input_width);
	DEBUG_MSG_VDSP ("input_height = %d\n", osd->input_height);
	DEBUG_MSG_VDSP ("csc_en = %d\n", osd->csc_en);
	DEBUG_MSG_VDSP ("csc_parms[0] = %08x\n", osd->csc_parms[0]);
	DEBUG_MSG_VDSP ("csc_parms[1] = %08x\n", osd->csc_parms[1]);
	DEBUG_MSG_VDSP ("csc_parms[2] = %08x\n", osd->csc_parms[2]);
	DEBUG_MSG_VDSP ("csc_parms[3] = %08x\n", osd->csc_parms[3]);
	DEBUG_MSG_VDSP ("csc_parms[4] = %08x\n", osd->csc_parms[4]);
	DEBUG_MSG_VDSP ("csc_parms[5] = %08x\n", osd->csc_parms[5]);
	DEBUG_MSG_VDSP ("csc_parms[6] = %08x\n", osd->csc_parms[6]);
	DEBUG_MSG_VDSP ("csc_parms[7] = %08x\n", osd->csc_parms[7]);
	DEBUG_MSG_VDSP ("csc_parms[8] = %08x\n", osd->csc_parms[8]);
	DEBUG_MSG_VDSP ("global_blend = %d\n", osd->global_blend);
	DEBUG_MSG_VDSP ("transparent_color = %d\n", osd->transparent_color);
	DEBUG_MSG_VDSP ("transparent_color_en = %d\n", osd->transparent_color_en);
	DEBUG_MSG_VDSP ("zbuf0 = %08x\n", (u32) osd->zbuf0);
	DEBUG_MSG_VDSP ("zbuf1 = %08x\n", (u32) osd->zbuf1);
	DEBUG_MSG_VDSP ("zbuf2 = %08x\n", (u32) osd->zbuf2);
	DEBUG_MSG_VDSP ("zbuf3 = %08x\n", (u32) osd->zbuf3);
	DEBUG_MSG_VDSP ("osdupdptr = %08x\n", (u32) osd->osdupdptr);

	osd->zbuf0 = (void *) ipc_virt_to_phys ((u32) osd->zbuf0);
	osd->zbuf1 = (void *) ipc_virt_to_phys ((u32) osd->zbuf1);
	osd->zbuf2 = (void *) ipc_virt_to_phys ((u32) osd->zbuf2);
	osd->zbuf3 = (void *) ipc_virt_to_phys ((u32) osd->zbuf3);
	osd->osdupdptr = (void *) ipc_virt_to_phys ((u32) osd->osdupdptr);

	ambcache_clean_range (osd, sizeof (*osd));

	arg.voutid = voutid;
	arg.info = (struct i_display_osd_info *) osd;
	DEBUG_MSG_VDSP ("[@@@@@@@@@ display_osd_apply] voutid = %d \n", voutid);
	stat = i_display_osd_apply_1(&arg, &rval, IPC_i_display_osd);
	if (stat != IPC_SUCCESS)
		return -EIO;

	return rval;
}
EXPORT_SYMBOL(display_osd_apply);

int display_osd_back2itron(int voutid)
{
	enum clnt_stat stat;
	int rval;
	
	stat = i_display_osd_back2itron_1(&voutid, &rval, IPC_i_display_osd);
	if (stat != IPC_SUCCESS)
		return -EIO;

	return rval;
}

EXPORT_SYMBOL(display_osd_back2itron);

#if defined(CONFIG_PROC_FS)

extern struct proc_dir_entry *proc_aipc;

static struct proc_dir_entry *proc_vdspdrv = NULL;
static struct proc_dir_entry *proc_vdspdrv_vdspdrv = NULL;
static struct proc_dir_entry *proc_vdspdrv_osd = NULL;

static struct proc_dir_entry *proc_vdspdrv_vid0_plane = NULL;
static struct proc_dir_entry *proc_vdspdrv_vid1_plane = NULL;
static struct proc_dir_entry *proc_vdspdrv_osd0_plane = NULL;
static struct proc_dir_entry *proc_vdspdrv_osd1_plane = NULL;

/*
 * /proc/aipc/vdspdrv/vdspdrv read function.
 */
static int vdspdrv_procfs_read(char *page, char **start, off_t off,
			       int count, int *eof, void *data)
{
	int len = 0;
	int i;
	unsigned int *cmd;

	vdspdrv_refresh();
	vdspdrv_refresh_osd(0);
	vdspdrv_refresh_osd(1);
	
	if (vdspdrv->active == 0) {
		len += snprintf(page + len, PAGE_SIZE - len,
				"vdspdrv is inactive\n");
		goto done;
	}

	len += snprintf(page + len, PAGE_SIZE - len,
			"initdata =	0x%.8x\n",
			(unsigned int) vdspdrv->initdata);
	len += snprintf(page + len, PAGE_SIZE - len,
			"cmdize =	%d\n", vdspdrv->cmdsize);
	len += snprintf(page + len, PAGE_SIZE - len,
			"opmode =	%d\n", vdspdrv->opmode);
	len += snprintf(page + len, PAGE_SIZE - len,
			"bufaddr =	0x%.8x\n", vdspdrv->bufaddr);

	len += snprintf(page + len, PAGE_SIZE - len,
			"captured commands: (%d/%d)\n",
			vdspdrv->ncapcmds, vdspdrv->maxcapcmds);
	for (i = 0; i < vdspdrv->ncapcmds; i++) {
		if ((i % 8) == 0)
			len += snprintf(page + len, PAGE_SIZE - len, "\t");
		len += snprintf(page + len, PAGE_SIZE - len,
			 "0x%.4x ", vdspdrv->capcmds[i]);
		if ((i %8) == 7)
			len += snprintf(page + len, PAGE_SIZE - len, "\n");
	}
	if ((i % 8) != 0)
		len += snprintf(page + len, PAGE_SIZE - len, "\n");

	len += snprintf(page + len, PAGE_SIZE - len,
			"history: (%d/%d)\n",
			vdspdrv->nhistory, vdspdrv->maxhistory);
	cmd = (unsigned int *) vdspdrv->history;
	for (i = 0; i < vdspdrv->nhistory; i++) {
		if ((i % 8) == 0)
			len += snprintf(page + len, PAGE_SIZE - len, "\t");
		len += snprintf(page + len, PAGE_SIZE - len,
				"0x%.4x ", *cmd);
		if ((i %8) == 7)
			len += snprintf(page + len, PAGE_SIZE - len, "\n");
		cmd = (unsigned int *)
			((unsigned int) cmd + vdspdrv->cmdsize);
	}
	if ((i % 8) != 0)
		len += snprintf(page + len, PAGE_SIZE - len, "\n");

done:

	*eof = 1;

	return len;
}

/*
 * /proc/aipc/vdspdrv/osd read function.
 */
static int osd_procfs_read(char *page, char **start, off_t off,
			   int count, int *eof, void *data)
{
	int len = 0;
	int i;

	vdspdrv_refresh();
	vdspdrv_refresh_osd(0);
	vdspdrv_refresh_osd(1);

	if (vdspdrv->active == 0) {
		len += snprintf(page + len, PAGE_SIZE - len,
				"vdspdrv is inactive\n");
		goto done;
	}

	for (i = 0; i < 2; i++) {
		if (vdspdrv_osd[i]->zbuf0 == NULL &&
		    vdspdrv_osd[i]->osdupdptr == NULL)
			continue;

		len += snprintf(page + len, PAGE_SIZE - len,
				"[OSD %d:]\n", i);
		len += snprintf(page + len, PAGE_SIZE - len,
				"enabled:	%d\n",
				vdspdrv_osd[i]->enabled);
		len += snprintf(page + len, PAGE_SIZE - len,
				"interlaced:	%d\n",
				vdspdrv_osd[i]->interlaced);
		len += snprintf(page + len, PAGE_SIZE - len,
				"colorformat:	%d\n",
				vdspdrv_osd[i]->colorformat);
		len += snprintf(page + len, PAGE_SIZE - len,
				"bitsperpixel:	%d\n",
				vdspdrv_osd[i]->bitsperpixel);

		len += snprintf(page + len, PAGE_SIZE - len,
				"flip:		%d\n", vdspdrv_osd[i]->flip);

		len += snprintf(page + len, PAGE_SIZE - len,
				"offset_x:	%d\n",
				vdspdrv_osd[i]->offset_x);
		len += snprintf(page + len, PAGE_SIZE - len,
				"offset_y:	%d\n",
				vdspdrv_osd[i]->offset_y);
		len += snprintf(page + len, PAGE_SIZE - len,
				"width:	%d\n", vdspdrv_osd[i]->width);
		len += snprintf(page + len, PAGE_SIZE - len,
				"height:	%d\n", vdspdrv_osd[i]->height);
		len += snprintf(page + len, PAGE_SIZE - len,
				"pitch:	%d\n", vdspdrv_osd[i]->pitch);

		len += snprintf(page + len, PAGE_SIZE - len,
				"repeat_field:	%d\n",
				vdspdrv_osd[i]->repeat_field);
		len += snprintf(page + len, PAGE_SIZE - len,
				"rescaler_en:	%d\n",
				vdspdrv_osd[i]->rescaler_en);
		len += snprintf(page + len, PAGE_SIZE - len,
				"premultiplied:	%d\n",
				vdspdrv_osd[i]->premultiplied);
		len += snprintf(page + len, PAGE_SIZE - len,
				"input width:	%d\n",
				vdspdrv_osd[i]->input_width);
		len += snprintf(page + len, PAGE_SIZE - len,
				"input height:	%d\n",
				vdspdrv_osd[i]->input_height);

		len += snprintf(page + len, PAGE_SIZE - len,
				"global blend:	%d\n",
				vdspdrv_osd[i]->global_blend);
		len += snprintf(page + len, PAGE_SIZE - len,
				"transparent color:	%d\n",
				vdspdrv_osd[i]->transparent_color);
		len += snprintf(page + len, PAGE_SIZE - len,
				"transparent color en:	%d\n",
				vdspdrv_osd[i]->transparent_color_en);

		len += snprintf(page + len, PAGE_SIZE - len,
				"zbuf0:	0x%.8x\n",
				(unsigned int) vdspdrv_osd[i]->zbuf0);
		if (vdspdrv_osd[i]->zbuf1)
			len += snprintf(page + len, PAGE_SIZE - len,
					"zbuf1:	0x%.8x\n",
					(unsigned int) vdspdrv_osd[i]->zbuf1);
		if (vdspdrv_osd[i]->zbuf2)
			len += snprintf(page + len, PAGE_SIZE - len,
					"zbuf2:	0x%.8x\n",
					(unsigned int) vdspdrv_osd[i]->zbuf2);
		if (vdspdrv_osd[i]->zbuf3)
			len += snprintf(page + len, PAGE_SIZE - len,
					"zbuf3:	0x%.8x\n",
					(unsigned int) vdspdrv_osd[i]->zbuf3);
		len += snprintf(page + len, PAGE_SIZE - len,
				"osdupdptr:	0x%.8x\n",
				(unsigned int) vdspdrv_osd[i]->osdupdptr);
	}

done:

	*eof = 1;

	return len;
}

/*
 * /proc/aipc/vdspdrv/vidx_plane read function.
 */
static int vid_plane_procfs_read(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	int len = 0;

	len = snprintf(page, PAGE_SIZE, "%d\n",
		       vdspdrv_video_plane_enabled((int) data));

	*eof = 1;

	return len;
}

/*
 * /proc/aipc/vdspdrv/vidx_plane write function.
 */
static int vid_plane_procfs_write(struct file *file,
				  const char __user *buffer,
				  unsigned long count, void *data)
{
	char *buf;
	int val;

	buf = kmalloc(GFP_KERNEL, count);
	if (buf == NULL)
		return -ENOMEM;

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	if (sscanf(buf, "%d", &val) == 1) {
		vdspdrv_en_video_plane((int) data, val);
	}

	kfree(buf);

	return count;
}

/*
 * /proc/aipc/vdspdrv/osdx_plane read function.
 */
static int osd_plane_procfs_read(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	int len;

	len = snprintf(page, PAGE_SIZE, "%d\n",
		       vdspdrv_osd[(int) data]->enabled);

	*eof = 1;

	return len;
}

/*
 * /proc/aipc/vdspdrv/osdx_plane write function.
 */
static int osd_plane_procfs_write(struct file *file,
				  const char __user *buffer,
				  unsigned long count, void *data)
{
	char *buf;
	int val;

	buf = kmalloc(GFP_KERNEL, count);
	if (buf == NULL)
		return -ENOMEM;

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	if (sscanf(buf, "%d", &val) == 1) {
		vdspdrv_osd_enable((int) data, val);
	}

	kfree(buf);

	return count;
}

/*
 * Install /proc/aipc/vdspdrv
 */
static void i_display_dsp_procfs_init(void)
{
	proc_vdspdrv = proc_mkdir("vdspdrv", proc_aipc);
	if (proc_vdspdrv == NULL) {
		pr_err("create vdspdrv dir failed!\n");
		return;
	}

	proc_vdspdrv_vdspdrv = create_proc_entry("vdspdrv", 0, proc_vdspdrv);
	if (proc_vdspdrv_vdspdrv) {
		proc_vdspdrv_vdspdrv->data = NULL;
		proc_vdspdrv_vdspdrv->read_proc = vdspdrv_procfs_read;
		proc_vdspdrv_vdspdrv->write_proc = NULL;
	}

	proc_vdspdrv_osd = create_proc_entry("osd", 0, proc_vdspdrv);
	if (proc_vdspdrv_osd) {
		proc_vdspdrv_osd->data = NULL;
		proc_vdspdrv_osd->read_proc = osd_procfs_read;
		proc_vdspdrv_osd->write_proc = NULL;
	}

	proc_vdspdrv_vid0_plane = create_proc_entry("vid0_plane", 0,
						   proc_vdspdrv);
	if (proc_vdspdrv_vid0_plane) {
		proc_vdspdrv_vid0_plane->data = (void *) 0;
		proc_vdspdrv_vid0_plane->read_proc = vid_plane_procfs_read;
		proc_vdspdrv_vid0_plane->write_proc = vid_plane_procfs_write;
	}

	proc_vdspdrv_vid1_plane = create_proc_entry("vid1_plane", 0,
						   proc_vdspdrv);
	if (proc_vdspdrv_vid1_plane) {
		proc_vdspdrv_vid1_plane->data = (void *) 1;
		proc_vdspdrv_vid1_plane->read_proc = vid_plane_procfs_read;
		proc_vdspdrv_vid1_plane->write_proc = vid_plane_procfs_write;
	}

	proc_vdspdrv_osd0_plane = create_proc_entry("osd0_plane", 0,
						   proc_vdspdrv);
	if (proc_vdspdrv_osd0_plane) {
		proc_vdspdrv_osd0_plane->data = (void *) 0;
		proc_vdspdrv_osd0_plane->read_proc = osd_plane_procfs_read;
		proc_vdspdrv_osd0_plane->write_proc = osd_plane_procfs_write;
	}

	proc_vdspdrv_osd1_plane = create_proc_entry("osd1_plane", 0,
						   proc_vdspdrv);
	if (proc_vdspdrv_osd1_plane) {
		proc_vdspdrv_osd1_plane->data = (void *) 1;
		proc_vdspdrv_osd1_plane->read_proc = osd_plane_procfs_read;
		proc_vdspdrv_osd1_plane->write_proc = osd_plane_procfs_write;
	}
}

/*
 * Uninstall /proc/aipc/vdspdrv
 */
static void i_display_dsp_procfs_cleanup(void)
{
	if (proc_vdspdrv_osd1_plane) {
		remove_proc_entry("osd1_plane", proc_vdspdrv);
		proc_vdspdrv_osd1_plane = NULL;
	}

	if (proc_vdspdrv_osd0_plane) {
		remove_proc_entry("osd0_plane", proc_vdspdrv);
		proc_vdspdrv_osd0_plane = NULL;
	}

	if (proc_vdspdrv_vid1_plane) {
		remove_proc_entry("vid1_plane", proc_vdspdrv);
		proc_vdspdrv_vid1_plane = NULL;
	}

	if (proc_vdspdrv_vid0_plane) {
		remove_proc_entry("vid0_plane", proc_vdspdrv);
		proc_vdspdrv_vid0_plane = NULL;
	}

	if (proc_vdspdrv_osd) {
		remove_proc_entry("osd", proc_vdspdrv);
		proc_vdspdrv_osd = NULL;
	}

	if (proc_vdspdrv_vdspdrv) {
		remove_proc_entry("vdspdrv", proc_vdspdrv);
		proc_vdspdrv_vdspdrv = NULL;
	}

	if (proc_vdspdrv) {
		remove_proc_entry("vdspdrv", proc_aipc);
		proc_vdspdrv = NULL;
	}
}

#endif  /* CONFIG_PROC_FS */

/*
 * Client initialization.
 */
static int __init i_display_dsp_init(void)
{
	int rval = 0;

	vdspdrv = (struct vdspdrv *) &G_vdspdrv.vdspdrv;
	vdspdrv_osd[0] = &G_vdspdrv.osd[0];
	vdspdrv_osd[1] = &G_vdspdrv.osd[1];

	IPC_i_display_dsp = ipc_clnt_prog_register(&i_display_dsp_prog);
	if (IPC_i_display_dsp == NULL) {
		rval = -EPERM;
		goto done;
	}

	IPC_i_display_osd = ipc_clnt_prog_register(&i_display_osd_prog);
	if (IPC_i_display_osd == NULL) {
		rval = -EPERM;
		goto done;
	}

#if defined(CONFIG_PROC_FS)
	i_display_dsp_procfs_init();
#endif

done:

	if (rval < 0) {
		if (IPC_i_display_dsp) {
			ipc_clnt_prog_unregister(&i_display_dsp_prog, IPC_i_display_dsp);
			IPC_i_display_dsp = NULL;
		}
		if (IPC_i_display_osd) {
			ipc_clnt_prog_unregister(&i_display_osd_prog, IPC_i_display_osd);
			IPC_i_display_osd = NULL;
		}
	}

	return 0;
}

/*
 * Client exit.
 */
static void __exit i_display_dsp_cleanup(void)
{
	if (IPC_i_display_dsp) {
		ipc_clnt_prog_unregister(&i_display_dsp_prog, IPC_i_display_dsp);
		IPC_i_display_dsp = NULL;
	}
	if (IPC_i_display_osd) {
		ipc_clnt_prog_unregister(&i_display_osd_prog, IPC_i_display_osd);
		IPC_i_display_osd = NULL;
	}

#if defined(CONFIG_PROC_FS)
	i_display_dsp_procfs_cleanup();
#endif
}

subsys_initcall_sync(i_display_dsp_init);
module_exit(i_display_dsp_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Charles Chiou <cchiou@ambarella.com>");
MODULE_DESCRIPTION("Ambarella IPC: i_diplay.x");
