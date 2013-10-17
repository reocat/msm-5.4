/*
 * arch/arm/plat-ambarella/generic/pwm.c
 *
 * Author: Zhenwu Xue, <zwxue@ambarella.com>
 *
 * Copyright (C) 2004-2012, Ambarella, Inc.
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
#include <linux/pwm.h>
#include <linux/pwm_backlight.h>
#include <linux/proc_fs.h>
#include <linux/clk.h>

#include <asm/uaccess.h>

#include <mach/hardware.h>
#include <mach/board.h>
#include <plat/pwm.h>
#include <plat/clk.h>

/* ==========================================================================*/
#define PWM_DEFAULT_FREQUENCY   2200000
#define PWM_DEFAULT_DIVIDER	1

#define PWM_CMD_SIZE		(5)
#define PWM_MAX_INSTANCES	(4)
#define PWM_ARRAY_SIZE		(PWM_MAX_INSTANCES * PWM_CMD_SIZE)

/* ==========================================================================*/
static struct clk gclk_pwm = {
	.parent		= NULL,
	.name		= "gclk_pwm",
	.rate		= 0,
	.frac_mode	= 0,
	.ctrl_reg	= PLL_REG_UNAVAILABLE,
	.pres_reg	= PLL_REG_UNAVAILABLE,
	.post_reg	= CG_PWM_REG,
	.frac_reg	= PLL_REG_UNAVAILABLE,
	.ctrl2_reg	= PLL_REG_UNAVAILABLE,
	.ctrl3_reg	= PLL_REG_UNAVAILABLE,
	.lock_reg	= PLL_REG_UNAVAILABLE,
	.lock_bit	= 0,
	.divider	= 0,
	.max_divider	= (1 << 24) - 1,
	.extra_scaler	= 0,
	.ops		= &ambarella_rct_scaler_ops,
};

static struct clk *ambarella_pwm_register_clk(void)
{
	struct clk *pgclk_pwm = NULL;
	struct clk *pgclk_apb = NULL;

	pgclk_pwm = clk_get(NULL, "gclk_pwm");
	if (IS_ERR(pgclk_pwm)) {
		pgclk_apb = clk_get(NULL, "gclk_apb");
		if (IS_ERR(pgclk_apb)) {
			BUG();
		}
		gclk_pwm.parent = pgclk_apb;
		ambarella_clk_add(&gclk_pwm);
		pgclk_pwm = &gclk_pwm;
		pr_info("SYSCLK:PWM[%lu]\n", clk_get_rate(pgclk_pwm));
	}

	return pgclk_pwm;
}

static void ambarella_pwm_set_pll(u32 freq_hz)
{
	clk_set_rate(ambarella_pwm_register_clk(), freq_hz);
}

static u32 ambarella_pwm_get_pll(void)
{
	return clk_get_rate(ambarella_pwm_register_clk());
}

/*================================ PWM Device ================================*/
struct reg_bit_field {
	unsigned int		addr;
	unsigned int		msb;
	unsigned int		lsb;
};

#define SET_REG_BIT_FILED(reg, val)	\
{	\
	unsigned int		_reg, _tmp, _val;	\
	\
	_tmp = 0x1 << ((reg).msb - (reg).lsb + 1);	\
	_tmp = _tmp - 1;	\
	_val = ((val) & _tmp) << (reg).lsb;	\
	_tmp = _tmp << (reg).lsb;	\
	_reg = amba_readl((reg).addr);	\
	_reg &= (~_tmp);	\
	_reg |= _val;	\
	amba_writel((reg).addr, _reg);	\
}

typedef unsigned int (*get_pwm_clock_t)(void);
typedef void (*set_pwm_clock_t)(unsigned int);

struct ambarella_pwm_device {
	unsigned int		pwm_id;
	unsigned int		gpio_id;
	unsigned int		active_level;
	get_pwm_clock_t		get_clock;
	struct reg_bit_field	enable;
	struct reg_bit_field	divider;
	struct reg_bit_field	high;
	struct reg_bit_field	low;
	unsigned int		optional_clk_src;
	struct reg_bit_field	clock_source;
	unsigned int		optional_inversion;
	struct reg_bit_field	inversion_enable;
	unsigned int		use_count;
	const char		*label;
	struct list_head	node;
};

