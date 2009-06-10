/*
 * drivers/input/misc/ambarella_input.h
 *
 * Author: Anthony Ginger <hfjiang@ambarella.com>
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

#ifndef __AMBARELLA_INPUT_H__
#define __AMBARELLA_INPUT_H__

#define CONFIG_AMBARELLA_INPUT_SIZE	(256)

#define AMBA_INPUT_SOURCE_MASK	(0x0F)
#define AMBA_INPUT_SOURCE_IR	(0x01)
#define AMBA_INPUT_SOURCE_ADC	(0x02)
#define AMBA_INPUT_SOURCE_GPIO	(0x04)
#define AMBA_INPUT_SOURCE_VI	(0x08)

#define AMBA_INPUT_TYPE_MASK	(0xF0)
#define AMBA_INPUT_TYPE_KEY	(0x10)
#define AMBA_INPUT_TYPE_REL	(0x20)
#define AMBA_INPUT_TYPE_ABS	(0x40)

#define	AMBA_INPUT_IR_KEY	(AMBA_INPUT_SOURCE_IR | AMBA_INPUT_TYPE_KEY)
#define	AMBA_INPUT_IR_REL	(AMBA_INPUT_SOURCE_IR | AMBA_INPUT_TYPE_REL)
#define	AMBA_INPUT_IR_ABS	(AMBA_INPUT_SOURCE_IR | AMBA_INPUT_TYPE_ABS)
#define	AMBA_INPUT_ADC_KEY	(AMBA_INPUT_SOURCE_ADC | AMBA_INPUT_TYPE_KEY)
#define	AMBA_INPUT_ADC_REL	(AMBA_INPUT_SOURCE_ADC | AMBA_INPUT_TYPE_REL)
#define	AMBA_INPUT_ADC_ABS	(AMBA_INPUT_SOURCE_ADC | AMBA_INPUT_TYPE_ABS)
#define	AMBA_INPUT_GPIO_KEY	(AMBA_INPUT_SOURCE_GPIO | AMBA_INPUT_TYPE_KEY)
#define	AMBA_INPUT_GPIO_REL	(AMBA_INPUT_SOURCE_GPIO | AMBA_INPUT_TYPE_REL)
#define	AMBA_INPUT_GPIO_ABS	(AMBA_INPUT_SOURCE_GPIO | AMBA_INPUT_TYPE_ABS)
#define	AMBA_INPUT_VI_KEY	(AMBA_INPUT_SOURCE_VI | AMBA_INPUT_TYPE_KEY)
#define	AMBA_INPUT_VI_REL	(AMBA_INPUT_SOURCE_VI | AMBA_INPUT_TYPE_REL)
#define	AMBA_INPUT_VI_ABS	(AMBA_INPUT_SOURCE_VI | AMBA_INPUT_TYPE_ABS)

#define	AMBA_INPUT_END			(0xFFFFFFFF)

enum ambarella_ir_protocol {
	AMBA_IR_PROTOCOL_NEC,
	AMBA_IR_PROTOCOL_PANASONIC,
	AMBA_IR_PROTOCOL_SONY,
	AMBA_IR_PROTOCOL_PHILIPS,
	AMBA_IR_PROTOCOL_END
};

struct ambarella_key_table {
	u32					type;
	union {
	struct {
		u32				key_code;
		u32				key_flag;
		u32				raw_id;
	} ir_key;
	struct {
		u32				key_code;
		s32				rel_step;
		u32				raw_id;
	} ir_rel;
	struct {
		s32				abs_x;
		s32				abs_y;
		u32				raw_id;
	} ir_abs;
	struct {
		u32				key_code;
		u16				reserve;
		u16				chan;
		u16				low_level;
		u16				high_level;
	} adc_key;
	struct {
		u16				key_code;
		s16				rel_step;
		u16				reserve;
		u16				chan;
		u16				low_level;
		u16				high_level;
	} adc_rel;
	struct {
		s16				abs_x;
		s16				abs_y;
		u16				reserve;
		u16				chan;
		u16				low_level;
		u16				high_level;
	} adc_abs;
	struct {
		u32				key_code;
		u32				avtive_val;
		u16				reserve;
		u8				id;
		u8				irq_mode;
	} gpio_key;
	struct {
		u32				key_code;
		s32				rel_step;
		u16				reserve;
		u8				id;
		u8				irq_mode;
	} gpio_rel;
	struct {
		s32				abs_x;
		s32				abs_y;
		u16				reserve;
		u8				id;
		u8				irq_mode;
	} gpio_abs;
	struct {
		u32				reserve;
		u32				reserve0;
		u32				reserve1;
	} vi_key;
	struct {
		u32				reserve;
		u32				reserve0;
		u32				reserve1;
	} vi_rel;
	struct {
		u32				reserve;
		u32				reserve0;
		u32				reserve1;
	} vi_abs;
	};
};

#ifdef __KERNEL__

#define CONFIG_AMBARELLA_INPUT_STR_SIZE	(32)
#define MAX_IR_BUFFER			(512)
struct ambarella_ir_info {
	unsigned char __iomem 		*regbase;

	u32				id;
	struct input_dev		*dev;
	struct resource			*mem;
	unsigned int			irq;
	unsigned int			gpio_id;

	char				name[CONFIG_AMBARELLA_INPUT_STR_SIZE];
	char				phys[CONFIG_AMBARELLA_INPUT_STR_SIZE];
	char				uniq[CONFIG_AMBARELLA_INPUT_STR_SIZE];

	int				(*ir_parse)(struct ambarella_ir_info *pinfo, u32 *uid);
	int				ir_pread;
	int				ir_pwrite;
	u16				tick_buf[MAX_IR_BUFFER];
	struct ambarella_key_table	*pkeymap;

	struct workqueue_struct		*workqueue;

	u32				init_adc;
	struct delayed_work		detect_adc;
	u32				last_adc_key[ADC_MAX_INSTANCES];
	u32				last_ir_uid;
	u32				last_ir_flag;
};

int ambarella_ir_nec_parse(struct ambarella_ir_info *pinfo, u32 *uid);
int ambarella_ir_panasonic_parse(struct ambarella_ir_info *pinfo, u32 *uid);
int ambarella_ir_sony_parse(struct ambarella_ir_info *pinfo, u32 *uid);
int ambarella_ir_philips_parse(struct ambarella_ir_info *pinfo, u32 *uid);

int ambarella_ir_inc_read_ptr(struct ambarella_ir_info *pinfo);
int ambarella_ir_move_read_ptr(struct ambarella_ir_info *pinfo, int offset);
u16 ambarella_ir_read_data(struct ambarella_ir_info *pinfo, int pointer);

#ifdef CONFIG_INPUT_AMBARELLA_DEBUG
#define ambi_dbg(format, arg...)		\
	dev_printk(KERN_DEBUG , pinfo->dev->dev.parent , format , ## arg)
#else
static inline int __attribute__ ((format (printf, 1, 2)))
ambi_dbg(const char * fmt, ...)
{
	return 0;
}
#endif

#define ambi_err(format, arg...)		\
	dev_printk(KERN_ERR , pinfo->dev->dev.parent , format , ## arg)
#define ambi_info(format, arg...)		\
	dev_printk(KERN_INFO , pinfo->dev->dev.parent , format , ## arg)
#define ambi_warn(format, arg...)		\
	dev_printk(KERN_WARNING , pinfo->dev->dev.parent , format , ## arg)
#define ambi_notice(format, arg...)		\
	dev_printk(KERN_NOTICE , pinfo->dev->dev.parent , format , ## arg)

#ifdef CONFIG_AMBARELLA_INPUT_KEY_SYNC_SUPPORT
#define AMBI_CAN_SYNC()		(input_sync(pinfo->dev))
#else
#define AMBI_CAN_SYNC()
#endif
#define AMBI_MUST_SYNC()	(input_sync(pinfo->dev))

#endif	//__KERNEL__

#endif	//__AMBARELLA_INPUT_H__

