/*
 * drivers/drivers/video/ambarella/ambarella_fb.c
 *
 *	2008/07/22 - [linnsong] Create
 *	2009/03/03 - [Anthony Ginger] Port to 2.6.28
 *	2009/12/15 - [Zhenwu Xue] Change fb_setcmap
 *
 * Copyright (C) 2004-2009, Ambarella, Inc.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/list.h>
#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>
#include <linux/clk.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>

#include <asm/sizes.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/page.h>
#include <asm/pgtable.h>

#include <mach/fb.h>

/* video=ambafb:<x_res>x<y_res>,<x_virtual>x<y_virtual>,<color_format>[,<prealloc_start>,<prealloc_length>] */
static int use_command_line_options = 0;
static int cl_xres = -1, cl_yres = -1, cl_xvirtual = -1, cl_yvirtual = -1;
static int cl_format = -1;
static unsigned int cl_prealloc_start = 0, cl_prealloc_length = 0;

static const struct ambarella_fb_color_table_s {
	enum ambarella_fb_color_format	color_format;
	int				bits_per_pixel;
	struct fb_bitfield		red;
	struct fb_bitfield		green;
	struct fb_bitfield		blue;
	struct fb_bitfield		transp;
} ambarella_fb_color_format_table[] =
{
	{
		.color_format = AMBAFB_COLOR_CLUT_8BPP,
		.bits_per_pixel = 8,
		.red =
			{
				.offset = 0,
				.length = 0,
				.msb_right = 0,
			},
		.green =
			{
				.offset = 0,
				.length = 0,
				.msb_right = 0,
			},
		.blue =
			{
				.offset = 0,
				.length = 0,
				.msb_right = 0,
			},
		.transp =
			{
				.offset = 0,
				.length = 0,
				.msb_right = 0,
		},
	},

	{
		.color_format = AMBAFB_COLOR_RGB565,
		.bits_per_pixel = 16,
		.red =
			{
				.offset = 11,
				.length = 5,
				.msb_right = 0,
			},
		.green =
			{
				.offset = 5,
				.length = 6,
				.msb_right = 0,
			},
		.blue =
			{
				.offset = 0,
				.length = 5,
				.msb_right = 0,
			},
		.transp =
			{
				.offset = 0,
				.length = 0,
				.msb_right = 0,
		},
	},

	{
		.color_format = AMBAFB_COLOR_BGR565,
		.bits_per_pixel = 16,
		.red =
			{
				.offset = 0,
				.length = 5,
				.msb_right = 0,
			},
		.green =
			{
				.offset = 5,
				.length = 6,
				.msb_right = 0,
			},
		.blue =
			{
				.offset = 11,
				.length = 5,
				.msb_right = 0,
			},
		.transp =
			{
				.offset = 0,
				.length = 0,
				.msb_right = 0,
		},
	},

	{
		.color_format = AMBAFB_COLOR_AGBR4444,
		.bits_per_pixel = 16,
		.red =
			{
				.offset = 0,
				.length = 4,
				.msb_right = 0,
			},
		.green =
			{
				.offset = 8,
				.length = 4,
				.msb_right = 0,
			},
		.blue =
			{
				.offset = 4,
				.length = 4,
				.msb_right = 0,
			},
		.transp =
			{
				.offset = 12,
				.length = 4,
				.msb_right = 0,
		},
	},

	{
		.color_format = AMBAFB_COLOR_RGBA4444,
		.bits_per_pixel = 16,
		.red =
			{
				.offset = 12,
				.length = 4,
				.msb_right = 0,
			},
		.green =
			{
				.offset = 8,
				.length = 4,
				.msb_right = 0,
			},
		.blue =
			{
				.offset = 4,
				.length = 4,
				.msb_right = 0,
			},
		.transp =
			{
				.offset = 0,
				.length = 4,
				.msb_right = 0,
		},
	},

	{
		.color_format = AMBAFB_COLOR_BGRA4444,
		.bits_per_pixel = 16,
		.red =
			{
				.offset = 4,
				.length = 4,
				.msb_right = 0,
			},
		.green =
			{
				.offset = 8,
				.length = 4,
				.msb_right = 0,
			},
		.blue =
			{
				.offset = 12,
				.length = 4,
				.msb_right = 0,
			},
		.transp =
			{
				.offset = 0,
				.length = 4,
				.msb_right = 0,
		},
	},

	{
		.color_format = AMBAFB_COLOR_ABGR4444,
		.bits_per_pixel = 16,
		.red =
			{
				.offset = 0,
				.length = 4,
				.msb_right = 0,
			},
		.green =
			{
				.offset = 4,
				.length = 4,
				.msb_right = 0,
			},
		.blue =
			{
				.offset = 8,
				.length = 4,
				.msb_right = 0,
			},
		.transp =
			{
				.offset = 12,
				.length = 4,
				.msb_right = 0,
		},
	},

	{
		.color_format = AMBAFB_COLOR_ARGB4444,
		.bits_per_pixel = 16,
		.red =
			{
				.offset = 8,
				.length = 4,
				.msb_right = 0,
			},
		.green =
			{
				.offset = 4,
				.length = 4,
				.msb_right = 0,
			},
		.blue =
			{
				.offset = 0,
				.length = 4,
				.msb_right = 0,
			},
		.transp =
			{
				.offset = 12,
				.length = 4,
				.msb_right = 0,
		},
	},

	{
		.color_format = AMBAFB_COLOR_AGBR1555,
		.bits_per_pixel = 16,
		.red =
			{
				.offset = 0,
				.length = 5,
				.msb_right = 0,
			},
		.green =
			{
				.offset = 10,
				.length = 5,
				.msb_right = 0,
			},
		.blue =
			{
				.offset = 5,
				.length = 5,
				.msb_right = 0,
			},
		.transp =
			{
				.offset = 15,
				.length = 1,
				.msb_right = 0,
		},
	},

	{
		.color_format = AMBAFB_COLOR_GBR1555,
		.bits_per_pixel = 16,
		.red =
			{
				.offset = 0,
				.length = 5,
				.msb_right = 0,
			},
		.green =
			{
				.offset = 10,
				.length = 5,
				.msb_right = 0,
			},
		.blue =
			{
				.offset = 5,
				.length = 5,
				.msb_right = 0,
			},
		.transp =
			{
				.offset = 0,
				.length = 0,
				.msb_right = 0,
		},
	},

	{
		.color_format = AMBAFB_COLOR_RGBA5551,
		.bits_per_pixel = 16,
		.red =
			{
				.offset = 11,
				.length = 5,
				.msb_right = 0,
			},
		.green =
			{
				.offset = 6,
				.length = 5,
				.msb_right = 0,
			},
		.blue =
			{
				.offset = 1,
				.length = 5,
				.msb_right = 0,
			},
		.transp =
			{
				.offset = 0,
				.length = 1,
				.msb_right = 0,
		},
	},

	{
		.color_format = AMBAFB_COLOR_BGRA5551,
		.bits_per_pixel = 16,
		.red =
			{
				.offset = 1,
				.length = 5,
				.msb_right = 0,
			},
		.green =
			{
				.offset = 6,
				.length = 5,
				.msb_right = 0,
			},
		.blue =
			{
				.offset = 11,
				.length = 5,
				.msb_right = 0,
			},
		.transp =
			{
				.offset = 0,
				.length = 1,
				.msb_right = 0,
		},
	},

	{
		.color_format = AMBAFB_COLOR_ABGR1555,
		.bits_per_pixel = 16,
		.red =
			{
				.offset = 0,
				.length = 5,
				.msb_right = 0,
			},
		.green =
			{
				.offset = 5,
				.length = 5,
				.msb_right = 0,
			},
		.blue =
			{
				.offset = 10,
				.length = 5,
				.msb_right = 0,
			},
		.transp =
			{
				.offset = 15,
				.length = 1,
				.msb_right = 0,
		},
	},

	{
		.color_format = AMBAFB_COLOR_ARGB1555,
		.bits_per_pixel = 16,
		.red =
			{
				.offset = 10,
				.length = 5,
				.msb_right = 0,
			},
		.green =
			{
				.offset = 5,
				.length = 5,
				.msb_right = 0,
			},
		.blue =
			{
				.offset = 0,
				.length = 5,
				.msb_right = 0,
			},
		.transp =
			{
				.offset = 15,
				.length = 1,
				.msb_right = 0,
		},
	},

	{
		.color_format = AMBAFB_COLOR_AGBR8888,
		.bits_per_pixel = 32,
		.red =
			{
				.offset = 0,
				.length = 8,
				.msb_right = 0,
			},
		.green =
			{
				.offset = 16,
				.length = 8,
				.msb_right = 0,
			},
		.blue =
			{
				.offset = 8,
				.length = 8,
				.msb_right = 0,
			},
		.transp =
			{
				.offset = 24,
				.length = 8,
				.msb_right = 0,
		},
	},

	{
		.color_format = AMBAFB_COLOR_RGBA8888,
		.bits_per_pixel = 32,
		.red =
			{
				.offset = 24,
				.length = 8,
				.msb_right = 0,
			},
		.green =
			{
				.offset = 16,
				.length = 8,
				.msb_right = 0,
			},
		.blue =
			{
				.offset = 8,
				.length = 8,
				.msb_right = 0,
			},
		.transp =
			{
				.offset = 0,
				.length = 8,
				.msb_right = 0,
		},
	},

	{
		.color_format = AMBAFB_COLOR_BGRA8888,
		.bits_per_pixel = 32,
		.red =
			{
				.offset = 8,
				.length = 8,
				.msb_right = 0,
			},
		.green =
			{
				.offset = 16,
				.length = 8,
				.msb_right = 0,
			},
		.blue =
			{
				.offset = 24,
				.length = 8,
				.msb_right = 0,
			},
		.transp =
			{
				.offset = 0,
				.length = 8,
				.msb_right = 0,
		},
	},

	{
		.color_format = AMBAFB_COLOR_ABGR8888,
		.bits_per_pixel = 32,
		.red =
			{
				.offset = 0,
				.length = 8,
				.msb_right = 0,
			},
		.green =
			{
				.offset = 8,
				.length = 8,
				.msb_right = 0,
			},
		.blue =
			{
				.offset = 16,
				.length = 8,
				.msb_right = 0,
			},
		.transp =
			{
				.offset = 24,
				.length = 8,
				.msb_right = 0,
		},
	},

	{
		.color_format = AMBAFB_COLOR_ARGB8888,
		.bits_per_pixel = 32,
		.red =
			{
				.offset = 16,
				.length = 8,
				.msb_right = 0,
			},
		.green =
			{
				.offset = 8,
				.length = 8,
				.msb_right = 0,
			},
		.blue =
			{
				.offset = 0,
				.length = 8,
				.msb_right = 0,
			},
		.transp =
			{
				.offset = 24,
				.length = 8,
				.msb_right = 0,
		},
	},
};

