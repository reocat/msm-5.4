/*
 * drivers/pinctrl/ambarella/pinctrl-amb.c
 *
 * History:
 *	2013/12/18 - [Cao Rongrong] created file
 *
 * Copyright (C) 2012-2016, Ambarella, Inc.
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
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/interrupt.h>
#include <linux/irqdomain.h>
#include <linux/of_irq.h>
#include <plat/pinctrl.h>

#define PIN_NAME_LENGTH		10

//#define GROUP_SUFFIX		"-grp"
//#define GSUFFIX_LEN		sizeof(GROUP_SUFFIX)
//#define FUNCTION_SUFFIX		"-mux"
//#define FSUFFIX_LEN		sizeof(FUNCTION_SUFFIX)

/*
 * pin configuration type and its value are packed together into a 16-bits.
 * The upper 8-bits represent the configuration type and the lower 8-bits
 * hold the value of the configuration type.
 */
//#define PINCFG_TYPE_MASK		0xFF
//#define PINCFG_VALUE_SHIFT		8
//#define PINCFG_VALUE_MASK		(0xFF << PINCFG_VALUE_SHIFT)
//#define PINCFG_PACK(type, value)	(((value) << PINCFG_VALUE_SHIFT) | type)

/**
 * enum pincfg_type - possible pin configuration types supported.
 * @PINCFG_TYPE_PUD: Pull up/down configuration.
 * @PINCFG_TYPE_DRV: Drive strength configuration.
 */
//enum pincfg_type {
//	PINCFG_TYPE_PUD,
//	PINCFG_TYPE_DRV,
//};

/**
 * struct amb_pin_group: represent group of pins for pincfg setting.
 * @name: name of the pin group, used to lookup the group.
 * @pins: the pins included in this group.
 * @num_pins: number of pins included in this group.
 * @altfunc: the altfunc to apply to all pins in this group.
 * @multi_alt: if the pins of this group have multi altfuncs, calling
 *             extra function to set it.
 */
struct amb_pin_group {
	const char		*name;
	const unsigned int	*pins;
	u8			num_pins;
	int			altfunc;
	int			multi_alt;
};


/**
 * struct amb_pmx_func: represent a pin function.
 * @name: name of the pin function, used to lookup the function.
 * @groups: one or more names of pin groups that provide this function.
 * @num_groups: number of groups included in @groups.
 * @function: the function number to be programmed when selected.
 */
struct amb_pmx_func {
	const char		*name;
	const char		**groups;
	u8			num_groups;
	unsigned long		function;
};

/**
 * struct amb_pinctrl_priv_data: driver's private runtime data.
 * @regbase: ioremapped based address of the register space.
 * @gc: gpio chip registered with gpiolib.
 * @pin_groups: list of pin groups parsed from device tree.
 * @nr_groups: number of pin groups available.
 * @pmx_functions: list of pin functions parsed from device tree.
 * @nr_functions: number of pin functions available.
 */
struct amb_pinctrl_priv_data {
	struct device			*dev;
	struct ambarella_platform_pinctrl *plat_data;
	void __iomem			*regbase[GPIO_INSTANCES];
	struct gpio_chip		*gc;
	struct pinctrl_gpio_range	grange;
	//struct irq_domain		*irq_domain;
	unsigned long			used[BITS_TO_LONGS(AMBGPIO_SIZE)];

	const struct amb_pin_group	*pin_groups;
	unsigned int			nr_groups;
	const struct amb_pmx_func	*pmx_functions;
	unsigned int			nr_functions;
};


/* list of all possible config options supported */
//static struct pin_config {
//	char		*prop_cfg;
//	unsigned int	cfg_type;
//} pcfgs[] = {
//	{ "ambarella,amb-pin-pud", PINCFG_TYPE_PUD },
//	{ "ambarella,amb-pin-drv", PINCFG_TYPE_DRV },
//};

//static const unsigned idc0_pins[] = {0, 1};
//static const unsigned idc2_pins[] = {86, 87};
//static const unsigned idc3_pins[] = {36, 37};
//static const unsigned ssi0_pins[] = {2, 3, 4};
//static const unsigned ssi2_pins[] = {88, 89, 90};
//static const unsigned echi_pins[] = {7, 9};
static const unsigned i2s0_pins[] = {77, 78, 79, 80, 81};
//static const unsigned i2s1_pins[] = {8, 124, 125, 126, 127};
//static const unsigned uart0_pins[] = {14, 15};
//static const unsigned uart1_pins[] = {52, 53};
//static const unsigned uart1_ctsrts_pins[] = {50, 51};
//static const unsigned uart2_pins[] = {10, 18};
//static const unsigned uart3_pins[] = {19, 20};
//static const unsigned pwm0_pins[] = {16};
//static const unsigned pwm1_pins[] = {45};
//static const unsigned pwm2_pins[] = {46};
//static const unsigned pwm3_pins[] = {50};
//static const unsigned pwm4_pins[] = {51};
//static const unsigned sd0_pins[] = {22, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76};
//static const unsigned sdio0_pins[] = {23, };	////
//static const unsigned ir0_pins[] = {35};
//static const unsigned nand0_wp_pins[] = {39};

