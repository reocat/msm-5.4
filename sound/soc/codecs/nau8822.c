/*
 * nau8822.c  --  NAU8822 ALSA SoC Audio Codec driver
 *
 * Copyright (C) 2009-2010 Guennadi Liakhovetski <g.liakhovetski@gmx.de>
 * Copyright (C) 2007 Carlos Munoz <carlos@kenati.com>
 * Copyright 2006-2009 Wolfson Microelectronics PLC.
 * Based on wm8974 and wm8990 by Liam Girdwood <lrg@slimlogic.co.uk>
 * 2013.3.27 modified from wm8978.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <asm/div64.h>

#include "nau8822.h"


//#define NAU8822_DEBUG

#ifdef NAU8822_DEBUG
#define DBG(fmt, arg...)			printk(fmt, ##arg)
#else
#define DBG(fmt, arg...)
#endif

static struct reg_default nau8822_reg[] = {
{0x1, 0x11a},
{0x2, 0x1bf},
{0x3, 0x0},
{0x4, 0x118},
{0x5, 0x1},
{0x6, 0x149},
{0x7, 0x0},
{0x8, 0x0},
{0x9, 0x0},
{0xa, 0xc},
{0xb, 0x1ff},
{0xc, 0x1ff},
{0xd, 0x000},
{0xe, 0x108},
{0xf, 0x1ff},
{0x10, 0x1ff},
{0x11, 0x6},
{0x12, 0x12c},
{0x13, 0x2c},
{0x14, 0x2c},
{0x15, 0x2c},
{0x16, 0x2c},
{0x18, 0x32},
{0x19, 0x0},
{0x1b, 0x100},
{0x1c, 0x100},
{0x1d, 0x100},
{0x1e, 0x100},
{0x20, 0x38},
{0x21, 0xb},
{0x22, 0x32},
{0x23, 0x10},
{0x24, 0x8},
{0x25, 0xc},
{0x26, 0x93},
{0x27, 0xe9},
{0x28, 0x0},
{0x29, 0x0},
{0x2b, 0x10},
{0x2c, 0x181},
{0x2d, 0x130},
{0x2e, 0x130},
{0x2f, 0x100},
{0x30, 0x100},
{0x31, 0x7},
{0x32, 0x1},
{0x33, 0x1},
{0x34, 0x165},
{0x35, 0x165},
{0x36, 0x139},
{0x37, 0x139},
{0x38, 0x140},
{0x39, 0x140},
{0x3a, 0x0},
{0x3b, 0x0},
{0x3c, 0x20},
{0x3d, 0x20},
{0x3e, 0xfd},

{0x3f, 0x1a},
{0x40, 0x0},
{0x41, 0x0},
{0x42, 0x0},
{0x43, 0x0},
{0x44, 0x0},
{0x45, 0x0},
{0x46, 0x0},
{0x47, 0x1a},
{0x48, 0x8},
{0x49, 0x0},
{0x4a, 0x0},
{0x4b, 0x0},
{0x4c, 0x0},
{0x4d, 0x0},
{0x4e, 0x0},
{0x4f, 0x0},
};



static bool nau8822_volatile(struct device *dev, unsigned int reg)
{
	//DBG("\t[NAU8822]  [%-10s] : Enter %d\n", __FUNCTION__,__LINE__);
	switch (reg) {
		case NAU8822_RESET:
			return true;
		default:
			return false;
	}

}


/*static bool nau8822_readable(struct device *dev, unsigned int reg)
{
	//DBG("\t[NAU8822]  [%-10s] : Enter %d reg=[%d]\n", __FUNCTION__,__LINE__,reg);
	switch (reg) {
		case NAU8822_RESET:
		case NAU8822_0x17:
		case NAU8822_0x1a:
		case NAU8822_0x1f:
		case NAU8822_0x2a:
			return false;
		default:
			return true;
	}

}*/

/* codec private data */
struct nau8822_priv {
	struct snd_soc_codec codec;
	struct regmap *regmap;
	unsigned int f_pllout;	//f_pllout = f2/4,f2: 90MHz< f2 <100MHz
	unsigned int f_mclk;	//f_mclk is always 12.288M,f_mclk*[12]=f1
	unsigned int f_256fs;	//IMCLK=f_256fs=f_pllout/mclk_div(mclk_div=1,1.5,2,3,4,5,6,8,12)0x06[5-7],UNIT:MHz
	unsigned int extra_clk;
	enum nau8822_sysclk sysclk;	//system clock source: PLL or MCLK
	enum nau8822_sysmod sysmod;
	//u8 reg_cache[NAU8822_CACHEREGNUM];
	u8 reg_cache[80];
};


static const char *nau8822_companding[] = {"Off", "NC", "u-law", "A-law"};
static const char *nau8822_eqmode[] = {"Capture", "Playback"};
static const char *nau8822_bw[] = {"Narrow", "Wide"};
static const char *nau8822_eq1[] = {"80Hz", "105Hz", "135Hz", "175Hz"};
static const char *nau8822_eq2[] = {"230Hz", "300Hz", "385Hz", "500Hz"};
static const char *nau8822_eq3[] = {"650Hz", "850Hz", "1.1kHz", "1.4kHz"};
static const char *nau8822_eq4[] = {"1.8kHz", "2.4kHz", "3.2kHz", "4.1kHz"};
static const char *nau8822_eq5[] = {"5.3kHz", "6.9kHz", "9kHz", "11.7kHz"};
static const char *nau8822_alc3[] = {"ALC", "Limiter"};
static const char *nau8822_alc1[] = {"Off", "Right", "Left", "Both"};

