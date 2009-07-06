/*
 * cs24l51.c  --  CS24L51 ALSA Soc Audio driver
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

#include "cs24l51.h"

/* codec private data */
struct cs24l51_priv {
	unsigned int mode;
	unsigned int mclk;
};


/*
 * We read all of the CS24L51 registers to pre-fill the register cache
 */
static int cs24l51_fill_cache(struct snd_soc_codec *codec)
{
	struct i2c_client *client = codec->control_data;
	u8 *cache = codec->reg_cache;
	s32 val, i;

	for(i = 1; i < CS24L51_NUMREGS; i++){
		val = i2c_smbus_read_byte_data(client, i);
		if(val < 0){
			pr_err("CS24L51: I2C read error: address = 0x%x\n", i);
			return -EIO;
		}
		
		cache[i] = val;
	}

	return 0;
}

/*
 * Read from the CS24L51 register cache.
 *
 * This CS24L51 registers are cached to avoid excessive I2C I/O operations.
 * After the initial read to pre-fill the cache, the CS24L51 never updates
 * the register values, so we won't have a cache coherncy problem.
 */
static inline unsigned int cs24l51_read_reg_cache(struct snd_soc_codec *codec,
	unsigned int reg)
{
	u8 *cache = codec->reg_cache;

	if (reg < 1 || reg >= CS24L51_NUMREGS)
		return -EIO;

	return cache[reg];
}

/*
 * Write to a CS24L51 register via the I2C bus.
 *
 * This function writes the given value to the given CS24L51 register, and
 * also updates the register cache.
 *
 * Note that we don't use the hw_write function pointer of snd_soc_codec.
 * That's because it's too clunky: the hw_write_t prototype does not match
 * i2c_smbus_write_byte_data(), and it's just another layer of overhead.
 */
static int cs24l51_write(struct snd_soc_codec *codec,
		unsigned int reg, unsigned int value)
{
	u8 *cache = codec->reg_cache;

	if (reg < 1 || reg >= CS24L51_NUMREGS)
		return -EIO;

	/* Only perform an I2C operation if the new value is different */
	if (cache[reg] != value) {
		struct i2c_client *client = codec->control_data;
		if (i2c_smbus_write_byte_data(client, reg, value)) {
			pr_err("CS24L51: I2C write failed\n");
			return -EIO;
		}

		/* We've written to the hardware, so update the cache */
		cache[reg] = value;
	}

	return 0;
}

static int cs24l51_setbits(struct snd_soc_codec *codec,
		unsigned int reg, unsigned int mask)
{
	u8 value;

	value = snd_soc_read(codec, reg);
	value |= mask;

	return snd_soc_write(codec, reg, value);
}

static int cs24l51_clrbits(struct snd_soc_codec *codec,
		unsigned int reg, unsigned int mask)
{
	u8 value;

	value = snd_soc_read(codec, reg);
	value &= (~mask);

	return snd_soc_write(codec, reg, value);
}

static void cs24l51_dump_reg(struct snd_soc_codec *codec)
{
	int i, val;

	for(i = 1; i < CS24L51_NUMREGS; i++) {
		val = cs24l51_read_reg_cache(codec, i);
		pr_debug("0x%02x:0x%02x    ", i, val);
		if(i % 8 == 0)
			printk("\n");
	}

}


static const struct snd_kcontrol_new cs24l51_snd_controls[] = {
};


/* add non dapm controls */
static int cs24l51_add_controls(struct snd_soc_codec *codec)
{
	struct snd_kcontrol *snd_kctl;
	int err, i;

	for (i = 0; i < ARRAY_SIZE(cs24l51_snd_controls); i++) {
		snd_kctl = snd_soc_cnew(&cs24l51_snd_controls[i], codec, NULL);
		err = snd_ctl_add(codec->card, snd_kctl);
		if (err < 0)
			return err;
	}

	return 0;
}


