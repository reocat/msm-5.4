/*
 * arch/arm/mach-ambarella/toss_fb.c
 *
 * Copyright (C) 2008-2009, Ambarella Inc.
 */

/*
 * Aggregate the objects to save time on the linear traversal of dspctx.
 */
struct tossed_fb_info_s {
	struct toss_vout_mixer_setup_s *mixer_setup;
	struct toss_vout_video_setup_s *video_setup;
	struct toss_vout_default_img_setup_s *default_img_setup;
	struct toss_vout_osd_setup_s *osd_setup;
	struct toss_vout_osd_buf_setup_s *osd_buf_setup;
	struct toss_vout_osd_clut_setup_s *osd_clut_setup;
};

/*
 * Locate an object in dspctx matching 'cmd_code' and 'vout_id'.
 */
static void find_tossed_fb_info(const struct toss_devctx_dsp_s *dspctx,
				struct tossed_fb_info_s *info,
				unsigned int vout_id)
{
	struct osd_cmd_hdr {
		u32 cmd_code;
		u16 vout_id;
	} *och;
	int n;

	for (n = 0; n < dspctx->filtered_cmds &&
		     n < MAX_TOSS_DEVCTX_DSP_CMDS; n++) {
		och = (struct osd_cmd_hdr *) &dspctx->cmd[n];
		if (och->vout_id  != vout_id)
			continue;  /* Mismatch in vout_id */

		/* Matched vout_id: extract object */
		switch (och->cmd_code) {
		case 0x7001:
			info->mixer_setup =
				(struct toss_vout_mixer_setup_s *) och;
			break;
		case 0x7002:
			info->video_setup =
				(struct toss_vout_video_setup_s *) och;
			break;
		case 0x7003:
			info->default_img_setup =
				(struct toss_vout_default_img_setup_s *) och;
		case 0x7004:
			info->osd_setup =
				(struct toss_vout_osd_setup_s *) och;
			break;
		case 0x7005:
			info->osd_buf_setup =
				(struct toss_vout_osd_buf_setup_s *) och;
			break;
		case 0x7006:
			info->osd_clut_setup =
				(struct toss_vout_osd_clut_setup_s *) och;
			break;
		}
	}
}

/*
 * TOSS' implementation of fb_pan_display.
 */
static int toss_fb_pan_display(struct fb_var_screeninfo *var,
			       struct fb_info *info,
			       struct ambarella_fb_info *ambfbinfo)
{
	return 0;
}

/*
 * TOSS' implementation of fb_setcmap.
 */
static int toss_fb_setcmap(struct fb_cmap *cmap, struct fb_info *info,
			      struct ambarella_fb_info *ambfbinfo)
{
	return 0;
}

/*
 * TOSS' implementation of fb_check_var.
 */
static int toss_fb_check_var(struct fb_var_screeninfo *var,
			     struct fb_info *info,
			     struct ambarella_fb_info *ambfbinfo)
{
	return 0;
}

/*
 * TOSS' implementation of fb_set_par.
 */
static int toss_fb_set_par(struct fb_info *info,
			   struct ambarella_fb_info *ambinfo)
{
	return 0;
}

/*
 * TOSS' imeplementation of fb_set_blank.
 */
static int toss_fb_set_blank(int blank_mode, struct fb_info *info,
			 struct ambarella_fb_info *ambinfo)
{
	return 0;
}

/*
 * Attempt to get FB data from TOSS.
 */
