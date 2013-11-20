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

/* ==========================================================================*/
static struct clk gclk_audio = {
	.parent		= NULL,
	.name		= "gclk_audio",
	.rate		= 0,
	.frac_mode	= 1,
	.ctrl_reg	= PLL_AUDIO_CTRL_REG,
	.pres_reg	= SCALER_AUDIO_PRE_REG,
	.post_reg	= SCALER_AUDIO_POST_REG,
	.frac_reg	= PLL_AUDIO_FRAC_REG,
	.ctrl2_reg	= PLL_AUDIO_CTRL2_REG,
	.ctrl3_reg	= PLL_AUDIO_CTRL3_REG,
	.lock_reg	= PLL_LOCK_REG,
	.lock_bit	= 7,
	.divider	= 0,
	.max_divider	= 0,
	.extra_scaler	= 1,
	.ops		= &ambarella_rct_pll_ops,
};

static struct clk *ambarella_audio_register_clk(void)
{
	struct clk *pgclk_audio = NULL;

	pgclk_audio = clk_get(NULL, "gclk_audio");
	if (IS_ERR(pgclk_audio)) {
		ambarella_clk_add(&gclk_audio);
		pgclk_audio = &gclk_audio;
		pr_info("SYSCLK:AUDIO[%lu]\n", clk_get_rate(pgclk_audio));
	}

	return pgclk_audio;
}

static void set_audio_pll(u32 clksrc, u32 mclk)
{
	clk_set_rate(ambarella_audio_register_clk(), mclk);
}

static void aucodec_digitalio_on_0(void)
{
#if (CHIP_REV == A5S) || (CHIP_REV == I1)
	unsigned long flags;

	ambarella_gpio_raw_lock(1, &flags);
	amba_clrbitsl(GPIO1_REG(GPIO_AFSEL_OFFSET), 0x80000000);
	ambarella_gpio_raw_unlock(1, &flags);

	/* GPIO77~GPIO81 program as hardware mode */
	ambarella_gpio_raw_lock(2, &flags);
	amba_setbitsl(GPIO2_REG(GPIO_AFSEL_OFFSET), 0x0003e000);
	ambarella_gpio_raw_unlock(2, &flags);

#elif (CHIP_REV == S2)
	unsigned long flags;

	ambarella_gpio_raw_lock(3, &flags);
	amba_clrbitsl(GPIO3_REG(GPIO_AFSEL_OFFSET), 0x00000030);
	ambarella_gpio_raw_unlock(3, &flags);

	/* GPIO77~GPIO81 program as hardware mode */
	ambarella_gpio_raw_lock(2, &flags);
	amba_setbitsl(GPIO2_REG(GPIO_AFSEL_OFFSET), 0x0003e000);
	ambarella_gpio_raw_unlock(2, &flags);
#elif (CHIP_REV == S2L)
	pr_err("aucodec_digitalio_on: S2L maybe not set GPIO Fixed \n");
#if 0
	unsigned long flags;

	ambarella_gpio_raw_lock(3, &flags);
	amba_clrbitsl(GPIO3_REG(GPIO_AFSEL_OFFSET), 0x00000030);
	ambarella_gpio_raw_unlock(3, &flags);

	/* GPIO77~GPIO81 program as hardware mode */
	ambarella_gpio_raw_lock(1, &flags);
	amba_setbitsl(GPIO1_REG(GPIO_AFSEL_OFFSET), 0x00003e00);
	ambarella_gpio_raw_unlock(1, &flags);
#endif
#else
	pr_err("aucodec_digitalio_on: Unknown Chip Architecture\n");
#endif
}

static void aucodec_digitalio_on_1(void)
{
#if (CHIP_REV == A5S) || (CHIP_REV == I1)
	unsigned long flags;

	ambarella_gpio_raw_lock(1, &flags);
	amba_clrbitsl(GPIO1_REG(GPIO_AFSEL_OFFSET), 0x80000000);
	ambarella_gpio_raw_unlock(1, &flags);

	/* GPIO77~GPIO78 and GPIO81~GPIO83 program as hardware mode */
	ambarella_gpio_raw_lock(2, &flags);
	amba_setbitsl(GPIO2_REG(GPIO_AFSEL_OFFSET), 0x000e6000);
	ambarella_gpio_raw_unlock(2, &flags);

#elif (CHIP_REV == S2)
	unsigned long flags;

	ambarella_gpio_raw_lock(3, &flags);
	amba_clrbitsl(GPIO3_REG(GPIO_AFSEL_OFFSET), 0x0000000c);
	ambarella_gpio_raw_unlock(3, &flags);

	/* GPIO82~GPIO83 program as hardware mode */
	ambarella_gpio_raw_lock(2, &flags);
	amba_setbitsl(GPIO2_REG(GPIO_AFSEL_OFFSET), 0x000c0000);
	ambarella_gpio_raw_unlock(2, &flags);

#else
	pr_err("aucodec_digitalio_on: Unknown Chip Architecture\n");
#endif
}

static void aucodec_digitalio_on_2(void)
{
#if (CHIP_REV == A5S) || (CHIP_REV == I1)
	unsigned long flags;

	ambarella_gpio_raw_lock(1, &flags);
	amba_clrbitsl(GPIO1_REG(GPIO_AFSEL_OFFSET), 0x80000000);
	ambarella_gpio_raw_unlock(1, &flags);

	/* GPIO77~GPIO78, GPIO81 and GPIO84~GPIO85 program as hardware mode */
	ambarella_gpio_raw_lock(2, &flags);
	amba_setbitsl(GPIO2_REG(GPIO_AFSEL_OFFSET), 0x00326000);
	ambarella_gpio_raw_unlock(2, &flags);
#else
	pr_err("aucodec_digitalio_on: Unknown Chip Architecture\n");
#endif
}

static void i2s_channel_select(u32 ch)
{
#if (CHIP_REV == A5S) || (CHIP_REV == I1) || (CHIP_REV == S2) || \
	(CHIP_REV == S2L)
	u32 ch_reg_num;

	ch_reg_num = amba_readl(I2S_REG(I2S_CHANNEL_SELECT_OFFSET));

	switch (ch) {
	case 2:
		if (ch_reg_num != 0) {
			amba_writel(I2S_REG(I2S_CHANNEL_SELECT_OFFSET),
				I2S_2CHANNELS_ENB);
		}
		break;
	case 4:
		if (ch_reg_num != 1) {
			amba_writel(I2S_REG(I2S_CHANNEL_SELECT_OFFSET),
				I2S_4CHANNELS_ENB);
		}
		break;
	case 6:
		if (ch_reg_num != 2) {
			amba_writel(I2S_REG(I2S_CHANNEL_SELECT_OFFSET),
				I2S_6CHANNELS_ENB);
		}
		break;
	default:
		printk("Don't support %d channels\n", ch);
		break;
	}
#endif
}

static struct ambarella_i2s_controller ambarella_platform_i2s_controller0 = {
	.set_audio_pll		= set_audio_pll,
	.aucodec_digitalio_0	= aucodec_digitalio_on_0,
	.aucodec_digitalio_1	= aucodec_digitalio_on_1,
	.aucodec_digitalio_2	= aucodec_digitalio_on_2,
	.channel_select		= i2s_channel_select,
};

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
		.platform_data		= &ambarella_platform_i2s_controller0,
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

