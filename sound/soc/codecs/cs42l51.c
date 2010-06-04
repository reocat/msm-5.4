/*
 * cs42l51.c  --  CS42L51 ALSA Soc Audio driver
 *
 * Author: Cao Rongrong <rrcao@ambarella.com>
 *
 * History:
 *	2009/07/01 - [Cao Rongrong] Created file
 * 
 * Copyright (C) 2004-2009, Ambarella, Inc.
 *
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
#include <linux/moduleparam.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "cs42l51.h"

/* codec private data */
struct cs42l51_priv {
	unsigned int mode;
	unsigned int mclk;
};


/*
 * We read all of the CS42L51 registers to pre-fill the register cache
 */
static int cs42l51_fill_cache(struct snd_soc_codec *codec)
{
	struct i2c_client *client = codec->control_data;
	u8 *cache = codec->reg_cache;
	s32 val, i;

	for(i = 1; i < CS42L51_NUMREGS; i++){
		val = i2c_smbus_read_byte_data(client, i);
		if(val < 0){
			pr_err("CS42L51: I2C read error: address = 0x%x\n", i);
			return -EIO;
		}

		cache[i] = val;
	}

	return 0;
}

/*
 * Read from the CS42L51 register cache.
 *
 * This CS42L51 registers are cached to avoid excessive I2C I/O operations.
 * After the initial read to pre-fill the cache, the CS42L51 never updates
 * the register values, so we won't have a cache coherncy problem.
 */
static inline unsigned int cs42l51_read_reg_cache(struct snd_soc_codec *codec,
	unsigned int reg)
{
	u8 *cache = codec->reg_cache;

	if (reg < 1 || reg >= CS42L51_NUMREGS)
		return -EIO;

	return cache[reg];
}

/*
 * Write to a CS42L51 register via the I2C bus.
 *
 * This function writes the given value to the given CS42L51 register, and
 * also updates the register cache.
 *
 * Note that we don't use the hw_write function pointer of snd_soc_codec.
 * That's because it's too clunky: the hw_write_t prototype does not match
 * i2c_smbus_write_byte_data(), and it's just another layer of overhead.
 */
static int cs42l51_write(struct snd_soc_codec *codec,
		unsigned int reg, unsigned int value)
{
	u8 *cache = codec->reg_cache;

	if (reg < 1 || reg >= CS42L51_NUMREGS)
		return -EIO;

	/* Only perform an I2C operation if the new value is different */
	if (cache[reg] != value) {
		struct i2c_client *client = codec->control_data;
		if (i2c_smbus_write_byte_data(client, reg, value)) {
			pr_err("CS42L51: I2C write failed\n");
			return -EIO;
		}
		/* We've written to the hardware, so update the cache */
		cache[reg] = value;
	}

	return 0;
}

static int cs42l51_setbits(struct snd_soc_codec *codec,
		unsigned int reg, unsigned int mask)
{
	u8 value;

	value = snd_soc_read(codec, reg);
	value |= mask;

	return snd_soc_write(codec, reg, value);
}

static int cs42l51_clrbits(struct snd_soc_codec *codec,
		unsigned int reg, unsigned int mask)
{
	u8 value;

	value = snd_soc_read(codec, reg);
	value &= (~mask);

	return snd_soc_write(codec, reg, value);
}

static void cs42l51_dump_reg(struct snd_soc_codec *codec)
{
	int i, val;

	for(i = 1; i < CS42L51_NUMREGS; i++) {
		val = cs42l51_read_reg_cache(codec, i);
		pr_debug("0x%02x:0x%02x    ", i, val);
		if(i % 8 == 0)
			printk("\n");
	}

}


static const char *cs42l51_sel_data[] = {
	"PCM",
	"Signal Processing Engine",
	"ADC Serial Port",
};

static const char *cs42l51_mic_bias[] = {
	"0.8VA",
	"0.7VA",
	"0.6VA",
	"0.5VA",
};

static const char *cs42l51_ng_delay[] = {
	"50ms",
	"100ms",
	"150ms",
	"200ms",
};

static const char *cs42l51_deemph[] = {
	"None",
	"44.1Khz"};