static const SOC_ENUM_SINGLE_DECL(adc_compand, NAU8822_COMPANDING_CONTROL, 1,
				  nau8822_companding);
static const SOC_ENUM_SINGLE_DECL(dac_compand, NAU8822_COMPANDING_CONTROL, 3,
				  nau8822_companding);
static const SOC_ENUM_SINGLE_DECL(eqmode, NAU8822_EQ1, 8, nau8822_eqmode);
static const SOC_ENUM_SINGLE_DECL(eq1, NAU8822_EQ1, 5, nau8822_eq1);
static const SOC_ENUM_SINGLE_DECL(eq2bw, NAU8822_EQ2, 8, nau8822_bw);
static const SOC_ENUM_SINGLE_DECL(eq2, NAU8822_EQ2, 5, nau8822_eq2);
static const SOC_ENUM_SINGLE_DECL(eq3bw, NAU8822_EQ3, 8, nau8822_bw);
static const SOC_ENUM_SINGLE_DECL(eq3, NAU8822_EQ3, 5, nau8822_eq3);
static const SOC_ENUM_SINGLE_DECL(eq4bw, NAU8822_EQ4, 8, nau8822_bw);
static const SOC_ENUM_SINGLE_DECL(eq4, NAU8822_EQ4, 5, nau8822_eq4);
static const SOC_ENUM_SINGLE_DECL(eq5, NAU8822_EQ5, 5, nau8822_eq5);
static const SOC_ENUM_SINGLE_DECL(alc3, NAU8822_ALC_CONTROL_3, 8, nau8822_alc3);
static const SOC_ENUM_SINGLE_DECL(alc1, NAU8822_ALC_CONTROL_1, 7, nau8822_alc1);

static const DECLARE_TLV_DB_SCALE(digital_tlv, -12750, 50, 1);
static const DECLARE_TLV_DB_SCALE(eq_tlv, -1200, 100, 0);
static const DECLARE_TLV_DB_SCALE(inpga_tlv, -1200, 75, 0);
static const DECLARE_TLV_DB_SCALE(spk_tlv, -5700, 100, 0);
static const DECLARE_TLV_DB_SCALE(boost_tlv, -1500, 300, 1);
static const DECLARE_TLV_DB_SCALE(limiter_tlv, 0, 100, 0);

static const struct snd_kcontrol_new nau8822_snd_controls[] = {

	SOC_SINGLE("Digital Loopback Switch",
		NAU8822_COMPANDING_CONTROL, 0, 1, 0),

	SOC_ENUM("ADC Companding", adc_compand),
	SOC_ENUM("DAC Companding", dac_compand),

	SOC_DOUBLE("DAC Inversion Switch", NAU8822_DAC_CONTROL, 0, 1, 1, 0),

	SOC_DOUBLE_R_TLV("PCM Volume",
		NAU8822_LEFT_DAC_DIGITAL_VOLUME, NAU8822_RIGHT_DAC_DIGITAL_VOLUME,
		0, 255, 0, digital_tlv),

	SOC_SINGLE("High Pass Filter Switch", NAU8822_ADC_CONTROL, 8, 1, 0),
	SOC_SINGLE("High Pass Cut Off", NAU8822_ADC_CONTROL, 4, 7, 0),
	SOC_DOUBLE("ADC Inversion Switch", NAU8822_ADC_CONTROL, 0, 1, 1, 0),

	SOC_DOUBLE_R_TLV("ADC Volume",
		NAU8822_LEFT_ADC_DIGITAL_VOLUME, NAU8822_RIGHT_ADC_DIGITAL_VOLUME,
		0, 255, 0, digital_tlv),

	SOC_ENUM("Equaliser Function", eqmode),
	SOC_ENUM("EQ1 Cut Off", eq1),
	SOC_SINGLE_TLV("EQ1 Volume", NAU8822_EQ1,  0, 24, 1, eq_tlv),

	SOC_ENUM("Equaliser EQ2 Bandwith", eq2bw),
	SOC_ENUM("EQ2 Cut Off", eq2),
	SOC_SINGLE_TLV("EQ2 Volume", NAU8822_EQ2,  0, 24, 1, eq_tlv),

	SOC_ENUM("Equaliser EQ3 Bandwith", eq3bw),
	SOC_ENUM("EQ3 Cut Off", eq3),
	SOC_SINGLE_TLV("EQ3 Volume", NAU8822_EQ3,  0, 24, 1, eq_tlv),

	SOC_ENUM("Equaliser EQ4 Bandwith", eq4bw),
	SOC_ENUM("EQ4 Cut Off", eq4),
	SOC_SINGLE_TLV("EQ4 Volume", NAU8822_EQ4,  0, 24, 1, eq_tlv),

	SOC_ENUM("EQ5 Cut Off", eq5),
	SOC_SINGLE_TLV("EQ5 Volume", NAU8822_EQ5, 0, 24, 1, eq_tlv),

	SOC_SINGLE("DAC Playback Limiter Switch",
		NAU8822_DAC_LIMITER_1, 8, 1, 0),
	SOC_SINGLE("DAC Playback Limiter Decay",
		NAU8822_DAC_LIMITER_1, 4, 15, 0),
	SOC_SINGLE("DAC Playback Limiter Attack",
		NAU8822_DAC_LIMITER_1, 0, 15, 0),

	SOC_SINGLE("DAC Playback Limiter Threshold",
		NAU8822_DAC_LIMITER_2, 4, 7, 0),
	SOC_SINGLE_TLV("DAC Playback Limiter Volume",
		NAU8822_DAC_LIMITER_2, 0, 12, 0, limiter_tlv),
	SOC_DOUBLE_R("Speaker Switch",
		NAU8822_LOUT2_SPK_CONTROL, NAU8822_ROUT2_SPK_CONTROL, 6, 1, 1),
	SOC_DOUBLE_R_TLV("Speaker Playback Volume",
		NAU8822_LOUT2_SPK_CONTROL, NAU8822_ROUT2_SPK_CONTROL,
		0, 63, 0, spk_tlv),

};