static const struct amb_pin_group amb_pin_groups[] = {
	{
		.name = "i2s0-grp",
		.pins = i2s0_pins,
		.num_pins = ARRAY_SIZE(i2s0_pins),
		.altfunc = AMB_ALTFUNC_HW_1,
		.multi_alt = 1,
	},
};


static const char * i2s0grps[] = {"i2s0-grp"};

static const struct amb_pmx_func amb_pmx_functions[] = {
	{
		.name = "i2s0",
		.groups = i2s0grps,
		.num_groups = ARRAY_SIZE(i2s0grps),
	},
};

/* check if the selector is a valid pin group selector */
static int amb_get_group_count(struct pinctrl_dev *pctldev)
{
	struct amb_pinctrl_priv_data *priv;

	priv = pinctrl_dev_get_drvdata(pctldev);
	return priv->nr_groups;
}

/* return the name of the group selected by the group selector */
static const char *amb_get_group_name(struct pinctrl_dev *pctldev,
						unsigned selector)
{
	struct amb_pinctrl_priv_data *priv;

	priv = pinctrl_dev_get_drvdata(pctldev);
	return priv->pin_groups[selector].name;
}

/* return the pin numbers associated with the specified group */
static int amb_get_group_pins(struct pinctrl_dev *pctldev,
		unsigned selector, const unsigned **pins, unsigned *num_pins)
{
	struct amb_pinctrl_priv_data *priv;

	priv = pinctrl_dev_get_drvdata(pctldev);
	*pins = priv->pin_groups[selector].pins;
	*num_pins = priv->pin_groups[selector].num_pins;
	return 0;
}

/* create pinctrl_map entries by parsing device tree nodes */
//static int amb_dt_node_to_map(struct pinctrl_dev *pctldev,
//			struct device_node *np, struct pinctrl_map **maps,
//			unsigned *nmaps)
//{
//	struct device *dev = pctldev->dev;
//	struct pinctrl_map *map;
//	unsigned long *cfg = NULL;
//	char *gname, *fname;
//	int cfg_cnt = 0, map_cnt = 0, idx = 0;

//	/* count the number of config options specfied in the node */
//	for (idx = 0; idx < ARRAY_SIZE(pcfgs); idx++)
//		if (of_find_property(np, pcfgs[idx].prop_cfg, NULL))
//			cfg_cnt++;

//	/*
//	 * Find out the number of map entries to create. All the config options
//	 * can be accomadated into a single config map entry.
//	 */
//	if (cfg_cnt)
//		map_cnt = 1;
//	if (of_find_property(np, "ambarella,amb-pin-function", NULL))
//		map_cnt++;
//	if (!map_cnt) {
//		dev_err(dev, "node %s does not have either config or function "
//				"configurations\n", np->name);
//		return -EINVAL;
//	}

//	/* Allocate memory for pin-map entries */
//	map = kzalloc(sizeof(*map) * map_cnt, GFP_KERNEL);
//	if (!map) {
//		dev_err(dev, "could not alloc memory for pin-maps\n");
//		return -ENOMEM;
//	}
//	*nmaps = 0;

//	/*
//	 * Allocate memory for pin group name. The pin group name is derived
//	 * from the node name from which these map entries are be created.
//	 */
//	gname = kzalloc(strlen(np->name) + GSUFFIX_LEN, GFP_KERNEL);
//	if (!gname) {
//		dev_err(dev, "failed to alloc memory for group name\n");
//		goto free_map;
//	}
//	sprintf(gname, "%s%s", np->name, GROUP_SUFFIX);

//	/*
//	 * don't have config options? then skip over to creating function
//	 * map entries.
//	 */
//	if (!cfg_cnt)
//		goto skip_cfgs;

//	/* Allocate memory for config entries */
//	cfg = kzalloc(sizeof(*cfg) * cfg_cnt, GFP_KERNEL);
//	if (!cfg) {
//		dev_err(dev, "failed to alloc memory for configs\n");
//		goto free_gname;
//	}

