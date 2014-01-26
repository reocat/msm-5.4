/*
 * arch/arm/plat-ambarella/generic/audio.c
 *
 * Author: Anthony Ginger <hfjiang@ambarella.com>
 *
 * Copyright (C) 2004-2010, Ambarella, Inc.
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
#include <linux/dma-mapping.h>
#include <linux/export.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include <mach/hardware.h>
#include <plat/audio.h>
#include <plat/clk.h>

static struct resource ambarella_i2s0_resources[] = {
	[0] = {
		.start	= I2S_BASE,
		.end	= I2S_BASE + 0x0FFF,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= I2STX_IRQ,
		.end	= I2SRX_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device ambarella_i2s0 = {
	.name		= "ambarella-i2s",
	.id		= 0,
	.resource	= ambarella_i2s0_resources,
	.num_resources	= ARRAY_SIZE(ambarella_i2s0_resources),
	.dev		= {
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};

struct platform_device ambarella_pcm0 = {
	.name		= "ambarella-pcm-audio",
	.id		= -1,
};

struct platform_device ambarella_dummy_codec0 = {
	.name		= "ambdummy-codec",
	.id		= -1,
};

struct platform_device ambarella_ambevk_audio_device = {
	.name	= "snd_soc_card_ambevk",
	.id		= -1,
};

struct platform_device ambarella_ipcam_audio_device = {
	.name	= "snd_soc_card_ipcam",
	.id		= -1,
};

struct platform_device ambarella_i1evk_audio_device = {
	.name	= "snd_soc_card_i1evk",
	.id		= -1,
};

struct platform_device ambarella_dummy_audio_device = {
	.name	= "snd_soc_card_ambdummy",
	.id		= -1,
};

struct platform_device ambarella_a5spa2_audio_device = {
	.name	= "snd_soc_card_a5sevk",
	.id		= -1,
};

/* ==========================================================================*/
static struct srcu_notifier_head audio_notifier_list;
static struct notifier_block audio_notify;
static struct ambarella_i2s_interface audio_i2s_intf;

struct ambarella_i2s_interface get_audio_i2s_interface(void)
{
	return audio_i2s_intf;
}
EXPORT_SYMBOL(get_audio_i2s_interface);

static int audio_notify_transition(struct notifier_block *nb,
		unsigned long val, void *data)
{
	switch(val) {
	case AUDIO_NOTIFY_INIT:
		audio_i2s_intf.state = AUDIO_NOTIFY_INIT;
		memcpy(&audio_i2s_intf, data,
			sizeof(struct ambarella_i2s_interface));
		break;

	case AUDIO_NOTIFY_SETHWPARAMS:
		audio_i2s_intf.state = AUDIO_NOTIFY_SETHWPARAMS;
		memcpy(&audio_i2s_intf, data,
			sizeof(struct ambarella_i2s_interface));
		break;

	case AUDIO_NOTIFY_REMOVE:
		memset(&audio_i2s_intf, 0,
			sizeof(struct ambarella_i2s_interface));
		audio_i2s_intf.state = AUDIO_NOTIFY_REMOVE;
		break;
	default:
		audio_i2s_intf.state = AUDIO_NOTIFY_UNKNOWN;
		break;
	}

	return 0;
}

void ambarella_audio_notify_transition (
	struct ambarella_i2s_interface *data, unsigned int type)
{
	srcu_notifier_call_chain(&audio_notifier_list, type, data);
}
EXPORT_SYMBOL(ambarella_audio_notify_transition);

int ambarella_audio_register_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_register( &audio_notifier_list, nb);
}
EXPORT_SYMBOL(ambarella_audio_register_notifier);


int ambarella_audio_unregister_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&audio_notifier_list, nb);
}
EXPORT_SYMBOL(ambarella_audio_unregister_notifier);


int __init ambarella_init_audio(void)
{
	int retval = 0;

	srcu_init_notifier_head(&audio_notifier_list);

	memset(&audio_i2s_intf, 0, sizeof(struct ambarella_i2s_interface));
	audio_i2s_intf.state = AUDIO_NOTIFY_UNKNOWN;

	audio_notify.notifier_call = audio_notify_transition;
	retval = ambarella_audio_register_notifier(&audio_notify);

	return retval;
}