/* Mixer #1: Output (OUT1, OUT2) Mixer: mix AUX, Input mixer output and DAC */
static const struct snd_kcontrol_new nau8822_left_out_mixer[] = {
	SOC_DAPM_SINGLE("Line Bypass Switch", NAU8822_LEFT_MIXER_CONTROL, 1, 1, 0),
	SOC_DAPM_SINGLE("Aux Playback Switch", NAU8822_LEFT_MIXER_CONTROL, 5, 1, 0),
	SOC_DAPM_SINGLE("PCM Playback Switch", NAU8822_LEFT_MIXER_CONTROL, 0, 1, 0),
};

static const struct snd_kcontrol_new nau8822_right_out_mixer[] = {
	SOC_DAPM_SINGLE("Line Bypass Switch", NAU8822_RIGHT_MIXER_CONTROL, 1, 1, 0),
	SOC_DAPM_SINGLE("Aux Playback Switch", NAU8822_RIGHT_MIXER_CONTROL, 5, 1, 0),
	SOC_DAPM_SINGLE("PCM Playback Switch", NAU8822_RIGHT_MIXER_CONTROL, 0, 1, 0),
};

/* OUT3/OUT4 Mixer not implemented */

/* Mixer #2: Input PGA Mute */
static const struct snd_kcontrol_new nau8822_left_input_mixer[] = {
	SOC_DAPM_SINGLE("L2 Switch", NAU8822_INPUT_CONTROL, 2, 1, 0),
	SOC_DAPM_SINGLE("MicN Switch", NAU8822_INPUT_CONTROL, 1, 1, 0),
	SOC_DAPM_SINGLE("MicP Switch", NAU8822_INPUT_CONTROL, 0, 1, 0),
};
static const struct snd_kcontrol_new nau8822_right_input_mixer[] = {
	SOC_DAPM_SINGLE("R2 Switch", NAU8822_INPUT_CONTROL, 6, 1, 0),
	SOC_DAPM_SINGLE("MicN Switch", NAU8822_INPUT_CONTROL, 5, 1, 0),
	SOC_DAPM_SINGLE("MicP Switch", NAU8822_INPUT_CONTROL, 4, 1, 0),
};