//	/* Prepare a list of config settings */
//	for (idx = 0, cfg_cnt = 0; idx < ARRAY_SIZE(pcfgs); idx++) {
//		u32 value;
//		if (!of_property_read_u32(np, pcfgs[idx].prop_cfg, &value))
//			cfg[cfg_cnt++] =
//				PINCFG_PACK(pcfgs[idx].cfg_type, value);
//	}

//	/* create the config map entry */
//	map[*nmaps].data.configs.group_or_pin = gname;
//	map[*nmaps].data.configs.configs = cfg;
//	map[*nmaps].data.configs.num_configs = cfg_cnt;
//	map[*nmaps].type = PIN_MAP_TYPE_CONFIGS_GROUP;
//	*nmaps += 1;

//skip_cfgs:
//	/* create the function map entry */
//	if (of_find_property(np, "ambarella,amb-pin-function", NULL)) {
//		fname = kzalloc(strlen(np->name) + FSUFFIX_LEN,	GFP_KERNEL);
//		if (!fname) {
//			dev_err(dev, "failed to alloc memory for func name\n");
//			goto free_cfg;
//		}
//		sprintf(fname, "%s%s", np->name, FUNCTION_SUFFIX);

//		map[*nmaps].data.mux.group = gname;
//		map[*nmaps].data.mux.function = fname;
//		map[*nmaps].type = PIN_MAP_TYPE_MUX_GROUP;
//		*nmaps += 1;
//	}

//	*maps = map;
//	return 0;

//free_cfg:
//	kfree(cfg);
//free_gname:
//	kfree(gname);
//free_map:
//	kfree(map);
//	return -ENOMEM;
//}

/* free the memory allocated to hold the pin-map table */
//static void amb_dt_free_map(struct pinctrl_dev *pctldev,
//			     struct pinctrl_map *map, unsigned num_maps)
//{
//	int idx;

//	for (idx = 0; idx < num_maps; idx++) {
//		if (map[idx].type == PIN_MAP_TYPE_MUX_GROUP) {
//			kfree(map[idx].data.mux.function);
//			if (!idx)
//				kfree(map[idx].data.mux.group);
//		} else if (map->type == PIN_MAP_TYPE_CONFIGS_GROUP) {
//			kfree(map[idx].data.configs.configs);
//			if (!idx)
//				kfree(map[idx].data.configs.group_or_pin);
//		}
//	};

//	kfree(map);
//}

/* list of pinctrl callbacks for the pinctrl core */
static const struct pinctrl_ops amb_pctrl_ops = {
	.get_groups_count	= amb_get_group_count,
	.get_group_name		= amb_get_group_name,
	.get_group_pins		= amb_get_group_pins,
	//.dt_node_to_map		= amb_dt_node_to_map,
	//.dt_free_map		= amb_dt_free_map,
};

/* check if the selector is a valid pin function selector */
static int amb_pinmux_request(struct pinctrl_dev *pctldev, unsigned pin)
{
	struct amb_pinctrl_priv_data *priv;
	int ret = 0;

	priv = pinctrl_dev_get_drvdata(pctldev);

	if (test_and_set_bit(pin, priv->used))
		ret = -EBUSY;

	return ret;
}

/* check if the selector is a valid pin function selector */
static int amb_pinmux_free(struct pinctrl_dev *pctldev, unsigned pin)
{
	struct amb_pinctrl_priv_data *priv;

	priv = pinctrl_dev_get_drvdata(pctldev);
	clear_bit(pin, priv->used);

	return 0;
}

/* check if the selector is a valid pin function selector */
static int amb_pinmux_get_fcount(struct pinctrl_dev *pctldev)
{
	struct amb_pinctrl_priv_data *priv;

	priv = pinctrl_dev_get_drvdata(pctldev);
	return priv->nr_functions;
}

/* return the name of the pin function specified */
static const char *amb_pinmux_get_fname(struct pinctrl_dev *pctldev,
			unsigned selector)
{
	struct amb_pinctrl_priv_data *priv;

	priv = pinctrl_dev_get_drvdata(pctldev);
	return priv->pmx_functions[selector].name;
}

/* return the groups associated for the specified function selector */
static int amb_pinmux_get_groups(struct pinctrl_dev *pctldev,
			unsigned selector, const char * const **groups,
			unsigned * const num_groups)
{
	struct amb_pinctrl_priv_data *priv;

	priv = pinctrl_dev_get_drvdata(pctldev);
	*groups = priv->pmx_functions[selector].groups;
	*num_groups = priv->pmx_functions[selector].num_groups;
	return 0;
}

