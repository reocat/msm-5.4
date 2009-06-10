/*
 * adav803.c  --  ADAV803 ALSA Soc Audio driver
 *
 * Author: Cao Rongrong <rrcao@ambarella.com>
 *
 * History:
 *	2009/03/05 - [Cao Rongrong] Created file
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

#include "adav803.h"


struct snd_soc_codec_device soc_codec_dev_adav803;

enum AudioCodec_MCLK {
	AudioCodec_18_432M = 0,
	AudioCodec_16_9344M = 1,
	AudioCodec_12_288M = 2,
	AudioCodec_11_2896M = 3,
	AudioCodec_9_216M = 4,
	AudioCodec_8_4672M = 5,
	AudioCodec_8_192M = 6,
	AudioCodec_6_144 = 7,
	AudioCodec_5_6448M = 8,
	AudioCodec_4_608M = 9,
	AudioCodec_4_2336M = 10,
	AudioCodec_4_096M = 11,
	AudioCodec_3_072M = 12,
	AudioCodec_2_8224M = 13,	
	AudioCodec_2_048M = 14
};

#define MAX_OVERSAMPLE_IDX_NUM	9
enum AudioCodec_OverSample {
	AudioCodec_128xfs = 0,
	AudioCodec_256xfs = 1,
	AudioCodec_384xfs = 2,
	AudioCodec_512xfs = 3,
	AudioCodec_768xfs = 4,
	AudioCodec_1024xfs = 5,
	AudioCodec_1152xfs = 6,
	AudioCodec_1536xfs = 7,
	AudioCodec_2304xfs = 8
};

/* codec private data */
struct adav803_priv {
	unsigned int sysclk;
	unsigned int fmt_dir;
};

static u8 adav803_mclk_table[5] = { 
	(ADAV803_u65_MCLK | ADAV803_u65_4X ),	/* 128 x fs */
	(ADAV803_u65_MCLK  | ADAV803_u65_8X ),	/* 256 x fs */
	(ADAV803_u65_MCLK32 | ADAV803_u65_8X ),	/* 384 x fs */
	(ADAV803_u65_MCLK12 | ADAV803_u65_8X ),	/* 512 x fs */
	(ADAV803_u65_MCLK13 | ADAV803_u65_8X )	/* 768 x fs */
};


/*
 * We read all of the ADAV803 registers to pre-fill the register cache
 */
static int adav803_fill_cache(struct snd_soc_codec *codec)
{
	u8 *cache = codec->reg_cache;
	struct i2c_client *client = codec->control_data;
	s32 val, i;

	for(i = 0; i < ADAV803_NUMREGS; i++){
		val = i2c_smbus_read_byte_data(client, i << 1);
		if(val < 0){
			pr_err("I2C read error: address = 0x%x\n", i);
			return -EIO;
		}
		
		cache[i] = val;
	}

	return 0;
}

/*
 * Read from the ADAV803 register cache.
 *
 * This ADAV803 registers are cached to avoid excessive I2C I/O operations.
 * After the initial read to pre-fill the cache, the ADAV803 never updates
 * the register values, so we won't have a cache coherncy problem.
 */
static inline unsigned int adav803_read_reg_cache(struct snd_soc_codec *codec,
	unsigned int reg)
{
	u8 *cache = codec->reg_cache;

	if (reg >= ADAV803_NUMREGS)
		return -EIO;

	return cache[reg];
}

/*
 * Write to a ADAV803 register via the I2C bus.
 *
 * This function writes the given value to the given ADAV803 register, and
 * also updates the register cache.
 *
 * Note that we don't use the hw_write function pointer of snd_soc_codec.
 * That's because it's too clunky: the hw_write_t prototype does not match
 * i2c_smbus_write_byte_data(), and it's just another layer of overhead.
 */
static int adav803_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	u8 *cache = codec->reg_cache;

	if (reg >= ADAV803_NUMREGS)
		return -EIO;

	/* Only perform an I2C operation if the new value is different */
	if (cache[reg] != value) {
		struct i2c_client *client = codec->control_data;
		if (i2c_smbus_write_byte_data(client, reg << 1, value)) {
			printk(KERN_ERR "cs4270: I2C write failed\n");
			return -EIO;
		}

		/* We've written to the hardware, so update the cache */
		cache[reg] = value;
	}

	return 0;
}