static const struct snd_soc_dapm_widget cs24l51_dapm_widgets[] = {
	/* Left DAC to Left Outputs */
	SND_SOC_DAPM_DAC("Left DAC", "Left Playback", CS24L51_PWRCTL1_REG, 5, 1),
	SND_SOC_DAPM_OUTPUT("LLOUT"),

	/* Right DAC to Right Outputs */
	SND_SOC_DAPM_OUTPUT("RLOUT"),
	SND_SOC_DAPM_DAC("Right DAC", "Right Playback", CS24L51_PWRCTL1_REG, 6, 1),

	/* Left Input to Left ADC */
	SND_SOC_DAPM_INPUT("LLIN"),
	SND_SOC_DAPM_PGA("Left PGA", CS24L51_PWRCTL1_REG, 3, 1, NULL, 0),
	SND_SOC_DAPM_ADC("Left ADC", "Left Capture", CS24L51_PWRCTL1_REG, 1, 1),

	/* Right Input to Right ADC */
	SND_SOC_DAPM_INPUT("RLIN"),
	SND_SOC_DAPM_PGA("Right PGA", CS24L51_PWRCTL1_REG, 4, 1, NULL, 0),
	SND_SOC_DAPM_ADC("Right ADC", "Right Capture", CS24L51_PWRCTL1_REG, 2, 1),
};

static const struct snd_soc_dapm_route intercon[] = {
	/* outputs */
	{"LLOUT", NULL, "Left DAC"},
	{"RLOUT", NULL, "Right DAC"},

	/* inputs */
	{"Left PGA", NULL, "LLIN"},
	{"Right PGA", NULL, "RLIN"},
	{"Left ADC", NULL, "Left PGA"},
	{"Right ADC", NULL, "Right PGA"},
};

static int cs24l51_add_widgets(struct snd_soc_codec *codec)
{
	snd_soc_dapm_new_controls(codec, cs24l51_dapm_widgets,
				  ARRAY_SIZE(cs24l51_dapm_widgets));

	snd_soc_dapm_add_routes(codec, intercon, ARRAY_SIZE(intercon));

	snd_soc_dapm_new_widgets(codec);
	return 0;
}

static int cs24l51_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params,
			struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec = socdev->codec;
	struct cs24l51_priv *cs24l51 = codec->private_data;
	u32 sfreq;

	/* Freeze register setting */
	cs24l51_setbits(codec, CS24L51_DAC_CTL_REG, CS24L51_FREEZE_ENABLE);

	sfreq = params_rate(params);	/* Sampling rate, in Hz */
	/* Set speed */
	cs24l51_clrbits(codec, CS24L51_MICPWR_SPDCTL_REG, CS24L51_SPEED_MASK);
	switch (sfreq) {
	case 96000:
	case 88200:
	case 64000:
		cs24l51_setbits(codec, CS24L51_MICPWR_SPDCTL_REG, CS24L51_SPEED_DSM);
		break;
	case 48000:
	case 44100:
	case 32000:
		cs24l51_setbits(codec, CS24L51_MICPWR_SPDCTL_REG, CS24L51_SPEED_SSM);
		break;
	case 24000:
	case 22050:
	case 16000:
		cs24l51_setbits(codec, CS24L51_MICPWR_SPDCTL_REG, CS24L51_SPEED_HSM);
		break;
	case 12000:
	case 11025:
	case 8000:
		cs24l51_setbits(codec, CS24L51_MICPWR_SPDCTL_REG, CS24L51_SPEED_QSM);
		break;
	default:
		pr_err("CS24L51: unknown rate\n");
		return -EINVAL;
	}

	/* Set interface format */
	cs24l51_clrbits(codec, CS24L51_INTFCTL_REG, CS24L51_INTF_DACDIF_MASK);
	switch (cs24l51->mode) {
	case SND_SOC_DAIFMT_I2S:
		cs24l51_setbits(codec, CS24L51_INTFCTL_REG, CS24L51_INTF_DACDIF_I2S);
		cs24l51_setbits(codec, CS24L51_INTFCTL_REG, CS24L51_INTF_ADCDIF_I2S);
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		cs24l51_setbits(codec, CS24L51_INTFCTL_REG, CS24L51_INTF_DACDIF_LJ24);
		cs24l51_clrbits(codec, CS24L51_INTFCTL_REG, CS24L51_INTF_ADCDIF_I2S);
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		pr_err("CS24L51: CS24L51 support SND_SOC_DAIFMT_RIGHT_J, "
			"but we don't implement it yet\n");
	default:
		pr_err("CS24L51: unknown format\n");
		return -EINVAL;
	}

	/*De-freeze register setting now */
	cs24l51_clrbits(codec, CS24L51_DAC_CTL_REG, CS24L51_FREEZE_ENABLE);

	cs24l51_dump_reg(codec);

	return 0;
}