/* enable a specified pinmux by writing to registers */
static int amb_pinmux_enable(struct pinctrl_dev *pctldev,
			unsigned selector, unsigned group)
{
	struct amb_pinctrl_priv_data *priv;
	const struct amb_pin_group *pin_groups;
	void __iomem *regbase;
	u32 i, bank, offset;

	priv = pinctrl_dev_get_drvdata(pctldev);
	pin_groups = &priv->pin_groups[group];

	for (i = 0; i < pin_groups->num_pins; i++) {
		bank = PINID_TO_BANK(pin_groups->pins[i]);
		offset = PINID_TO_OFFSET(pin_groups->pins[i]);

		regbase = priv->regbase[bank];
		amba_setbitsl(regbase + GPIO_AFSEL_OFFSET, (0x1 << offset));
		amba_clrbitsl(regbase + GPIO_MASK_OFFSET, (0x1 << offset));

		priv->plat_data->set_pin_altfunc(pin_groups->pins[i],
				pin_groups->altfunc, pin_groups->multi_alt);
	}

	return 0;
}

/* disable a specified pinmux by writing to registers */
static void amb_pinmux_disable(struct pinctrl_dev *pctldev,
			unsigned selector, unsigned group)
{
	struct amb_pinctrl_priv_data *priv;
	const struct amb_pin_group *pin_groups;

	priv = pinctrl_dev_get_drvdata(pctldev);
	pin_groups = &priv->pin_groups[group];

	/* FIXME: poke out the mux, set the pin to some default state? */
	dev_dbg(priv->dev, "disable group %s, %u pins\n",
			pin_groups->name, pin_groups->num_pins);
}

static int amb_pinmux_gpio_request_enable(struct pinctrl_dev *pctldev,
			struct pinctrl_gpio_range *range,
			unsigned pin)
{
	struct amb_pinctrl_priv_data *priv;
	int ret = 0;

	priv = pinctrl_dev_get_drvdata(pctldev);

	if (!range) {
		dev_err(priv->dev, "invalid range\n");
		return -EINVAL;
	}
	if (!range->gc) {
		dev_err(priv->dev, "missing GPIO chip in range\n");
		return -EINVAL;
	}

	if (test_and_set_bit(pin, priv->used))
		ret = -EBUSY;

	return ret;
}

static void amb_pinmux_gpio_disable_free(struct pinctrl_dev *pctldev,
			struct pinctrl_gpio_range *range,
			unsigned pin)
{
	struct amb_pinctrl_priv_data *priv;

	priv = pinctrl_dev_get_drvdata(pctldev);
	dev_dbg(priv->dev, "disable pin %u as GPIO\n", pin);
	/* Set the pin to some default state, GPIO is usually default */

	clear_bit(pin, priv->used);
}

/*
 * The calls to gpio_direction_output() and gpio_direction_input()
 * leads to this function call (via the pinctrl_gpio_direction_{input|output}()
 * function called from the gpiolib interface).
 */
static int amb_pinmux_gpio_set_direction(struct pinctrl_dev *pctldev,
			struct pinctrl_gpio_range *range,
			unsigned pin, bool input)
{
	struct amb_pinctrl_priv_data *priv;
	void __iomem *regbase;
	u32 bank, offset, mask;

	priv = pinctrl_dev_get_drvdata(pctldev);

	bank = PINID_TO_BANK(pin);
	offset = PINID_TO_OFFSET(pin);
	regbase = priv->regbase[bank];
	mask = (0x1 << offset);

	amba_clrbitsl(regbase + GPIO_AFSEL_OFFSET, mask);
	if (input)
		amba_clrbitsl(regbase + GPIO_DIR_OFFSET, mask);
	else
		amba_setbitsl(regbase + GPIO_DIR_OFFSET, mask);

	return 0;
}

/* list of pinmux callbacks for the pinmux vertical in pinctrl core */
static const struct pinmux_ops amb_pinmux_ops = {
	.request		= amb_pinmux_request,
	.free			= amb_pinmux_free,
	.get_functions_count	= amb_pinmux_get_fcount,
	.get_function_name	= amb_pinmux_get_fname,
	.get_function_groups	= amb_pinmux_get_groups,
	.enable			= amb_pinmux_enable,
	.disable		= amb_pinmux_disable,
	.gpio_request_enable	= amb_pinmux_gpio_request_enable,
	.gpio_disable_free	= amb_pinmux_gpio_disable_free,
	.gpio_set_direction	= amb_pinmux_gpio_set_direction,
};