/* ========================================================================== */
static int ambarella_fb_proc_read(char *page, char **start,
	off_t off, int count, int *eof, void *data)
{
	int					len;
	struct ambarella_platform_fb		*ambafb_data;
	struct fb_info				*info;

	ambafb_data = (struct ambarella_platform_fb *)data;
	info = ambafb_data->proc_fb_info;

	*start = page + off;
	wait_event_interruptible(ambafb_data->proc_wait,
		(ambafb_data->proc_wait_flag > 0));
	ambafb_data->proc_wait_flag = 0;
	dev_dbg(info->device, "%s:%d %d.\n", __func__, __LINE__,
		ambafb_data->proc_wait_flag);
	len = sprintf(*start, "%04d %04d %04d %04d %04d %04d\n",
		info->var.xres, info->var.yres,
		info->fix.line_length,
		info->var.xoffset, info->var.yoffset,
		ambafb_data->color_format);

	if (off + len > count) {
		*eof = 1;
		len = 0;
	}

	return len;
}

/* ========================================================================== */
static int ambafb_setcmap(struct fb_cmap *cmap, struct fb_info *info)
{
	int					errorCode = 0;
	struct ambarella_platform_fb		*ambafb_data;
	struct ambarella_fb_info		ambafb_info;
	int					pos;
	u16					*r, *g, *b, *t;
	u8					*pclut_table, *pblend_table;

	if (cmap == &info->cmap) {
		errorCode = 0;
		goto ambafb_setcmap_exit;
	}

	if (cmap->start != 0 || cmap->len != 256) {
		dev_dbg(info->device, "%s: Incorrect parameters: start = %d, len = %d\n",
			__func__, cmap->start, cmap->len);
		errorCode = -1;
		goto ambafb_setcmap_exit;
	}

	if (!cmap->red || !cmap->green || !cmap->blue) {
		dev_dbg(info->device, "%s: Incorrect rgb pointers!\n",
			__func__);
		errorCode = -1;
		goto ambafb_setcmap_exit;
	}

	ambafb_data = (struct ambarella_platform_fb *)info->par;

	mutex_lock(&ambafb_data->lock);

	r = cmap->red;
	g = cmap->green;
	b = cmap->blue;
	t = cmap->transp;
	pclut_table = ambafb_data->clut_table;
	pblend_table = ambafb_data->blend_table;
	for (pos = 0; pos < 256; pos++) {
		*pclut_table++ = *r++;
		*pclut_table++ = *g++;
		*pclut_table++ = *b++;
		if (t) *pblend_table++ = *t++;
	}

	if (ambafb_data->setcmap) {
		ambafb_info.color_format = ambafb_data->color_format;
		mutex_unlock(&ambafb_data->lock);
		errorCode =
			ambafb_data->setcmap(cmap, info, &ambafb_info);
	} else {
		mutex_unlock(&ambafb_data->lock);
	}

ambafb_setcmap_exit:
	dev_dbg(info->device, "%s:%d %d.\n", __func__, __LINE__, errorCode);
	return errorCode;
}