static int cs24l51_mute(struct snd_soc_dai *dai, int mute)
{
#if 0
	struct snd_soc_codec *codec = dai->codec;
	u16 mute_reg = cs24l51_read_reg_cache(codec, CS24L51_DAC_Ctrl1_Reg_Adr);

	if (mute)
		cs24l51_write(codec, CS24L51_DAC_Ctrl1_Reg_Adr, mute_reg & 0xfc);
	else
		cs24l51_write(codec, CS24L51_DAC_Ctrl1_Reg_Adr, mute_reg | 0x03);
#endif
	return 0;
}

static int cs24l51_set_dai_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct cs24l51_priv *cs24l51 = codec->private_data;

	switch (clk_id) {
	case CS24L51_SYSCLK:
		cs24l51->mclk = freq;
		break;
	default:
		pr_err("CLK SOURCE (%d) is not supported yet\n", clk_id);
		return -EINVAL;
	}

	return 0;
}

static int cs24l51_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct cs24l51_priv *cs24l51 = codec->private_data;

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		cs24l51_clrbits(codec, CS24L51_INTFCTL_REG, CS24L51_INTF_MASTER);
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_LEFT_J:
	case SND_SOC_DAIFMT_RIGHT_J:
	case SND_SOC_DAIFMT_I2S:
		cs24l51->mode = fmt & SND_SOC_DAIFMT_FORMAT_MASK;
		break;
	default:
		pr_err("CS24L51: invalid DAI format\n");
		return -EINVAL;
	}

	return 0;
}

static int cs24l51_set_bias_level(struct snd_soc_codec *codec,
				 enum snd_soc_bias_level level)
{
#if 0
	u8 reg;

	switch (level) {
	case SND_SOC_BIAS_ON:
		/* everything on, dac unmute */
		cs24l51_clrbits(codec, CS24L51_PWRCTL1_REG, CS24L51_PWRCTL_PDN);
		break;

	case SND_SOC_BIAS_PREPARE:
		break;

	case SND_SOC_BIAS_STANDBY:
	case SND_SOC_BIAS_OFF:
		/* everything off, dac mute */
		cs24l51_setbits(codec, CS24L51_PWRCTL1_REG, CS24L51_PWRCTL_PDN);
		break;
	}
#endif
	codec->bias_level = level;
	return 0;
}

#define CS24L51_RATES SNDRV_PCM_RATE_8000_48000
#define CS24L51_FORMATS SNDRV_PCM_FMTBIT_S16_LE

struct snd_soc_dai cs24l51_dai = {
	.name = "CS24L51",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = CS24L51_RATES,
		.formats = CS24L51_FORMATS,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = CS24L51_RATES,
		.formats = CS24L51_FORMATS,
	},
	.ops = {
		.hw_params = cs24l51_hw_params,
		.digital_mute = cs24l51_mute,
		.set_sysclk = cs24l51_set_dai_sysclk,
		.set_fmt = cs24l51_set_dai_fmt,
	},
};
EXPORT_SYMBOL_GPL(cs24l51_dai);

static int cs24l51_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->codec;

	cs24l51_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int cs24l51_resume(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->codec;
	u8 *cache = codec->reg_cache;
	int i;

	/* Sync reg_cache with the hardware */
	for (i = 1; i < CS24L51_NUMREGS; i++) {
		cs24l51_write(codec, i, cache[i]);
	}
	cs24l51_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	cs24l51_set_bias_level(codec, codec->suspend_bias_level);

	return 0;
}