static int try_tossed_video_setup(void)
{
	struct toss_devctx_dsp_s *dspctx;
	struct tossed_fb_info_s info;
	struct toss_vout_mixer_setup_s *mixer_setup;
	struct toss_vout_video_setup_s *video_setup;
	struct toss_vout_default_img_setup_s *default_img_setup;
	struct toss_vout_osd_setup_s *osd_setup;
	struct toss_vout_osd_buf_setup_s *osd_buf_setup;
	struct toss_vout_osd_clut_setup_s *osd_clut_setup;
	struct ambarella_platform_fb *pfb;
	int i, flag;

	if (toss == NULL)
		return ENODEV;

	dspctx = &toss->devctx[toss->oldctx].dsp;

	for (i = 0; i < CONFIG_AMBARELLA_FB_NUM; i++) {
		find_tossed_fb_info(dspctx, &info, i);
		mixer_setup = info.mixer_setup;
		video_setup = info.video_setup;
		default_img_setup = info.default_img_setup;
		osd_setup = info.osd_setup;
		osd_buf_setup = info.osd_buf_setup;
		osd_clut_setup = info.osd_clut_setup;

		/* Check mandatory objects */
		flag = ((int) mixer_setup |
			(int) osd_setup |
			(int) osd_buf_setup |
			(int) osd_clut_setup);
		if (flag == 0)
			continue;  /* No setup information at all */
		else {
			flag = 0;
			if (mixer_setup == NULL) {
				flag++;
				pr_warning("fb: mixer_setup missing!\n");
			}
			if (osd_setup == NULL) {
				flag++;
				pr_warning("fb: osd_setup missing!\n");
			}
			if (osd_buf_setup == NULL) {
				flag++;
				pr_warning("fb: osd_buf_setup missing!\n");
			}
			if (osd_setup != NULL &&
			    osd_setup->en &&
			    osd_setup->src == 0 &&
			    osd_clut_setup == NULL) {
				flag++;
				pr_warning("fb: clut_setup missing!\n");
			}

			if (flag > 0)
				continue;  /* Avoid danger of missing info */
		}

		if (!osd_setup->en)
			continue;  /* OSD turned off! Can't do anything */

#if (CONFIG_AMBARELLA_FB_NUM >= 1)
		if (i == 0)
			pfb = &ambarella_platform_fb0;
#endif
#if (CONFIG_AMBARELLA_FB_NUM >= 2)
		if (i == 1)
			pfb = &ambarella_platform_fb1;
#endif

		/* --------------------------------- */
		/* Matched! modify 'pfb' accordingly */
		/* --------------------------------- */

		/* --- mixer_setup --- */
		if (mixer_setup->interlaced)
			pfb->screen_var.vmode = FB_VMODE_INTERLACED;
		else
			pfb->screen_var.vmode = FB_VMODE_NONINTERLACED;


		/* --- osd_setup --- */
		/* src */
		switch (osd_setup->src) {
		case 0:
			pfb->color_format = AMBAFB_COLOR_CLUT_8BPP;
			pfb->screen_var.bits_per_pixel = 8;
			break;
		case 1:
			switch (osd_setup->osd_direct_mode) {
			case 0:
				pfb->color_format = AMBAFB_COLOR_YUV565;
				break;
			case 1:
				pfb->color_format = AMBAFB_COLOR_AYUV4444;
				break;
			case 2:
				pfb->color_format = AMBAFB_COLOR_AYUV1555;
			case 3:
				pfb->color_format = AMBAFB_COLOR_YUV555;
				break;
			default:
				pr_warning("fb: unknown osd_direct_mode %d!\n",
					   osd_setup->osd_direct_mode);
				break;
			}
			pfb->screen_var.bits_per_pixel = 16;
			break;
		default:
			pr_warning("fb: unknown src %d!\n", osd_setup->src);
			break;
		}

		pfb->screen_var.xres = osd_setup->win_width;
		pfb->screen_var.xres_virtual = osd_setup->win_width;
		pfb->screen_var.yres = osd_setup->win_height;
		pfb->screen_var.yres_virtual = osd_setup->win_height;
		pr_info("toss-fb: width = %d height = %d\n",
			osd_setup->win_width, osd_setup->win_height);

		/* --- osd_buf_setup --- */
		pfb->screen_fix.smem_start = osd_buf_setup->osd_buf_dram_addr;
		pr_info("toss-fb: smem = 0x%.8x\n",
			(unsigned int) pfb->screen_fix.smem_start);
		pfb->screen_fix.smem_len =
			(osd_buf_setup->osd_buf_pitch * osd_setup->win_height);
		pfb->use_prealloc = 1;
		pfb->prealloc_line_length = osd_buf_setup->osd_buf_pitch;

		/* --- osd_clut_setup --- */
		if (osd_setup->src == 0 && osd_clut_setup != NULL) {
			pfb->clut_table = (u8 *) ambarella_phys_to_virt(
				osd_clut_setup->clut_dram_addr);
		}

		pfb->pan_display = toss_fb_pan_display;
		pfb->setcmap = toss_fb_setcmap;
		pfb->check_var = toss_fb_check_var;
		pfb->set_par = toss_fb_set_par;
		pfb->set_blank = toss_fb_set_blank;
	}

	return 0;
}