static const struct snd_soc_dapm_widget nau8822_dapm_widgets[] = {
	SND_SOC_DAPM_DAC("Left DAC", "Left HiFi Playback",
			 NAU8822_POWER_MANAGEMENT_3, 0, 0),
	SND_SOC_DAPM_DAC("Right DAC", "Right HiFi Playback",
			 NAU8822_POWER_MANAGEMENT_3, 1, 0),
	SND_SOC_DAPM_ADC("Left ADC", "Left HiFi Capture",
			 NAU8822_POWER_MANAGEMENT_2, 0, 0),
	SND_SOC_DAPM_ADC("Right ADC", "Right HiFi Capture",
			 NAU8822_POWER_MANAGEMENT_2, 1, 0),

	/* Mixer #1: OUT1,2 */
	SOC_MIXER_ARRAY("Left Output Mixer", NAU8822_POWER_MANAGEMENT_3,
			2, 0, nau8822_left_out_mixer),
	SOC_MIXER_ARRAY("Right Output Mixer", NAU8822_POWER_MANAGEMENT_3,
			3, 0, nau8822_right_out_mixer),

	SOC_MIXER_ARRAY("Left Input Mixer", NAU8822_POWER_MANAGEMENT_2,
			2, 0, nau8822_left_input_mixer),
	SOC_MIXER_ARRAY("Right Input Mixer", NAU8822_POWER_MANAGEMENT_2,
			3, 0, nau8822_right_input_mixer),

	SND_SOC_DAPM_PGA("Left Boost Mixer", NAU8822_POWER_MANAGEMENT_2,
			 4, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Right Boost Mixer", NAU8822_POWER_MANAGEMENT_2,
			 5, 0, NULL, 0),

	SND_SOC_DAPM_PGA("Left Capture PGA", NAU8822_LEFT_INP_PGA_CONTROL,
			 6, 1, NULL, 0),
	SND_SOC_DAPM_PGA("Right Capture PGA", NAU8822_RIGHT_INP_PGA_CONTROL,
			 6, 1, NULL, 0),

	SND_SOC_DAPM_PGA("Left Headphone Out", NAU8822_POWER_MANAGEMENT_2,
			 7, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Right Headphone Out", NAU8822_POWER_MANAGEMENT_2,
			 8, 0, NULL, 0),

	SND_SOC_DAPM_PGA("Left Speaker Out", NAU8822_POWER_MANAGEMENT_3,
			 6, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Right Speaker Out", NAU8822_POWER_MANAGEMENT_3,
			 5, 0, NULL, 0),

	SND_SOC_DAPM_MIXER("OUT4 VMID", NAU8822_POWER_MANAGEMENT_3,
			   8, 0, NULL, 0),

	//SND_SOC_DAPM_MICBIAS("Mic Bias", NAU8822_POWER_MANAGEMENT_1, 4, 0),
	SND_SOC_DAPM_SUPPLY("Mic Bias", NAU8822_POWER_MANAGEMENT_1, 4, 0, NULL, 0),

	SND_SOC_DAPM_INPUT("LMICN"),
	SND_SOC_DAPM_INPUT("LMICP"),
	SND_SOC_DAPM_INPUT("RMICN"),
	SND_SOC_DAPM_INPUT("RMICP"),
	SND_SOC_DAPM_INPUT("LAUX"),
	SND_SOC_DAPM_INPUT("RAUX"),
	SND_SOC_DAPM_INPUT("L2"),
	SND_SOC_DAPM_INPUT("R2"),
	SND_SOC_DAPM_OUTPUT("LHP"),
	SND_SOC_DAPM_OUTPUT("RHP"),
	SND_SOC_DAPM_OUTPUT("LSPK"),
	SND_SOC_DAPM_OUTPUT("RSPK"),
};

static const struct snd_soc_dapm_route audio_map[] = {
	/* Output mixer */
	{"Right Output Mixer", "PCM Playback Switch", "Right DAC"},
	{"Right Output Mixer", "Aux Playback Switch", "RAUX"},
	{"Right Output Mixer", "Line Bypass Switch", "Right Boost Mixer"},

	{"Left Output Mixer", "PCM Playback Switch", "Left DAC"},
	{"Left Output Mixer", "Aux Playback Switch", "LAUX"},
	{"Left Output Mixer", "Line Bypass Switch", "Left Boost Mixer"},

	/* Outputs */
	{"Right Headphone Out", NULL, "Right Output Mixer"},
	{"RHP", NULL, "Right Headphone Out"},

	{"Left Headphone Out", NULL, "Left Output Mixer"},
	{"LHP", NULL, "Left Headphone Out"},

	{"Right Speaker Out", NULL, "Right Output Mixer"},
	{"RSPK", NULL, "Right Speaker Out"},

	{"Left Speaker Out", NULL, "Left Output Mixer"},
	{"LSPK", NULL, "Left Speaker Out"},

	/* Input Mixer */
	{"Right ADC", NULL, "Right Boost Mixer"},

	{"Right Boost Mixer", NULL, "RAUX"},
	{"Right Boost Mixer", NULL, "Right Capture PGA"},
	{"Right Boost Mixer", NULL, "R2"},
	{"Right Boost Mixer", NULL, "Mic Bias"},

	{"Left ADC", NULL, "Left Boost Mixer"},

	{"Left Boost Mixer", NULL, "LAUX"},
	{"Left Boost Mixer", NULL, "Left Capture PGA"},
	{"Left Boost Mixer", NULL, "L2"},
	{"Left Boost Mixer", NULL, "Mic Bias"},

	/* Input PGA */
	{"Right Capture PGA", NULL, "Right Input Mixer"},
	{"Left Capture PGA", NULL, "Left Input Mixer"},

	{"Right Input Mixer", "R2 Switch", "R2"},
	{"Right Input Mixer", "MicN Switch", "RMICN"},
	{"Right Input Mixer", "MicP Switch", "RMICP"},

	{"Left Input Mixer", "L2 Switch", "L2"},
	{"Left Input Mixer", "MicN Switch", "LMICN"},
	{"Left Input Mixer", "MicP Switch", "LMICP"},
};



//PLL divisors, MCLK is 12.288M Fixed, oversample is 256fs Fixed,
//The pll must be used for almost all sample rates except 48KHz
struct nau8822_pll_div {
   u32 k;//the integer portion
   u8 n;//the fractional portion
   u8 div2;//the prescaler for the input clock
};

#define FIXED_PLL_SIZE (1 << 24)

static void pll_factors(struct snd_soc_codec *codec,
		struct nau8822_pll_div *pll_div, unsigned int target, unsigned int source)
{
	u64 k_part;
	unsigned int k, n_div, n_mod;
	DBG("\t[NAU8822]  [%-10s] : Enter %d\n", __FUNCTION__,__LINE__);