/*
 * initialise the CS24L51 driver
 * register the mixer and dsp interfaces with the kernel
 */
static int cs24l51_init(struct snd_soc_device *socdev)
{
	struct snd_soc_codec *codec = socdev->codec;
	struct i2c_client *client = codec->control_data;
	struct cs24l51_setup_data *setup = socdev->codec_data;
	int ret = -ENODEV, data;

	codec->name = "CS24L51";
	codec->owner = THIS_MODULE;
	codec->reg_cache_size = CS24L51_NUMREGS;
	codec->reg_cache = kzalloc(CS24L51_NUMREGS, GFP_KERNEL);
	if (!codec->reg_cache) {
		pr_err("cs24l51: could not allocate register cache\n");
		return -ENOMEM;
	}
	codec->read = cs24l51_read_reg_cache;
	codec->write = cs24l51_write;
	codec->set_bias_level = cs24l51_set_bias_level;
	codec->dai = &cs24l51_dai;
	codec->num_dai = 1;

	/* Reset CS24L51 codec */
	gpio_direction_output(setup->rst_pin, GPIO_LOW);
	msleep(setup->rst_delay);
	gpio_direction_output(setup->rst_pin, GPIO_HIGH);

	/* Verify that we have a CS24L51 codec */
	data = i2c_smbus_read_byte_data(client, CS24L51_CHIPID_REG);
	if (data < 0) {
		pr_err("CS24L51: failed to read I2C\n");
		goto init_err;
	}
	/* The top 5 bits of the chip ID should be 11011. */
	if ((data & CS24L51_CHIPID_ID) != 0xD8) {
		/* The device at this address is not a CS24L51 codec */
		pr_err("CS24L51: The device at this address is not a CS24L51 codec\n");
		goto init_err;
	}

	pr_info("Found CS24L51 at I2C address %X\n", client->addr);
	pr_info("CS24L51 hardware revision %X\n", data & CS24L51_CHIPID_REV);

	/* register pcms */
	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0) {
		pr_err("cs24l51: failed to create pcms\n");
		goto init_err;
	}

	/* power on device */
	cs24l51_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	/* fill the reg cache for read afterwards */
	cs24l51_fill_cache(codec);

	/* Initial CS24L51 codec  */
	cs24l51_setbits(codec, CS24L51_PWRCTL1_REG, CS24L51_PWRCTL_PDN);
	/* Disable auto-detect speed mode */
	cs24l51_clrbits(codec, CS24L51_MICPWR_SPDCTL_REG, CS24L51_AUTO_ENABLE);
	/* Set CS24L51 into SLAVE mode */
	cs24l51_clrbits(codec, CS24L51_INTFCTL_REG, CS24L51_INTF_MASTER);
	/* Power on CS24L51 codec  */
	cs24l51_clrbits(codec, CS24L51_PWRCTL1_REG, CS24L51_PWRCTL_PDN);

	cs24l51_add_controls(codec);
	cs24l51_add_widgets(codec);

	ret = snd_soc_init_card(socdev);
	if (ret < 0) {
		pr_err("cs24l51: failed to register card\n");
		goto card_err;
	}

	pr_info("CS24L51 Audio Codec Registered\n");

	return ret;

card_err:
	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
init_err:
	kfree(codec->reg_cache);
	return ret;
}

static struct snd_soc_device *cs24l51_socdev;

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)


static int cs24l51_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct snd_soc_device *socdev = cs24l51_socdev;
	struct snd_soc_codec *codec = socdev->codec;
	int ret;

	i2c_set_clientdata(i2c, codec);
	codec->control_data = i2c;

	ret = cs24l51_init(socdev);
	if (ret < 0)
		pr_err("failed to initialise CS24L51\n");

	return ret;
}

static int cs24l51_i2c_remove(struct i2c_client *client)
{
	struct snd_soc_codec *codec = i2c_get_clientdata(client);
	kfree(codec->reg_cache);
	return 0;
}