static int ambafb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	int					errorCode = 0;
	struct ambarella_platform_fb		*ambafb_data;
	struct ambarella_fb_info		ambafb_info;
	u32					framesize = 0;

	ambafb_data = (struct ambarella_platform_fb *)info->par;

	mutex_lock(&ambafb_data->lock);
	if (ambafb_data->check_var) {
		ambafb_info.color_format = ambafb_data->color_format;
		errorCode =
			ambafb_data->check_var(var, info, &ambafb_info);
	}
	mutex_unlock(&ambafb_data->lock);

	framesize = info->fix.line_length * var->yres_virtual;
	if (framesize > info->fix.smem_len) {
		errorCode = -ENOMEM;
		dev_err(info->device, "%s: framesize[%d] too big [%d]!\n",
			__func__, framesize, info->fix.smem_len);
	}

	dev_dbg(info->device, "%s:%d %d.\n", __func__, __LINE__, errorCode);

	return errorCode;
}

static int ambafb_set_par(struct fb_info *info)
{
	int					errorCode = 0, i;
	struct ambarella_platform_fb		*ambafb_data;
	struct fb_var_screeninfo		*pvar;
	struct ambarella_fb_info		ambafb_info;
	static struct fb_var_screeninfo		*cur_var;
	int					res_changed = 0;
	int					color_format_changed = 0;
	enum ambarella_fb_color_format		new_color_format;
	const struct ambarella_fb_color_table_s	*pTable;

	ambafb_data = (struct ambarella_platform_fb *)info->par;
	mutex_lock(&ambafb_data->lock);
	cur_var = &ambafb_data->screen_var;
	pvar = &info->var;

	if (!ambafb_data->set_par)
		goto ambafb_set_par_quick_exit;

	/* Resolution changed */
	if (pvar->xres != cur_var->xres || pvar->yres != cur_var->yres) {
		res_changed = 1;
	}

	/* Color format changed */
	if (pvar->bits_per_pixel != cur_var->bits_per_pixel ||
		pvar->red.offset != cur_var->red.offset ||
		pvar->red.length != cur_var->red.length ||
		pvar->red.msb_right != cur_var->red.msb_right ||
		pvar->green.offset != cur_var->green.offset ||
		pvar->green.length != cur_var->green.length ||
		pvar->green.msb_right != cur_var->green.msb_right ||
		pvar->blue.offset != cur_var->blue.offset ||
		pvar->blue.length != cur_var->blue.length ||
		pvar->blue.msb_right != cur_var->blue.msb_right ||
		pvar->transp.offset != cur_var->transp.offset ||
		pvar->transp.length != cur_var->transp.length ||
		pvar->transp.msb_right != cur_var->transp.msb_right) {

		color_format_changed = 1;
	}

	if (!res_changed && !color_format_changed)
		goto ambafb_set_par_quick_exit;

	/* Find color format */
	new_color_format = ambafb_data->color_format;
	pTable = ambarella_fb_color_format_table;
	for (i = 0; i < ARRAY_SIZE(ambarella_fb_color_format_table); i++) {
		if (pTable->bits_per_pixel == pvar->bits_per_pixel &&
			pTable->red.offset == pvar->red.offset &&
			pTable->red.length == pvar->red.length &&
			pTable->red.msb_right == pvar->red.msb_right &&
			pTable->green.offset == pvar->green.offset &&
			pTable->green.length == pvar->green.length &&
			pTable->green.msb_right == pvar->green.msb_right &&
			pTable->blue.offset == pvar->blue.offset &&
			pTable->blue.length == pvar->blue.length &&
			pTable->blue.msb_right == pvar->blue.msb_right &&
			pTable->transp.offset == pvar->transp.offset &&
			pTable->transp.length == pvar->transp.length &&
			pTable->transp.msb_right == pvar->transp.msb_right) {

			new_color_format = pTable->color_format;
			break;
		}
		pTable++;
	}

	ambafb_info.color_format = new_color_format;
	ambafb_data->color_format = new_color_format;
	memcpy(cur_var, pvar, sizeof(*pvar));
	mutex_unlock(&ambafb_data->lock);

	errorCode = ambafb_data->set_par(info, &ambafb_info);
	goto ambafb_set_par_normal_exit;

ambafb_set_par_quick_exit:
	memcpy(cur_var, pvar, sizeof(*pvar));
	mutex_unlock(&ambafb_data->lock);

ambafb_set_par_normal_exit:
	dev_dbg(info->device, "%s:%d %d.\n", __func__, __LINE__, errorCode);

	return errorCode;
}