static const char *adav803_deemph[] = {"None", "44.1Khz", "32Khz", "48Khz"};
static const char *adav803_dac_route[] = { "Left-Right", "Both Right", "Both Right", "Right-Left"};

static const struct soc_enum adav803_enum[] = {
	SOC_ENUM_SINGLE(ADAV803_DAC_Ctrl2_Reg_Adr, 0, 4, adav803_deemph),
	SOC_ENUM_SINGLE(ADAV803_DAC_Ctrl1_Reg_Adr, 4, 4, adav803_dac_route),
};

static const struct snd_kcontrol_new adav803_snd_controls[] = {
	SOC_DOUBLE_R("Master Playback Volume", ADAV803_DAC_Left_Volume_Adr,
		ADAV803_DAC_Right_Volume_Adr, 0, 255, 0),
	SOC_DOUBLE("DAC Switch", ADAV803_DAC_Ctrl1_Reg_Adr,0, 1, 1, 0),
	SOC_DOUBLE_R("Capture Volume", ADAV803_ADC_Left_Volume_Adr,
		ADAV803_ADC_Right_Volume_Adr, 0, 255, 0),
	SOC_DOUBLE_R("Capture PGA", ADAV803_ADC_Left_PGA_Adr,
		ADAV803_ADC_Right_PGA_Adr, 0, 63, 1),
	SOC_ENUM("Playback De-emphasis", adav803_enum[0]),
	SOC_ENUM("Route", adav803_enum[1]),
};


/* add non dapm controls */
static int adav803_add_controls(struct snd_soc_codec *codec)
{
	int err, i;

	for (i = 0; i < ARRAY_SIZE(adav803_snd_controls); i++) {
		err = snd_ctl_add(codec->card,
				  snd_soc_cnew(&adav803_snd_controls[i],
						codec, NULL));
		if (err < 0)
			return err;
	}

	return 0;
}


static const struct snd_soc_dapm_widget adav803_dapm_widgets[] = {
	SND_SOC_DAPM_DAC("DAC", "Playback", ADAV803_DAC_Ctrl1_Reg_Adr, 7, 1),
	SND_SOC_DAPM_OUTPUT("LLOUT"),
	SND_SOC_DAPM_OUTPUT("RLOUT"),

	SND_SOC_DAPM_PGA("Left Line In", ADAV803_ADC_Ctrl1_Reg_Adr, 1, 1, NULL, 0),
	SND_SOC_DAPM_PGA("Right Line In", ADAV803_ADC_Ctrl1_Reg_Adr, 0, 1, NULL, 0),
	SND_SOC_DAPM_ADC("ADC", "Capture", ADAV803_ADC_Ctrl1_Reg_Adr, 5, 1),
	SND_SOC_DAPM_INPUT("LLIN"),
	SND_SOC_DAPM_INPUT("RLIN"),
};

static const struct snd_soc_dapm_route intercon[] = {
	/* outputs */
	{"LLOUT", NULL, "DAC"},
	{"RLOUT", NULL, "DAC"},

	/* inputs */
	{"Left Line In", NULL, "LLIN"},
	{"Right Line In", NULL, "RLIN"},
	{"ADC", NULL, "Left Line In"},
	{"ADC", NULL, "Right Line In"},
};

static int adav803_add_widgets(struct snd_soc_codec *codec)
{
	snd_soc_dapm_new_controls(codec, adav803_dapm_widgets,
				  ARRAY_SIZE(adav803_dapm_widgets));

	snd_soc_dapm_add_routes(codec, intercon, ARRAY_SIZE(intercon));

	snd_soc_dapm_new_widgets(codec);
	return 0;
}