static const struct i2c_device_id cs24l51_i2c_id[] = {
	{ "cs24l51", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, cs24l51_i2c_id);

static struct i2c_driver cs24l51_i2c_driver = {
	.driver = {
		.name = "CS24L51 I2C Codec",
		.owner = THIS_MODULE,
	},
	.probe =    cs24l51_i2c_probe,
	.remove =   cs24l51_i2c_remove,
	.id_table = cs24l51_i2c_id,
};

static int cs24l51_add_i2c_device(struct platform_device *pdev,
				 const struct cs24l51_setup_data *setup)
{
	struct i2c_board_info info;
	struct i2c_adapter *adapter;
	struct i2c_client *client;
	int ret;

	ret = i2c_add_driver(&cs24l51_i2c_driver);
	if (ret != 0) {
		dev_err(&pdev->dev, "can't add i2c driver\n");
		return ret;
	}

	memset(&info, 0, sizeof(struct i2c_board_info));
	info.addr = setup->i2c_address;
	strlcpy(info.type, "cs24l51", I2C_NAME_SIZE);

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
	i2c_del_driver(&cs24l51_i2c_driver);
	return -ENODEV;
}
#endif

static int cs24l51_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct cs24l51_setup_data *setup;
	struct snd_soc_codec *codec;
	struct cs24l51_priv *cs24l51;
	int ret = -ENOMEM;

	setup = socdev->codec_data;
	codec = kzalloc(sizeof(struct snd_soc_codec), GFP_KERNEL);
	if (codec == NULL)
		goto exit;

	cs24l51 = kzalloc(sizeof(struct cs24l51_priv), GFP_KERNEL);
	if (cs24l51 == NULL)
		goto priv_alloc_err;

	codec->private_data = cs24l51;
	socdev->codec = codec;
	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);

	ret = -ENODEV;
	if (setup->rst_pin == 0)
		goto gpio_requst_err;

	ret = gpio_request(setup->rst_pin, "codec-reset");
	if (ret < 0) {
		pr_err("Request codec-reset GPIO(%d) failed\n", setup->rst_pin);
		goto gpio_requst_err;
	}

	cs24l51_socdev = socdev;

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	if (setup->i2c_address) {
		ret = cs24l51_add_i2c_device(pdev, setup);
		if (ret != 0) {
			pr_err("can't add i2c driver");
			goto add_i2c_err;
		}
	}
#else
	/* Add other interfaces here */
#endif

	return 0;

add_i2c_err:
	gpio_free(setup->rst_pin);
gpio_requst_err:
	kfree(codec->private_data);
priv_alloc_err:
	kfree(codec);
exit:
	return ret;
}

/* power down chip */
static int cs24l51_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->codec;
	struct cs24l51_setup_data *setup = socdev->codec_data;

	if (codec->control_data)
		cs24l51_set_bias_level(codec, SND_SOC_BIAS_OFF);

	gpio_free(setup->rst_pin);

	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	i2c_unregister_device(codec->control_data);
	i2c_del_driver(&cs24l51_i2c_driver);
#endif
	if (codec->private_data)
		kfree(codec->private_data);
	if (codec)
		kfree(codec);

	return 0;
}

struct snd_soc_codec_device soc_codec_dev_cs24l51 = {
	.probe = 	cs24l51_probe,
	.remove = 	cs24l51_remove,
	.suspend = 	cs24l51_suspend,
	.resume =	cs24l51_resume,
};
EXPORT_SYMBOL_GPL(soc_codec_dev_cs24l51);

static int __init ambarella_cs24l51_init(void)
{
	return snd_soc_register_dai(&cs24l51_dai);
}
module_init(ambarella_cs24l51_init);

static void __exit ambarella_cs24l51_exit(void)
{
	snd_soc_unregister_dai(&cs24l51_dai);
}
module_exit(ambarella_cs24l51_exit);

MODULE_AUTHOR("Cao Rongrong <rrcao@ambarella.com>");
MODULE_DESCRIPTION("ASoC CS24L51 codec driver");
MODULE_LICENSE("GPL");