static int ambafb_pan_display(struct fb_var_screeninfo *var,
	struct fb_info *info)
{
	int					errorCode = 0;
	struct ambarella_platform_fb		*ambafb_data;
	struct ambarella_fb_info		ambafb_info;

	ambafb_data = (struct ambarella_platform_fb *)info->par;

	ambafb_data->proc_wait_flag++;
	wake_up(&(ambafb_data->proc_wait));
	mutex_lock(&ambafb_data->lock);
	if (ambafb_data->pan_display) {
		ambafb_info.color_format = ambafb_data->color_format;
		errorCode =
			ambafb_data->pan_display(var, info, &ambafb_info);
	}
	mutex_unlock(&ambafb_data->lock);

	dev_dbg(info->device, "%s:%d %d.\n", __func__, __LINE__, errorCode);

	return 0;
}

static int ambafb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	unsigned long size = vma->vm_end - vma->vm_start;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;

	if (offset + size > info->fix.smem_len)
		return -EINVAL;

	offset += info->fix.smem_start;
	if (remap_pfn_range(vma, vma->vm_start, offset >> PAGE_SHIFT,
			    size, vma->vm_page_prot))
		return -EAGAIN;

	dev_dbg(info->device, "%s: P(0x%08lx)->V(0x%08lx), size = 0x%08lx.\n",
		__func__, offset, vma->vm_start, size);

	return 0;
}

