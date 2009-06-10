/*
 * include/asm-arm/arch-ambarella/config.h
 *
 * Author: Anthony Ginger <hfjiang@ambarella.com>
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

#ifndef __AMBARELLA_FB_H
#define __AMBARELLA_FB_H

#define AMBARELLA_CLUT_BYTES			(3)
#define AMBARELLA_CLUT_TABLE_SIZE		(256 * AMBARELLA_CLUT_BYTES)
#define AMBARELLA_BLEND_TABLE_SIZE		(256)

enum ambarella_fb_color_format {
	AMBAFB_COLOR_AUTO	= 0,
	AMBAFB_COLOR_CLUT_8BPP,
	AMBAFB_COLOR_YUY565,
	AMBAFB_COLOR_RGB565,
};

enum ambarella_dsp_status {
	AMBA_DSP_ENCODE_MODE	= 0x00,
	AMBA_DSP_DECODE_MODE	= 0x01,
	AMBA_DSP_RESET_MODE	= 0x02,
	AMBA_DSP_UNKNOWN_MODE	= 0x03,
	AMBA_DSP_QUICKLOGO_MODE	= 0x04,
};

enum ambarella_fb_status {
	AMBAFB_UNKNOWN_MODE	= 0x00,
	AMBAFB_ACTIVE_MODE,
	AMBAFB_STOP_MODE,
};

struct ambarella_fb_info {
	enum ambarella_fb_color_format color_format;
};

typedef int (*ambarella_fb_pan_display_fn)(struct fb_var_screeninfo *var,
	struct fb_info *info, struct ambarella_fb_info *);
typedef int (*ambarella_fb_setcmap_fn)(struct fb_cmap *cmap,
	struct fb_info *info, struct ambarella_fb_info *);
typedef int (*ambarella_fb_check_var_fn)(struct fb_var_screeninfo *var,
	struct fb_info *info, struct ambarella_fb_info *);
typedef int (*ambarella_fb_set_par_fn)(struct fb_info *info,
	struct ambarella_fb_info *);

struct ambarella_fb_iav_info {
	struct fb_var_screeninfo screen_var;
	struct fb_fix_screeninfo screen_fix;
	enum ambarella_dsp_status dsp_status;

	ambarella_fb_pan_display_fn pan_display;
	ambarella_fb_setcmap_fn setcmap;
	ambarella_fb_check_var_fn check_var;
	ambarella_fb_set_par_fn set_par;
};

struct ambarella_platform_fb {
	struct mutex lock;
	struct fb_var_screeninfo screen_var;
	struct fb_fix_screeninfo screen_fix;
	enum ambarella_dsp_status dsp_status;
	enum ambarella_fb_status fb_status;
	u8 clut_table[AMBARELLA_CLUT_TABLE_SIZE];
	u8 blend_table[AMBARELLA_BLEND_TABLE_SIZE];
	enum ambarella_fb_color_format color_format;
	u32 x_multiplication;
	u32 y_multiplication;

	ambarella_fb_pan_display_fn pan_display;
	ambarella_fb_setcmap_fn setcmap;
	ambarella_fb_check_var_fn check_var;
	ambarella_fb_set_par_fn set_par;

	struct fb_info *proc_fb_info;
	struct proc_dir_entry *proc_file;
	wait_queue_head_t proc_wait;
	u32 proc_wait_flag;
};

extern int ambarella_fb_get_platform_info(u32, struct ambarella_platform_fb *);
extern int ambarella_fb_set_iav_info(u32, struct ambarella_fb_iav_info *);

#endif