static const char *cs42l51_DAC_szc[] = {
	"Immediate Change",
	"Zero Cross",
	"Soft Ramp",
	"Soft Ramp On ZC",
};

static const char *cs42l51_bass_freq[] = {
	"50Hz",
	"100Hz",
	"200Hz",
	"250Hz",
};

static const char *cs42l51_treble_freq[] = {
	"5KHz",
	"7KHz",
	"10KHz",
	"15KHz",
};

static const char *cs42l51_input_select[] = {
	"Line 1",
	"Line 2",
	"Line 3",
	"Mic",
};

static const struct soc_enum cs42l51_DAC_data_sel_enum =
	SOC_ENUM_SINGLE(CS42L51_DAC_CTL_REG, 6, 3, cs42l51_sel_data);
static const struct soc_enum cs42l51_mic_bias_enum =
	SOC_ENUM_SINGLE(CS42L51_MICCTL_REG, 2, 4, cs42l51_mic_bias);
static const struct soc_enum cs42l51_ng_delay_enum =
	SOC_ENUM_SINGLE(CS42L51_NOISEGATE_MISC_REG, 0, 4, cs42l51_ng_delay);
static const struct soc_enum cs42l51_deemph_enum =
	SOC_ENUM_SINGLE(CS42L51_DAC_CTL_REG, 3, 2, cs42l51_deemph);
static const struct soc_enum cs42l51_DAC_szc_enum =
	SOC_ENUM_SINGLE(CS42L51_DAC_CTL_REG, 0, 4, cs42l51_DAC_szc);
static const struct soc_enum cs42l51_bass_freq_enum =
	SOC_ENUM_SINGLE(CS42L51_BP_TONE_CONF_REG, 1, 4, cs42l51_bass_freq);
static const struct soc_enum cs42l51_treble_freq_enum =
	SOC_ENUM_SINGLE(CS42L51_BP_TONE_CONF_REG, 3, 4, cs42l51_treble_freq);


static const struct snd_kcontrol_new cs42l51_snd_controls1[] = {
	SOC_ENUM("DAC Data Selection", cs42l51_DAC_data_sel_enum),
	SOC_SINGLE("Headphone Playback Gain", CS42L51_DAC_OUTCTL_REG, 5, 7, 0),

	SOC_ENUM("Mic Bias Level", cs42l51_mic_bias_enum),
	SOC_SINGLE("Mic Bias Select", CS42L51_MICCTL_REG, 4, 1, 0),

	SOC_SINGLE("Capture DigitalMix Switch", CS42L51_INTFCTL_REG, 1, 1, 0),
	SOC_SINGLE("Capture MicrophoneMix Switch", CS42L51_INTFCTL_REG, 0, 1, 0),

	SOC_SINGLE("Capture SignalVolume Switch", CS42L51_MICCTL_REG, 7, 1, 0),
	SOC_DOUBLE_R("Capture Volume",
		CS42L51_ALCA_PGAA_CTL_REG, CS42L51_ALCB_PGAB_CTL_REG, 0, 31, 0),
	SOC_DOUBLE("Capture Boost Switch", CS42L51_MICCTL_REG, 5, 6, 1, 0),
	SOC_DOUBLE("Capture Invert Switch", CS42L51_ADCSEL_INVMUTE_REG, 2, 3, 1, 0),
	SOC_DOUBLE("Capture ZC Switch", CS42L51_ADCCTL_REG, 0, 2, 1, 0),
	SOC_DOUBLE("Capture Soft Ramp Switch", CS42L51_ADCCTL_REG, 1, 3, 1, 0),

	/* Capture Noise Gate */
	SOC_SINGLE("Capture NG Switch", CS42L51_NOISEGATE_MISC_REG, 6, 1, 0),
	SOC_SINGLE("Capture NG Gang Switch", CS42L51_NOISEGATE_MISC_REG, 7, 1, 0),
	SOC_SINGLE("Capture NG Boost (+30db) Switch", CS42L51_NOISEGATE_MISC_REG, 5, 1, 0),
	SOC_SINGLE("Capture NG Threshold", CS42L51_NOISEGATE_MISC_REG, 2, 7, 0),
	SOC_ENUM("Capture NG Delay Time", cs42l51_ng_delay_enum),

	/* Capture ALC */
	SOC_DOUBLE("Capture ALC Switch", CS42L51_ALCEN_ATKRATE_REG, 6, 7, 1, 0),
	SOC_SINGLE("Capture ALC Attack Rate", CS42L51_ALCEN_ATKRATE_REG, 0, 63, 0),
	SOC_SINGLE("Capture ALC Release Rate", CS42L51_ALC_RELRATE_REG, 0, 63, 0),
	SOC_SINGLE("Capture ALC Maximum Threshold", CS42L51_ALC_THD_REG, 5, 7, 0),
	SOC_SINGLE("Capture ALC Minimum Threshold", CS42L51_ALC_THD_REG, 2, 7, 0),
	SOC_DOUBLE_R("Capture ALC Attenuator",
		CS42L51_ADCA_ATT_REG, CS42L51_ADCB_ATT_REG, 0, 255, 0),
	SOC_DOUBLE_R("Capture ALC Soft Ramp Switch",
		CS42L51_ALCA_PGAA_CTL_REG, CS42L51_ALCB_PGAB_CTL_REG, 7, 1, 1),
	SOC_DOUBLE_R("Capture ALC ZC Switch",
		CS42L51_ALCA_PGAA_CTL_REG, CS42L51_ALCB_PGAB_CTL_REG, 6, 1, 1),

	/* Capture High Pass Filter */
	SOC_DOUBLE("Capture HPF Switch", CS42L51_ADCCTL_REG, 5, 7, 1, 0),
	SOC_DOUBLE("Capture HPF-Freeze Switch", CS42L51_ADCCTL_REG, 4, 6, 1, 0),
};