static int ambafb_blank(int blank_mode, struct fb_info *info)
{
	int					errorCode = 0;
	struct ambarella_platform_fb		*ambafb_data;
	struct ambarella_fb_info		ambafb_info;

	ambafb_data = (struct ambarella_platform_fb *)info->par;

	mutex_lock(&ambafb_data->lock);
	if (ambafb_data->set_blank) {
		ambafb_info.color_format = ambafb_data->color_format;
		errorCode =
			ambafb_data->set_blank(blank_mode, info, &ambafb_info);
	}
	mutex_unlock(&ambafb_data->lock);

	dev_dbg(info->device, "%s:%d %d.\n", __func__, __LINE__, errorCode);

	return errorCode;
}

static struct fb_ops ambafb_ops = {
	.owner          = THIS_MODULE,
	.fb_check_var	= ambafb_check_var,
	.fb_set_par	= ambafb_set_par,
	.fb_pan_display	= ambafb_pan_display,
	.fb_fillrect	= sys_fillrect,
	.fb_copyarea	= sys_copyarea,
	.fb_imageblit	= sys_imageblit,
	.fb_mmap	= ambafb_mmap,
	.fb_setcmap	= ambafb_setcmap,
	.fb_blank	= ambafb_blank,
};

static int __init ambafb_setup(char *options,
	struct ambarella_platform_fb *ambafb_data)
{
	if (!options || !*options) return -1;