void adav803_get_clock_info(u32 sfreq, audiocodec_clk_info_t *clkinfo)
{
	switch(sfreq){
	case 48000:
		clkinfo->mclk_idx = AudioCodec_12_288M;
		clkinfo->oversample_idx = AudioCodec_256xfs;
		break;
	case 44100:
		clkinfo->mclk_idx = AudioCodec_11_2896M;
		clkinfo->oversample_idx = AudioCodec_256xfs;
		break;
	case 32000:
		clkinfo->mclk_idx = AudioCodec_8_192M;
		clkinfo->oversample_idx = AudioCodec_256xfs;
		break;
	case 24000:
		clkinfo->mclk_idx = AudioCodec_6_144;
		clkinfo->oversample_idx = AudioCodec_256xfs;
		break;
	case 22050:
		clkinfo->mclk_idx = AudioCodec_5_6448M;
		clkinfo->oversample_idx = AudioCodec_256xfs;
		break;
	case 16000:
		clkinfo->mclk_idx = AudioCodec_4_096M;
		clkinfo->oversample_idx = AudioCodec_256xfs;
		break;
	case 12000:
		clkinfo->mclk_idx = AudioCodec_3_072M;
		clkinfo->oversample_idx = AudioCodec_256xfs;
		break;
	case 11025:
		clkinfo->mclk_idx = AudioCodec_2_8224M;
		clkinfo->oversample_idx = AudioCodec_256xfs;
		break;
	case 8000:
		clkinfo->mclk_idx = AudioCodec_2_048M;
		clkinfo->oversample_idx = AudioCodec_256xfs;
		break;
	default:
		clkinfo->mclk_idx = 0xff;
		clkinfo->oversample_idx = 0xff;
		break;
	}	
}


static int adav803_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec = socdev->codec;
	struct adav803_priv *adav803 = codec->private_data;

	u32 sfreq = params_rate(params);
	audiocodec_clk_info_t clk_info;
	u8 oversample_idx, data;
	u8 data1, data2, data3, data4;

	adav803_get_clock_info(sfreq, &clk_info);
	oversample_idx = clk_info.oversample_idx;
	if (oversample_idx >= 5) {
		oversample_idx = AudioCodec_256xfs;
	}

	data = adav803_mclk_table[oversample_idx];

	switch (sfreq) {
	case 48000 :
	case 24000 :
	case 12000 :
		data |= ADAV803_u65_48K_DEEM;
		break;
	case 44100 :
	case 22050 :
	case 11025 :
		data |= ADAV803_u65_441K_DEEM;
		break;
	case 32000 :
	case 16000 :
	case 8000 :
		data |= ADAV803_u65_32K_DEEM;
		break;
	}

	adav803_write(codec, ADAV803_DAC_Ctrl2_Reg_Adr, data);

	/* bit size */
	data1 = adav803_read_reg_cache(codec, ADAV803_Playback_Ctrl_Reg_Adr);
	data2 = adav803_read_reg_cache(codec, ADAV803_AuxIn_Reg_Adr);
	data3 = adav803_read_reg_cache(codec, ADAV803_Record_Ctrl_Reg_Adr);
	data4 = adav803_read_reg_cache(codec, ADAV803_AuxOut_Reg_Adr);

	/* I2S interface format */
	data1 = data1 & 0xe0;	/* DAC is in Slave Clock Mode */
	data2 = data2 & 0xe0;	/* AuxIn is in Slave Clock Mode */
	data3 = data3 & 0xcc;	/* ADC is in Slave Clock Mode */
	data4 = (data4 & 0xcc) | 0x10;	/* AuxOut is in Slave Clock Mode */

	switch (adav803->fmt_dir) {
	case SND_SOC_DAIFMT_LEFT_J:
		data1 = data1 | ADAV803_u04_LeftJustified;
		data2 = data2 | ADAV803_u05_LeftJustified;
		data3 = data3 | ADAV803_u06_LeftJustified;
		data4 = data4 | ADAV803_u07_LeftJustified;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		switch(params_format(params)){
		case SNDRV_PCM_FORMAT_S16_LE:
			data1 |= ADAV803_u04_16bit_RightJustified;
			data2 |= ADAV803_u05_16bit_RightJustified;
			data3 |= ((ADAV803_u06_16Bit<< 2) | ADAV803_u06_RightJustified);
			data4 |= ((ADAV803_u07_16Bit<< 2) | ADAV803_u07_RightJustified);
			break;

		case SNDRV_PCM_FORMAT_S18_3LE:
			data1 |= ADAV803_u04_18bit_RightJustified;
			data2 |= ADAV803_u05_18bit_RightJustified;
			data3 |= ((ADAV803_u06_18Bit<< 2) | ADAV803_u06_RightJustified);
			data4 |= ((ADAV803_u07_18Bit<< 2) | ADAV803_u07_RightJustified);
			break;

		case SNDRV_PCM_FORMAT_S20_3LE:
			data1 |= ADAV803_u04_20bit_RightJustified;
			data2 |= ADAV803_u05_20bit_RightJustified;
			data3 |= ((ADAV803_u06_20Bit<< 2) | ADAV803_u06_RightJustified);
			data4 |= ((ADAV803_u07_20Bit<< 2) | ADAV803_u07_RightJustified);
			break;

		case SNDRV_PCM_FORMAT_S24_LE:
			data1 |= ADAV803_u04_24bit_RightJustified;
			data2 |= ADAV803_u05_24bit_RightJustified;
			data3 |= ((ADAV803_u06_24Bit<< 2) | ADAV803_u06_RightJustified);
			data4 |= ((ADAV803_u07_24Bit<< 2) | ADAV803_u07_RightJustified);
			break;
		}
		break;	
	case SND_SOC_DAIFMT_I2S:
		data1 = data1 | ADAV803_u04_I2S;
		data2 = data2 | ADAV803_u05_I2S;
		data3 = data3 | ADAV803_u06_I2S;
		data4 = data4 | ADAV803_u07_I2S;
		break;
	default:
		printk("not supported mode");
		return -EINVAL;
	}

	adav803_write(codec, ADAV803_Playback_Ctrl_Reg_Adr, data1);
	adav803_write(codec, ADAV803_AuxIn_Reg_Adr, data2);
	adav803_write(codec, ADAV803_Record_Ctrl_Reg_Adr, data3);
	adav803_write(codec, ADAV803_AuxOut_Reg_Adr, data4);

	return 0;
}