/*
 * Note: If you want to use the following controls,
 * you must select DAC data from "Signal Processing Engine" first.
 */
static const struct snd_kcontrol_new cs42l51_snd_controls2[] = {
	/* De-Emphasis for 44.1KHz */
	SOC_ENUM("De-Emphasis (44.1KHz) Switch (SPE)", cs42l51_deemph_enum),
	/* Soft Ramp and Zero Cross */
	SOC_ENUM("DAC Soft Ramp and Zero Cross (SPE)", cs42l51_DAC_szc_enum),
	/* Output Volume */
	SOC_DOUBLE_R("PCM Mixer Volume (SPE)",
		CS42L51_PCMA_MIXVOL_REG, CS42L51_PCMB_MIXVOL_REG, 0, 127, 0),
	SOC_DOUBLE_R("PCM Mixer Mute (SPE)",
		CS42L51_PCMA_MIXVOL_REG, CS42L51_PCMB_MIXVOL_REG, 7, 1, 0),
	SOC_DOUBLE_R("ADC Mixer Volume (SPE)",
		CS42L51_ADCA_MIXVOL_REG, CS42L51_ADCB_MIXVOL_REG, 0, 127, 0),
	SOC_DOUBLE_R("ADC Mixer Mute (SPE)",
		CS42L51_ADCA_MIXVOL_REG, CS42L51_ADCB_MIXVOL_REG, 7, 1, 0),
	SOC_DOUBLE_R("AOUT Volume (SPE)",
		CS42L51_AOUTA_VOLCTL_REG, CS42L51_AOUTB_VOLCTL_REG, 0, 255, 0),
	/* Channel Mixer and Swap */
	SOC_DOUBLE("PCM Channel Mixer (SPE)",
		CS42L51_ADCPCM_MIX_REG, 6, 4, 3, 0),
	SOC_DOUBLE("ADC Channel Mixer (SPE)",
		CS42L51_ADCPCM_MIX_REG, 2, 0, 3, 0),
	/* Bass and Treble */
	SOC_ENUM("Bass Corner Frequency (SPE)", cs42l51_bass_freq_enum),
	SOC_ENUM("Treble Corner Frequency (SPE)", cs42l51_treble_freq_enum),
	SOC_SINGLE("Bass Gain Level (SPE)", CS42L51_TONE_CTL_REG, 0, 15, 1),
	SOC_SINGLE("Treble Gain Level (SPE)", CS42L51_TONE_CTL_REG, 4, 15, 1),
	/* Limiter */
	SOC_SINGLE("Limiter and Peak Detect Switch (SPE)",
		CS42L51_LIMIT_RELRATE_REG, 7, 1, 0),
	SOC_SINGLE("Limiter Soft Ramp Switch (SPE)",
		CS42L51_LIMIT_RELRATE_REG, 1, 1, 0),
	SOC_SINGLE("Limiter Zero Cross Switch (SPE)",
		CS42L51_LIMIT_RELRATE_REG, 0, 1, 0),
	SOC_SINGLE("Limiter Release Rate (SPE)",
		CS42L51_LIMIT_RELRATE_REG, 0, 63, 0),
	SOC_SINGLE("Limiter Attack Rate (SPE)",
		CS42L51_LIMIT_ATKRATE_REG, 0, 63, 0),
	SOC_SINGLE("Limiter Maximum Threshold (SPE)",
		CS42L51_LIMIT_THD_SZC_REG, 5, 7, 0),
	SOC_SINGLE("Limiter Minimum Threshold (SPE)",
		CS42L51_LIMIT_THD_SZC_REG, 2, 7, 0),
};



