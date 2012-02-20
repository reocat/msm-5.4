#include <linux/kernel.h>
#include <linux/init.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <mach/hardware.h>
#include <mach/board.h>

#include <linux/regulator/machine.h>

#include <linux/mfd/tps6586x.h>
#include <linux/i2c.h>
#include "board-device.h"
/* ==========================================================================*/

static struct regulator_consumer_supply sm0_consumers[] __initdata = {
	{
		.supply = "cpu_vcc",
	},
};
static struct regulator_init_data tps6586x_default_sm0_data __initdata = {
	.constraints = {
		.name = "SM0_V1P1",
		.min_uV = 725000,
		.max_uV = 1500000,
		.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE,
		.valid_modes_mask = REGULATOR_MODE_STANDBY | REGULATOR_MODE_NORMAL,
		.state_mem = {
			.disabled = 1,
		},
	//	.initial_state = PM_SUSPEND_MEM,
		.always_on = 1,
		.boot_on = 1,
	},
	 .num_consumer_supplies = ARRAY_SIZE(sm0_consumers),
	 .consumer_supplies = sm0_consumers,
};

static struct regulator_consumer_supply sm1_consumers[] __initdata = {
	{
		.supply = "ddr3_vcc",
	},
};
static struct regulator_init_data tps6586x_default_sm1_data __initdata = {
	.constraints = {
		.name = "SM1_V1P5",
		.min_uV = 725000,
		.max_uV = 1500000,
		.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE,
		.valid_modes_mask = REGULATOR_MODE_STANDBY | REGULATOR_MODE_NORMAL,
		.state_mem = {
			.disabled = 1,
		},
	//	.initial_state = PM_SUSPEND_MEM,
		.always_on = 1,
		.boot_on = 1,
	},
	 .num_consumer_supplies = ARRAY_SIZE(sm1_consumers),
	 .consumer_supplies = sm1_consumers,
};

static struct regulator_consumer_supply sm2_consumers[] __initdata = {
	{
		.supply = "gen_vcc",
	},
	{
		.supply = "lp_vcc",
	},
};
static struct regulator_init_data tps6586x_default_sm2_data __initdata = {
	.constraints = {
		.name = "SM2_V3P15",
		.min_uV = 1025000,
		.max_uV = 3700000,
		.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE,
		.valid_modes_mask = REGULATOR_MODE_STANDBY | REGULATOR_MODE_NORMAL,
		.state_mem = {
			.disabled = 1,
		},
	//	.initial_state = PM_SUSPEND_MEM,
		.always_on = 1,
		.boot_on = 1,
	},
	 .num_consumer_supplies = ARRAY_SIZE(sm2_consumers),
	 .consumer_supplies = sm2_consumers,
};

static struct regulator_init_data tps6586x_default_ldo0_data __initdata = {
	.constraints = {
		.name = "LDO0_V2P8_SEN",
		.min_uV = 1200000,
		.max_uV = 3300000,
		.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE,
		.valid_modes_mask = REGULATOR_MODE_STANDBY | REGULATOR_MODE_NORMAL,
		.state_mem = {
			.disabled = 1,
		},
	//	.initial_state = PM_SUSPEND_MEM,
		.always_on = 1,
		.boot_on = 1,
	},
//	 .num_consumer_supplies = ARRAY_SIZE(sm2_consumers),
//	 .consumer_supplies = sm2_consumers,
};

static struct regulator_init_data tps6586x_default_ldo1_data __initdata = {
	.constraints = {
		.name = "LDO1_V1P8_AUD",//ldo1 voltage max is 1.5v, support 1.8v?
		.min_uV = 800000,
		.max_uV = 1500000,
		.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE,
		.valid_modes_mask = REGULATOR_MODE_STANDBY | REGULATOR_MODE_NORMAL,
		.state_mem = {
			.disabled = 1,
		},
	//	.initial_state = PM_SUSPEND_MEM,
		.always_on = 1,
		.boot_on = 1,
	},
//	 .num_consumer_supplies = ARRAY_SIZE(sm2_consumers),
//	 .consumer_supplies = sm2_consumers,
};

