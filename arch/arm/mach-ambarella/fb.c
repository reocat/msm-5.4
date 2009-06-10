/*
 * arch/arm/mach-ambarella/fb.c
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/bootmem.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/dma-mapping.h>

#include <asm/page.h>
#include <asm/io.h>
#include <asm/setup.h>

#include <asm/mach/map.h>

#include <linux/fb.h>

#include <mach/hardware.h>
#include <mach/fb.h>

/* ==========================================================================*/
#ifdef CONFIG_AMBARELLA_FB0
static struct ambarella_platform_fb ambarella_platform_fb0 = {
	.screen_var		= {
		.xres		= 720,
		.yres		= 480,
		.xres_virtual	= 720,
		.yres_virtual	= 480,
		.xoffset	= 0,
		.yoffset	= 0,
		.bits_per_pixel = 8,
		.red		= {.offset = 0, .length = 8, .msb_right = 0},
		.green		= {.offset = 0, .length = 8, .msb_right = 0},
		.blue		= {.offset = 0, .length = 8, .msb_right = 0},
		.activate	= FB_ACTIVATE_NOW,
		.height		= -1,
		.width		= -1,
		.pixclock	= 36101,
		.left_margin	= 24,
		.right_margin	= 96,
		.upper_margin	= 10,
		.lower_margin	= 32,
		.hsync_len	= 40,
		.vsync_len	= 3,
		.vmode		= FB_VMODE_NONINTERLACED,
	},
	.screen_fix		= {
		.id		= "Ambarella FB",
		.type		= FB_TYPE_PACKED_PIXELS,
		.visual		= FB_VISUAL_PSEUDOCOLOR,
		.xpanstep	= 1,
		.ypanstep	= 1,
		.ywrapstep	= 1,
		.accel		= FB_ACCEL_NONE,
		.line_length	= 1024,
	},
	.dsp_status		= AMBA_DSP_UNKNOWN_MODE,
	.fb_status		= AMBAFB_UNKNOWN_MODE,
	.clut_table		= {
		0, 128, 128,
		1, 128, 128,
		2, 128, 128,
		3, 128, 128,
		4, 128, 128,
		5, 128, 128,
		6, 128, 128,
		7, 128, 128,
		8, 128, 128,
		9, 128, 128,
		10, 128, 128,
		11, 128, 128,
		12, 128, 128,
		13, 128, 128,
		14, 128, 128,
		15, 128, 128,
		16, 128, 128,
		17, 128, 128,
		18, 128, 128,
		19, 128, 128,
		20, 128, 128,
		21, 128, 128,
		22, 128, 128,
		23, 128, 128,
		24, 128, 128,
		25, 128, 128,
		26, 128, 128,
		27, 128, 128,
		28, 128, 128,
		29, 128, 128,
		30, 128, 128,
		31, 128, 128,
		32, 128, 128,
		33, 128, 128,
		34, 128, 128,
		35, 128, 128,
		36, 128, 128,
		37, 128, 128,
		38, 128, 128,
		39, 128, 128,
		40, 128, 128,
		41, 128, 128,
		42, 128, 128,
		43, 128, 128,
		44, 128, 128,
		45, 128, 128,
		46, 128, 128,
		47, 128, 128,
		48, 128, 128,
		49, 128, 128,
		50, 128, 128,
		51, 128, 128,
		52, 128, 128,
		53, 128, 128,
		54, 128, 128,
		55, 128, 128,
		56, 128, 128,
		57, 128, 128,
		58, 128, 128,
		59, 128, 128,
		60, 128, 128,
		61, 128, 128,
		62, 128, 128,
		63, 128, 128,
		64, 128, 128,
		65, 128, 128,
		66, 128, 128,
		67, 128, 128,
		68, 128, 128,
		69, 128, 128,
		70, 128, 128,
		71, 128, 128,
		72, 128, 128,
		73, 128, 128,
		74, 128, 128,
		75, 128, 128,
		76, 128, 128,
		77, 128, 128,
		78, 128, 128,
		79, 128, 128,
		80, 128, 128,
		81, 128, 128,
		82, 128, 128,
		83, 128, 128,
		84, 128, 128,
		85, 128, 128,
		86, 128, 128,
		87, 128, 128,
		88, 128, 128,
		89, 128, 128,
		90, 128, 128,
		91, 128, 128,
		92, 128, 128,
		93, 128, 128,
		94, 128, 128,
		95, 128, 128,
		96, 128, 128,
		97, 128, 128,
		98, 128, 128,
		99, 128, 128,
		100, 128, 128,
		101, 128, 128,
		102, 128, 128,
		103, 128, 128,
		104, 128, 128,
		105, 128, 128,
		106, 128, 128,
		107, 128, 128,
		108, 128, 128,
		109, 128, 128,
		110, 128, 128,
		111, 128, 128,
		112, 128, 128,
		113, 128, 128,
		114, 128, 128,
		115, 128, 128,
		116, 128, 128,
		117, 128, 128,
		118, 128, 128,
		119, 128, 128,
		120, 128, 128,
		121, 128, 128,
		122, 128, 128,
		123, 128, 128,
		124, 128, 128,
		125, 128, 128,
		126, 128, 128,
		127, 128, 128,
		128, 128, 128,
		129, 128, 128,
		130, 128, 128,
		131, 128, 128,
		132, 128, 128,
		133, 128, 128,
		134, 128, 128,
		135, 128, 128,
		136, 128, 128,
		137, 128, 128,
		138, 128, 128,
		139, 128, 128,
		140, 128, 128,
		141, 128, 128,
		142, 128, 128,
		143, 128, 128,
		144, 128, 128,
		145, 128, 128,
		146, 128, 128,
		147, 128, 128,
		148, 128, 128,
		149, 128, 128,
		150, 128, 128,
		151, 128, 128,
		152, 128, 128,
		153, 128, 128,
		154, 128, 128,
		155, 128, 128,
		156, 128, 128,
		157, 128, 128,
		158, 128, 128,
		159, 128, 128,
		160, 128, 128,
		161, 128, 128,
		162, 128, 128,
		163, 128, 128,
		164, 128, 128,
		165, 128, 128,
		166, 128, 128,
		167, 128, 128,
		168, 128, 128,
		169, 128, 128,
		170, 128, 128,
		171, 128, 128,
		172, 128, 128,
		173, 128, 128,
		174, 128, 128,
		175, 128, 128,
		176, 128, 128,
		177, 128, 128,
		178, 128, 128,
		179, 128, 128,
		180, 128, 128,
		181, 128, 128,
		182, 128, 128,
		183, 128, 128,
		184, 128, 128,
		185, 128, 128,
		186, 128, 128,
		187, 128, 128,
		188, 128, 128,
		189, 128, 128,
		190, 128, 128,
		191, 128, 128,
		192, 128, 128,
		193, 128, 128,
		194, 128, 128,
		195, 128, 128,
		196, 128, 128,
		197, 128, 128,
		198, 128, 128,
		199, 128, 128,
		200, 128, 128,
		201, 128, 128,
		202, 128, 128,
		203, 128, 128,
		204, 128, 128,
		205, 128, 128,
		206, 128, 128,
		207, 128, 128,
		208, 128, 128,
		209, 128, 128,
		210, 128, 128,
		211, 128, 128,
		212, 128, 128,
		213, 128, 128,
		214, 128, 128,
		215, 128, 128,
		216, 128, 128,
		217, 128, 128,
		218, 128, 128,
		219, 128, 128,
		220, 128, 128,
		221, 128, 128,
		222, 128, 128,
		223, 128, 128,
		224, 128, 128,
		225, 128, 128,
		226, 128, 128,
		227, 128, 128,
		228, 128, 128,
		229, 128, 128,
		230, 128, 128,
		231, 128, 128,
		232, 128, 128,
		233, 128, 128,
		234, 128, 128,
		235, 128, 128,
		236, 128, 128,
		237, 128, 128,
		238, 128, 128,
		239, 128, 128,
		240, 128, 128,
		241, 128, 128,
		242, 128, 128,
		243, 128, 128,
		244, 128, 128,
		245, 128, 128,
		246, 128, 128,
		247, 128, 128,
		248, 128, 128,
		249, 128, 128,
		250, 128, 128,
		251, 128, 128,
		252, 128, 128,
		253, 128, 128,
		254, 128, 128,
		255, 128, 128,
		},
	.blend_table		= {
		0,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
	},
	.color_format		= AMBAFB_COLOR_CLUT_8BPP,
	.x_multiplication	= 1,
	.y_multiplication	= 1,