/* add non dapm controls */
static int cs42l51_add_controls(struct snd_soc_codec *codec)
{
	struct snd_kcontrol *snd_kctl;
	int err, i;

	for (i = 0; i < ARRAY_SIZE(cs42l51_snd_controls1); i++) {
		snd_kctl = snd_soc_cnew(&cs42l51_snd_controls1[i], codec, NULL);
		err = snd_ctl_add(codec->card, snd_kctl);
		if (err < 0)
			return err;
	}

	for (i = 0; i < ARRAY_SIZE(cs42l51_snd_controls2); i++) {
		snd_kctl = snd_soc_cnew(&cs42l51_snd_controls2[i], codec, NULL);
		err = snd_ctl_add(codec->card, snd_kctl);
		if (err < 0)
			return err;
	}

	return 0;
}


static const struct soc_enum cs42l51_leftin_mux_enum =
	SOC_ENUM_SINGLE(CS42L51_ADCSEL_INVMUTE_REG, 4, 4, cs42l51_input_select);

static const struct soc_enum cs42l51_rightin_mux_enum =
	SOC_ENUM_SINGLE(CS42L51_ADCSEL_INVMUTE_REG, 6, 4, cs42l51_input_select);

static const struct snd_kcontrol_new cs42l51_leftin_mux_controls =
	SOC_DAPM_ENUM("Input Select", cs42l51_leftin_mux_enum);

static const struct snd_kcontrol_new cs42l51_rightin_mux_controls =
	SOC_DAPM_ENUM("Input Select", cs42l51_rightin_mux_enum);

static const struct snd_soc_dapm_widget cs42l51_dapm_widgets[] = {
	/* Left DAC to Left Outputs */
	SND_SOC_DAPM_DAC("Left DAC", "Left Playback", CS42L51_PWRCTL1_REG, 5, 1),
	SND_SOC_DAPM_OUTPUT("LHPOUT"),

	/* Right DAC to Right Outputs */
	SND_SOC_DAPM_DAC("Right DAC", "Right Playback", CS42L51_PWRCTL1_REG, 6, 1),
	SND_SOC_DAPM_OUTPUT("RHPOUT"),

	/* Left Input to Left ADC */
	SND_SOC_DAPM_INPUT("LLIN1"),
	SND_SOC_DAPM_INPUT("LLIN2"),
	SND_SOC_DAPM_INPUT("MICIN1"),
	SND_SOC_DAPM_MUX("Left Input Mux",
		SND_SOC_NOPM, 0, 0, &cs42l51_leftin_mux_controls),
	SND_SOC_DAPM_PGA("Left PGA", CS42L51_PWRCTL1_REG, 3, 1, NULL, 0),
	SND_SOC_DAPM_ADC("Left ADC", "Left Capture", CS42L51_PWRCTL1_REG, 1, 1),

	/* Right Input to Right ADC */
	SND_SOC_DAPM_INPUT("RLIN1"),
	SND_SOC_DAPM_INPUT("RLIN2"),
	SND_SOC_DAPM_INPUT("MICIN2"),
	SND_SOC_DAPM_MICBIAS("Mic Bias", CS42L51_MICPWR_SPDCTL_REG, 1, 1),
	SND_SOC_DAPM_MUX("Right Input Mux",
		SND_SOC_NOPM, 0, 0, &cs42l51_rightin_mux_controls),
	SND_SOC_DAPM_PGA("Right PGA", CS42L51_PWRCTL1_REG, 4, 1, NULL, 0),
	SND_SOC_DAPM_ADC("Right ADC", "Right Capture", CS42L51_PWRCTL1_REG, 2, 1),
};