	n_div = target / source;
	if (n_div < 6) {
		source >>= 1;
		pll_div->div2 = 1;
		n_div = target / source;
	} else {
		pll_div->div2 = 0;
	}

	if (n_div < 6 || n_div > 12)
		DBG( "\t[NAU8822]   N value exceeds recommended range! N = %u\n",
			 n_div);

	pll_div->n = n_div;
	n_mod = target - source * n_div;
	k_part = FIXED_PLL_SIZE * (long long)n_mod + source / 2;

	do_div(k_part, source);

	k = k_part & 0xFFFFFFFF;

	pll_div->k = k;
}

/* MCLK dividers */
static const int mclk_numerator[]	= {1, 3, 2, 3, 4, 6, 8, 12};
static const int mclk_denominator[]	= {1, 2, 1, 1, 1, 1, 1, 1};

/*
 * find index >= idx, such that, for a given f_out,
 * 3 * f_mclk / 4 <= f_PLLOUT < 13 * f_mclk / 4
 * f_out can be f_256fs or f_opclk, currently only used for f_256fs. Can be
 * generalised for f_opclk with suitable coefficient arrays, but currently
 * the OPCLK divisor is calculated directly, not iteratively.
 */
static int nau8822_enum_mclk(unsigned int f_out, unsigned int f_mclk,
			    unsigned int *f_pllout)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mclk_numerator); i++) {
		unsigned int f_pllout_x4 = 4 * f_out * mclk_numerator[i] /
			mclk_denominator[i];
		if (3 * f_mclk <= f_pllout_x4 && f_pllout_x4 < 13 * f_mclk) {
			*f_pllout = f_pllout_x4 / 4;
			return i;
		}
	}

	return -EINVAL;
}

/*
 * Calculate internal frequencies and dividers, according to Figure 40
 * "PLL and Clock Select Circuit" in NAU8822 datasheet Rev. 2.6
 */
static int nau8822_configure_pll(struct snd_soc_codec *codec)
{
	struct nau8822_priv *nau8822 = snd_soc_codec_get_drvdata(codec);
	struct nau8822_pll_div pll_div;
	unsigned int f_mclk = nau8822->f_mclk;
	unsigned int f_256fs = nau8822->f_256fs;
	unsigned int f2;
	int idx;

	DBG("\t[NAU8822]  [%-10s] : Enter %d\n", __FUNCTION__,__LINE__);

	if (!f_mclk)
		return -EINVAL;

	if (f_256fs) {
		/*
		 * Not using OPCLK, but PLL is used for the codec, choose R:
		 * 6 <= R = f2 / f1 < 13, to put 1 <= MCLKDIV <= 12.
		 * f_256fs = f_mclk * prescale * R / 4 / MCLKDIV, where
		 * prescale = 1, or prescale = 2. Prescale is calculated inside
		 * pll_factors(). We have to select f_PLLOUT, such that
		 * f_mclk * 3 / 4 <= f_PLLOUT < f_mclk * 13 / 4. Must be
		 * f_mclk * 3 / 48 <= f_256fs < f_mclk * 13 / 4. This means MCLK
		 * must be 3.781MHz <= f_MCLK <= 32.768MHz
		 */
		idx = nau8822_enum_mclk(f_256fs, f_mclk, &nau8822->f_pllout);
		if (idx < 0)
			return idx;

		/* GPIO1 into default mode as input - before configuring PLL */
		snd_soc_update_bits(codec, NAU8822_GPIO_CONTROL, 7, 0);
	} else {
		return -EINVAL;
	}

	f2 = nau8822->f_pllout * 4;

	DBG( "\t[NAU8822] %s: f_MCLK=%uHz, f_PLLOUT=%uHz  idx is %d\n", __func__,
		nau8822->f_mclk, nau8822->f_pllout,idx);

	pll_factors(codec, &pll_div, f2, nau8822->f_mclk);

	DBG("\t[NAU8822] %s: calculated PLL N=0x%x, K=0x%x, div2=%d\n",
		__func__, pll_div.n, pll_div.k, pll_div.div2+1);

	/* Turn PLL off for configuration... */
	snd_soc_update_bits(codec, NAU8822_POWER_MANAGEMENT_1, 0x20, 0);

	snd_soc_write(codec, NAU8822_PLL_N, (pll_div.div2 << 4) | pll_div.n);
	snd_soc_write(codec, NAU8822_PLL_K1, pll_div.k >> 18);
	snd_soc_write(codec, NAU8822_PLL_K2, (pll_div.k >> 9) & 0x1ff);
	snd_soc_write(codec, NAU8822_PLL_K3, pll_div.k & 0x1ff);

	snd_soc_update_bits(codec, NAU8822_CLOCKING, 0xe0, idx<<5);//set IMCLK
	snd_soc_update_bits(codec, NAU8822_CLOCKING, 0x0c, 0x0c);//Set bclk
	snd_soc_update_bits(codec, NAU8822_CLOCKING, 0x01, nau8822->sysmod);//Set slave/master mode
	snd_soc_update_bits(codec, NAU8822_CLOCKING, 0x100, 0x100);

	/* ...and on again */
	snd_soc_update_bits(codec, NAU8822_POWER_MANAGEMENT_1, 0x20, 0x20);


	return 0;
}