static int adav803_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 mute_reg = adav803_read_reg_cache(codec, ADAV803_DAC_Ctrl1_Reg_Adr);

	if (mute)
		adav803_write(codec, ADAV803_DAC_Ctrl1_Reg_Adr, mute_reg & 0xfc);
	else
		adav803_write(codec, ADAV803_DAC_Ctrl1_Reg_Adr, mute_reg | 0x03);

	return 0;
}

static int adav803_set_dai_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	int ret = 0;

	if (freq > 12288000)
		ret = -EINVAL;

	return ret;
}


static int adav803_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int adav803_set_bias_level(struct snd_soc_codec *codec,
				 enum snd_soc_bias_level level)
{
	u8 reg;

	switch (level) {
	case SND_SOC_BIAS_ON:
		/* everything on, dac unmute */
		
		/* Power on and unmute DAC */
		reg = adav803_read_reg_cache(codec, ADAV803_DAC_Ctrl1_Reg_Adr);
		reg |= (ADAV803_u64_Right_UnMute & ADAV803_u64_Left_UnMute);
		reg &= (~ADAV803_u64_DR_ALL);
		adav803_write(codec, ADAV803_DAC_Ctrl1_Reg_Adr, reg);

		/* Power on ADC */
		reg = adav803_read_reg_cache(codec, ADAV803_ADC_Ctrl1_Reg_Adr);
		reg &= (~(ADAV803_ADC_Ctrl1_PWR_Down |
			ADAV803_ADC_Ctrl1_Analog_PWR_Down |
			ADAV803_ADC_Ctrl1_PGA_PWR_Left_Down |
			ADAV803_ADC_Ctrl1_PGA_PWR_Right_Down));
		adav803_write(codec, ADAV803_ADC_Ctrl1_Reg_Adr, reg);
		break;

	case SND_SOC_BIAS_PREPARE:
		break;

	case SND_SOC_BIAS_STANDBY:
	case SND_SOC_BIAS_OFF:
		/* everything off, dac mute */

		/* Power down and mute DAC */
		reg = adav803_read_reg_cache(codec, ADAV803_DAC_Ctrl1_Reg_Adr);
		reg &= (~(ADAV803_u64_Right_UnMute & ADAV803_u64_Left_UnMute));
		reg |= ADAV803_u64_DR_ALL;
		adav803_write(codec, ADAV803_DAC_Ctrl1_Reg_Adr, reg);

		/* Power down ADC */
		reg = adav803_read_reg_cache(codec, ADAV803_ADC_Ctrl1_Reg_Adr);
		reg |= (ADAV803_ADC_Ctrl1_PWR_Down |
			ADAV803_ADC_Ctrl1_Analog_PWR_Down |
			ADAV803_ADC_Ctrl1_PGA_PWR_Left_Down |
			ADAV803_ADC_Ctrl1_PGA_PWR_Right_Down);
		adav803_write(codec, ADAV803_ADC_Ctrl1_Reg_Adr, reg);

		/* Power down PLL */
		reg = adav803_read_reg_cache(codec, ADAV803_PLL_Ctrl1_Reg_Adr);
		reg |= (ADAV803_PLL_Ctrl1_PLL2PD_PWR_Down | 
			ADAV803_PLL_Ctrl1_PLL1PD_PWR_Down |
			ADAV803_PLL_Ctrl1_XTAL_PWR_Down);
		adav803_write(codec, ADAV803_PLL_Ctrl1_Reg_Adr, reg);

		/* Power down SPDIF Receiver */
		reg = adav803_read_reg_cache(codec, ADAV803_PLL_Output_Enable_Adr);
		reg |= ADAV803_u7a_DIRIN_PWR_Down;
		adav803_write(codec, ADAV803_PLL_Output_Enable_Adr, reg);
		break;
	}