static const struct snd_soc_dapm_route intercon[] = {
	/* outputs */
	{"LHPOUT", NULL, "Left DAC"},
	{"RHPOUT", NULL, "Right DAC"},

	/* left inputs */
	{"Left Input Mux", "Line 1", "LLIN1"},
	{"Left Input Mux", "Line 2", "LLIN2"},
	{"Left Input Mux", "Line 3", "MICIN1"},
	{"Left Input Mux", "Mic", "MICIN1"},

	{"Left PGA", NULL, "Left Input Mux"},
	{"Left ADC", NULL, "Left PGA"},

	/* right inputs */
	{"Right Input Mux", "Line 1", "RLIN1"},
	{"Right Input Mux", "Line 2", "RLIN2"},
	{"Right Input Mux", "Line 3", "MICIN2"},
	{"Right Input Mux", "Mic", "MICIN2"},

	{"Right PGA", NULL, "Right Input Mux"},
	{"Right ADC", NULL, "Right PGA"},
};

static int cs42l51_add_widgets(struct snd_soc_codec *codec)
{
	snd_soc_dapm_new_controls(codec, cs42l51_dapm_widgets,
				  ARRAY_SIZE(cs42l51_dapm_widgets));

	snd_soc_dapm_add_routes(codec, intercon, ARRAY_SIZE(intercon));

	snd_soc_dapm_new_widgets(codec);
	return 0;
}

static int cs42l51_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params,
			struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec = socdev->codec;
	struct cs42l51_priv *cs42l51 = codec->private_data;
	u32 sfreq;

	/* Freeze register setting */
	cs42l51_setbits(codec, CS42L51_DAC_CTL_REG, CS42L51_FREEZE_ENABLE);

	sfreq = params_rate(params);	/* Sampling rate, in Hz */
	/* Set speed */
	cs42l51_clrbits(codec, CS42L51_MICPWR_SPDCTL_REG, CS42L51_SPEED_MASK);
	switch (sfreq) {
	case 96000:
	case 88200:
	case 64000:
		cs42l51_setbits(codec, CS42L51_MICPWR_SPDCTL_REG, CS42L51_SPEED_DSM);
		break;
	case 48000:
	case 44100:
	case 32000:
		cs42l51_setbits(codec, CS42L51_MICPWR_SPDCTL_REG, CS42L51_SPEED_SSM);
		break;
	case 24000:
	case 22050:
	case 16000:
		cs42l51_setbits(codec, CS42L51_MICPWR_SPDCTL_REG, CS42L51_SPEED_HSM);
		break;
	case 12000:
	case 11025:
	case 8000:
		cs42l51_setbits(codec, CS42L51_MICPWR_SPDCTL_REG, CS42L51_SPEED_QSM);
		break;
	default:
		pr_err("CS42L51: unknown rate\n");
		return -EINVAL;
	}

	/* Set interface format */
	cs42l51_clrbits(codec, CS42L51_INTFCTL_REG, CS42L51_INTF_DACDIF_MASK);
	switch (cs42l51->mode) {
	case SND_SOC_DAIFMT_I2S:
		cs42l51_setbits(codec, CS42L51_INTFCTL_REG, CS42L51_INTF_DACDIF_I2S);
		cs42l51_setbits(codec, CS42L51_INTFCTL_REG, CS42L51_INTF_ADCDIF_I2S);
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		cs42l51_setbits(codec, CS42L51_INTFCTL_REG, CS42L51_INTF_DACDIF_LJ24);
		cs42l51_clrbits(codec, CS42L51_INTFCTL_REG, CS42L51_INTF_ADCDIF_I2S);
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		pr_err("CS42L51: CS42L51 support SND_SOC_DAIFMT_RIGHT_J, "
			"but we don't implement it yet\n");
	default:
		pr_err("CS42L51: unknown format\n");
		return -EINVAL;
	}

	/*De-freeze register setting now */
	cs42l51_clrbits(codec, CS42L51_DAC_CTL_REG, CS42L51_FREEZE_ENABLE);

	cs42l51_dump_reg(codec);

	return 0;
}