/*
 * Set ADC and Voice DAC format.
 */
static int nau8822_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	/*
	 * BCLK polarity mask = 0x100, LRC clock polarity mask = 0x80,
	 * Data Format mask = 0x18: all will be calculated anew
	 */
	u16 iface = snd_soc_read(codec, NAU8822_AUDIO_INTERFACE) & ~0x198;
	u16 clk = snd_soc_read(codec, NAU8822_CLOCKING);

	DBG("\t[NAU8822]  %s\n", __func__);

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		clk |= 1;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		clk &= ~1;
		break;
	default:
		return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		iface |= 0x10;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface |= 0x8;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		iface |= 0x18;
		break;
	default:
		return -EINVAL;
	}

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_IF:
		iface |= 0x180;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		iface |= 0x100;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		iface |= 0x80;
		break;
	default:
		return -EINVAL;
	}

	snd_soc_write(codec, NAU8822_AUDIO_INTERFACE, iface);
	snd_soc_write(codec, NAU8822_CLOCKING, clk);

	return 0;
}



static int nau8822_set_dai_sysclk(struct snd_soc_dai *codec_dai, int clk_id,
				 unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct nau8822_priv *nau8822 = snd_soc_codec_get_drvdata(codec);

	DBG("\t[NAU8822]  %s: ID %d, freq %u\n", __func__, clk_id, freq);

	nau8822->f_mclk=freq;
	switch(clk_id){
	case NAU8822_MCLK_IN:
		nau8822->sysmod=SLAVE;
		nau8822->extra_clk = 0;
		break;
	case NAU8822_BCLK_IN:
		nau8822->sysmod=SLAVE;
		nau8822->extra_clk = 1;
		dev_dbg(codec->dev, "nau8822: Can only support  this mode with a outside 12.288MHz CLK \n");
		break;
	case NAU8822_MCLK_IN_BCLK_OUT:
		nau8822->sysmod=MASTER;
		nau8822->extra_clk = 0;
		break;
	default:
		dev_err(codec->dev, "nau8822: Input mode error: %d\n", clk_id);
		return -EINVAL;
	}
	return 0;
}
/*
 * Set PCM DAI bit size and sample rate.
 */
static int nau8822_pcm_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct nau8822_priv *nau8822 = snd_soc_codec_get_drvdata(codec);
	int rate, ret=0;

	//u16 clking = snd_soc_read(codec, NAU8822_CLOCKING);
	u16 iface_ctl = snd_soc_read(codec, NAU8822_AUDIO_INTERFACE) & 0x19f;
	u16 add_ctl = snd_soc_read(codec, NAU8822_ADDITIONAL_CONTROL) & 0x1f1;

	if (!nau8822->f_mclk)
		return -EINVAL;

	/* bit size */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		iface_ctl |= 0x20;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		iface_ctl |= 0x40;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		iface_ctl |= 0x60;
		break;
	}

	/* filter coefficient */
	rate = params_rate(params);
	nau8822->f_256fs=rate*256;
	switch (rate) {
	case 8000:
		add_ctl |= 0x5 << 1;
		break;
	case 11025:
		add_ctl |= 0x4 << 1;
		break;
	case 16000:
		add_ctl |= 0x3 << 1;
		break;
	case 22050:
		add_ctl |= 0x2 << 1;
		break;
	case 32000:
		add_ctl |= 0x1 << 1;
		break;
	case 44100:
	case 48000:
		break;
	case 96000:
	case 192000:
		snd_soc_update_bits(codec, NAU8822_192KHZ_SAMPLING, 0x24, 0x24);//set 96K
		break;
	}

	/* Sampling rate is known now, can configure the MCLK divider */
	DBG("\t[NAU8822] sample rate is %d \n",rate);

	snd_soc_write(codec, NAU8822_ADDITIONAL_CONTROL, add_ctl);

	//
	if (rate== 48000&& nau8822->extra_clk == 0){
		/* Clock CODEC directly from MCLK */
		DBG("%s(%d)\n",__FILE__,__LINE__);
		snd_soc_update_bits(codec, NAU8822_CLOCKING, 0x100, 0);

		/* GPIO1 into default mode as input - before configuring PLL */
		snd_soc_update_bits(codec, NAU8822_GPIO_CONTROL, 7, 0);

		/* Turn off PLL */
		DBG("%s(%d) 0x1 %03X\n",__FILE__,__LINE__,0x00);
		snd_soc_update_bits(codec, NAU8822_POWER_MANAGEMENT_1, 0x20, 0);
		nau8822->sysclk = NAU8822_MCLK;
		snd_soc_update_bits(codec, NAU8822_CLOCKING, 0xe0, 0);//set IMCLK
		snd_soc_update_bits(codec, NAU8822_CLOCKING, 0x0c, 0x0c);//Set bclk
		snd_soc_update_bits(codec, NAU8822_CLOCKING, 0x01, nau8822->sysmod);//Set slave/master mode
		DBG("[%-10s] : Leave  %d [Mode=%d rate=%d  256fs=%d] \n", __FUNCTION__,__LINE__,nau8822->sysclk,rate,nau8822->f_256fs);
		return 0;
	}else if (nau8822->sysmod == SLAVE) {
		nau8822->sysclk = NAU8822_PLL;
		snd_soc_update_bits(codec, NAU8822_CLOCKING, 0x100, 0);
		ret = nau8822_configure_pll(codec);
	}else if (nau8822->sysmod == MASTER) {
		nau8822->sysclk = NAU8822_PLL;
		snd_soc_update_bits(codec, NAU8822_CLOCKING, 0x100, 0x100);
		ret = nau8822_configure_pll(codec);
	}else {
		dev_err(codec->dev, "nau8822: Can not support this mode 1\n");
	}
	if(ret <0){
			dev_err(codec->dev, "%s: Config pll error : %d\n", __func__, ret);
			return  ret;
	}

	return 0;
}