	codec->bias_level = level;
	return 0;
}

#define ADAV803_RATES SNDRV_PCM_RATE_8000_48000

#define ADAV803_FORMATS SNDRV_PCM_FMTBIT_S16_LE

struct snd_soc_dai adav803_dai = {
	.name = "ADAV803",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = ADAV803_RATES,
		.formats = ADAV803_FORMATS,},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = ADAV803_RATES,
		.formats = ADAV803_FORMATS,},
	.ops = {
		.hw_params = adav803_hw_params,
	},
	.dai_ops = {
		.digital_mute = adav803_mute,
		.set_sysclk = adav803_set_dai_sysclk,
		.set_fmt = adav803_set_dai_fmt,
	}
};
EXPORT_SYMBOL_GPL(adav803_dai);

static int adav803_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->codec;

	adav803_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int adav803_resume(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->codec;
	u8 *cache = codec->reg_cache;
	int i;

	/* Sync reg_cache with the hardware */
	for (i = 0; i < ADAV803_NUMREGS; i++) {
		adav803_write(codec, i, cache[i]);
	}
	adav803_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	adav803_set_bias_level(codec, codec->suspend_bias_level);

	return 0;
}

/*
 * initialise the ADAV803 driver
 * register the mixer and dsp interfaces with the kernel
 */