static int cs42l51_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	u8 reg_msk = CS42L51_MUTE_DAC_A | CS42L51_MUTE_DAC_B;

	if (mute)
		cs42l51_setbits(codec, CS42L51_ADCSEL_INVMUTE_REG, reg_msk);
	else
		cs42l51_clrbits(codec, CS42L51_ADCSEL_INVMUTE_REG, reg_msk);

	return 0;
}

static int cs42l51_set_dai_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct cs42l51_priv *cs42l51 = codec->private_data;

	switch (clk_id) {
	case CS42L51_SYSCLK:
		cs42l51->mclk = freq;
		break;
	default:
		pr_err("CLK SOURCE (%d) is not supported yet\n", clk_id);
		return -EINVAL;
	}

	return 0;
}

static int cs42l51_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct cs42l51_priv *cs42l51 = codec->private_data;

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		cs42l51_clrbits(codec, CS42L51_INTFCTL_REG, CS42L51_INTF_MASTER);
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_LEFT_J:
	case SND_SOC_DAIFMT_RIGHT_J:
	case SND_SOC_DAIFMT_I2S:
		cs42l51->mode = fmt & SND_SOC_DAIFMT_FORMAT_MASK;
		break;
	default:
		pr_err("CS42L51: invalid DAI format\n");
		return -EINVAL;
	}

	return 0;
}

static int cs42l51_set_bias_level(struct snd_soc_codec *codec,
				 enum snd_soc_bias_level level)
{
	u8 reg_msk;

	switch (level) {
	case SND_SOC_BIAS_ON:
		/* everything on, dac unmute */
		reg_msk = CS42L51_MUTE_ADC_A | CS42L51_MUTE_ADC_B;
		cs42l51_clrbits(codec, CS42L51_ADCSEL_INVMUTE_REG, reg_msk);
		reg_msk = CS42L51_MUTE_DAC_A | CS42L51_MUTE_DAC_B;
		cs42l51_clrbits(codec, CS42L51_DAC_OUTCTL_REG, reg_msk);
		cs42l51_clrbits(codec, CS42L51_PWRCTL1_REG, CS42L51_PWRCTL_PDN);
		break;

	case SND_SOC_BIAS_PREPARE:
		break;

	case SND_SOC_BIAS_STANDBY:
		cs42l51_setbits(codec, CS42L51_PWRCTL1_REG, CS42L51_PWRCTL_PDN);
		break;
	case SND_SOC_BIAS_OFF:
		/* everything off, dac mute */
		reg_msk = CS42L51_MUTE_ADC_A | CS42L51_MUTE_ADC_B;
		cs42l51_setbits(codec, CS42L51_ADCSEL_INVMUTE_REG, reg_msk);
		reg_msk = CS42L51_MUTE_DAC_A | CS42L51_MUTE_DAC_B;
		cs42l51_setbits(codec, CS42L51_DAC_OUTCTL_REG, reg_msk);
		cs42l51_setbits(codec, CS42L51_PWRCTL1_REG, CS42L51_PWRCTL_PDN);
		break;
	}

	codec->bias_level = level;
	return 0;
}

#define CS42L51_RATES SNDRV_PCM_RATE_8000_48000
#define CS42L51_FORMATS SNDRV_PCM_FMTBIT_S16_LE

struct snd_soc_dai cs42l51_dai = {
	.name = "CS42L51",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = CS42L51_RATES,
		.formats = CS42L51_FORMATS,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = CS42L51_RATES,
		.formats = CS42L51_FORMATS,
	},
	.ops = {
		.hw_params = cs42l51_hw_params,
		.digital_mute = cs42l51_mute,
		.set_sysclk = cs42l51_set_dai_sysclk,
		.set_fmt = cs42l51_set_dai_fmt,
	},
};
EXPORT_SYMBOL_GPL(cs42l51_dai);