static int nau8822_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;

	DBG("\t[NAU8822]  %s: %d\n", __func__, mute);

	if (mute)
		snd_soc_update_bits(codec, NAU8822_DAC_CONTROL, 0x40, 0x40);
	else
		snd_soc_update_bits(codec, NAU8822_DAC_CONTROL, 0x40, 0);

	return 0;
}

static int nau8822_set_bias_level(struct snd_soc_codec *codec,
				 enum snd_soc_bias_level level)
{
	u16 power1 = snd_soc_read(codec, NAU8822_POWER_MANAGEMENT_1) & ~3;

	switch (level) {
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
		power1 |= 1;  /* VMID 75k */
		snd_soc_write(codec, NAU8822_POWER_MANAGEMENT_1, power1);
		break;
	case SND_SOC_BIAS_STANDBY:
		/* bit 3: enable bias, bit 2: enable I/O tie off buffer */
		power1 |= 0xc;

		if (snd_soc_codec_get_bias_level(codec) == SND_SOC_BIAS_OFF) {
			/* Initial cap charge at VMID 5k */
			snd_soc_write(codec, NAU8822_POWER_MANAGEMENT_1,
				      power1 | 0x3);
			mdelay(100);
		}

		power1 |= 0x2;  /* VMID 500k */
		snd_soc_write(codec, NAU8822_POWER_MANAGEMENT_1, power1);
		break;
	case SND_SOC_BIAS_OFF:
		/* Preserve PLL - OPCLK may be used by someone */
		snd_soc_update_bits(codec, NAU8822_POWER_MANAGEMENT_1, ~0x20, 0);
		snd_soc_write(codec, NAU8822_POWER_MANAGEMENT_2, 0);
		snd_soc_write(codec, NAU8822_POWER_MANAGEMENT_3, 0);
		break;
	}

	DBG("\t[NAU8822]  %s: %d, %x\n", __func__, level, power1);

	return 0;
}

#define NAU8822_RATES (SNDRV_PCM_RATE_8000_192000)

#define NAU8822_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
	SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_ops nau8822_dai_ops = {
	.hw_params	= nau8822_pcm_hw_params,
	.digital_mute	= nau8822_mute,
	.set_fmt	= nau8822_set_dai_fmt,
	.set_sysclk	= nau8822_set_dai_sysclk,
};

/* Also supports 12kHz */
 struct snd_soc_dai_driver nau8822_dai[] = {
 	{
	.name = "nau8822-hifi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = NAU8822_RATES,
		.formats = NAU8822_FORMATS,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = NAU8822_RATES,
		.formats = NAU8822_FORMATS,
	},
	.ops = &nau8822_dai_ops,
	},
};

static int nau8822_suspend(struct snd_soc_codec *codec)
{
	nau8822_set_bias_level(codec, SND_SOC_BIAS_OFF);
	/* Also switch PLL off */
	snd_soc_write(codec, NAU8822_POWER_MANAGEMENT_1, 0);

	return 0;
}

static int nau8822_resume(struct snd_soc_codec *codec)
{
	nau8822_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	return 0;
}


static int nau8822_probe(struct snd_soc_codec *codec)
{
	struct nau8822_priv *nau8822 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	nau8822->extra_clk = 0;

	/* Reset the codec */
	ret = snd_soc_write(codec, NAU8822_RESET, 0);
	if ( ret < 0) {
		dev_err(codec->dev, "Failed to issue reset\n");
		return ret;
	}
	nau8822_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	/*Basic power enable*/
	snd_soc_write(codec,NAU8822_POWER_MANAGEMENT_1,0x11a);//micbias/VMID enable


	/*Set ADC and DAC oversample*/
	snd_soc_write(codec, NAU8822_DAC_CONTROL, 0x0c);//set DAC 128x (best SNR)
	snd_soc_write(codec, NAU8822_ADC_CONTROL, 0x108);//set ADC 128x (best SNR)

	/*Enable Left Mic input and Gain*/
	snd_soc_write(codec, NAU8822_INPUT_CONTROL, 0x181);	//Set bias Voltage level 0.5 * Vdda
	snd_soc_write(codec, NAU8822_LEFT_INP_PGA_CONTROL,0x130);  //PGA GAIN 10.5 dB
	snd_soc_write(codec, NAU8822_RIGHT_INP_PGA_CONTROL,0x130); //PGA GAIN 10.5 dB


	/*Enable Stereo SPK*/
	snd_soc_write(codec, NAU8822_POWER_MANAGEMENT_3, 0x00);//Mute output
	snd_soc_write(codec, NAU8822_LOUT2_SPK_CONTROL, 0x139); //Speaker driver gain 0 dB
	snd_soc_write(codec, NAU8822_LEFT_MIXER_CONTROL,0x15);//Enable Stereo SPK
	snd_soc_write(codec, NAU8822_RIGHT_MIXER_CONTROL,0x15);//Enable Stereo SPK
	snd_soc_write(codec, NAU8822_OUTPUT_CONTROL, 0x7); //speakerx1.5
	snd_soc_write(codec, NAU8822_BEEP_CONTROL, 0x18);

	/*Companding Mode*/
	snd_soc_write(codec, NAU8822_COMPANDING_CONTROL, 0x0);


	//snd_soc_write(codec, 0x4e, 0x008);

	return 0;
}