static struct regulator_init_data tps6586x_default_ldo2_data __initdata = {
	.constraints = {
		.name = "LDO2_V1P5_SEN",
		.min_uV = 800000,
		.max_uV = 1500000,
		.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE,
		.valid_modes_mask = REGULATOR_MODE_STANDBY | REGULATOR_MODE_NORMAL,
		.state_mem = {
			.disabled = 1,
		},
	//	.initial_state = PM_SUSPEND_MEM,
		.always_on = 1,
		.boot_on = 1,
	},
//	 .num_consumer_supplies = ARRAY_SIZE(sm2_consumers),
//	 .consumer_supplies = sm2_consumers,
};

static struct regulator_init_data tps6586x_default_ldo3_data __initdata = {
	.constraints = {
		.name = "LDO3_V3P15_GY",
		.min_uV = 1200000,
		.max_uV = 3300000,
		.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE,
		.valid_modes_mask = REGULATOR_MODE_STANDBY | REGULATOR_MODE_NORMAL,
		.state_mem = {
			.disabled = 1,
		},
	//	.initial_state = PM_SUSPEND_MEM,
		.always_on = 1,
		.boot_on = 1,
	},
//	 .num_consumer_supplies = ARRAY_SIZE(sm2_consumers),
//	 .consumer_supplies = sm2_consumers,
};

static struct regulator_init_data tps6586x_default_ldo4_data __initdata = {
	.constraints = {
		.name = "LDO4_V2P5",
		.min_uV = 1700000,
		.max_uV = 2500000,
		.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE,
		.valid_modes_mask = REGULATOR_MODE_STANDBY | REGULATOR_MODE_NORMAL,
		.state_mem = {
			.disabled = 1,
		},
	//	.initial_state = PM_SUSPEND_MEM,
		.always_on = 1,
		.boot_on = 1,
	},
//	 .num_consumer_supplies = ARRAY_SIZE(sm2_consumers),
//	 .consumer_supplies = sm2_consumers,
};

static struct regulator_init_data tps6586x_default_ldo5_data __initdata = {
	.constraints = {
		.name = "LDO5_V1P8",
		.min_uV = 1200000,
		.max_uV = 3300000,
		.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE,
		.valid_modes_mask = REGULATOR_MODE_STANDBY | REGULATOR_MODE_NORMAL,
		.state_mem = {
			.disabled = 1,
		},
	//	.initial_state = PM_SUSPEND_MEM,
		.always_on = 1,
		.boot_on = 1,
	},
//	 .num_consumer_supplies = ARRAY_SIZE(sm2_consumers),
//	 .consumer_supplies = sm2_consumers,
};

static struct regulator_init_data tps6586x_default_ldo6_data __initdata = {
	.constraints = {
		.name = "LDO6_V_SDXC",
		.min_uV = 1200000,
		.max_uV = 3300000,
		.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE,
		.valid_modes_mask = REGULATOR_MODE_STANDBY | REGULATOR_MODE_NORMAL,
		.state_mem = {
			.disabled = 1,
		},
	//	.initial_state = PM_SUSPEND_MEM,
		.always_on = 1,
		.boot_on = 1,
	},
//	 .num_consumer_supplies = ARRAY_SIZE(sm2_consumers),
//	 .consumer_supplies = sm2_consumers,
};

static struct regulator_init_data tps6586x_default_ldo7_data __initdata = {
	.constraints = {
		.name = "LDO7_V2P8A_G",
		.min_uV = 1200000,
		.max_uV = 3300000,
		.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE,
		.valid_modes_mask = REGULATOR_MODE_STANDBY | REGULATOR_MODE_NORMAL,
		.state_mem = {
			.disabled = 1,
		},
	//	.initial_state = PM_SUSPEND_MEM,
		.always_on = 1,
		.boot_on = 1,
	},
//	 .num_consumer_supplies = ARRAY_SIZE(sm2_consumers),
//	 .consumer_supplies = sm2_consumers,
};

static struct regulator_init_data tps6586x_default_ldo8_data __initdata = {
	.constraints = {
		.name = "LDO8_V2P8_LCD",
		.min_uV = 1200000,
		.max_uV = 3300000,
		.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE,
		.valid_modes_mask = REGULATOR_MODE_STANDBY | REGULATOR_MODE_NORMAL,
		.state_mem = {
			.disabled = 1,
		},
	//	.initial_state = PM_SUSPEND_MEM,
		.always_on = 1,
		.boot_on = 1,
	},
//	 .num_consumer_supplies = ARRAY_SIZE(sm2_consumers),
//	 .consumer_supplies = sm2_consumers,
};