static int cs42l51_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->codec;

	cs42l51_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int cs42l51_resume(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->codec;
	u8 *cache = codec->reg_cache;
	int i;

	cs42l51_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	/* Sync reg_cache with the hardware */
	for (i = 1; i < CS42L51_NUMREGS; i++) {
		cs42l51_write(codec, i, cache[i]);
	}

	cs42l51_set_bias_level(codec, codec->suspend_bias_level);

	return 0;
}

/*
 * initialise the CS42L51 driver
 * register the mixer and dsp interfaces with the kernel
 */
static int cs42l51_init(struct snd_soc_device *socdev)
{
	struct snd_soc_codec *codec = socdev->codec;
	struct i2c_client *client = codec->control_data;
	struct cs42l51_setup_data *setup = socdev->codec_data;
	int ret = -ENODEV, data;

	codec->name = "CS42L51";
	codec->owner = THIS_MODULE;
	codec->reg_cache_size = CS42L51_NUMREGS;
	codec->reg_cache = kzalloc(CS42L51_NUMREGS, GFP_KERNEL);
	if (!codec->reg_cache) {
		pr_err("cs42l51: could not allocate register cache\n");
		return -ENOMEM;
	}
	codec->read = cs42l51_read_reg_cache;
	codec->write = cs42l51_write;
	codec->set_bias_level = cs42l51_set_bias_level;
	codec->dai = &cs42l51_dai;
	codec->num_dai = 1;

	/* Reset CS42L51 codec */
	gpio_direction_output(setup->rst_pin, GPIO_LOW);
	msleep(setup->rst_delay);
	gpio_direction_output(setup->rst_pin, GPIO_HIGH);

	/* Verify that we have a CS42L51 codec */
	data = i2c_smbus_read_byte_data(client, CS42L51_CHIPID_REG);
	if (data < 0) {
		pr_err("CS42L51: failed to read I2C\n");
		goto init_err;
	}
	/* The top 5 bits of the chip ID should be 11011. */
	if ((data & CS42L51_CHIPID_ID) != 0xD8) {
		/* The device at this address is not a CS42L51 codec */
		pr_err("CS42L51: The device at this address is not a CS42L51 codec\n");
		goto init_err;
	}

	pr_info("Found CS42L51 at I2C address %X\n", client->addr);
	pr_info("CS42L51 hardware revision %X\n", data & CS42L51_CHIPID_REV);

	/* register pcms */
	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0) {
		pr_err("cs42l51: failed to create pcms\n");
		goto init_err;
	}

	/* power on device */
	cs42l51_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	/* fill the reg cache for read afterwards */
	cs42l51_fill_cache(codec);

	/* Disable auto-detect speed mode */
	cs42l51_clrbits(codec, CS42L51_MICPWR_SPDCTL_REG, CS42L51_AUTO_ENABLE);

	cs42l51_add_controls(codec);
	cs42l51_add_widgets(codec);

	ret = snd_soc_init_card(socdev);
	if (ret < 0) {
		pr_err("cs42l51: failed to register card\n");
		goto card_err;
	}

	pr_info("CS42L51 Audio Codec Registered\n");

	return ret;

card_err:
	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
init_err:
	kfree(codec->reg_cache);
	return ret;
}

static struct snd_soc_device *cs42l51_socdev;

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)


static int cs42l51_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct snd_soc_device *socdev = cs42l51_socdev;
	struct snd_soc_codec *codec = socdev->codec;
	int ret;

	i2c_set_clientdata(i2c, codec);
	codec->control_data = i2c;

	ret = cs42l51_init(socdev);
	if (ret < 0)
		pr_err("failed to initialise CS42L51\n");

	return ret;
}

static int cs42l51_i2c_remove(struct i2c_client *client)
{
	struct snd_soc_codec *codec = i2c_get_clientdata(client);
	kfree(codec->reg_cache);
	return 0;
}