static int adav803_init(struct snd_soc_device *socdev)
{
	struct snd_soc_codec *codec = socdev->codec;
	int ret = 0;
	u8 data;

	codec->name = "ADAV803";
	codec->owner = THIS_MODULE;
	codec->read = adav803_read_reg_cache;
	codec->write = adav803_write;
	codec->set_bias_level = adav803_set_bias_level;
	codec->dai = &adav803_dai;
	codec->num_dai = 1;
	codec->reg_cache_size = ADAV803_NUMREGS;
	codec->reg_cache = kzalloc(ADAV803_NUMREGS, GFP_KERNEL);
	if (!codec->reg_cache) {
		printk(KERN_ERR "adav803: could not allocate register cache\n");
		return -ENOMEM;
	}

	/* register pcms */
	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0) {
		printk(KERN_ERR "adav803: failed to create pcms\n");
		goto pcm_err;
	}

	/* power on device */
	adav803_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	adav803_fill_cache(codec);

	data = ADAV803_u09_Rx_NOCLK_PLL | ADAV803_u09_RxCLK_128fs |
		ADAV803_u09_Rx_AutoDeemp_Off | ADAV803_u09_Rx_ERROR_Zero |
		ADAV803_u09_Rx_LoseLock_LastValid;
	adav803_write(codec, ADAV803_SPDIF_Rx_Ctrl1_Reg_Adr, data);

	data = ADAV803_u0a_RxMute_Off | ADAV803_u0a_Rx_LRCLK_FromPLL |
		ADAV803_u0a_Rx_Serial_Record | 0x03;
	adav803_write(codec, ADAV803_SPDIF_Rx_Ctrl2_Reg_Adr, data);

	data = 0x0C;
	adav803_write(codec, ADAV803_Rx_Buffer_Config_Adr, data);

	data = ADAV803_u0c_Tx_Valid_Audio | ADAV803_u0c_Tx_Rx_Ratio11 |
		ADAV803_u0c_Tx_InterCLK1 | ADAV803_u0c_Tx_Enable;
	adav803_write(codec, ADAV803_SPDIF_Tx_Ctrl_Reg_Adr, data);

	data = 0x00;
	adav803_write(codec, ADAV803_Autobuffer_Adr, data);

	data = ADAV803_u1e_Tx_unmute| ADAV803_u1e_NoDeemp;
	adav803_write(codec, ADAV803_SPDIF_Tx_Mute_Reg_Adr, data);

	/* Datapath Control Register 1 */ // TODO
	data = ADAV803_u62_SRC1_ADC | ADAV803_u62_REC2_ADC |
		ADAV803_u62_AUXO2_DIR ;
	adav803_write(codec, ADAV803_DataPath1_Reg_Adr, data);

	/* Datapath Control Register 2 */ // TODO
	data = adav803_read_reg_cache(codec, ADAV803_DataPath2_Reg_Adr);
	data &= 0xc0;
	data |= (ADAV803_u63_DAC2_Playback | ADAV803_u63_DIT2_Playback);
	adav803_write(codec, ADAV803_DataPath2_Reg_Adr, data);

	/* DAC Control Register 3 */
	data = adav803_read_reg_cache(codec, ADAV803_DAC_Ctrl3_Reg_Adr);
	data &= 0xf8;
	data |= (ADAV803_u66_ZFVOL_Enable | ADAV803_u66_ZFDATA_Enable |
		ADAV803_u66_ZFPOL_ActiveHigh);
	adav803_write(codec, ADAV803_DAC_Ctrl3_Reg_Adr, data);

	/* DAC Control Register 4 */
	data = adav803_read_reg_cache(codec, ADAV803_DAC_Ctrl4_Reg_Adr);
	data &= 0x8f;
	data |= (ADAV803_u67_INTAS_ZEROL | ADAV803_u67_ONE_ZERO);
	adav803_write(codec, ADAV803_DAC_Ctrl4_Reg_Adr, data);

	/* DAC Control Register 1: Set Mute of DAC */
	data = adav803_read_reg_cache(codec, ADAV803_DAC_Ctrl1_Reg_Adr);
	data &= 0xfc;
	adav803_write(codec, ADAV803_DAC_Ctrl1_Reg_Adr, data);

	/* DAC Left Volume Register: Set default Volume (0db) */
	data = 0xff;
	adav803_write(codec, ADAV803_DAC_Left_Volume_Adr, data);

	/* DAC Right Volume Register: Set default Volume (0db) */
	data = 0xff;
	adav803_write(codec, ADAV803_DAC_Right_Volume_Adr, data);

	/* ADC Left Channel PGA Gain Register: Set default PGA (0db) */
	data = adav803_read_reg_cache(codec, ADAV803_ADC_Left_PGA_Adr);
	data &= 0xc0;
	adav803_write(codec, ADAV803_ADC_Left_PGA_Adr, data);

	/* ADC Right Channel PGA Gain Register: Set default PGA (0db) */
	data = adav803_read_reg_cache(codec, ADAV803_ADC_Right_PGA_Adr);
	data &= 0xc0;
	adav803_write(codec, ADAV803_ADC_Right_PGA_Adr, data);

	/* ADC Left Volume Register: Set default Volume (0db) */
	data = 0xff;
	adav803_write(codec, ADAV803_ADC_Left_Volume_Adr, data);

	/* ADC Right Volume Register: Set default Volume (0db) */
	data = 0xff;
	adav803_write(codec, ADAV803_ADC_Right_Volume_Adr, data);

	/*  Internal Clocking Control Register 1 */
	data = ADAV803_u76_DACCLKSrc_MCLKI| ADAV803_u76_ADCCLKSrc_MCLKI |
		ADAV803_u76_ICLK2Src_MCLKI ;
	adav803_write(codec, ADAV803_Internal_CLK_Ctrl1_Adr, data);

	/*  Internal Clocking Control Register 2 */
	data = adav803_read_reg_cache(codec, ADAV803_ADC_Right_PGA_Adr);
	data &= 0xe0;
	data = ADAV803_u77_ICLK1Src_MCLKI| ADAV803_u77_PLL2_FS3 |
		ADAV803_u77_PLL1_FS12;
	adav803_write(codec, ADAV803_Internal_CLK_Ctrl2_Adr, data);

	/*  Internal Clocking Control Register 2 */
	data = adav803_read_reg_cache(codec, ADAV803_ADC_Right_PGA_Adr);
	data |= 0xc0;
	adav803_write(codec, ADAV803_PLL_Source_Adr, data);

	/* ALC Control Register 1: Disable ALC */
	data = 0x00;
	adav803_write(codec, ADAV803_ALC_Ctrl1_Reg_Adr, data);

	/*  S/PDIF Loopback Control Register: Disable Loopback */
	data = adav803_read_reg_cache(codec, ADAV803_ADC_Right_PGA_Adr);
	data &= 0xfe;
	adav803_write(codec, ADAV803_SPDIF_In_Reg_Adr, data);

	adav803_add_controls(codec);
	adav803_add_widgets(codec);

	ret = snd_soc_register_card(socdev);
	if (ret < 0) {
		printk(KERN_ERR "adav803: failed to register card\n");
		goto card_err;
	}

	pr_info("ADAV803 Audio Codec Registered\n");

	return ret;