	.pan_display		= NULL,
	.setcmap		= NULL,
	.check_var		= NULL,
	.set_par		= NULL,

	.proc_fb_info		= NULL,
	.proc_file		= NULL,
};

struct platform_device ambarella_fb0 = {
	.name			= "ambarella-fb",
	.id			= 0,
	.resource		= NULL,
	.num_resources		= 0,
	.dev			= {
		.platform_data		= &ambarella_platform_fb0,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_32BIT_MASK,
	}
};
#endif

#ifdef CONFIG_AMBARELLA_FB1
static struct ambarella_platform_fb ambarella_platform_fb1 = {
	.screen_var		= {
		.xres		= 720,
		.yres		= 480,
		.xres_virtual	= 720,
		.yres_virtual	= 480,
		.xoffset	= 0,
		.yoffset	= 0,
		.bits_per_pixel = 8,
		.red		= {.offset = 0, .length = 8, .msb_right = 0},
		.green		= {.offset = 0, .length = 8, .msb_right = 0},
		.blue		= {.offset = 0, .length = 8, .msb_right = 0},
		.activate	= FB_ACTIVATE_NOW,
		.height		= -1,
		.width		= -1,
		.pixclock	= 36101,
		.left_margin	= 24,
		.right_margin	= 96,
		.upper_margin	= 10,
		.lower_margin	= 32,
		.hsync_len	= 40,
		.vsync_len	= 3,
		.vmode		= FB_VMODE_NONINTERLACED,
	},
	.screen_fix		= {
		.id		= "Ambarella FB",
		.type		= FB_TYPE_PACKED_PIXELS,
		.visual		= FB_VISUAL_PSEUDOCOLOR,
		.xpanstep	= 1,
		.ypanstep	= 1,
		.ywrapstep	= 1,
		.accel		= FB_ACCEL_NONE,
		.line_length	= 1024,
	},
	.dsp_status		= AMBA_DSP_UNKNOWN_MODE,
	.fb_status		= AMBAFB_UNKNOWN_MODE,
	.clut_table		= {
		0, 128, 128,
		1, 128, 128,
		2, 128, 128,
		3, 128, 128,
		4, 128, 128,
		5, 128, 128,
		6, 128, 128,
		7, 128, 128,
		8, 128, 128,
		9, 128, 128,
		10, 128, 128,
		11, 128, 128,
		12, 128, 128,
		13, 128, 128,
		14, 128, 128,
		15, 128, 128,
		16, 128, 128,
		17, 128, 128,
		18, 128, 128,
		19, 128, 128,
		20, 128, 128,
		21, 128, 128,
		22, 128, 128,
		23, 128, 128,
		24, 128, 128,
		25, 128, 128,
		26, 128, 128,
		27, 128, 128,
		28, 128, 128,
		29, 128, 128,
		30, 128, 128,
		31, 128, 128,
		32, 128, 128,
		33, 128, 128,
		34, 128, 128,
		35, 128, 128,
		36, 128, 128,
		37, 128, 128,
		38, 128, 128,
		39, 128, 128,
		40, 128, 128,
		41, 128, 128,
		42, 128, 128,
		43, 128, 128,
		44, 128, 128,
		45, 128, 128,
		46, 128, 128,
		47, 128, 128,
		48, 128, 128,
		49, 128, 128,
		50, 128, 128,
		51, 128, 128,
		52, 128, 128,
		53, 128, 128,
		54, 128, 128,
		55, 128, 128,
		56, 128, 128,
		57, 128, 128,
		58, 128, 128,
		59, 128, 128,
		60, 128, 128,
		61, 128, 128,
		62, 128, 128,
		63, 128, 128,
		64, 128, 128,
		65, 128, 128,
		66, 128, 128,
		67, 128, 128,
		68, 128, 128,
		69, 128, 128,
		70, 128, 128,
		71, 128, 128,
		72, 128, 128,
		73, 128, 128,
		74, 128, 128,
		75, 128, 128,
		76, 128, 128,
		77, 128, 128,
		78, 128, 128,
		79, 128, 128,
		80, 128, 128,
		81, 128, 128,
		82, 128, 128,
		83, 128, 128,
		84, 128, 128,
		85, 128, 128,
		86, 128, 128,
		87, 128, 128,
		88, 128, 128,
		89, 128, 128,
		90, 128, 128,
		91, 128, 128,
		92, 128, 128,
		93, 128, 128,
		94, 128, 128,
		95, 128, 128,
		96, 128, 128,
		97, 128, 128,
		98, 128, 128,
		99, 128, 128,
		100, 128, 128,
		101, 128, 128,
		102, 128, 128,
		103, 128, 128,
		104, 128, 128,
		105, 128, 128,
		106, 128, 128,
		107, 128, 128,
		108, 128, 128,
		109, 128, 128,
		110, 128, 128,
		111, 128, 128,
		112, 128, 128,
		113, 128, 128,
		114, 128, 128,
		115, 128, 128,
		116, 128, 128,
		117, 128, 128,
		118, 128, 128,
		119, 128, 128,
		120, 128, 128,
		121, 128, 128,
		122, 128, 128,
		123, 128, 128,
		124, 128, 128,
		125, 128, 128,
		126, 128, 128,
		127, 128, 128,
		128, 128, 128,
		129, 128, 128,
		130, 128, 128,
		131, 128, 128,
		132, 128, 128,
		133, 128, 128,
		134, 128, 128,
		135, 128, 128,
		136, 128, 128,
		137, 128, 128,
		138, 128, 128,
		139, 128, 128,
		140, 128, 128,
		141, 128, 128,
		142, 128, 128,
		143, 128, 128,
		144, 128, 128,
		145, 128, 128,
		146, 128, 128,
		147, 128, 128,
		148, 128, 128,
		149, 128, 128,
		150, 128, 128,
		151, 128, 128,
		152, 128, 128,
		153, 128, 128,
		154, 128, 128,
		155, 128, 128,
		156, 128, 128,
		157, 128, 128,
		158, 128, 128,
		159, 128, 128,
		160, 128, 128,
		161, 128, 128,
		162, 128, 128,
		163, 128, 128,
		164, 128, 128,
		165, 128, 128,
		166, 128, 128,
		167, 128, 128,
		168, 128, 128,
		169, 128, 128,
		170, 128, 128,
		171, 128, 128,
		172, 128, 128,
		173, 128, 128,
		174, 128, 128,
		175, 128, 128,
		176, 128, 128,
		177, 128, 128,
		178, 128, 128,
		179, 128, 128,
		180, 128, 128,
		181, 128, 128,
		182, 128, 128,
		183, 128, 128,
		184, 128, 128,
		185, 128, 128,
		186, 128, 128,
		187, 128, 128,
		188, 128, 128,
		189, 128, 128,
		190, 128, 128,
		191, 128, 128,
		192, 128, 128,
		193, 128, 128,
		194, 128, 128,
		195, 128, 128,
		196, 128, 128,
		197, 128, 128,
		198, 128, 128,
		199, 128, 128,
		200, 128, 128,
		201, 128, 128,
		202, 128, 128,
		203, 128, 128,
		204, 128, 128,
		205, 128, 128,
		206, 128, 128,
		207, 128, 128,
		208, 128, 128,
		209, 128, 128,
		210, 128, 128,
		211, 128, 128,
		212, 128, 128,
		213, 128, 128,
		214, 128, 128,
		215, 128, 128,
		216, 128, 128,
		217, 128, 128,
		218, 128, 128,
		219, 128, 128,
		220, 128, 128,
		221, 128, 128,
		222, 128, 128,
		223, 128, 128,
		224, 128, 128,
		225, 128, 128,
		226, 128, 128,
		227, 128, 128,
		228, 128, 128,
		229, 128, 128,
		230, 128, 128,
		231, 128, 128,
		232, 128, 128,
		233, 128, 128,
		234, 128, 128,
		235, 128, 128,
		236, 128, 128,
		237, 128, 128,
		238, 128, 128,
		239, 128, 128,
		240, 128, 128,
		241, 128, 128,
		242, 128, 128,
		243, 128, 128,
		244, 128, 128,
		245, 128, 128,
		246, 128, 128,
		247, 128, 128,
		248, 128, 128,
		249, 128, 128,
		250, 128, 128,
		251, 128, 128,
		252, 128, 128,
		253, 128, 128,
		254, 128, 128,
		255, 128, 128,
		},
	.blend_table		= {
		0,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
		12,  12, 12, 12, 12, 12, 12, 12,
	},
	.color_format		= AMBAFB_COLOR_CLUT_8BPP,
	.x_multiplication	= 1,
	.y_multiplication	= 1,