static struct regulator_init_data tps6586x_default_ldo9_data __initdata = {
	.constraints = {
		.name = "LDO9_V3P15_BT",
		.min_uV = 1200000,
		.max_uV = 3300000,
		.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE,
		.valid_modes_mask = REGULATOR_MODE_STANDBY | REGULATOR_MODE_NORMAL,
		.state_mem = {
			.disabled = 1,
		},
	//	.initial_state = PM_SUSPEND_MEM,
		.always_on = 1,
		.boot_on = 1,
	},
//	 .num_consumer_supplies = ARRAY_SIZE(sm2_consumers),
//	 .consumer_supplies = sm2_consumers,
};

static struct regulator_init_data tps6586x_default_ldo_rtc_data __initdata = {
	.constraints = {
		.name = "LDO_RTC_OUT",
		.min_uV = 2500000,
		.max_uV = 3300000,
		.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE,
		.valid_modes_mask = REGULATOR_MODE_STANDBY | REGULATOR_MODE_NORMAL,
		.state_mem = {
			.disabled = 1,
		},
	//	.initial_state = PM_SUSPEND_MEM,
		.always_on = 1,
		.boot_on = 1,
	},
//	 .num_consumer_supplies = ARRAY_SIZE(sm2_consumers),
//	 .consumer_supplies = sm2_consumers,
};

static struct tps658640_backlight_pdata tps658640_default_backlight_pdata __initdata = {
	.pwm_out = 0,
	.mode = 0,
	.max_cycle = 0x7f,
};

static struct tps6586x_subdev_info ti6586x_subdevs[] __initdata= {
	{
		.id = 0,
		.name = "tps6586x-regulator",
		.platform_data = &tps6586x_default_sm0_data,
	},
	{
		.id = 1,
		.name = "tps6586x-regulator",
		.platform_data = &tps6586x_default_sm1_data,
	},
	{
		.id = 2,
		.name = "tps6586x-regulator",
		.platform_data = &tps6586x_default_sm2_data,
	},
	{
		.id = 3,
		.name = "tps6586x-regulator",
		.platform_data = &tps6586x_default_ldo0_data,
	},
	{
		.id = 4,
		.name = "tps6586x-regulator",
		.platform_data = &tps6586x_default_ldo1_data,
	},
	{
		.id = 5,
		.name = "tps6586x-regulator",
		.platform_data = &tps6586x_default_ldo2_data,
	},
	{
		.id = 6,
		.name = "tps6586x-regulator",
		.platform_data = &tps6586x_default_ldo3_data,
	},
	{
		.id = 7,
		.name = "tps6586x-regulator",
		.platform_data = &tps6586x_default_ldo4_data,
	},
	{
		.id = 8,
		.name = "tps6586x-regulator",
		.platform_data = &tps6586x_default_ldo5_data,
	},
	{
		.id = 9,
		.name = "tps6586x-regulator",
		.platform_data = &tps6586x_default_ldo6_data,
	},
	{
		.id = 10,
		.name = "tps6586x-regulator",
		.platform_data = &tps6586x_default_ldo7_data,
	},
	{
		.id = 11,
		.name = "tps6586x-regulator",
		.platform_data = &tps6586x_default_ldo8_data,
	},
	{
		.id = 12,
		.name = "tps6586x-regulator",
		.platform_data = &tps6586x_default_ldo9_data,
	},
	{
		.id = 13,
		.name = "tps6586x-regulator",
		.platform_data = &tps6586x_default_ldo_rtc_data,
	},
	{
		.id = 14,
		.name = "tps6586x-backlight",
		.platform_data = &tps658640_default_backlight_pdata,
	},
	{
		.id = 15,
		.name = "tps6586x-on",
	},
};

static struct tps6586x_platform_data elephant_board_TI6586x_pdata __initdata = {
	.num_subdevs = 16,
	.subdevs = ti6586x_subdevs,
	.gpio_base = EXT_GPIO(0),
	.irq_base = EXT_IRQ(0),
};

struct i2c_board_info ambarella_ti6586x_board_info __initdata= {
	.type		= "tps6586x",
	.addr		= 0x34,
	.platform_data	= &elephant_board_TI6586x_pdata,
};

struct i2c_board_info ambarella_bq27410_board_info __initdata= {
	.type		= "bq27500",
	.addr		= 0x55,
};