/* set the pin config settings for a specified pin */
static int amb_pinconf_set(struct pinctrl_dev *pctldev, unsigned int pin,
			unsigned long config)
{
	return 0;
}

/* get the pin config settings for a specified pin */
static int amb_pinconf_get(struct pinctrl_dev *pctldev,
			unsigned int pin, unsigned long *config)
{
	return 0;
}

/* set the pin config settings for a specified pin group */
static int amb_pinconf_group_set(struct pinctrl_dev *pctldev,
			unsigned group, unsigned long config)
{
	struct amb_pinctrl_priv_data *priv;
	const unsigned int *pins;
	unsigned int cnt;

	priv = pinctrl_dev_get_drvdata(pctldev);
	pins = priv->pin_groups[group].pins;

	for (cnt = 0; cnt < priv->pin_groups[group].num_pins; cnt++)
		amb_pinconf_set(pctldev, pins[cnt], config);

	return 0;
}

/* get the pin config settings for a specified pin group */
static int amb_pinconf_group_get(struct pinctrl_dev *pctldev,
			unsigned int group, unsigned long *config)
{
	struct amb_pinctrl_priv_data *priv;
	const unsigned int *pins;

	priv = pinctrl_dev_get_drvdata(pctldev);
	pins = priv->pin_groups[group].pins;
	amb_pinconf_get(pctldev, pins[0], config);
	return 0;
}

/* list of pinconfig callbacks for pinconfig vertical in the pinctrl code */
static const struct pinconf_ops amb_pinconf_ops = {
	.pin_config_get		= amb_pinconf_get,
	.pin_config_set		= amb_pinconf_set,
	.pin_config_group_get	= amb_pinconf_group_get,
	.pin_config_group_set	= amb_pinconf_group_set,
};

/* gpiolib gpio_request callback function */
static int amb_gpio_request(struct gpio_chip *gc, unsigned pin)
{
	return pinctrl_request_gpio(gc->base + pin);
}

/* gpiolib gpio_set callback function */
static void amb_gpio_free(struct gpio_chip *gc, unsigned pin)
{
	pinctrl_free_gpio(gc->base + pin);
}

/* gpiolib gpio_free callback function */
static void amb_gpio_set(struct gpio_chip *gc, unsigned pin, int value)
{
	struct amb_pinctrl_priv_data *priv = dev_get_drvdata(gc->dev);
	void __iomem *regbase;
	u32 bank, offset, mask;

	bank = PINID_TO_BANK(pin);
	offset = PINID_TO_OFFSET(pin);
	regbase = priv->regbase[bank];
	mask = (0x1 << offset);

	amba_writel(regbase + GPIO_MASK_OFFSET, mask);
	if (value == GPIO_LOW)
		mask = 0;
	amba_writel(regbase + GPIO_DATA_OFFSET, mask);
}

/* gpiolib gpio_get callback function */
static int amb_gpio_get(struct gpio_chip *gc, unsigned pin)
{
	struct amb_pinctrl_priv_data *priv = dev_get_drvdata(gc->dev);
	void __iomem *regbase;
	u32 bank, offset, mask, data;

	bank = PINID_TO_BANK(pin);
	offset = PINID_TO_OFFSET(pin);
	regbase = priv->regbase[bank];
	mask = (0x1 << offset);

	amba_writel(regbase + GPIO_MASK_OFFSET, mask);
	data = amba_readl(regbase + GPIO_DATA_OFFSET);
	data = (data >> offset) & 0x1;

	return (data ? GPIO_HIGH : GPIO_LOW);
}

/* gpiolib gpio_direction_input callback function */
static int amb_gpio_direction_input(struct gpio_chip *gc, unsigned pin)
{
	return pinctrl_gpio_direction_input(gc->base + pin);
}

/* gpiolib gpio_direction_output callback function */
static int amb_gpio_direction_output(struct gpio_chip *gc,
		unsigned pin, int value)
{
	int ret;

	ret = pinctrl_gpio_direction_output(gc->base + pin);
	if (ret < 0)
		return ret;

	amb_gpio_set(gc, pin, value);

	return 0;
}

/* gpiolib gpio_to_irq callback function */
static int amb_gpio_to_irq(struct gpio_chip *gc, unsigned pin)
{
	if ((gc->base + pin) < GPIO_MAX_LINES)
		return GPIO_INT_VEC(gc->base + pin);

	return -ENXIO;
}