static struct ambarella_pwm_device ambarella_pwm0 = {
	.pwm_id		= 0,
	.gpio_id	= GPIO(16),
	.active_level	= 1,
	.get_clock	= ambarella_pwm_get_pll,
	.enable		= {
		.addr	= PWM_REG(PWM_ENABLE_OFFSET),
		.msb	= 0,
		.lsb	= 0,
	},
	.divider	= {
		.addr	= PWM_REG(PWM_MODE_OFFSET),
		.msb	= 10,
		.lsb	= 1,
	},
	.high		= {
		.addr	= PWM_REG(PWM_CONTROL_OFFSET),
		.msb	= 31,
		.lsb	= 16,
	},
	.low		= {
		.addr	= PWM_REG(PWM_CONTROL_OFFSET),
		.msb	= 15,
		.lsb	= 0,
	},
	.optional_clk_src	= 0,
	.optional_inversion	= 0,
	.use_count	= 0,
	.label		= NULL,
};

static struct ambarella_pwm_device ambarella_pwm1 = {
	.pwm_id		= 1,
	.gpio_id	= GPIO(45),
	.active_level	= 1,
	.get_clock	= ambarella_pwm_get_pll,
	.enable		= {
		.addr	= PWM_ST_REG(PWM_B0_ENABLE_OFFSET),
		.msb	= 0,
		.lsb	= 0,
	},
	.divider	= {
		.addr	= PWM_ST_REG(PWM_B0_ENABLE_OFFSET),
		.msb	= 10,
		.lsb	= 1,
	},
	.high		= {
		.addr	= PWM_ST_REG(PWM_B0_DATA1_OFFSET),
		.msb	= 19,
		.lsb	= 10,
	},
	.low		= {
		.addr	= PWM_ST_REG(PWM_B0_DATA1_OFFSET),
		.msb	= 9,
		.lsb	= 0,
	},
	.optional_clk_src	= 1,
	.clock_source	= {
		.addr	= PWM_ST_REG(PWM_B0_ENABLE_OFFSET),
		.msb	= 31,
		.lsb	= 31,
	},
	.optional_inversion	= 0,

	.use_count	= 0,
	.label		= NULL,
};

static struct ambarella_pwm_device ambarella_pwm2 = {
	.pwm_id		= 2,
	.gpio_id	= GPIO(46),
	.active_level	= 1,
	.get_clock	= ambarella_pwm_get_pll,
	.enable		= {
		.addr	= PWM_ST_REG(PWM_B1_ENABLE_OFFSET),
		.msb	= 0,
		.lsb	= 0,
	},
	.divider	= {
		.addr	= PWM_ST_REG(PWM_B1_ENABLE_OFFSET),
		.msb	= 10,
		.lsb	= 1,
	},
	.high		= {
		.addr	= PWM_ST_REG(PWM_B1_DATA1_OFFSET),
		.msb	= 19,
		.lsb	= 10,
	},
	.low		= {
		.addr	= PWM_ST_REG(PWM_B1_DATA1_OFFSET),
		.msb	= 9,
		.lsb	= 0,
	},
	.optional_clk_src	= 0,
	.optional_inversion	= 1,
	.inversion_enable	= {
		.addr	= PWM_ST_REG(PWM_B1_ENABLE_OFFSET),
		.msb	= 31,
		.lsb	= 31,
	},
	.use_count	= 0,
	.label		= NULL,
};