static const struct i2c_device_id cs42l51_i2c_id[] = {
	{ "cs42l51", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, cs42l51_i2c_id);

static struct i2c_driver cs42l51_i2c_driver = {
	.driver = {
		.name = "CS42L51 I2C Codec",
		.owner = THIS_MODULE,
	},
	.probe =    cs42l51_i2c_probe,
	.remove =   cs42l51_i2c_remove,
	.id_table = cs42l51_i2c_id,
};

static int cs42l51_add_i2c_device(struct platform_device *pdev,
				 const struct cs42l51_setup_data *setup)
{
	struct i2c_board_info info;
	struct i2c_adapter *adapter;
	struct i2c_client *client;
	int ret;

	ret = i2c_add_driver(&cs42l51_i2c_driver);
	if (ret != 0) {
		dev_err(&pdev->dev, "can't add i2c driver\n");
		return ret;
	}

	memset(&info, 0, sizeof(struct i2c_board_info));
	info.addr = setup->i2c_address;
	strlcpy(info.type, "cs42l51", I2C_NAME_SIZE);

	adapter = i2c_get_adapter(setup->i2c_bus);
	if (!adapter) {
		dev_err(&pdev->dev, "can't get i2c adapter %d\n",
			setup->i2c_bus);
		goto err_driver;
	}

	client = i2c_new_device(adapter, &info);
	i2c_put_adapter(adapter);
	if (!client) {
		dev_err(&pdev->dev, "can't add i2c device at 0x%x\n",
			(unsigned int)info.addr);
		goto err_driver;
	}

	return 0;

err_driver:
	i2c_del_driver(&cs42l51_i2c_driver);
	return -ENODEV;
}
#endif

static int cs42l51_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct cs42l51_setup_data *setup;
	struct snd_soc_codec *codec;
	struct cs42l51_priv *cs42l51;
	int ret = -ENOMEM;

	setup = socdev->codec_data;
	codec = kzalloc(sizeof(struct snd_soc_codec), GFP_KERNEL);
	if (codec == NULL)
		goto exit;

	cs42l51 = kzalloc(sizeof(struct cs42l51_priv), GFP_KERNEL);
	if (cs42l51 == NULL)
		goto priv_alloc_err;

	codec->private_data = cs42l51;
	socdev->codec = codec;
	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);

	if (setup->rst_pin == 0)
		goto gpio_request_err;

	ret = gpio_request(setup->rst_pin, "cs42l51-reset");
	if (ret < 0) {
		pr_err("Request codec-reset GPIO(%d) failed\n", setup->rst_pin);
		goto gpio_request_err;
	}

	cs42l51_socdev = socdev;
	ret = -ENODEV;

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	if (setup->i2c_address) {
		ret = cs42l51_add_i2c_device(pdev, setup);
		if (ret != 0) {
			pr_err("can't add i2c driver");
			goto add_i2c_err;
		}
	}
#else
	/* Add other interfaces here */
#endif

	if(ret != 0)
		goto add_i2c_err;

	return 0;

add_i2c_err:
	gpio_free(setup->rst_pin);
gpio_request_err:
	kfree(codec->private_data);
priv_alloc_err:
	kfree(codec);
exit:
	return ret;
}

/* power down chip */
static int cs42l51_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->codec;
	struct cs42l51_setup_data *setup = socdev->codec_data;

	if (codec->control_data)
		cs42l51_set_bias_level(codec, SND_SOC_BIAS_OFF);

	gpio_free(setup->rst_pin);

	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	i2c_unregister_device(codec->control_data);
	i2c_del_driver(&cs42l51_i2c_driver);
#endif
	if (codec->private_data)
		kfree(codec->private_data);
	if (codec)
		kfree(codec);

	return 0;
}

struct snd_soc_codec_device soc_codec_dev_cs42l51 = {
	.probe = 	cs42l51_probe,
	.remove = 	cs42l51_remove,
	.suspend = 	cs42l51_suspend,
	.resume =	cs42l51_resume,
};
EXPORT_SYMBOL_GPL(soc_codec_dev_cs42l51);

static int __init ambarella_cs42l51_init(void)
{
	return snd_soc_register_dai(&cs42l51_dai);
}
module_init(ambarella_cs42l51_init);

static void __exit ambarella_cs42l51_exit(void)
{
	snd_soc_unregister_dai(&cs42l51_dai);
}
module_exit(ambarella_cs42l51_exit);

MODULE_AUTHOR("Cao Rongrong <rrcao@ambarella.com>");
MODULE_DESCRIPTION("ASoC CS42L51 codec driver");
MODULE_LICENSE("GPL");