static void amb_gpio_dbg_show(struct seq_file *s, struct gpio_chip *gc)
{
	struct amb_pinctrl_priv_data *priv = dev_get_drvdata(gc->dev);
	void __iomem *regbase;
	u32 afsel = 0, data = 0, dir = 0, mask = 0;
	u32 i, bank, offset;

	for (i = 0; i < gc->ngpio; i++) {
		offset = PINID_TO_OFFSET(i);
		if (offset == 0) {
			bank = PINID_TO_BANK(i);
			regbase = priv->regbase[bank];

			afsel = amba_readl(regbase + GPIO_AFSEL_OFFSET);
			dir = amba_readl(regbase + GPIO_DIR_OFFSET);
			mask = amba_readl(regbase + GPIO_MASK_OFFSET);
			amba_writel(regbase + GPIO_MASK_OFFSET, ~afsel);
			data = amba_readl(regbase + GPIO_DATA_OFFSET);
			amba_writel(regbase + GPIO_MASK_OFFSET, mask);

			seq_printf(s, "\nGPIO[%d]:\t[%d - %d]\n",
				bank, i, i + GPIO_BANK_SIZE - 1);
			seq_printf(s, "GPIO_BASE:\t0x%08X\n", (u32)regbase);
			seq_printf(s, "GPIO_AFSEL:\t0x%08X\n", afsel);
			seq_printf(s, "GPIO_DIR:\t0x%08X\n", dir);
			seq_printf(s, "GPIO_MASK:\t0x%08X:0x%08X\n", mask, ~afsel);
			seq_printf(s, "GPIO_DATA:\t0x%08X\n", data);
		}

		seq_printf(s, "GPIO %d: ", i);
		if (afsel & (1 << offset)) {
			seq_printf(s, "HW\n");
		} else {
			seq_printf(s, "%s\t%s\n",
				(dir & (1 << offset)) ? "out" : "in",
				(data & (1 << offset)) ? "set" : "clear");
		}
	}
}

/* parse the pin numbers listed in the 'ambarella,amb-pins' property */
//static int amb_pinctrl_parse_dt_pins(struct platform_device *pdev,
//			struct device_node *cfg_np, unsigned int **pin_list,
//			unsigned int *npins)
//{
//	struct device *dev = &pdev->dev;
//	struct property *prop;

//	prop = of_find_property(cfg_np, "ambarella,amb-pins", NULL);
//	if (!prop)
//		return -ENOENT;

//	*npins = prop->length / sizeof(unsigned long);
//	if (!*npins) {
//		dev_err(dev, "invalid pin list in %s node", cfg_np->name);
//		return -EINVAL;
//	}

//	*pin_list = devm_kzalloc(dev, *npins * sizeof(**pin_list), GFP_KERNEL);
//	if (!*pin_list) {
//		dev_err(dev, "failed to allocate memory for pin list\n");
//		return -ENOMEM;
//	}

//	return of_property_read_u32_array(cfg_np, "ambarella,amb-pins",
//			*pin_list, *npins);
//}

/*
 * Parse the information about all the available pin groups and pin functions
 * from device node of the pin-controller.
 */
//static int amb_pinctrl_parse_dt(struct platform_device *pdev,
//				struct amb_pinctrl_priv_data *priv)
//{
//	struct device *dev = &pdev->dev;
//	struct device_node *dev_np = dev->of_node;
//	struct device_node *cfg_np;
//	struct amb_pin_group *groups, *grp;
//	struct amb_pmx_func *functions, *func;
//	unsigned *pin_list;
//	unsigned int npins, grp_cnt, func_idx = 0;
//	char *gname, *fname;
//	int ret;

//	grp_cnt = of_get_child_count(dev_np);
//	if (!grp_cnt)
//		return -EINVAL;

//	groups = devm_kzalloc(dev, grp_cnt * sizeof(*groups), GFP_KERNEL);
//	if (!groups) {
//		dev_err(dev, "failed allocate memory for ping group list\n");
//		return -EINVAL;
//	}
//	grp = groups;

//	functions = devm_kzalloc(dev, grp_cnt * sizeof(*functions), GFP_KERNEL);
//	if (!functions) {
//		dev_err(dev, "failed to allocate memory for function list\n");
//		return -EINVAL;
//	}
//	func = functions;

//	/*
//	 * Iterate over all the child nodes of the pin controller node
//	 * and create pin groups and pin function lists.
//	 */
//	for_each_child_of_node(dev_np, cfg_np) {
//		u32 function;