	.pan_display		= NULL,
	.setcmap		= NULL,
	.check_var		= NULL,
	.set_par		= NULL,

	.proc_fb_info		= NULL,
	.proc_file		= NULL,
};

struct platform_device ambarella_fb1 = {
	.name			= "ambarella-fb",
	.id			= 1,
	.resource		= NULL,
	.num_resources		= 0,
	.dev			= {
		.platform_data		= &ambarella_platform_fb1,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_32BIT_MASK,
	}
};
#endif

static int __init parse_videolfb(const struct tag *tag)
{
#ifdef CONFIG_AMBARELLA_FB0
	ambarella_platform_fb0.screen_var.xres = tag->u.videolfb.lfb_width;
	ambarella_platform_fb0.screen_var.xres_virtual = tag->u.videolfb.lfb_width;
	ambarella_platform_fb0.screen_var.yres = tag->u.videolfb.lfb_height;
	ambarella_platform_fb0.screen_var.yres_virtual = tag->u.videolfb.lfb_height;

	ambarella_platform_fb0.screen_var.red.offset = tag->u.videolfb.red_pos;
	ambarella_platform_fb0.screen_var.green.offset = tag->u.videolfb.green_pos;
	ambarella_platform_fb0.screen_var.blue.offset = tag->u.videolfb.blue_pos;
	ambarella_platform_fb0.screen_var.red.length = tag->u.videolfb.red_size;
	ambarella_platform_fb0.screen_var.green.length = tag->u.videolfb.green_size;
	ambarella_platform_fb0.screen_var.blue.length = tag->u.videolfb.blue_size;
	ambarella_platform_fb0.screen_var.red.msb_right = 0;
	ambarella_platform_fb0.screen_var.green.msb_right = 0;
	ambarella_platform_fb0.screen_var.blue.msb_right = 0;
	ambarella_platform_fb0.screen_var.transp.offset = tag->u.videolfb.rsvd_pos;
	ambarella_platform_fb0.screen_var.transp.length = tag->u.videolfb.rsvd_size;
	ambarella_platform_fb0.screen_var.transp.msb_right = 0;

	ambarella_platform_fb0.screen_var.bits_per_pixel = tag->u.videolfb.lfb_depth;
	ambarella_platform_fb0.screen_fix.line_length = tag->u.videolfb.lfb_linelength;

	//tag->u.videolfb.lfb_base = 0xc3800000;
	//tag->u.videolfb.lfb_size = 0x00001000;

	ambarella_platform_fb0.dsp_status = AMBA_DSP_QUICKLOGO_MODE;
#endif

#ifdef CONFIG_AMBARELLA_FB1
	ambarella_platform_fb1.screen_var.xres = tag->u.videolfb.lfb_width;
	ambarella_platform_fb1.screen_var.xres_virtual = tag->u.videolfb.lfb_width;
	ambarella_platform_fb1.screen_var.yres = tag->u.videolfb.lfb_height;
	ambarella_platform_fb1.screen_var.yres_virtual = tag->u.videolfb.lfb_height;

	ambarella_platform_fb1.screen_var.red.offset = tag->u.videolfb.red_pos;
	ambarella_platform_fb1.screen_var.green.offset = tag->u.videolfb.green_pos;
	ambarella_platform_fb1.screen_var.blue.offset = tag->u.videolfb.blue_pos;
	ambarella_platform_fb1.screen_var.red.length = tag->u.videolfb.red_size;
	ambarella_platform_fb1.screen_var.green.length = tag->u.videolfb.green_size;
	ambarella_platform_fb1.screen_var.blue.length = tag->u.videolfb.blue_size;
	ambarella_platform_fb1.screen_var.red.msb_right = 0;
	ambarella_platform_fb1.screen_var.green.msb_right = 0;
	ambarella_platform_fb1.screen_var.blue.msb_right = 0;
	ambarella_platform_fb1.screen_var.transp.offset = tag->u.videolfb.rsvd_pos;
	ambarella_platform_fb1.screen_var.transp.length = tag->u.videolfb.rsvd_size;
	ambarella_platform_fb1.screen_var.transp.msb_right = 0;

	ambarella_platform_fb1.screen_var.bits_per_pixel = tag->u.videolfb.lfb_depth;
	ambarella_platform_fb1.screen_fix.line_length = tag->u.videolfb.lfb_linelength;

	//tag->u.videolfb.lfb_base = 0xc3800000;
	//tag->u.videolfb.lfb_size = 0x00001000;

	ambarella_platform_fb1.dsp_status = AMBA_DSP_QUICKLOGO_MODE;
#endif

	return 0;
}
__tagtable(ATAG_VIDEOLFB, parse_videolfb);