	sscanf(options, "%dx%d,%dx%d,%d,%x,%x", &cl_xres, &cl_yres,
		&cl_xvirtual, &cl_yvirtual, &cl_format,
		&cl_prealloc_start, &cl_prealloc_length);
	if (cl_xres > 0 && cl_yres > 0 && cl_xvirtual > 0 && cl_yvirtual > 0
		&& cl_format >= 0) {
		use_command_line_options = 1;
	}

	return 0;
}

static int __init ambafb_probe(struct platform_device *pdev)
{
	int					errorCode = 0;
	struct fb_info				*info;
	char					*option;
	struct ambarella_platform_fb		*ambafb_data = NULL;
	u32					i;
	u32					framesize;
	u32					line_length;

	ambafb_data = (struct ambarella_platform_fb *)pdev->dev.platform_data;
	if (ambafb_data == NULL) {
		dev_err(&pdev->dev, "%s: ambafb_data is NULL!\n", __func__);
		errorCode = -ENODEV;
		goto ambafb_probe_exit;
	}

	/* Get Command Line Options */
	if (fb_get_options("ambafb", &option)) {
		errorCode = -ENODEV;
		goto ambafb_probe_exit;
	}
	if (!option)
		option = "1280x720,1280x720,1";
	ambafb_setup(option, ambafb_data);
	if (ambafb_data->use_prealloc == 1) {
		cl_prealloc_start = ambafb_data->screen_fix.smem_start;
		cl_prealloc_length = ambafb_data->screen_fix.smem_len;
	}

	info = framebuffer_alloc(sizeof(ambafb_data), &pdev->dev);
	if (info == NULL) {
		dev_err(&pdev->dev, "%s: framebuffer_alloc fail!\n", __func__);
		errorCode = -ENOMEM;
		goto ambafb_probe_exit;
	}

	mutex_lock(&ambafb_data->lock);

	info->fbops = &ambafb_ops;
	info->par = ambafb_data;
	info->var = ambafb_data->screen_var;
	info->fix = ambafb_data->screen_fix;
	info->flags = FBINFO_FLAG_DEFAULT;

	if (use_command_line_options) {
		info->var.xres = cl_xres;
		info->var.yres = cl_yres;
		info->var.xres_virtual = cl_xvirtual;
		info->var.yres_virtual = cl_yvirtual;
		ambafb_data->color_format = cl_format;
	}

	/* Fill Color-related Variables */
	for (i = 0; i < ARRAY_SIZE(ambarella_fb_color_format_table); i++) {
		if (ambarella_fb_color_format_table[i].color_format == ambafb_data->color_format)
			break;
	}
	if (i < ARRAY_SIZE(ambarella_fb_color_format_table)) {
		info->var.bits_per_pixel = ambarella_fb_color_format_table[i].bits_per_pixel;
		info->var.red = ambarella_fb_color_format_table[i].red;
		info->var.green = ambarella_fb_color_format_table[i].green;
		info->var.blue = ambarella_fb_color_format_table[i].blue;
		info->var.transp = ambarella_fb_color_format_table[i].transp;
	}

	/* Malloc Framebuffer Memory */
	line_length = (info->var.xres_virtual *
		(info->var.bits_per_pixel / 8) + 31) & 0xffffffe0;
	if (cl_prealloc_length <= 0) {
		info->fix.line_length = line_length;
	} else {
		info->fix.line_length =
			(line_length > ambafb_data->prealloc_line_length) ?
			line_length : ambafb_data->prealloc_line_length;
	}
	framesize = info->fix.line_length * info->var.yres_virtual;
	if (framesize % PAGE_SIZE) {
		framesize /= PAGE_SIZE;
		framesize++;
		framesize *= PAGE_SIZE;
	}
	if (cl_prealloc_length <= 0) {
		info->screen_base = kzalloc(framesize, GFP_KERNEL);
		if (info->screen_base == NULL) {
			dev_err(&pdev->dev, "%s: Can't get fbmem!\n", __func__);
			errorCode = -ENOMEM;
			goto ambafb_probe_release_framebuffer;
		}
		info->fix.smem_start = virt_to_phys(info->screen_base);
		info->fix.smem_len = framesize;
	} else {
		info->fix.smem_start = cl_prealloc_start;
		info->fix.smem_len = cl_prealloc_length;
		if ((info->fix.smem_start == 0) ||
			(info->fix.smem_len < framesize)) {
			dev_err(&pdev->dev, "%s: prealloc[0x%08x < 0x%08x]!\n",
				__func__, info->fix.smem_len, framesize);
			errorCode = -ENOMEM;
			goto ambafb_probe_release_framebuffer;
		}
		info->screen_base = (char __iomem *)
			ambarella_phys_to_virt(info->fix.smem_start);
	}

	errorCode = fb_alloc_cmap(&info->cmap, 256, 1);
	if (errorCode < 0) {
		dev_err(&pdev->dev, "%s: fb_alloc_cmap fail!\n", __func__);
		errorCode = -ENOMEM;
		goto ambafb_probe_release_framebuffer;
	}
	for (i = info->cmap.start; i < info->cmap.len; i++) {
		info->cmap.red[i] =
			ambafb_data->clut_table[i * AMBARELLA_CLUT_BYTES + 0];
		info->cmap.red[i] <<= 8;
		info->cmap.green[i] =
			ambafb_data->clut_table[i * AMBARELLA_CLUT_BYTES + 1];
		info->cmap.green[i] <<= 8;
		info->cmap.blue[i] =
			ambafb_data->clut_table[i * AMBARELLA_CLUT_BYTES + 2];
		info->cmap.blue[i] <<= 8;
		if (info->cmap.transp) {
			info->cmap.transp[i] = ambafb_data->blend_table[i];
			info->cmap.transp[i] <<= 8;
		}
	}

	platform_set_drvdata(pdev, info);
	ambafb_data->fb_status = AMBAFB_ACTIVE_MODE;
	mutex_unlock(&ambafb_data->lock);

	errorCode = register_framebuffer(info);
	if (errorCode < 0) {
		dev_err(&pdev->dev, "%s: register_framebuffer fail!\n",
			__func__);
		errorCode = -ENOMEM;
		mutex_lock(&ambafb_data->lock);
		goto ambafb_probe_dealloc_cmap;
	}

	ambafb_data->proc_fb_info = info;
	ambafb_data->proc_file = create_proc_entry(dev_name(&pdev->dev),
			S_IRUGO | S_IWUSR, get_ambarella_proc_dir());
	if (ambafb_data->proc_file == NULL) {
		dev_err(&pdev->dev, "Register %s failed!\n",
			dev_name(&pdev->dev));
		errorCode = -ENOMEM;
		goto ambafb_probe_unregister_framebuffer;
	} else {
		ambafb_data->proc_file->read_proc = ambarella_fb_proc_read;
		ambafb_data->proc_file->owner = THIS_MODULE;
		ambafb_data->proc_file->data = ambafb_data;
	}

	mutex_lock(&ambafb_data->lock);
	ambafb_data->screen_var = info->var;
	ambafb_data->screen_fix = info->fix;
	mutex_unlock(&ambafb_data->lock);

	dev_info(&pdev->dev,
		"probe p[%dx%d] v[%dx%d] c[%d] l[%d] @ [0x%08lx:0x%08x]!\n",
		info->var.xres, info->var.yres, info->var.xres_virtual,
		info->var.yres_virtual, ambafb_data->color_format,
		info->fix.line_length,
		info->fix.smem_start, info->fix.smem_len);
	goto ambafb_probe_exit;

ambafb_probe_unregister_framebuffer:
	unregister_framebuffer(info);

ambafb_probe_dealloc_cmap:
	fb_dealloc_cmap(&info->cmap);

ambafb_probe_release_framebuffer:
	framebuffer_release(info);
	ambafb_data->fb_status = AMBAFB_STOP_MODE;
	mutex_unlock(&ambafb_data->lock);

ambafb_probe_exit:
	return errorCode;
}