static struct ambarella_pwm_device ambarella_pwm3 = {
	.pwm_id		= 3,
	.gpio_id	= GPIO(50),
	.active_level	= 1,
	.get_clock	= ambarella_pwm_get_pll,
	.enable		= {
		.addr	= PWM_ST_REG(PWM_C0_ENABLE_OFFSET),
		.msb	= 0,
		.lsb	= 0,
	},
	.divider	= {
		.addr	= PWM_ST_REG(PWM_C0_ENABLE_OFFSET),
		.msb	= 10,
		.lsb	= 1,
	},
	.high		= {
		.addr	= PWM_ST_REG(PWM_C0_DATA1_OFFSET),
		.msb	= 19,
		.lsb	= 10,
	},
	.low		= {
		.addr	= PWM_ST_REG(PWM_C0_DATA1_OFFSET),
		.msb	= 9,
		.lsb	= 0,
	},
	.optional_clk_src	= 1,
	.clock_source	= {
		.addr	= PWM_ST_REG(PWM_C0_ENABLE_OFFSET),
		.msb	= 31,
		.lsb	= 31,
	},
	.optional_inversion	= 0,
	.use_count	= 0,
	.label		= NULL,
};

static struct ambarella_pwm_device ambarella_pwm4 = {
	.pwm_id		= 4,
	.gpio_id	= GPIO(51),
	.active_level	= 1,
	.get_clock	= ambarella_pwm_get_pll,
	.enable		= {
		.addr	= PWM_ST_REG(PWM_C1_ENABLE_OFFSET),
		.msb	= 0,
		.lsb	= 0,
	},
	.divider	= {
		.addr	= PWM_ST_REG(PWM_C1_ENABLE_OFFSET),
		.msb	= 10,
		.lsb	= 1,
	},
	.high		= {
		.addr	= PWM_ST_REG(PWM_C1_DATA1_OFFSET),
		.msb	= 19,
		.lsb	= 10,
	},
	.low		= {
		.addr	= PWM_ST_REG(PWM_C1_DATA1_OFFSET),
		.msb	= 9,
		.lsb	= 0,
	},
	.optional_clk_src	= 0,
	.optional_inversion	= 1,
	.inversion_enable	= {
		.addr	= PWM_ST_REG(PWM_C1_ENABLE_OFFSET),
		.msb	= 31,
		.lsb	= 31,
	},
	.use_count	= 0,
	.label		= NULL,
};

/*============================= PWM Backlight Device =========================*/
static int pwm_backlight_init(struct device *dev)
{
	int					retval = 0;
	struct platform_device			*pdev;
	struct platform_pwm_backlight_data	*data;

	pdev = container_of(dev, struct platform_device, dev);
	data = pdev->dev.platform_data;

	switch (pdev->id) {
	case 0:
		data->max_brightness		=
			ambarella_board_generic.pwm0_config.max_duty;
		data->dft_brightness		=
			ambarella_board_generic.pwm0_config.default_duty;
		data->pwm_period_ns		=
			ambarella_board_generic.pwm0_config.period_ns;
		ambarella_pwm0.active_level	=
			ambarella_board_generic.pwm0_config.active_level;
		break;

	case 1:
		data->max_brightness		=
			ambarella_board_generic.pwm1_config.max_duty;
		data->dft_brightness		=
			ambarella_board_generic.pwm1_config.default_duty;
		data->pwm_period_ns		=
			ambarella_board_generic.pwm1_config.period_ns;
		ambarella_pwm1.active_level	=
			ambarella_board_generic.pwm1_config.active_level;
		break;

	case 2:
		data->max_brightness		=
			ambarella_board_generic.pwm2_config.max_duty;
		data->dft_brightness		=
			ambarella_board_generic.pwm2_config.default_duty;
		data->pwm_period_ns		=
			ambarella_board_generic.pwm2_config.period_ns;
		ambarella_pwm2.active_level	=
			ambarella_board_generic.pwm2_config.active_level;
		break;

	case 3:
		data->max_brightness		=
			ambarella_board_generic.pwm3_config.max_duty;
		data->dft_brightness		=
			ambarella_board_generic.pwm3_config.default_duty;
		data->pwm_period_ns		=
			ambarella_board_generic.pwm3_config.period_ns;
		ambarella_pwm3.active_level	=
			ambarella_board_generic.pwm3_config.active_level;
		break;

	case 4:
		data->max_brightness		=
			ambarella_board_generic.pwm4_config.max_duty;
		data->dft_brightness		=
			ambarella_board_generic.pwm4_config.default_duty;
		data->pwm_period_ns		=
			ambarella_board_generic.pwm4_config.period_ns;
		ambarella_pwm4.active_level	=
			ambarella_board_generic.pwm4_config.active_level;
		break;

	default:
		retval = -EINVAL;
		break;
	}

	return retval;
}