card_err:
	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
pcm_err:
	kfree(codec->reg_cache);
	return ret;
}

static struct snd_soc_device *adav803_socdev;

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)


static int adav803_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct snd_soc_device *socdev = adav803_socdev;
	struct snd_soc_codec *codec = socdev->codec;
	int ret;

	i2c_set_clientdata(i2c, codec);
	codec->control_data = i2c;

	ret = adav803_init(socdev);
	if (ret < 0)
		pr_err("failed to initialise ADAV803\n");

	return ret;
}

static int adav803_i2c_remove(struct i2c_client *client)
{
	struct snd_soc_codec *codec = i2c_get_clientdata(client);
	kfree(codec->reg_cache);
	return 0;
}

static const struct i2c_device_id adav803_i2c_id[] = {
	{ "adav803", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, adav803_i2c_id);

static struct i2c_driver adav803_i2c_driver = {
	.driver = {
		.name = "ADAV803 I2C Codec",
		.owner = THIS_MODULE,
	},
	.probe =    adav803_i2c_probe,
	.remove =   adav803_i2c_remove,
	.id_table = adav803_i2c_id,
};

static int adav803_add_i2c_device(struct platform_device *pdev,
				 const struct adav803_setup_data *setup)
{
	struct i2c_board_info info;
	struct i2c_adapter *adapter;
	struct i2c_client *client;
	int ret;

	ret = i2c_add_driver(&adav803_i2c_driver);
	if (ret != 0) {
		dev_err(&pdev->dev, "can't add i2c driver\n");
		return ret;
	}

	memset(&info, 0, sizeof(struct i2c_board_info));
	info.addr = setup->i2c_address;
	strlcpy(info.type, "adav803", I2C_NAME_SIZE);

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
	i2c_del_driver(&adav803_i2c_driver);
	return -ENODEV;
}
#endif

static int adav803_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct adav803_setup_data *setup;
	struct snd_soc_codec *codec;
	struct adav803_priv *adav803;
	int ret = 0;

	setup = socdev->codec_data;
	codec = kzalloc(sizeof(struct snd_soc_codec), GFP_KERNEL);
	if (codec == NULL)
		return -ENOMEM;

	adav803 = kzalloc(sizeof(struct adav803_priv), GFP_KERNEL);
	if (adav803 == NULL) {
		kfree(codec);
		return -ENOMEM;
	}

	codec->private_data = adav803;
	socdev->codec = codec;
	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);

	adav803_socdev = socdev;
	ret = -ENODEV;

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	if (setup->i2c_address) {
		ret = adav803_add_i2c_device(pdev, setup);
		if (ret != 0)
			printk(KERN_ERR "can't add i2c driver");
	}
#else
	/* Add other interfaces here */
#endif

	if (ret != 0) {
		kfree(codec->private_data);
		kfree(codec);
	}
	return ret;
}

/* power down chip */
static int adav803_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->codec;

	if (codec->control_data)
		adav803_set_bias_level(codec, SND_SOC_BIAS_OFF);

	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	i2c_unregister_device(codec->control_data);
	i2c_del_driver(&adav803_i2c_driver);
#endif
	kfree(codec->private_data);
	kfree(codec);

	return 0;
}

struct snd_soc_codec_device soc_codec_dev_adav803 = {
	.probe = 	adav803_probe,
	.remove = 	adav803_remove,
	.suspend = 	adav803_suspend,
	.resume =	adav803_resume,
};
EXPORT_SYMBOL_GPL(soc_codec_dev_adav803);

MODULE_AUTHOR("Cao Rongrong <rrcao@ambarella.com>");
MODULE_DESCRIPTION("ASoC ADAV803 driver");
MODULE_LICENSE("GPL");