static int __devexit ambafb_remove(struct platform_device *pdev)
{
	struct fb_info				*info;
	struct ambarella_platform_fb		*ambafb_data = NULL;

	info = platform_get_drvdata(pdev);
	if (info) {
		remove_proc_entry(dev_name(&pdev->dev),
			get_ambarella_proc_dir());
		unregister_framebuffer(info);

		ambafb_data = (struct ambarella_platform_fb *)info->par;
		mutex_lock(&ambafb_data->lock);
		ambafb_data->fb_status = AMBAFB_STOP_MODE;
		fb_dealloc_cmap(&info->cmap);
		if ((cl_prealloc_length <= 0) &&
			(info->screen_base != NULL)) {
			kfree(info->screen_base);
		}
		ambafb_data->screen_fix.smem_start = 0;
		ambafb_data->screen_fix.smem_len = 0;
		platform_set_drvdata(pdev, NULL);
		framebuffer_release(info);
		mutex_unlock(&ambafb_data->lock);
	}

	return 0;
}

#ifdef CONFIG_PM
static int ambafb_suspend(struct platform_device *pdev, pm_message_t state)
{
	int					errorCode = 0;

	dev_info(&pdev->dev, "%s exit with %d @ %d\n",
		__func__, errorCode, state.event);

	return errorCode;
}

static int ambafb_resume(struct platform_device *pdev)
{
	int					errorCode = 0;

	dev_info(&pdev->dev, "%s exit with %d\n", __func__, errorCode);

	return errorCode;
}
#endif

static struct platform_driver ambafb_driver = {
	.probe	= ambafb_probe,
	.remove = __devexit_p(ambafb_remove),
#ifdef CONFIG_PM
	.suspend        = ambafb_suspend,
	.resume		= ambafb_resume,
#endif
	.driver = {
		.name	= "ambarella-fb",
	},
};

static int __init ambavoutfb_init(void)
{
	return platform_driver_register(&ambafb_driver);
}

static void __exit ambavoutfb_exit(void)
{
	platform_driver_unregister(&ambafb_driver);
}

MODULE_LICENSE("GPL");
module_init(ambavoutfb_init);
module_exit(ambavoutfb_exit);