/* power down chip */
static int nau8822_remove(struct snd_soc_codec *codec)
{
	DBG("\t[NAU8822] %s(%d)\n",__FUNCTION__,__LINE__);
	nau8822_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

 struct snd_soc_codec_driver soc_codec_dev_nau8822 = {
	.probe =	nau8822_probe,
	.remove =	nau8822_remove,
	.suspend =	nau8822_suspend,
	.resume =	nau8822_resume,
	.set_bias_level = nau8822_set_bias_level,

	.component_driver = {
		.controls = nau8822_snd_controls,
		.num_controls = ARRAY_SIZE(nau8822_snd_controls),
		.dapm_widgets = nau8822_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(nau8822_dapm_widgets),
		.dapm_routes = audio_map,
		.num_dapm_routes = ARRAY_SIZE(audio_map),
	}

};
EXPORT_SYMBOL_GPL(soc_codec_dev_nau8822);

static struct regmap_config nau8822_regmap = {
	.reg_bits = 7,
	.val_bits = 9,
	.max_register = NAU8822_OPTPUT_TIEOFF_CTRL,
	.reg_defaults = nau8822_reg,
	.num_reg_defaults = ARRAY_SIZE(nau8822_reg),
	.cache_type = REGCACHE_RBTREE,
	.volatile_reg = nau8822_volatile,
	//.readable_reg = nau8822_readable,

};

static  int nau8822_i2c_probe(struct i2c_client *i2c,
				      const struct i2c_device_id *id)
{
	struct nau8822_priv *nau8822;
	struct snd_soc_codec *codec;
	int ret;

	DBG("\t[NAU8822] %s(%d)\n",__FUNCTION__,__LINE__);
	nau8822 =  devm_kzalloc(&i2c->dev, sizeof(struct nau8822_priv), GFP_KERNEL);
	if (nau8822 == NULL)
		return -ENOMEM;

	codec = &nau8822->codec;

	i2c_set_clientdata(i2c, nau8822);
	nau8822->regmap = devm_regmap_init_i2c(i2c, &nau8822_regmap);

	if (IS_ERR(nau8822->regmap)){
		ret = PTR_ERR(nau8822->regmap);
		dev_err(&i2c->dev, "regmap_init() failed: %d\n", ret);
		return ret;
	}

	ret =  snd_soc_register_codec(&i2c->dev,
			&soc_codec_dev_nau8822, &nau8822_dai[0], ARRAY_SIZE(nau8822_dai));
	if (ret < 0){
		kfree(nau8822);
		DBG("\t[NAU8822 Error!] %s(%d)\n",__FUNCTION__,__LINE__);
	}

	return ret;
}

static  int nau8822_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	kfree(i2c_get_clientdata(client));
	return 0;
}

static const struct of_device_id nau8822_of_match[] = {
	{ .compatible = "wlf,nau8822" },
	{ },
};
MODULE_DEVICE_TABLE(of, nau8822_of_match);

static const struct i2c_device_id nau8822_i2c_id[] = {
	{ "nau8822", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, nau8822_i2c_id);

static struct i2c_driver nau8822_i2c_driver = {
	.driver = {
		.name = "nau8822",
		.owner = THIS_MODULE,
		.of_match_table = nau8822_of_match,
	},
	.probe =    nau8822_i2c_probe,
	.remove =   nau8822_i2c_remove,
	.id_table = nau8822_i2c_id,
};

static int __init nau8822_modinit(void)
{
	int ret = 0;
	DBG("[%-10s] : Enter\n", __FUNCTION__);
	ret = i2c_add_driver(&nau8822_i2c_driver);

	if (ret != 0) {
		pr_err("Failed to register NAU8822 I2C driver: %d\n", ret);
	}
	return ret;
}
module_init(nau8822_modinit);

static void __exit nau8822_exit(void)
{
	i2c_del_driver(&nau8822_i2c_driver);
}
module_exit(nau8822_exit);

MODULE_DESCRIPTION("ASoC NAU8822 codec driver");
MODULE_AUTHOR("Steven <hhan@ambarella.com>");
MODULE_LICENSE("GPL");