//		ret = amb_pinctrl_parse_dt_pins(pdev, cfg_np,
//					&pin_list, &npins);
//		if (ret) {
//			gname = NULL;
//			goto skip_to_pin_function;
//		}

//		/* derive pin group name from the node name */
//		gname = devm_kzalloc(dev, strlen(cfg_np->name) + GSUFFIX_LEN,
//					GFP_KERNEL);
//		if (!gname) {
//			dev_err(dev, "failed to alloc memory for group name\n");
//			return -ENOMEM;
//		}
//		sprintf(gname, "%s%s", cfg_np->name, GROUP_SUFFIX);

//		grp->name = gname;
//		grp->pins = pin_list;
//		grp->num_pins = npins;
//		grp++;

//skip_to_pin_function:
//		ret = of_property_read_u32(cfg_np, "ambarella,amb-pin-function",
//						&function);
//		if (ret)
//			continue;

//		/* derive function name from the node name */
//		fname = devm_kzalloc(dev, strlen(cfg_np->name) + FSUFFIX_LEN,
//					GFP_KERNEL);
//		if (!fname) {
//			dev_err(dev, "failed to alloc memory for func name\n");
//			return -ENOMEM;
//		}
//		sprintf(fname, "%s%s", cfg_np->name, FUNCTION_SUFFIX);

//		func->name = fname;
//		func->groups = devm_kzalloc(dev, sizeof(char *), GFP_KERNEL);
//		if (!func->groups) {
//			dev_err(dev, "failed to alloc memory for group list "
//					"in pin function");
//			return -ENOMEM;
//		}
//		func->groups[0] = gname;
//		func->num_groups = gname ? 1 : 0;
//		func->function = function;
//		func++;
//		func_idx++;
//	}

//	priv->pin_groups = groups;
//	priv->nr_groups = grp_cnt;
//	priv->pmx_functions = functions;
//	priv->nr_functions = func_idx;
//	return 0;
//}

/* register the pinctrl interface with the pinctrl subsystem */
static int amb_pinctrl_register(struct platform_device *pdev,
			struct amb_pinctrl_priv_data *priv)
{
	struct device *dev = &pdev->dev;
	struct pinctrl_desc *ctrldesc;
	struct pinctrl_dev *pctl_dev;
	struct pinctrl_pin_desc *pindesc;
	char *pin_names;
	int pin;
	//int ret;

	ctrldesc = devm_kzalloc(dev, sizeof(*ctrldesc), GFP_KERNEL);
	if (!ctrldesc) {
		dev_err(dev, "could not allocate memory for pinctrl desc\n");
		return -ENOMEM;
	}

	ctrldesc->name = "amb-pinctrl";
	ctrldesc->owner = THIS_MODULE;
	ctrldesc->pctlops = &amb_pctrl_ops;
	ctrldesc->pmxops = &amb_pinmux_ops;
	ctrldesc->confops = &amb_pinconf_ops;

	pindesc = devm_kzalloc(&pdev->dev,
			sizeof(*pindesc) * AMBGPIO_SIZE, GFP_KERNEL);
	if (!pindesc) {
		dev_err(&pdev->dev, "failed to allocate mem for pin desc\n");
		return -ENOMEM;
	}

	pin_names = devm_kzalloc(&pdev->dev,
			PIN_NAME_LENGTH * AMBGPIO_SIZE, GFP_KERNEL);
	if (!pin_names) {
		dev_err(&pdev->dev, "failed to allocate mem for pin names\n");
		return -ENOMEM;
	}

	ctrldesc->pins = pindesc;
	ctrldesc->npins = AMBGPIO_SIZE;

	/* dynamically populate the pin number and pin name for pindesc */
	for (pin = 0; pin < ctrldesc->npins; pin++) {
		pindesc[pin].number = pin;
		sprintf(pin_names, "io%d", pin);
		pindesc[pin].name = pin_names;
		pin_names += PIN_NAME_LENGTH;
	}

	priv->pin_groups = amb_pin_groups;
	priv->nr_groups = ARRAY_SIZE(amb_pin_groups);
	priv->pmx_functions = amb_pmx_functions;
	priv->nr_functions = ARRAY_SIZE(amb_pmx_functions);

	//ret = amb_pinctrl_parse_dt(pdev, priv);
	//if (ret)
	//	return ret;

	pctl_dev = pinctrl_register(ctrldesc, &pdev->dev, priv);
	if (!pctl_dev) {
		dev_err(&pdev->dev, "could not register pinctrl driver\n");
		return -EINVAL;
	}

