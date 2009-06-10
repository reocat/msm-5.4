/*
 * drivers/drivers/video/ambarella/ambarella_fb.c
 *
 *	2008/07/22 - [linnsong] Create
 *	2009/03/03 - [Anthony Ginger] Port to 2.6.28
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

/* video=ambafb:double_x:double_y,rgb565*/

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

	ambafb_data = (struct ambarella_platform_fb *)info->par;

	mutex_lock(&ambafb_data->lock);
	if (ambafb_data->setcmap) {
		ambafb_info.color_format = ambafb_data->color_format;
		errorCode =
			ambafb_data->setcmap(cmap, info, &ambafb_info);
	}
	mutex_unlock(&ambafb_data->lock);

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
	int					errorCode = 0;
	struct ambarella_platform_fb		*ambafb_data;
	struct ambarella_fb_info		ambafb_info;

	ambafb_data = (struct ambarella_platform_fb *)info->par;

	mutex_lock(&ambafb_data->lock);
	if (ambafb_data->set_par) {
		ambafb_info.color_format = ambafb_data->color_format;
		errorCode =
			ambafb_data->set_par(info, &ambafb_info);
	}
	mutex_unlock(&ambafb_data->lock);

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
};

static int __init ambafb_setup(char *options,
	struct ambarella_platform_fb *ambafb_data)
{
	char					*this_opt;

	if (!options || !*options)
		return 1;

	while ((this_opt = strsep(&options, ",")) != NULL) {
		if (!*this_opt)
			continue;
		if (strcmp(this_opt, "double_x") == 0) {
			ambafb_data->x_multiplication = 2;
		} else
		if (strcmp(this_opt, "double_y") == 0) {
			ambafb_data->y_multiplication = 2;
		} else
		if (strcmp(this_opt, "rgb565") == 0) {
			ambafb_data->color_format = AMBAFB_COLOR_RGB565;
		} else {
			//mode_option = this_opt;
		}
	}

	return 1;
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

	if (fb_get_options("ambafb", &option)) {
		errorCode = -ENODEV;
		goto ambafb_probe_exit;
	}
	ambafb_setup(option, ambafb_data);

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

	info->var.xres_virtual = info->var.xres * ambafb_data->x_multiplication;
	info->var.yres_virtual = info->var.yres * ambafb_data->y_multiplication;
	if (ambafb_data->color_format == AMBAFB_COLOR_RGB565) {
		info->var.bits_per_pixel = 16;
		info->var.red.offset = 11;
		info->var.red.length = 5;
		info->var.green.offset = 5;
		info->var.green.length = 6;
		info->var.blue.offset = 0;
		info->var.blue.length = 5;
	} else {
		info->var.bits_per_pixel = 8;
		info->var.red.offset = 0;
		info->var.red.length = 8;
		info->var.green.offset = 0;
		info->var.green.length = 8;
		info->var.blue.offset = 0;
		info->var.blue.length = 8;
	}
	line_length = (info->var.xres_virtual *
		(info->var.bits_per_pixel / 8) + 31) & 0xffffffe0;
	info->fix.line_length = line_length;
	framesize = info->fix.line_length * info->var.yres_virtual;
	if (framesize % PAGE_SIZE) {
		framesize /= PAGE_SIZE;
		framesize++;
		framesize *= PAGE_SIZE;
	}
	info->screen_base = kzalloc(framesize, GFP_KERNEL);
	if (info->screen_base == NULL) {
		dev_err(&pdev->dev, "%s: Can't get fbmem!\n", __func__);
		errorCode = -ENOMEM;
		goto ambafb_probe_release_framebuffer;
	}
	info->fix.smem_start = virt_to_phys(info->screen_base);
	info->fix.smem_len = framesize;

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
		kfree(info->screen_base);
		info->screen_base = NULL;
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