int ambarella_fb_get_platform_info(u32 fb_id,
	struct ambarella_platform_fb *platform_info)
{
	u32					status = -EPERM;

	if (fb_id == 0) {
#ifdef CONFIG_AMBARELLA_FB0
		mutex_lock(&ambarella_platform_fb0.lock);
		memcpy(platform_info, &ambarella_platform_fb0,
			sizeof(struct ambarella_platform_fb));
		mutex_unlock(&ambarella_platform_fb0.lock);

		status = 0;
#endif
	} else
	if (fb_id == 1) {
#ifdef CONFIG_AMBARELLA_FB1
		mutex_lock(&ambarella_platform_fb1.lock);
		memcpy(platform_info, &ambarella_platform_fb1,
			sizeof(struct ambarella_platform_fb));
		mutex_unlock(&ambarella_platform_fb1.lock);

		status = 0;
#endif
	}
	

	return status;
}
EXPORT_SYMBOL(ambarella_fb_get_platform_info);

int ambarella_fb_set_iav_info(u32 fb_id, struct ambarella_fb_iav_info *iav)
{
	u32					status = -EPERM;

	if (fb_id == 0) {
#ifdef CONFIG_AMBARELLA_FB0
		mutex_lock(&ambarella_platform_fb0.lock);
		ambarella_platform_fb0.screen_var = iav->screen_var;
		ambarella_platform_fb0.screen_fix = iav->screen_fix;
		ambarella_platform_fb0.pan_display = iav->pan_display;
		ambarella_platform_fb0.setcmap = iav->setcmap;
		ambarella_platform_fb0.check_var = iav->check_var;
		ambarella_platform_fb0.set_par = iav->set_par;
		ambarella_platform_fb0.dsp_status = iav->dsp_status;
		mutex_unlock(&ambarella_platform_fb0.lock);

		status = 0;
#endif
	} else
	if (fb_id == 1) {
#ifdef CONFIG_AMBARELLA_FB1
		mutex_lock(&ambarella_platform_fb1.lock);
		ambarella_platform_fb1.screen_var = iav->screen_var;
		ambarella_platform_fb1.screen_fix = iav->screen_fix;
		ambarella_platform_fb1.pan_display = iav->pan_display;
		ambarella_platform_fb1.setcmap = iav->setcmap;
		ambarella_platform_fb1.check_var = iav->check_var;
		ambarella_platform_fb1.set_par = iav->set_par;
		ambarella_platform_fb1.dsp_status = iav->dsp_status;
		mutex_unlock(&ambarella_platform_fb1.lock);

		status = 0;
#endif
	}

	return status;
}
EXPORT_SYMBOL(ambarella_fb_set_iav_info);

int __init ambarella_init_fb(void)
{
	int					errorCode = 0;

#ifdef CONFIG_AMBARELLA_FB0
	mutex_init(&ambarella_platform_fb0.lock);
	init_waitqueue_head(&ambarella_platform_fb0.proc_wait);
#endif

#ifdef CONFIG_AMBARELLA_FB1
	mutex_init(&ambarella_platform_fb1.lock);
	init_waitqueue_head(&ambarella_platform_fb1.proc_wait);
#endif

	return errorCode;
}

