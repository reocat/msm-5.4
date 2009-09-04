/*
 * arch/arm/mach-ambarella/include/mach/config.h
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

#ifndef __ASM_ARCH_HARDWARE_H
#error "include <mach/hardware.h> instead"
#endif

#ifndef __ASM_ARCH_CONFIG_H
#define __ASM_ARCH_CONFIG_H

#define MEM_MAP_CHECK_MASK		((1024 * 1024) - 1)

#define ATAG_AMBARELLA_DSP		0x44000110
#define ATAG_AMBARELLA_BSB		0x44000044
#define ATAG_AMBARELLA_REVMEM		0x44001111
#define ATAG_AMBARELLA_HAL		0x44000722

#define ATAG_AMBARELLA_NAND_BST		0x44040400
#define ATAG_AMBARELLA_NAND_BLD		0x44040401
#define ATAG_AMBARELLA_NAND_PTB		0x44040402
#define ATAG_AMBARELLA_NAND_PRI		0x44040403
#define ATAG_AMBARELLA_NAND_ROM		0x44040404
#define ATAG_AMBARELLA_NAND_BAK		0x44040405
#define ATAG_AMBARELLA_NAND_PBA		0x44040406
#define ATAG_AMBARELLA_NAND_DSP		0x44040407
#define ATAG_AMBARELLA_NAND_NFTL	0x44040408
#define ATAG_AMBARELLA_NAND_RAM		0x44040409

#define ATAG_AMBARELLA_NAND_CS		0x440404F0
#define ATAG_AMBARELLA_NAND_T0		0x440404F1
#define ATAG_AMBARELLA_NAND_T1		0x440404F2
#define ATAG_AMBARELLA_NAND_T2		0x440404F3

#define MAX_NAND_ERASE_BLOCK_SIZE	(2048 * 128)
#define MAX_AMBOOT_PARTITION_NR		(16)
#define MAX_AMBOOT_PARTITION_NANE_SIZE	(16)

#define AMBOOT_DEFAULT_BST_SIZE		(MAX_NAND_ERASE_BLOCK_SIZE)
#define AMBOOT_DEFAULT_PTB_SIZE		(MAX_NAND_ERASE_BLOCK_SIZE)
#define AMBOOT_DEFAULT_BLD_SIZE 	(MAX_NAND_ERASE_BLOCK_SIZE)
#define AMBOOT_DEFAULT_PRI_SIZE 	(4 * 1024 * 1024)
#define AMBOOT_DEFAULT_RXM_SIZE		(8 * 1024 * 1024)

#define AMBARELLA_BOARD_TYPE_AUTO	(0)
#define AMBARELLA_BOARD_TYPE_BUB	(1)
#define AMBARELLA_BOARD_TYPE_EVK	(2)
#define AMBARELLA_BOARD_TYPE_IPCAM	(3)

#define AMBARELLA_BOARD_CHIP_AUTO	(0)

#define AMBARELLA_BOARD_REV_AUTO	(0)

#define AMBARELLA_BOARD_VERSION(c,t,r)	(((c) << 16) + ((t) << 8) + (r))
#define AMBARELLA_BOARD_CHIP(v)		(((v) >> 16) & 0xFFFF)
#define AMBARELLA_BOARD_TYPE(v)		(((v) >> 8) & 0xFF)
#define AMBARELLA_BOARD_REV(v)		(((v) >> 0) & 0xFF)

#define MEMORY_RESERVE_MAX_NR		(16)
#define MEMORY_RESERVE_CHECK_MASK	(PAGE_SIZE - 1)

#ifndef __ASSEMBLER__

extern u32 get_ambarella_dspmem_phys(void);
extern u32 get_ambarella_dspmem_virt(void);
extern u32 get_ambarella_dspmem_size(void);

extern u32 get_ambarella_bsbmem_phys(void);
extern u32 get_ambarella_bsbmem_virt(void);
extern u32 get_ambarella_bsbmem_size(void);

extern u32 ambarella_phys_to_virt(u32 paddr);
extern u32 ambarella_virt_to_phys(u32 vaddr);

struct ambarella_mem_rev_desc {
	unsigned long physaddr;
	unsigned long size;
};
struct ambarella_mem_rev_info {
	u32 counter;
	struct ambarella_mem_rev_desc desc[MEMORY_RESERVE_MAX_NR];
};
extern u32 get_ambarella_mem_rev_info(struct ambarella_mem_rev_info *pinfo);

struct ambarella_mem_hal_desc {
	unsigned long physaddr;
	unsigned long size;
	unsigned long virtual;
};
extern void *get_ambarella_hal_vp(void);

extern u64 ambarella_dmamask;
extern u32 ambarella_debug_level;
extern u32 ambarella_debug_info;

struct ambarella_gpio_power_info {
	int	power_gpio;
	int	power_level;
	int	power_delay;		//ms
};
#define AMBA_GPIO_POWER_MODULE_PARAM_CALL(name_prefix, arg, perm) \
	module_param_call(name_prefix##power_gpio, param_set_int, param_get_int, &(arg.power_gpio), perm); \
	module_param_call(name_prefix##power_level, param_set_int, param_get_int, &(arg.power_level), perm); \
	module_param_call(name_prefix##power_delay, param_set_int, param_get_int, &(arg.power_delay), perm)
extern void ambarella_set_gpio_power(struct ambarella_gpio_power_info *pinfo, u32 poweron);

struct ambarella_gpio_input_info {
	int	input_gpio;
	int	input_level;
	int	input_delay;		//ms
};
#define AMBA_GPIO_INPUT_MODULE_PARAM_CALL(name_prefix, arg, perm) \
	module_param_call(name_prefix##input_gpio, param_set_int, param_get_int, &(arg.input_gpio), perm); \
	module_param_call(name_prefix##input_level, param_set_int, param_get_int, &(arg.input_level), perm); \
	module_param_call(name_prefix##input_delay, param_set_int, param_get_int, &(arg.input_delay), perm)
extern u32 ambarella_get_gpio_input(struct ambarella_gpio_input_info *pinfo);

struct ambarella_gpio_reset_info {
	int	reset_gpio;
	int	reset_level;
	int	reset_delay;		//ms
	int	resume_delay;		//ms
};
#define AMBA_GPIO_RESET_MODULE_PARAM_CALL(name_prefix, arg, perm) \
	module_param_call(name_prefix##reset_gpio, param_set_int, param_get_int, &(arg.reset_gpio), perm); \
	module_param_call(name_prefix##reset_level, param_set_int, param_get_int, &(arg.reset_level), perm); \
	module_param_call(name_prefix##reset_delay, param_set_int, param_get_int, &(arg.reset_delay), perm); \
	module_param_call(name_prefix##resume_delay, param_set_int, param_get_int, &(arg.resume_delay), perm)
extern void ambarella_set_gpio_reset(struct ambarella_gpio_reset_info *pinfo);

struct ambarella_gpio_irq_info {
	int	irq_gpio;
	int	irq_line;
	int	irq_type;
	int	irq_gpio_val;
	int	irq_gpio_mode;
};
#define AMBA_GPIO_IRQ_MODULE_PARAM_CALL(name_prefix, arg, perm) \
	module_param_call(name_prefix##irq_gpio, param_set_int, param_get_int, &(arg.irq_gpio), perm); \
	module_param_call(name_prefix##irq_line, param_set_int, param_get_int, &(arg.irq_line), perm); \
	module_param_call(name_prefix##irq_type, param_set_int, param_get_int, &(arg.irq_type), perm); \
	module_param_call(name_prefix##irq_gpio_val, param_set_int, param_get_int, &(arg.irq_gpio_val), perm); \
	module_param_call(name_prefix##irq_gpio_mode, param_set_int, param_get_int, &(arg.irq_gpio_mode), perm)
extern int ambarella_is_valid_gpio_irq(struct ambarella_gpio_irq_info *pgpio_irq);

extern void ambarella_sound_set_gpio_power(u32 poweron);

struct ambarella_uart_port_info {
	void					*port;	//struct uart_port *
	char					name[32];
	u32					flow_control;
};
struct ambarella_uart_platform_info {
	const int				total_port_num;
	int					registed_port_num;
	struct ambarella_uart_port_info		amba_port[PORTMAX];

	void					(*set_pll)(void);
	u32					(*get_pll)(void);
};
extern struct ambarella_uart_platform_info ambarella_uart_ports;

struct ambarella_sd_slot {
	int					(*check_owner)(void);
	void					(*request)(void);
	void					(*release)(void);
	struct ambarella_gpio_power_info	ext_power;
	u32					bounce_buffer;
	struct ambarella_gpio_irq_info		gpio_cd;
	int					cd_delay;	//jiffies
	struct ambarella_gpio_input_info	gpio_wp;
};
struct ambarella_sd_controller {
	u32					num_slots;
	struct ambarella_sd_slot		slot[SD_MAX_SLOT_NUM];
	u32					clk_limit;
	u32					wait_tmo;

	void					(*set_pll)(void);
	u32					(*get_pll)(void);
};
#define AMBA_SD_PARAM_CALL(controller_id, slot_id, arg, perm) \
	module_param_call(sd##controller_id##_slot##slot_id##_use_bounce_buffer, param_set_int, param_get_int, &(arg.slot[slot_id].bounce_buffer), perm); \
	module_param_call(sd##controller_id##_slot##slot_id##_cd_delay, param_set_int, param_get_int, &(arg.slot[slot_id].cd_delay), perm); \
	AMBA_GPIO_POWER_MODULE_PARAM_CALL(sd##controller_id##_slot##slot_id##_, arg.slot[slot_id].ext_power, perm); \
	AMBA_GPIO_IRQ_MODULE_PARAM_CALL(sd##controller_id##_slot##slot_id##_cd_, arg.slot[slot_id].gpio_cd, perm); \
	AMBA_GPIO_INPUT_MODULE_PARAM_CALL(sd##controller_id##_slot##slot_id##_wp_, arg.slot[slot_id].gpio_wp, perm)

#define AMBETH_MAC_SIZE				(6)
struct ambarella_eth_platform_info {
	u8					mac_addr[AMBETH_MAC_SIZE];
	int					napi_weight;
	int					max_work_count;
	int					mii_id;
	u32					phy_id;
	struct ambarella_gpio_power_info	mii_power;
	struct ambarella_gpio_reset_info	mii_reset;

	int					(*is_enabled)(void);
};
#define AMBA_ETH_PARAM_CALL(id, arg, perm) \
	module_param_call(eth##id##_napi_weight, param_set_int, param_get_int, &(arg.napi_weight), perm); \
	module_param_call(eth##id##_max_work_count, param_set_int, param_get_int, &(arg.max_work_count), perm); \
	module_param_call(eth##id##_mii_id, param_set_int, param_get_int, &(arg.mii_id), perm); \
	module_param_call(eth##id##_phy_id, param_set_uint, param_get_uint, &(arg.phy_id), perm); \
	AMBA_GPIO_POWER_MODULE_PARAM_CALL(eth##id##_mii_power_, arg.mii_power, perm); \
	AMBA_GPIO_RESET_MODULE_PARAM_CALL(eth##id##_mii_reset_, arg.mii_reset, perm)

#define SPI0_CS2_CS3_EN				0x00000002
struct ambarella_spi_cs_config {
	u8					bus_id;
	u8					cs_id;
	u8					cs_num;
	u8					cs_change;
	int					*cs_pins;
};
struct ambarella_spi_platform_info {
	int					use_interrupt;
	int					cs_num;
	int					*cs_pins;
	void    				(*cs_activate)  (struct ambarella_spi_cs_config *);
	void    				(*cs_deactivate)(struct ambarella_spi_cs_config *);
	void					(*rct_set_ssi_pll)(void);
	u32					(*get_ssi_freq_hz)(void);
};
#define AMBA_SPI_PARAM_CALL(id, arg, perm) \
	module_param_call(spi##id##_cs0, param_set_int, param_get_int, &(arg[0]), perm); \
	module_param_call(spi##id##_cs1, param_set_int, param_get_int, &(arg[1]), perm); \
	module_param_call(spi##id##_cs2, param_set_int, param_get_int, &(arg[2]), perm); \
	module_param_call(spi##id##_cs3, param_set_int, param_get_int, &(arg[3]), perm); \
	module_param_call(spi##id##_cs4, param_set_int, param_get_int, &(arg[4]), perm); \
	module_param_call(spi##id##_cs5, param_set_int, param_get_int, &(arg[5]), perm); \
	module_param_call(spi##id##_cs6, param_set_int, param_get_int, &(arg[6]), perm); \
	module_param_call(spi##id##_cs7, param_set_int, param_get_int, &(arg[7]), perm)

struct ambarella_idc_platform_info {
	int					clk_limit;	//Hz
	int					bulk_write_num;
	unsigned int				i2c_class;
	void					(*set_pin_muxing)(u32 on);
};
#define AMBA_IDC_PARAM_CALL(id, arg, perm) \
	module_param_call(idc##id##_clk_limit, param_set_int, param_get_int, &(arg.clk_limit), perm); \
	module_param_call(idc##id##_bulk_write_num, param_set_int, param_get_int, &(arg.bulk_write_num), perm)

struct ambarella_i2s_controller {
	void					(*aucodec_digitalio)(void);
	void					(*channel_select)(u32);
	void					(*set_audio_pll)(u8);
};

struct ambarella_udc_controller {
	void					(*set_pll)(void);
};

extern struct proc_dir_entry *get_ambarella_proc_dir(void);

struct ambarella_ir_controller {
	void					(*set_pll)(void);
	u32					(*get_pll)(void);
};

struct ambarella_wdt_controller {
	u32					(*get_pll)(void);
};
#endif /* __ASSEMBLER__ */

#define	AMBA_DEBUG_NULL			(0)
#define	AMBA_DEBUG_IAV			(1 << 0)
#define	AMBA_DEBUG_DSP			(1 << 1)
#define	AMBA_DEBUG_VIN			(1 << 2)
#define	AMBA_DEBUG_VOUT			(1 << 3)
#define	AMBA_DEBUG_AAA			(1 << 4)

#define	AMBA_DEV_MAJOR			(248)
#define	AMBA_DEV_MINOR_PUBLIC_START	(128)
#define	AMBA_DEV_MINOR_PUBLIC_END	(240)

#endif

