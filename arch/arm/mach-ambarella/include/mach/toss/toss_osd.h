/*
 * toss/toss_osd.h
 *
 * Terminal Operating Systerm Scheduler
 *
 * Copyright (C) 2004-2009, Ambarella, Inc.
 */

#ifndef __TOSS_OSD_H__
#define __TOSS_OSD_H__

struct toss_vout_mixer_setup_s
{
	u32 cmd_code;
	u16 vout_id;
	u8  interlaced;
	u8  frm_rate;
	u16 act_win_width;
	u16 act_win_height;
	u8  back_ground_v;
	u8  back_ground_u;
	u8  back_ground_y;
	u8  reserved;
	u8  highlight_v;
	u8  highlight_u;
	u8  highlight_y;
	u8  highlight_thresh;
};

struct toss_vout_video_setup_s
{
	u32 cmd_code;
	u16 vout_id;
	u8  en;
	u8  src;
	u8  flip;
	u8  rotate;
	u16 reserved;
	u16 win_offset_x;
	u16 win_offset_y;
	u16 win_width;
	u16 win_height;
	u32 default_img_y_addr;
	u32 default_img_uv_addr;
	u16 default_img_pitch;
	u8  default_img_repeat_field;
	u8  reserved2;
};

struct toss_vout_default_img_setup_s
{
	u32 cmd_code;
	u16 vout_id;
	u16 reserved;
	u32 default_img_y_addr;
	u32 default_img_uv_addr;
	u16 default_img_pitch;
	u8  default_img_repeat_field;
	u8  reserved2;
};

struct toss_vout_osd_setup_s
{
	u32 cmd_code;
	u16 vout_id;
	u8  en;
	u8  src;
	u8  flip;
	u8  rescaler_en;
	u8  premultiplied;
	u8  global_blend;
	u16 win_offset_x;
	u16 win_offset_y;
	u16 win_width;
	u16 win_height;
	u16 rescaler_input_width;
	u16 rescaler_input_height;
	u32 osd_buf_dram_addr;
	u16 osd_buf_pitch;
	u8  osd_buf_repeat_field;
	u8  osd_direct_mode;
	u16 osd_transparent_color;
	u8  osd_transparent_color_en;
	u8  reserved;
	u32 osd_buf_info_dram_addr;
};

struct toss_vout_osd_buf_setup_s
{
	u32 cmd_code;
	u16 vout_id;
	u16 reserved;
	u32 osd_buf_dram_addr;
	u16 osd_buf_pitch;
	u8  osd_buf_repeat_field;
	u8  reserved2;
};

struct toss_vout_osd_clut_setup_s
{
	u32 cmd_code;
	u16 vout_id;
	u16 reserved;
	u32 clut_dram_addr;
};

#endif