	priv->grange.name = "amb-gpio-range";
	priv->grange.id = 0;
	priv->grange.base = 0;
	priv->grange.pin_base = 0;
	priv->grange.npins = AMBGPIO_SIZE;
	priv->grange.gc = priv->gc;
	pinctrl_add_gpio_range(pctl_dev, &priv->grange);

	return 0;
}

/* register the gpiolib interface with the gpiolib subsystem */
static int amb_gpiolib_register(struct platform_device *pdev,
			struct amb_pinctrl_priv_data *priv)
{
	struct gpio_chip *gc;
	int ret;

	gc = devm_kzalloc(&pdev->dev, sizeof(*gc), GFP_KERNEL);
	if (!gc) {
		dev_err(&pdev->dev, "failed to allocate mem for gpio_chip\n");
		return -ENOMEM;
	}

	priv->gc = gc;
	gc->base = 0;
	gc->ngpio = AMBGPIO_SIZE;
	gc->dev = &pdev->dev;
	gc->request = amb_gpio_request;
	gc->free = amb_gpio_free;
	gc->set = amb_gpio_set;
	gc->get = amb_gpio_get;
	gc->direction_input = amb_gpio_direction_input;
	gc->direction_output = amb_gpio_direction_output;
	gc->to_irq = amb_gpio_to_irq;
	gc->dbg_show = amb_gpio_dbg_show;
	gc->label = "ambarella-gpio";
	gc->owner = THIS_MODULE;
	ret = gpiochip_add(gc);
	if (ret) {
		dev_err(&pdev->dev, "failed to register gpio_chip %s, error "
					"code: %d\n", gc->label, ret);
		return ret;
	}

	return 0;
}

/* unregister the gpiolib interface with the gpiolib subsystem */
static int amb_gpiolib_unregister(struct platform_device *pdev,
			struct amb_pinctrl_priv_data *priv)
{
	int ret;

	ret = gpiochip_remove(priv->gc);
	if (ret) {
		dev_err(&pdev->dev, "gpio chip remove failed\n");
		return ret;
	}

	return 0;
}

static int amb_pinctrl_probe(struct platform_device *pdev)
{
	struct amb_pinctrl_priv_data *priv;
	struct ambarella_platform_pinctrl *plat_data;
	struct resource *mem;
	int i, ret;

	plat_data = (struct ambarella_platform_pinctrl *)pdev->dev.platform_data;
	if ((plat_data == NULL) || (plat_data->set_pin_altfunc == NULL)) {
		dev_err(&pdev->dev, "Can't get platform_data!\n");
		return EPERM;
	}

	//if (!dev->of_node) {
	//	dev_err(dev, "device tree node not found\n");
	//	return -ENODEV;
	//}

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(&pdev->dev, "failed to allocate memory for private data\n");
		return -ENOMEM;
	}
	priv->dev = &pdev->dev;
	priv->plat_data = plat_data;

	for (i = 0; i < GPIO_INSTANCES; i++) {
		mem = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (mem == NULL) {
			dev_err(&pdev->dev, "failed to get mem resource!\n");
			return -ENXIO;
		}
		priv->regbase[i] = (void __iomem *)mem->start;
	}

	ret = amb_gpiolib_register(pdev, priv);
	if (ret)
		return ret;

	ret = amb_pinctrl_register(pdev, priv);
	if (ret) {
		amb_gpiolib_unregister(pdev, priv);
		return ret;
	}

	platform_set_drvdata(pdev, priv);
	dev_info(&pdev->dev, "Ambarella pinctrl driver registered\n");

	return 0;
}

static const struct of_device_id amb_pinctrl_dt_match[] = {
	{ .compatible = "ambarella,amb-pinctrl" },
	{},
};
MODULE_DEVICE_TABLE(of, amb_pinctrl_dt_match);

static struct platform_driver amb_pinctrl_driver = {
	.probe		= amb_pinctrl_probe,
	.driver = {
		.name	= "amb-pinctrl",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(amb_pinctrl_dt_match),
	},
};

static int __init amb_pinctrl_drv_register(void)
{
	return platform_driver_register(&amb_pinctrl_driver);
}
postcore_initcall(amb_pinctrl_drv_register);

static void __exit amb_pinctrl_drv_unregister(void)
{
	platform_driver_unregister(&amb_pinctrl_driver);
}
module_exit(amb_pinctrl_drv_unregister);

MODULE_AUTHOR("Cao Rongrong <rrcao@ambarella.com>");
MODULE_DESCRIPTION("Ambarella SoC pinctrl driver");
MODULE_LICENSE("GPL v2");