static struct platform_pwm_backlight_data amb_pwm0_pdata = {
	.pwm_id		= 0,
	.max_brightness	= 255,
	.dft_brightness	= 255,
	.pwm_period_ns	= 40000,
	.init		= pwm_backlight_init,
	.notify		= NULL,
	.exit		= NULL,
};

struct platform_device ambarella_pwm_platform_device0 = {
	.name		= "pwm-backlight",
	.id		= 0,
	.resource	= NULL,
	.num_resources	= 0,
	.dev		= {
		.platform_data		= &amb_pwm0_pdata,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};

static struct platform_pwm_backlight_data amb_pwm1_pdata = {
	.pwm_id		= 1,
	.max_brightness	= 100,
	.dft_brightness	= 100,
	.pwm_period_ns	= 10000,
	.init		= pwm_backlight_init,
	.notify		= NULL,
	.exit		= NULL,
};

struct platform_device ambarella_pwm_platform_device1 = {
	.name		= "pwm-backlight",
	.id		= 1,
	.resource	= NULL,
	.num_resources	= 0,
	.dev		= {
		.platform_data		= &amb_pwm1_pdata,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};

static struct platform_pwm_backlight_data amb_pwm2_pdata = {
	.pwm_id		= 2,
	.max_brightness	= 100,
	.dft_brightness	= 100,
	.pwm_period_ns	= 10000,
	.init		= pwm_backlight_init,
	.notify		= NULL,
	.exit		= NULL,
};

struct platform_device ambarella_pwm_platform_device2 = {
	.name		= "pwm-backlight",
	.id		= 2,
	.resource	= NULL,
	.num_resources	= 0,
	.dev		= {
		.platform_data		= &amb_pwm2_pdata,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};

static struct platform_pwm_backlight_data amb_pwm3_pdata = {
	.pwm_id		= 3,
	.max_brightness	= 100,
	.dft_brightness	= 100,
	.pwm_period_ns	= 10000,
	.init		= pwm_backlight_init,
	.notify		= NULL,
	.exit		= NULL,
};

struct platform_device ambarella_pwm_platform_device3 = {
	.name		= "pwm-backlight",
	.id		= 3,
	.resource	= NULL,
	.num_resources	= 0,
	.dev		= {
		.platform_data		= &amb_pwm3_pdata,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};

static struct platform_pwm_backlight_data amb_pwm4_pdata = {
	.pwm_id		= 4,
	.max_brightness	= 100,
	.dft_brightness	= 100,
	.pwm_period_ns	= 10000,
	.init		= pwm_backlight_init,
	.notify		= NULL,
	.exit		= NULL,
};

struct platform_device ambarella_pwm_platform_device4 = {
	.name		= "pwm-backlight",
	.id		= 4,
	.resource	= NULL,
	.num_resources	= 0,
	.dev		= {
		.platform_data		= &amb_pwm4_pdata,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};

/*============================= PWM Backlight Ops =========================*/
int ambarella_pwm_request(struct pwm_chip *chip, struct pwm_device *pwm)
{
	switch (pwm->hwpwm) {
	case 0:
		pwm->chip_data = &ambarella_pwm0;
		break;

	case 1:
		pwm->chip_data = &ambarella_pwm1;
		break;

	case 2:
		pwm->chip_data = &ambarella_pwm2;
		break;

	case 3:
		pwm->chip_data = &ambarella_pwm3;
		break;

	default:
		pwm->chip_data = &ambarella_pwm4;
		break;
	}

	return 0;
}

void ambarella_pwm_free(struct pwm_chip *chip, struct pwm_device *pwm)
{
	return;
}

int ambarella_pwm_config(struct pwm_chip *chip, struct pwm_device *_pwm,
		int duty_ns, int period_ns)
{
	struct ambarella_pwm_device	*pwm;
	int				retval = 0;
	unsigned int			clock, on, off;

	pwm = (struct ambarella_pwm_device *)_pwm->chip_data;
	if (!pwm->get_clock) {
		retval = -EINVAL;
		printk("%s: Can not get pwm clock!\n", __func__);
		goto pwm_config_exit;
	}

	if (pwm->optional_clk_src == 1) {
		SET_REG_BIT_FILED(pwm->clock_source, 1);
	}
	if (pwm->optional_inversion == 1) {
		SET_REG_BIT_FILED(pwm->inversion_enable, 0);
	}

	clock = (pwm->get_clock() + 50000) / 100000;
	SET_REG_BIT_FILED(pwm->divider, PWM_DEFAULT_DIVIDER - 1);

	on = (clock * duty_ns + 5000) / 10000;
	off = (clock * (period_ns - duty_ns) + 5000) / 10000;
	if (on == 0)
		on = 1;
	if (off == 0)
		off = 1;
	if (pwm->active_level) {
		SET_REG_BIT_FILED(pwm->high, on - 1);
		SET_REG_BIT_FILED(pwm->low, off - 1);
	} else {
		SET_REG_BIT_FILED(pwm->high, off - 1);
		SET_REG_BIT_FILED(pwm->low, on - 1);
	}

pwm_config_exit:
	return retval;
}

int ambarella_pwm_enable(struct pwm_chip *chip, struct pwm_device *_pwm)
{
	struct ambarella_pwm_device *pwm;

	pwm = (struct ambarella_pwm_device *)_pwm->chip_data;
	SET_REG_BIT_FILED(pwm->enable, 1);
	ambarella_gpio_config(pwm->gpio_id, GPIO_FUNC_HW);

	return 0;
}

void ambarella_pwm_disable(struct pwm_chip *chip, struct pwm_device *_pwm)
{
	struct ambarella_pwm_device *pwm;

	pwm = (struct ambarella_pwm_device *)_pwm->chip_data;
	SET_REG_BIT_FILED(pwm->enable, 0);
	ambarella_gpio_config(pwm->gpio_id, GPIO_FUNC_SW_OUTPUT);
	if (pwm->active_level) {
		ambarella_gpio_set(pwm->gpio_id, GPIO_LOW);
	} else {
		ambarella_gpio_set(pwm->gpio_id, GPIO_HIGH);
	}
}

static struct pwm_chip ambarella_pwm_chip;
static struct pwm_ops ambarella_pwm_ops = {
	.request	= ambarella_pwm_request,
	.free		= ambarella_pwm_free,
	.config		= ambarella_pwm_config,
	.enable		= ambarella_pwm_enable,
	.disable	= ambarella_pwm_disable,
};

static struct pwm_lookup ambarella_pwm_lut[] = {
	PWM_LOOKUP("pwm-backlight.0", 0, "pwm-backlight.0", NULL),
	PWM_LOOKUP("pwm-backlight.0", 1, "pwm-backlight.1", NULL),
	PWM_LOOKUP("pwm-backlight.0", 2, "pwm-backlight.2", NULL),
	PWM_LOOKUP("pwm-backlight.0", 3, "pwm-backlight.3", NULL),
	PWM_LOOKUP("pwm-backlight.0", 4, "pwm-backlight.4", NULL),
};

int __init ambarella_init_pwm(void)
{
	int retval = 0;

	ambarella_pwm_set_pll(PWM_DEFAULT_FREQUENCY);
	ambarella_pwm_chip.dev	= &ambarella_pwm_platform_device0.dev;
	ambarella_pwm_chip.ops	= &ambarella_pwm_ops;
	ambarella_pwm_chip.base	= 0;
	ambarella_pwm_chip.npwm	= 5;
	pwmchip_add(&ambarella_pwm_chip);
	pwm_add_table(ambarella_pwm_lut, ARRAY_SIZE(ambarella_pwm_lut));

	return retval;
}

