/**
 * Microchip gigabit switch common sysfs code
 *
 * Copyright (c) 2015-2019 Microchip Technology Inc.
 *	Tristram Ha <Tristram.Ha@microchip.com>
 *
 * Copyright (c) 2011-2014 Micrel, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/cache.h>
#include <linux/crc32.h>
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>

#include "ksz9477_reg.h"
#include "ksz_common.h"
#include "ksz_sw_sysfs.h"

#ifndef get_private_data

struct sw_attributes {
	int linkmd;
	int vlan_member;
	int authen_mode;
	int acl;
	int acl_first_rule;
	int acl_ruleset;
	int acl_mode;
	int acl_enable;
	int acl_src;
	int acl_equal;
	int acl_addr;
	int acl_type;
	int acl_cnt;
	int acl_msec;
	int acl_intr_mode;
	int acl_ip_addr;
	int acl_ip_mask;
	int acl_protocol;
	int acl_seqnum;
	int acl_port_mode;
	int acl_max_port;
	int acl_min_port;
	int acl_tcp_flag_enable;
	int acl_tcp_flag;
	int acl_tcp_flag_mask;
	int acl_prio_mode;
	int acl_prio;
	int acl_vlan_prio_replace;
	int acl_vlan_prio;
	int acl_map_mode;
	int acl_ports;
	int acl_index;
	int acl_act_index;
	int acl_act;
	int acl_rule_index;
	int acl_info;
	int acl_table;
};

#define DEV_NAME_SIZE			32

struct ksz_dev_attr {
	struct device_attribute dev_attr;
	char dev_name[DEV_NAME_SIZE];
};

static void get_private_data_(struct device *d, struct semaphore **proc_sem,
	struct ksz_device **dev)
{
	if (d->bus && (
	    d->bus == &spi_bus_type ||
	    d->bus == &i2c_bus_type ||
	    d->bus == &platform_bus_type
	    )) {
		*dev = dev_get_drvdata(d);
		*proc_sem = &(*dev)->proc_sem;
	}
}

#define get_private_data	get_private_data_
#endif

#ifndef get_num_val
static int get_num_val_(const char *buf)
{
	int num = -1;

	if ('0' == buf[0] && 'x' == buf[1])
		sscanf(&buf[2], "%x", (unsigned int *) &num);
	else if ('0' == buf[0] && 'b' == buf[1]) {
		int i = 2;

		num = 0;
		while (buf[i]) {
			num <<= 1;
			num |= buf[i] - '0';
			i++;
		}
	} else if ('0' == buf[0] && 'd' == buf[1])
		sscanf(&buf[2], "%u", &num);
	else
		sscanf(buf, "%d", &num);
	return num;
}  /* get_num_val */

#define get_num_val		get_num_val_
#endif

static int alloc_dev_attr(struct attribute **attrs, size_t attr_size, int item,
	struct ksz_dev_attr **ksz_attrs, struct attribute ***item_attrs,
	char *item_name, struct ksz_dev_attr **attrs_ptr)
{
	struct attribute **attr_ptr;
	struct device_attribute *dev_attr;
	struct ksz_dev_attr *new_attr;

	*item_attrs = kmalloc(attr_size * sizeof(void *), GFP_KERNEL);
	if (!*item_attrs)
		return -ENOMEM;

	attr_size--;
	attr_size *= sizeof(struct ksz_dev_attr);
	*ksz_attrs = *attrs_ptr;
	*attrs_ptr += attr_size / sizeof(struct ksz_dev_attr);

	new_attr = *ksz_attrs;
	attr_ptr = *item_attrs;
	while (*attrs != NULL) {
		if (item_name && !strcmp((*attrs)->name, item_name))
			break;
		dev_attr = container_of(*attrs, struct device_attribute, attr);
		memcpy(new_attr, dev_attr, sizeof(struct device_attribute));
		strncpy(new_attr->dev_name, (*attrs)->name, DEV_NAME_SIZE);
		if (0 <= item && item < TOTAL_PORT_NUM)
			new_attr->dev_name[0] += item + 1;
		new_attr->dev_attr.attr.name = new_attr->dev_name;
		*attr_ptr = &new_attr->dev_attr.attr;
		new_attr++;
		attr_ptr++;
		attrs++;
	}
	*attr_ptr = NULL;
	return 0;
}

static char *sw_name[] = {
	"lan1",
	"lan2",
	"lan3",
	"lan4",
	"lan5",
	"lan6",
	"lan7",
};

enum {
	PROC_SET_LINK_MD,
	PROC_SET_VLAN_MEMBER,
	PROC_SET_AUTHEN_MODE,
	PROC_SET_ACL,
	PROC_SET_ACL_FIRST_RULE,
	PROC_SET_ACL_RULESET,
	PROC_SET_ACL_MODE,
	PROC_SET_ACL_ENABLE,
	PROC_SET_ACL_SRC,
	PROC_SET_ACL_EQUAL,
	PROC_SET_ACL_MAC_ADDR,
	PROC_SET_ACL_TYPE,
	PROC_SET_ACL_CNT,
	PROC_SET_ACL_MSEC,
	PROC_SET_ACL_INTR_MODE,
	PROC_SET_ACL_IP_ADDR,
	PROC_SET_ACL_IP_MASK,
	PROC_SET_ACL_PROTOCOL,
	PROC_SET_ACL_SEQNUM,
	PROC_SET_ACL_PORT_MODE,
	PROC_SET_ACL_MAX_PORT,
	PROC_SET_ACL_MIN_PORT,
	PROC_SET_ACL_TCP_FLAG_ENABLE,
	PROC_SET_ACL_TCP_FLAG,
	PROC_SET_ACL_TCP_FLAG_MASK,
	PROC_SET_ACL_PRIO_MODE,
	PROC_SET_ACL_PRIO,
	PROC_SET_ACL_VLAN_PRIO_REPLACE,
	PROC_SET_ACL_VLAN_PRIO,
	PROC_SET_ACL_MAP_MODE,
	PROC_SET_ACL_PORTS,
	PROC_SET_ACL_INDEX,
	PROC_SET_ACL_ACTION_INDEX,
	PROC_SET_ACL_ACTION,
	PROC_SET_ACL_RULE_INDEX,
	PROC_SET_ACL_INFO,
	PROC_GET_ACL_TABLE,

	PROC_SW_PORT_LAST,
};


enum {
	SHOW_HELP_NONE,
	SHOW_HELP_ON_OFF,
	SHOW_HELP_NUM,
	SHOW_HELP_HEX,
	SHOW_HELP_HEX_2,
	SHOW_HELP_HEX_4,
	SHOW_HELP_HEX_8,
	SHOW_HELP_SPECIAL,
};

static char *help_formats[] = {
	"",
	"%d%s\n",
	"%u%s\n",
	"0x%x%s\n",
	"0x%02x%s\n",
	"0x%04x%s\n",
	"0x%08x%s\n",
	"%d%s\n",
};

static char *display_strs[] = {
	" (off)",
	" (on)",
};

static char *show_on_off(uint on)
{
	if (on <= 1)
		return display_strs[on];
	return NULL;
}  /* show_on_off */

static ssize_t sysfs_show(ssize_t len, char *buf, int type, int chk, char *ptr)
{
	if (type) {
		if (SHOW_HELP_ON_OFF == type)
			ptr = show_on_off(chk);
		len += sprintf(buf + len, help_formats[type], chk, ptr);
	}
	return len;
}  /* sysfs_show */

/* -------------------------------------------------------------------------- */

/* LINKMD */

enum {
	CABLE_UNKNOWN,
	CABLE_GOOD,
	CABLE_CROSSED,
	CABLE_REVERSED,
	CABLE_CROSSED_REVERSED,
	CABLE_OPEN,
	CABLE_SHORT
};

#define STATUS_FULL_DUPLEX		0x01
#define STATUS_CROSSOVER		0x02
#define STATUS_REVERSED			0x04

#define CABLE_LEN_MAXIMUM		15000
#define CABLE_LEN_MULTIPLIER		8

#define PHY_RESET_TIMEOUT		10

/**
 * delay_milli - delay in millisecond
 * @millisec:	Number of milliseconds to delay.
 *
 * This routine delays in milliseconds.
 */
static inline void delay_milli(uint millisec)
{
	unsigned long ticks = millisec * HZ / 1000;

	if (!ticks || in_interrupt())
		mdelay(millisec);
	else {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(ticks);
	}
}

/**
 * sw_get_link_md -
 * @sw:		The switch instance.
 * @port:	The port index.
 *
 * This routine is used to get the LinkMD status.
 */
static void sw_get_link_md(struct ksz_device *sw, uint port, struct ksz_cable_info *cable_info)
{
	u16 ctrl;
	u16 data;
	u16 giga;
	u16 link;
	u32 len;
	int i;
	int timeout;

	ksz_pread16(sw, port, P_LINK_STATUS, &link);

	/* Read second time in case the status is not latched. */
	if (!(link & PORT_LINK_STATUS))
		ksz_pread16(sw, port, P_LINK_STATUS, &link);
	ksz_pread16(sw, port, REG_PORT_PHY_DIGITAL_STATUS, &data);
	cable_info->status[0] = CABLE_UNKNOWN;
	if (link & PORT_LINK_STATUS) {
		int stat = 0;

		cable_info->status[0] = CABLE_GOOD;
		cable_info->length[0] = 1;
		cable_info->status[1] = CABLE_GOOD;
		cable_info->length[1] = 1;
		cable_info->status[2] = CABLE_GOOD;
		cable_info->length[2] = 1;
		cable_info->status[3] = CABLE_GOOD;
		cable_info->length[3] = 1;
		cable_info->status[4] = CABLE_GOOD;
		cable_info->length[4] = 1;

		if (!(data & PORT_PHY_STAT_MDI))
			stat |= STATUS_CROSSOVER;
#if 0
		if (data & PORT_REVERSED_POLARITY)
			stat |= STATUS_REVERSED;
#endif
		if ((stat & (STATUS_CROSSOVER | STATUS_REVERSED)) ==
				(STATUS_CROSSOVER | STATUS_REVERSED))
			cable_info->status[0] = CABLE_CROSSED_REVERSED;
		else if ((stat & STATUS_CROSSOVER) == STATUS_CROSSOVER)
			cable_info->status[0] = CABLE_CROSSED;
		else if ((stat & STATUS_REVERSED) == STATUS_REVERSED)
			cable_info->status[0] = CABLE_REVERSED;
		return;
	}

	/* Put in 1000 Mbps mode. */
	ksz_pread16(sw, port, P_PHY_CTRL, &ctrl);
	data = PORT_FULL_DUPLEX | PORT_SPEED_1000MBIT;
	ksz_pwrite16(sw, port, P_PHY_CTRL, data);
	ksz_pread16(sw, port, REG_PORT_PHY_1000_CTRL, &giga);
	data = PORT_AUTO_NEG_MANUAL;
	ksz_pwrite16(sw, port, REG_PORT_PHY_1000_CTRL, data);

	ksz_pread16(sw, port, REG_PORT_PHY_LINK_MD, &data);

	/* Disable transmitter. */
	data |= PORT_TX_DISABLE;
	ksz_pwrite16(sw, port, REG_PORT_PHY_LINK_MD, data);

	/* Wait at most 1 second.*/
	delay_milli(100);

	/* Enable transmitter. */
	data &= ~PORT_TX_DISABLE;
	ksz_pwrite16(sw, port, REG_PORT_PHY_LINK_MD, data);

	for (i = 1; i <= 4; i++) {

		/* Start cable diagnostic test. */
		data |= PORT_START_CABLE_DIAG;
		data |= (i - 1) << PORT_CABLE_DIAG_PAIR_S;
		ksz_pwrite16(sw, port, REG_PORT_PHY_LINK_MD, data);
		timeout = PHY_RESET_TIMEOUT;
		do {
			if (!--timeout)
				break;
			delay_milli(10);
			ksz_pread16(sw, port, REG_PORT_PHY_LINK_MD, &data);
		} while ((data & PORT_START_CABLE_DIAG));

		cable_info->length[i] = 0;
		cable_info->status[i] = CABLE_UNKNOWN;

		if (!(data & PORT_START_CABLE_DIAG)) {
			len = data & PORT_CABLE_FAULT_COUNTER;
			if (len >= 22)
				len -= 22;
			else
				len = 0;
			len *= CABLE_LEN_MULTIPLIER;
			len += 5;
			// len /= 10;
			cable_info->length[i] = len;
			data >>= PORT_CABLE_DIAG_RESULT_S;
			data &= PORT_CABLE_DIAG_RESULT_M;
			switch (data) {
			case PORT_CABLE_STAT_NORMAL:
				cable_info->status[i] = CABLE_GOOD;
				cable_info->length[i] = 1;
				break;
			case PORT_CABLE_STAT_OPEN:
				cable_info->status[i] = CABLE_OPEN;
				break;
			case PORT_CABLE_STAT_SHORT:
				cable_info->status[i] = CABLE_SHORT;
				break;
			}
		}
	}

	ksz_pwrite16(sw, port, REG_PORT_PHY_1000_CTRL, giga);
	ksz_pwrite16(sw, port, P_PHY_CTRL, ctrl);
	if (ctrl & PORT_AUTO_NEG_ENABLE) {
		ctrl |= PORT_AUTO_NEG_RESTART;
		ksz_pwrite16(sw, port, P_NEG_RESTART_CTRL, ctrl);
	}

	cable_info->length[0] = cable_info->length[1];
	cable_info->status[0] = cable_info->status[1];
	for (i = 2; i < 5; i++) {
		if (CABLE_GOOD == cable_info->status[0]) {
			if (cable_info->status[i] != CABLE_GOOD) {
				cable_info->status[0] = cable_info->status[i];
				cable_info->length[0] = cable_info->length[i];
				break;
			}
		}
	}
}

static u16 sw_get_non_tag_vlan(struct ksz_device *sw, uint p)
{
	struct ksz_port *port = &sw->ports[p];
	return port->non_tag_vlan_member;
}  /* sw_get_non_tag_vlan */

/**
 * sw_cfg_non_tag_vlan - configure port-based VLAN membership
 * @sw:		The switch instance.
 * @port:	The port index.
 * @member:	The port-based VLAN membership.
 *
 * This routine configures the port-based VLAN membership of the port.
 */
static void sw_cfg_non_tag_vlan(struct ksz_device *sw, uint p, u16 member)
{
	struct ksz_port *port = &sw->ports[p];
	port->non_tag_vlan_member = member | sw->host_mask | port->vid_member;
	ksz_pwrite32(sw, p, REG_PORT_VLAN_MEMBERSHIP__4, port->non_tag_vlan_member);
}  /* sw_cfg_non_tag_vlan */

/* -------------------------------------------------------------------------- */

/* ACL */

static inline void port_cfg_acl(struct ksz_device *sw, uint p, bool set)
{
	u8 data;
	ksz_pread8(sw, p, REG_PORT_MRI_AUTHEN_CTRL, &data);
	if (set)
		data |= PORT_ACL_ENABLE;
	else
		data &= ~PORT_ACL_ENABLE;
	ksz_pwrite8(sw, p, REG_PORT_MRI_AUTHEN_CTRL, data);
}

static inline int port_chk_acl(struct ksz_device *sw, uint p)
{
	u8 data;
	ksz_pread8(sw, p, REG_PORT_MRI_AUTHEN_CTRL, &data);
	return ((data & PORT_ACL_ENABLE) > 0)? 1 : 0;
}

static inline u8 port_get_authen_mode(struct ksz_device *sw, uint p)
{
	u8 data;
	ksz_pread8(sw, p, REG_PORT_MRI_AUTHEN_CTRL, &data);
	data &= PORT_AUTHEN_MODE;
	return data;
}

static void port_set_authen_mode(struct ksz_device *sw, uint p, u8 mode)
{
	u8 data;
	ksz_pread8(sw, p, REG_PORT_MRI_AUTHEN_CTRL, &data);
	data &= ~PORT_AUTHEN_MODE;
	data |= (mode & PORT_AUTHEN_MODE);
	ksz_pwrite8(sw, p, REG_PORT_MRI_AUTHEN_CTRL, data);
}

/**
 * get_acl_action_info - Get ACL action field information
 * @acl:	The ACL entry.
 * @data:	The ACL data.
 *
 * This helper routine gets ACL action field information.
 */
static void get_acl_action_info(struct ksz_acl_table *acl, u8 data[])
{
	acl->prio_mode = (data[10] >> ACL_PRIO_MODE_S) & ACL_PRIO_MODE_M;
	acl->prio = (data[10] >> ACL_PRIO_S) & ACL_PRIO_M;
	acl->vlan_prio_replace = !!(data[10] & ACL_VLAN_PRIO_REPLACE);
	acl->vlan_prio = data[11] >> ACL_VLAN_PRIO_S;
	acl->vlan_prio |= (data[10] & ACL_VLAN_PRIO_HI_M) << 1;
	acl->map_mode = (data[11] >> ACL_MAP_MODE_S) & ACL_MAP_MODE_M;

	/* byte 12 is not used at all. */
	acl->ports = data[13];
}  /* get_acl_action_info */

/**
 * get_acl_table_info - Get ACL table information
 * @acl:	The ACL entry.
 * @data:	The ACL data.
 *
 * This helper routine gets ACL table information.
 */
static void get_acl_table_info(struct ksz_acl_table *acl, u8 data[])
{
	u16 *ptr_16;
	u32 *ptr_32;
	int i;
	int j;
	int cnt = 0;

	acl->first_rule = data[0] & ACL_FIRST_RULE_M;
	acl->mode = (data[1] >> ACL_MODE_S) & ACL_MODE_M;
	acl->enable = (data[1] >> ACL_ENABLE_S) & ACL_ENABLE_M;
	acl->src = !!(data[1] & ACL_SRC);
	acl->equal = !!(data[1] & ACL_EQUAL);
	switch (acl->mode) {
	case ACL_MODE_LAYER_2:
		for (i = 0; i < 6; i++)
			acl->mac[i] = data[2 + i];
		ptr_16 = (u16 *) &data[8];
		acl->eth_type = be16_to_cpu(*ptr_16);
		if (ACL_ENABLE_2_COUNT == acl->enable) {
			cnt = 1;
			ptr_16 = (u16 *) &data[10];
			acl->cnt = (be16_to_cpu(*ptr_16) >> ACL_CNT_S) &
				ACL_CNT_M;
			acl->msec =
				!!(data[ACL_INTR_CNT_START] & ACL_MSEC_UNIT);
			acl->intr_mode =
				!!(data[ACL_INTR_CNT_START] & ACL_INTR_MODE);
		}
		break;
	case ACL_MODE_LAYER_3:
		j = 2;
		for (i = 0; i < 4; i++, j++)
			acl->ip4_addr[i] = data[j];
		for (i = 0; i < 4; i++, j++)
			acl->ip4_mask[i] = data[j];
		break;
	case ACL_MODE_LAYER_4:
		switch (acl->enable) {
		case ACL_ENABLE_4_TCP_SEQN_COMP:
			ptr_32 = (u32 *) &data[2];
			acl->seqnum = be32_to_cpu(*ptr_32);
			break;
		default:
			ptr_16 = (u16 *) &data[2];
			acl->max_port = be16_to_cpu(*ptr_16);
			++ptr_16;
			acl->min_port = be16_to_cpu(*ptr_16);
			acl->port_mode = (data[6] >> ACL_PORT_MODE_S) &
				ACL_PORT_MODE_M;
		}
		acl->protocol = (data[6] & 1) << 7;
		acl->protocol |= (data[7] >> 1);
		acl->tcp_flag_enable = !!(data[7] & ACL_TCP_FLAG_ENABLE);
		acl->tcp_flag_mask = data[8];
		acl->tcp_flag = data[9];
		break;
	default:
		break;
	}
	if (!cnt)
		get_acl_action_info(acl, data);
	ptr_16 = (u16 *) &data[ACL_RULESET_START];
	acl->ruleset = be16_to_cpu(*ptr_16);
}  /* get_acl_table_info */

/**
 * set_acl_action_info - Set ACL action field information
 * @acl:	The ACL entry.
 * @data:	The ACL data.
 *
 * This helper routine sets ACL action field information.
 */
static void set_acl_action_info(struct ksz_acl_table *acl, u8 data[])
{
	if (acl->data[0] != 0xff)
		memcpy(data, acl->data, ACL_TABLE_LEN);
	data[10] = (acl->prio_mode & ACL_PRIO_MODE_M) << ACL_PRIO_MODE_S;
	data[10] |= (acl->prio & ACL_PRIO_M) << ACL_PRIO_S;
	if (acl->vlan_prio_replace)
		data[10] |= ACL_VLAN_PRIO_REPLACE;
	data[10] |= (acl->vlan_prio >> 1);
	data[11] = acl->vlan_prio << ACL_VLAN_PRIO_S;
	data[11] |= (acl->map_mode & ACL_MAP_MODE_M) << ACL_MAP_MODE_S;

	/* byte 12 is not used at all. */
	data[13] = acl->ports;
}  /* set_acl_action_info */

/**
 * set_acl_ruleset_info - Set ACL ruleset field information
 * @acl:	The ACL entry.
 * @data:	The ACL data.
 *
 * This helper routine sets ACL ruleset field information.
 */
static void set_acl_ruleset_info(struct ksz_acl_table *acl, u8 data[])
{
	u16 *ptr_16;

	if (acl->data[0] != 0xff)
		memcpy(data, acl->data, ACL_TABLE_LEN);
	data[0] = acl->first_rule & ACL_FIRST_RULE_M;
	ptr_16 = (u16 *) &data[ACL_RULESET_START];
	*ptr_16 = cpu_to_be16(acl->ruleset);
}  /* set_acl_ruleset_info */

/**
 * set_acl_table_info - Set ACL table information
 * @acl:	The ACL entry.
 * @data:	The ACL data.
 *
 * This helper routine sets ACL table information.
 */
static void set_acl_table_info(struct ksz_acl_table *acl, u8 data[])
{
	u16 *ptr_16;
	u32 *ptr_32;
	int i;
	int j;

	if (acl->data[0] != 0xff)
		memcpy(data, acl->data, ACL_TABLE_LEN);
	data[1] = (acl->mode & ACL_MODE_M) << ACL_MODE_S;
	data[1] |= (acl->enable & ACL_ENABLE_M) << ACL_ENABLE_S;
	if (acl->src)
		data[1] |= ACL_SRC;
	if (acl->equal)
		data[1] |= ACL_EQUAL;
	switch (acl->mode) {
	case ACL_MODE_LAYER_2:
		for (i = 0; i < 6; i++)
			data[2 + i] = acl->mac[i];
		ptr_16 = (u16 *) &data[8];
		*ptr_16 = cpu_to_be16(acl->eth_type);
		if (ACL_ENABLE_2_COUNT == acl->enable) {
			data[ACL_INTR_CNT_START] = 0;
			ptr_16 = (u16 *) &data[10];
			*ptr_16 = cpu_to_be16((acl->cnt & ACL_CNT_M) <<
				ACL_CNT_S);
			data[12] = 0;
			if (acl->msec)
				data[ACL_INTR_CNT_START] |= ACL_MSEC_UNIT;
			if (acl->intr_mode)
				data[ACL_INTR_CNT_START] |= ACL_INTR_MODE;
		}
		break;
	case ACL_MODE_LAYER_3:
		j = 2;
		for (i = 0; i < 4; i++, j++)
			data[j] = acl->ip4_addr[i];
		for (i = 0; i < 4; i++, j++)
			data[j] = acl->ip4_mask[i];
		break;
	case ACL_MODE_LAYER_4:
		switch (acl->enable) {
		case ACL_ENABLE_4_TCP_SEQN_COMP:
			ptr_32 = (u32 *) &data[2];
			*ptr_32 = cpu_to_be32(acl->seqnum);
			data[6] = 0;
			break;
		default:
			ptr_16 = (u16 *) &data[2];
			*ptr_16 = cpu_to_be16(acl->max_port);
			++ptr_16;
			*ptr_16 = cpu_to_be16(acl->min_port);
			data[6] = (acl->port_mode & ACL_PORT_MODE_M) <<
				ACL_PORT_MODE_S;
		}
		data[6] |= (acl->protocol >> 7);
		data[7] = (acl->protocol << 1);
		if (acl->tcp_flag_enable)
			data[7] |= ACL_TCP_FLAG_ENABLE;
		data[8] = acl->tcp_flag_mask;
		data[9] = acl->tcp_flag;
		break;
	default:
		break;
	}
}  /* set_acl_table_info */

/**
 * wait_for_acl_table - Wait for ACL table
 * @sw:		The switch instance.
 * @port:	The port index.
 *
 * This helper function waits for ACL table to be ready for access.
 */
static int wait_for_acl_table(struct ksz_device *sw, uint port)
{
	u8 ctrl;
	int timeout = 100;

	// if (!port_chk_acl(sw, port))
	// 	dbg_msg("acl not on: %d", port);
	do {
		--timeout;
		if (!timeout)
			return 1;
		ksz_pread8(sw, port, REG_PORT_ACL_CTRL_0, &ctrl);
	} while (!(ctrl & (PORT_ACL_WRITE_DONE | PORT_ACL_READ_DONE)));
	return 0;
}  /* wait_for_acl_table */

/**
 * sw_r_acl_hw - read from ACL table using default access
 * @sw:		The switch instance.
 * @port:	The port index.
 * @addr:	The ACL index.
 * @data:	Buffer to hold the ACL data.
 *
 * This function reads from ACL table of the port using default access.
 */
static int sw_r_acl_hw(struct ksz_device *sw, uint port, u16 addr, u8 data[])
{
	int i;
	u8 ctrl = (addr & PORT_ACL_INDEX_M);

	ksz_pwrite8(sw, port, REG_PORT_ACL_CTRL_0, ctrl);
	do {
		ksz_pread8(sw, port, REG_PORT_ACL_CTRL_0, &ctrl);
	} while (!(ctrl & PORT_ACL_READ_DONE));
	for (i = 0; i < ACL_TABLE_LEN; i++) {
		ksz_pread8(sw, port, REG_PORT_ACL_0 + i, &data[i]);
	}
	return 0;
}  /* sw_r_acl_hw */

/**
 * sw_w_acl_hw - write to ACL table using default access
 * @sw:		The switch instance.
 * @port:	The port index.
 * @addr:	The ACL index.
 * @data:	The ACL data to write.
 *
 * This function writes to ACL table of the port using default access.
 */
static int sw_w_acl_hw(struct ksz_device *sw, uint port, u16 addr, u8 data[])
{
	int i;
	u8 ctrl = (addr & PORT_ACL_INDEX_M) | PORT_ACL_WRITE;

	for (i = 0; i < ACL_TABLE_LEN; i++) {
		ksz_pwrite8(sw, port, REG_PORT_ACL_0 + i, data[i]);
	}
	ksz_pwrite8(sw, port, REG_PORT_ACL_CTRL_0, ctrl);
	do {
		ksz_pread8(sw, port, REG_PORT_ACL_CTRL_0, &ctrl);
	} while (!(ctrl & PORT_ACL_WRITE_DONE));
	return 0;
}  /* sw_w_acl_hw */

/**
 * sw_r_acl_table - read from ACL table
 * @sw:		The switch instance.
 * @port:	The port index.
 * @addr:	The address of the table entry.
 * @acl:	Buffer to store the ACL entry.
 *
 * This function reads an entry of the ACL table of the port.
 *
 * Return 0 if the entry is valid; otherwise -1.
 */
static int sw_r_acl_table(struct ksz_device *sw, uint port, u16 addr,
	struct ksz_acl_table *acl)
{
	u8 data[20];
	int rc = -1;

	if (wait_for_acl_table(sw, port))
		goto r_acl_exit;
	ksz_pwrite16(sw, port, REG_PORT_ACL_BYTE_EN_MSB, ACL_BYTE_ENABLE);
	sw_r_acl_hw(sw, port, addr, data);
	get_acl_table_info(acl, data);
	memcpy(acl->data, data, ACL_TABLE_LEN);

r_acl_exit:
	if (acl->mode)
		rc = 0;
	acl->changed = 0;
	return rc;
}  /* sw_r_acl_table */

/**
 * sw_w_acl_action - write to ACL action field
 * @sw:		The switch instance.
 * @port:	The port index.
 * @addr:	The address of the table entry.
 * @acl:	The ACL entry.
 *
 * This routine writes to the action field of an entry of the ACL table.
 */
static void sw_w_acl_action(struct ksz_device *sw, uint port, u16 addr,
	struct ksz_acl_table *acl)
{
	u8 data[20];

	if (wait_for_acl_table(sw, port))
		goto w_acl_act_exit;
	set_acl_action_info(acl, data);
	ksz_pwrite16(sw, port, REG_PORT_ACL_BYTE_EN_MSB, ACL_ACTION_ENABLE);
	sw_w_acl_hw(sw, port, addr, data);
	memcpy(&acl->data[ACL_ACTION_START], &data[ACL_ACTION_START],
		ACL_ACTION_LEN);

w_acl_act_exit:
	acl->action_changed = 0;
}  /* sw_w_acl_action */

/**
 * sw_w_acl_ruleset - write to ACL ruleset field
 * @sw:		The switch instance.
 * @port:	The port index.
 * @addr:	The address of the table entry.
 * @acl:	The ACL entry.
 *
 * This routine writes to the ruleset field of an entry of the ACL table.
 */
static void sw_w_acl_ruleset(struct ksz_device *sw, uint port, u16 addr,
	struct ksz_acl_table *acl)
{
	u8 data[20];

	if (wait_for_acl_table(sw, port))
		goto w_acl_ruleset_exit;
	set_acl_ruleset_info(acl, data);
	ksz_pwrite16(sw, port, REG_PORT_ACL_BYTE_EN_MSB, ACL_RULESET_ENABLE);
	sw_w_acl_hw(sw, port, addr, data);

	/* First rule */
	acl->data[0] = data[0];
	memcpy(&acl->data[ACL_RULESET_START], &data[ACL_RULESET_START],
		ACL_RULESET_LEN);

w_acl_ruleset_exit:
	acl->ruleset_changed = 0;
}  /* sw_w_acl_ruleset */

/**
 * sw_w_acl_rule - write to ACL matching and process fields
 * @sw:		The switch instance.
 * @port:	The port index.
 * @addr:	The address of the table entry.
 * @acl:	The ACL entry.
 *
 * This routine writes to the matching and process fields of an entry of the
 * ACL table of the port.
 */
static void sw_w_acl_rule(struct ksz_device *sw, uint port, u16 addr,
	struct ksz_acl_table *acl)
{
	u8 data[20];
	u16 byte_enable = ACL_MATCH_ENABLE;
	int len = ACL_ACTION_START;

	if (ACL_MODE_LAYER_2 == acl->mode &&
	    ACL_ENABLE_2_COUNT == acl->enable) {
		byte_enable |= ACL_ACTION_ENABLE;
		len += ACL_ACTION_LEN;
	}
	if (wait_for_acl_table(sw, port))
		goto w_acl_rule_exit;
	set_acl_table_info(acl, data);
	ksz_pwrite16(sw, port, REG_PORT_ACL_BYTE_EN_MSB, byte_enable);
	sw_w_acl_hw(sw, port, addr, data);
	memcpy(acl->data, data, len);

w_acl_rule_exit:
	acl->changed = 0;
}  /* sw_w_acl_rule */

/**
 * sw_w_acl_table - write to ACL table
 * @sw:		The switch instance.
 * @port:	The port index.
 * @addr:	The address of the table entry.
 * @acl:	The ACL entry.
 *
 * This routine writes an entry of the ACL table of the port.
 */
static void sw_w_acl_table(struct ksz_device *sw, uint port, u16 addr,
	struct ksz_acl_table *acl)
{
	u8 data[20];

	acl->data[0] = 0xff;
	memset(data, 0, sizeof(data));
	if (wait_for_acl_table(sw, port))
		return;
	if (!(ACL_MODE_LAYER_2 == acl->mode &&
	    ACL_ENABLE_2_COUNT == acl->enable))
		set_acl_action_info(acl, data);
	set_acl_table_info(acl, data);
	set_acl_ruleset_info(acl, data);
	ksz_pwrite16(sw, port, REG_PORT_ACL_BYTE_EN_MSB, ACL_BYTE_ENABLE);
	sw_w_acl_hw(sw, port, addr, data);
	memcpy(acl->data, data, ACL_TABLE_LEN);
}  /* sw_w_acl_table */

/**
 * acl_action_info - format ACL action field information
 * @sw:		The switch instance.
 * @acl:	The ACL entry.
 * @index;	The entry index.
 * @buf:	Buffer to store the strings.
 * @len:	Lenght of buffer.
 *
 * This helper routine formats the ACL action field information.
 */
static int acl_action_info(struct ksz_device *sw, struct ksz_acl_table *acl,
			   u16 index, char *buf, int len)
{
	char prio = 'p';
	char vlan = 'v';
	uint ports = acl->ports;

	if (acl->prio_mode != ACL_PRIO_MODE_DISABLE)
		prio = 'P';
	if (acl->vlan_prio_replace)
		vlan = 'V';
	len += sprintf(buf + len,
		"%x: %c:%u=%u %c:%u=%u %u=%04x [%u]\n",
		index,
		prio, acl->prio_mode, acl->prio,
		vlan, acl->vlan_prio_replace, acl->vlan_prio,
		acl->map_mode, ports,
		acl->action_changed ? 8 : 1);
	return len;
}  /* acl_action_info */

/**
 * acl_ruleset_info - format ACL ruleset field information
 * @acl:	The ACL entry.
 * @index;	The entry index.
 * @buf:	Buffer to store the strings.
 * @len:	Lenght of buffer.
 *
 * This helper routine formats the ACL ruleset field information.
 */
static int acl_ruleset_info(struct ksz_acl_table *acl, u16 index, char *buf,
	int len)
{
	len += sprintf(buf + len,
		"%x: %x:%04x [%u]\n",
		index,
		acl->first_rule, acl->ruleset,
		acl->ruleset_changed ? 8 : 1);
	return len;
}  /* acl_ruleset_info */

/**
 * acl_info - format ACL matching and process field information
 * @acl:	The ACL entry.
 * @index;	The entry index.
 * @buf:	Buffer to store the strings.
 * @len:	Lenght of buffer.
 *
 * This helper routine formats the ACL matching and process field information.
 */
static int acl_info(struct ksz_acl_table *acl, u16 index, char *buf, int len)
{
	char enable = 'e';
	char equal = 'q';
	char src = 's';
	char cnt = 'c';
	char protocol = 'x';
	char flag = 'f';
	char seqnum = 's';
	char msec[4];

	switch (acl->mode) {
	case ACL_MODE_LAYER_2:
		enable = 'E';
		*msec = 0;
		if (ACL_ENABLE_2_COUNT == acl->enable) {
			equal = 'Q';
			src = 'S';
			cnt = 'C';
			if (acl->intr_mode)
				*msec = 0;
			else if (acl->msec)
				strcpy(msec, "ms ");
			else
				strcpy(msec, "us ");
		} else {
			equal = 'Q';
			if (ACL_ENABLE_2_TYPE != acl->enable)
				src = 'S';
		}
		len += sprintf(buf + len,
			"%x: %02X:%02X:%02X:%02X:%02X:%02X-%04x "
			"%c:%u.%u %s"
			"%c:%u %c:%u %c:%u "
			"[%u]\n",
			index,
			acl->mac[0], acl->mac[1], acl->mac[2],
			acl->mac[3], acl->mac[4], acl->mac[5],
			acl->eth_type,
			cnt, acl->intr_mode, acl->cnt, msec,
			enable, acl->enable,
			src, acl->src, equal, acl->equal,
			acl->changed ? 8 : acl->mode);
		break;
	case ACL_MODE_LAYER_3:
		if (ACL_ENABLE_3_IP == acl->enable ||
		    ACL_ENABLE_3_SRC_DST_COMP == acl->enable)
			enable = 'E';
		if (ACL_ENABLE_3_IP == acl->enable) {
			equal = 'Q';
			src = 'S';
		}
		len += sprintf(buf + len,
			"%x: %u.%u.%u.%u:%u.%u.%u.%u "
			"%c:%u %c:%u %c:%u "
			"[%u]\n",
			index,
			acl->ip4_addr[0], acl->ip4_addr[1],
			acl->ip4_addr[2], acl->ip4_addr[3],
			acl->ip4_mask[0], acl->ip4_mask[1],
			acl->ip4_mask[2], acl->ip4_mask[3],
			enable, acl->enable,
			src, acl->src, equal, acl->equal,
			acl->changed ? 8 : acl->mode);
		break;
	case ACL_MODE_LAYER_4:
		enable = 'E';
		if (ACL_ENABLE_4_PROTOCOL == acl->enable) {
			protocol = 'X';
			equal = 'Q';
		} else if (ACL_ENABLE_4_TCP_SEQN_COMP == acl->enable) {
			seqnum = 'S';
			equal = 'Q';
			if (acl->tcp_flag_enable)
				flag = 'F';
		} else if (ACL_ENABLE_4_TCP_PORT_COMP == acl->enable) {
			src = 'S';
			if (acl->tcp_flag_enable)
				flag = 'F';
		} else
			src = 'S';
		len += sprintf(buf + len,
			"%x: %u=%4x-%4x 0%c%x %c:%08x %c:%u=%x:%x "
			"%c:%u %c:%u %c:%u "
			"[%u]\n",
			index,
			acl->port_mode, acl->min_port, acl->max_port,
			protocol, acl->protocol, seqnum, acl->seqnum,
			flag, acl->tcp_flag_enable,
			acl->tcp_flag, acl->tcp_flag_mask,
			enable, acl->enable,
			src, acl->src, equal, acl->equal,
			acl->changed ? 8 : acl->mode);
		break;
	default:
		len += sprintf(buf + len,
			"%x: "
			"%c:%u %c:%u %c:%u "
			"[%u]\n",
			index,
			enable, acl->enable,
			src, acl->src, equal, acl->equal,
			acl->changed ? 8 : acl->mode);
		break;
	}
	return len;
}  /* acl_info */

/**
 * sw_d_acl_table - dump ACL table
 * @sw:		The switch instance.
 * @port:	The port index.
 *
 * This routine dumps ACL table of the port.
 */
static ssize_t sw_d_acl_table(struct ksz_device *sw, uint port, char *buf,
	ssize_t len)
{
	struct ksz_port *p;
	struct ksz_acl_table *acl;
	int i;
	int acl_on;
	int min = 0;

	p = &sw->ports[port];
	acl_on = port_chk_acl(sw, port);
	if (!acl_on) {
		printk(KERN_INFO "ACL not on for port %d", port);
		port_cfg_acl(sw, port, true);
	}
	for (i = 0; i < ACL_TABLE_ENTRIES; i++) {
		acl = &p->acl_info[i];
		acl->action_selected = false;
		sw_r_acl_table(sw, port, i, acl);
	}
	for (i = 0; i < ACL_TABLE_ENTRIES; i++) {
		acl = &p->acl_info[i];
		if (!acl->mode)
			continue;
		if (!min)
			len += sprintf(buf + len, "rules:\n");
		len = acl_info(acl, i, buf, len);
		min = 1;
	}
	if (min)
		len += sprintf(buf + len, "\n");
	min = 0;
	for (i = 0; i < ACL_TABLE_ENTRIES; i++) {
		acl = &p->acl_info[i];
		if (!acl->ruleset)
			continue;
		if (!min)
			len += sprintf(buf + len, "rulesets:\n");
		p->acl_info[acl->first_rule].action_selected = true;
		len = acl_ruleset_info(acl, i, buf, len);
		min = 1;
	}
	if (min)
		len += sprintf(buf + len, "\n");
	min = 0;
	for (i = 0; i < ACL_TABLE_ENTRIES; i++) {
		acl = &p->acl_info[i];
		if (ACL_PRIO_MODE_DISABLE == acl->prio_mode &&
		    ACL_MAP_MODE_DISABLE == acl->map_mode &&
		    !acl->ports &&
		    !acl->vlan_prio_replace && !acl->action_selected)
			continue;
		if (ACL_MODE_LAYER_2 == acl->mode &&
		    ACL_ENABLE_2_COUNT == acl->enable)
			continue;
		if (!min)
			len += sprintf(buf + len, "actions:\n");
		len = acl_action_info(sw, acl, i, buf, len);
		min = 1;
	}
	if (!acl_on) {
		port_cfg_acl(sw, port, false);
	}
	return len;
}  /* sw_d_acl_table */

static void sw_reset_acl(struct ksz_acl_table *acl)
{
	memset(acl, 0, sizeof(*acl));
}  /* sw_reset_acl */

static void sw_init_acl(struct ksz_device *sw)
{
	struct ksz_port *p;
	struct ksz_acl_table *acl;
	int i;
	uint n;
	uint port;

	for (n = 0; n < sw->mib_port_cnt; n++) {
		port_cfg_acl(sw, n, 1);
		p = &sw->ports[n];
		mutex_lock(&p->acllock);
		for (i = 0; i < ACL_TABLE_ENTRIES; i++) {
			acl = &p->acl_info[i];
			sw_r_acl_table(sw, port, i, acl);
		}
		/* Turn off ACL after reset. */
		port_cfg_acl(sw, port, 0);
		mutex_unlock(&p->acllock);
	}
}  /* sw_init_acl */

/* -------------------------------------------------------------------------- */

static ssize_t sysfs_port_read(struct ksz_device *sw, int proc_num, uint n,
	ssize_t len, char *buf)
{
	struct ksz_port *p;
	struct ksz_cable_info *cable_info;
	struct ksz_acl_table *acl;
	struct ksz_acl_table *action;
	struct ksz_acl_table *ruleset;
	int chk = 0;
	int type = SHOW_HELP_NONE;
	char note[40];

	note[0] = '\0';
	p = &sw->ports[n];
	cable_info = &p->cable_info;
	acl = &p->acl_info[p->acl_index];
	action = &p->acl_info[p->acl_act_index];
	ruleset = &p->acl_info[p->acl_rule_index];

	switch (proc_num) {
	case PROC_SET_VLAN_MEMBER:
		len += sprintf(buf + len, "%u\n",
			sw_get_non_tag_vlan(sw, n));
		type = SHOW_HELP_NONE;
		break;
	case PROC_SET_LINK_MD:
		len += sprintf(buf + len, "%u:%u %u:%u %u:%u %u:%u %u:%u\n",
			cable_info->length[0], cable_info->status[0],
			cable_info->length[1], cable_info->status[1],
			cable_info->length[2], cable_info->status[2],
			cable_info->length[3], cable_info->status[3],
			cable_info->length[4], cable_info->status[4]);
		len += sprintf(buf + len,
			"(%d=unknown; %d=normal; %d=open; %d=short)\n",
			CABLE_UNKNOWN, CABLE_GOOD, CABLE_OPEN,
			CABLE_SHORT);
		type = SHOW_HELP_NONE;
		break;
	case PROC_SET_AUTHEN_MODE:
		len += sprintf(buf + len, "%u\n",
			port_get_authen_mode(sw, n));
		type = SHOW_HELP_NONE;
		break;
	case PROC_SET_ACL:
		chk = port_chk_acl(sw, n);
		type = SHOW_HELP_ON_OFF;
		break;
	case PROC_SET_ACL_FIRST_RULE:
		chk = ruleset->first_rule;
		type = SHOW_HELP_HEX;
		break;
	case PROC_SET_ACL_RULESET:
		len += acl_ruleset_info(ruleset, p->acl_rule_index, buf, len);
		break;
	case PROC_SET_ACL_MODE:
		chk = acl->mode;
		switch (chk) {
		case 1:
			strcpy(note, " (layer 2)");
			break;
		case 2:
			strcpy(note, " (layer 3)");
			break;
		case 3:
			strcpy(note, " (layer 4)");
			break;
		default:
			strcpy(note, " (off)");
		}
		type = SHOW_HELP_SPECIAL;
		break;
	case PROC_SET_ACL_ENABLE:
		chk = acl->enable;
		switch (chk) {
		case 1:
			strcpy(note, " (type; ip; tcp port)");
			break;
		case 2:
			strcpy(note, " (mac; src/dst; udp port)");
			break;
		case 3:
			strcpy(note, " (both; -; tcp seq)");
			break;
		default:
			strcpy(note, " (count; -; protocol)");
		}
		type = SHOW_HELP_SPECIAL;
		break;
	case PROC_SET_ACL_SRC:
		chk = acl->src;
		type = SHOW_HELP_ON_OFF;
		break;
	case PROC_SET_ACL_EQUAL:
		chk = acl->equal;
		type = SHOW_HELP_ON_OFF;
		break;
	case PROC_SET_ACL_PRIO_MODE:
		chk = action->prio_mode;
		switch (chk) {
		case 1:
			strcpy(note, " (higher)");
			break;
		case 2:
			strcpy(note, " (lower)");
			break;
		case 3:
			strcpy(note, " (replace)");
			break;
		default:
			strcpy(note, " (disabled)");
		}
		type = SHOW_HELP_SPECIAL;
		break;
	case PROC_SET_ACL_PRIO:
		chk = action->prio;
		type = SHOW_HELP_NUM;
		break;
	case PROC_SET_ACL_VLAN_PRIO_REPLACE:
		chk = action->vlan_prio_replace;
		type = SHOW_HELP_ON_OFF;
		break;
	case PROC_SET_ACL_VLAN_PRIO:
		chk = action->vlan_prio;
		type = SHOW_HELP_NUM;
		break;
	case PROC_SET_ACL_MAP_MODE:
		chk = action->map_mode;
		switch (chk) {
		case 1:
			strcpy(note, " (or)");
			break;
		case 2:
			strcpy(note, " (and)");
			break;
		case 3:
			strcpy(note, " (replace)");
			break;
		default:
			strcpy(note, " (disabled)");
		}
		type = SHOW_HELP_SPECIAL;
		break;
	case PROC_SET_ACL_PORTS:
		chk = action->ports;
		type = SHOW_HELP_HEX_4;
		break;
	case PROC_SET_ACL_MAC_ADDR:
		len += sprintf(buf + len, "%02x:%02x:%02x:%02x:%02x:%02x\n",
			acl->mac[0], acl->mac[1], acl->mac[2],
			acl->mac[3], acl->mac[4], acl->mac[5]);
		break;
	case PROC_SET_ACL_TYPE:
		chk = acl->eth_type;
		type = SHOW_HELP_HEX_4;
		break;
	case PROC_SET_ACL_CNT:
		chk = acl->cnt;
		type = SHOW_HELP_NUM;
		break;
	case PROC_SET_ACL_MSEC:
		chk = acl->msec;
		type = SHOW_HELP_ON_OFF;
		break;
	case PROC_SET_ACL_INTR_MODE:
		chk = acl->intr_mode;
		type = SHOW_HELP_ON_OFF;
		break;
	case PROC_SET_ACL_IP_ADDR:
		len += sprintf(buf + len, "%u.%u.%u.%u\n",
			acl->ip4_addr[0], acl->ip4_addr[1],
			acl->ip4_addr[2], acl->ip4_addr[3]);
		break;
	case PROC_SET_ACL_IP_MASK:
		len += sprintf(buf + len, "%u.%u.%u.%u\n",
			acl->ip4_mask[0], acl->ip4_mask[1],
			acl->ip4_mask[2], acl->ip4_mask[3]);
		break;
	case PROC_SET_ACL_PROTOCOL:
		chk = acl->protocol;
		type = SHOW_HELP_NUM;
		break;
	case PROC_SET_ACL_PORT_MODE:
		chk = acl->port_mode;
		switch (chk) {
		case 1:
			strcpy(note, " (either)");
			break;
		case 2:
			strcpy(note, " (in range)");
			break;
		case 3:
			strcpy(note, " (out of range)");
			break;
		default:
			strcpy(note, " (disabled)");
		}
		type = SHOW_HELP_SPECIAL;
		break;
	case PROC_SET_ACL_MAX_PORT:
		chk = acl->max_port;
		type = SHOW_HELP_NUM;
		break;
	case PROC_SET_ACL_MIN_PORT:
		chk = acl->min_port;
		type = SHOW_HELP_NUM;
		break;
	case PROC_SET_ACL_SEQNUM:
		chk = acl->seqnum;
		type = SHOW_HELP_NUM;
		break;
	case PROC_SET_ACL_TCP_FLAG_ENABLE:
		chk = acl->tcp_flag_enable;
		type = SHOW_HELP_ON_OFF;
		break;
	case PROC_SET_ACL_TCP_FLAG:
		chk = acl->tcp_flag;
		type = SHOW_HELP_HEX;
		break;
	case PROC_SET_ACL_TCP_FLAG_MASK:
		chk = acl->tcp_flag_mask;
		type = SHOW_HELP_HEX;
		break;
	case PROC_SET_ACL_INDEX:
		chk = p->acl_index;
		type = SHOW_HELP_HEX;
		break;
	case PROC_SET_ACL_ACTION_INDEX:
		chk = p->acl_act_index;
		type = SHOW_HELP_HEX;
		break;
	case PROC_SET_ACL_ACTION:
		len += acl_action_info(sw, action, p->acl_act_index, buf,
				       len);
		break;
	case PROC_SET_ACL_RULE_INDEX:
		chk = p->acl_rule_index;
		type = SHOW_HELP_HEX;
		break;
	case PROC_SET_ACL_INFO:
		len += acl_info(acl, p->acl_index, buf, len);
		break;
	case PROC_GET_ACL_TABLE:
		mutex_lock(&p->acllock);
		len += sw_d_acl_table(sw, n, buf, len);
		mutex_unlock(&p->acllock);
		break;
	}

out:
	return sysfs_show(len, buf, type, chk, note);
}  /* sysfs_port_read */

static int sysfs_port_write(struct ksz_device *sw, int proc_num, uint n, int num,
	const char *buf)
{
	struct ksz_port *p;
	struct ksz_cable_info *cable_info;
	struct ksz_acl_table *acl;
	struct ksz_acl_table *action;
	struct ksz_acl_table *ruleset;
	int acl_on = 0;
	int processed = true;

	p = &sw->ports[n];
	cable_info = &p->cable_info;
	acl = &p->acl_info[p->acl_index];
	action = &p->acl_info[p->acl_act_index];
	ruleset = &p->acl_info[p->acl_rule_index];

	switch (proc_num) {
	case PROC_SET_VLAN_MEMBER:
		sw_cfg_non_tag_vlan(sw, n, num);
		break;
	case PROC_SET_LINK_MD:
		if (num <= 0)
			break;
		mutex_lock(&p->linkmdlock);
		sw_get_link_md(sw, n, cable_info);
		mutex_unlock(&p->linkmdlock);
		break;
	case PROC_SET_AUTHEN_MODE:
		if (num > PORT_AUTHEN_TRAP)
			break;
		port_set_authen_mode(sw, n, num);
		break;
	case PROC_SET_ACL:
		mutex_lock(&p->acllock);
		port_cfg_acl(sw, n, num);
		mutex_unlock(&p->acllock);
		break;
	case PROC_SET_ACL_RULESET:
	case PROC_SET_ACL_MODE:
	case PROC_SET_ACL_ACTION:
		mutex_lock(&p->acllock);
		acl_on = port_chk_acl(sw, n);
		if (!acl_on)
			port_cfg_acl(sw, n, true);
		mutex_unlock(&p->acllock);
		break;
	}
	switch (proc_num) {
	case PROC_SET_ACL_FIRST_RULE:
		ruleset->first_rule = (u16) num;
		ruleset->ruleset_changed = 1;
		break;
	case PROC_SET_ACL_RULESET:
		sscanf(buf, "%x", &num);
		ruleset->ruleset = (u16) num;
		mutex_lock(&p->acllock);
		sw_w_acl_ruleset(sw, n, p->acl_rule_index, ruleset);
		mutex_unlock(&p->acllock);
		break;
	case PROC_SET_ACL_MODE:
		if (0 <= num && num < 4) {
			acl->mode = num;
			mutex_lock(&p->acllock);
			sw_w_acl_rule(sw, n, p->acl_index, acl);
			mutex_unlock(&p->acllock);
		}
		break;
	case PROC_SET_ACL_ENABLE:
		if (0 <= num && num < 4) {
			acl->enable = num;
			acl->changed = 1;
		}
		break;
	case PROC_SET_ACL_SRC:
		if (num)
			acl->src = 1;
		else
			acl->src = 0;
		acl->changed = 1;
		break;
	case PROC_SET_ACL_EQUAL:
		if (num)
			acl->equal = 1;
		else
			acl->equal = 0;
		acl->changed = 1;
		break;
	case PROC_SET_ACL_PRIO_MODE:
		if (0 <= num && num < 4) {
			action->prio_mode = num;
			action->action_changed = 1;
		}
		break;
	case PROC_SET_ACL_PRIO:
		if (0 <= num && num <= KS_PRIO_M) {
			action->prio = num;
			action->action_changed = 1;
		}
		break;
	case PROC_SET_ACL_VLAN_PRIO_REPLACE:
		if (num)
			action->vlan_prio_replace = 1;
		else
			action->vlan_prio_replace = 0;
		action->action_changed = 1;
		break;
	case PROC_SET_ACL_VLAN_PRIO:
		if (0 <= num && num <= KS_PRIO_M) {
			action->vlan_prio = num;
			action->action_changed = 1;
		}
		break;
	case PROC_SET_ACL_MAP_MODE:
		if (0 <= num && num < 4) {
			action->map_mode = num;
			action->action_changed = 1;
		}
		break;
	case PROC_SET_ACL_PORTS:
		sscanf(buf, "%x", &num);
		action->ports = (u16) num;
		action->action_changed = 1;
		break;
	case PROC_SET_ACL_MAC_ADDR:
	{
		int i;
		int n[6];

		i = sscanf(buf, "%x:%x:%x:%x:%x:%x",
			&n[0], &n[1], &n[2], &n[3], &n[4], &n[5]);
		if (6 == i) {
			for (i = 0; i < 6; i++)
				acl->mac[i] = (u8) n[i];
			acl->changed = 1;
		}
		break;
	}
	case PROC_SET_ACL_TYPE:
		sscanf(buf, "%x", &num);
		acl->eth_type = (u16) num;
		acl->changed = 1;
		break;
	case PROC_SET_ACL_CNT:
		if (0 <= num && num <= ACL_CNT_M) {
			acl->cnt = (u16) num;
			acl->changed = 1;
		}
		break;
	case PROC_SET_ACL_MSEC:
		if (num)
			acl->msec = 1;
		else
			acl->msec = 0;
		acl->changed = 1;
		break;
	case PROC_SET_ACL_INTR_MODE:
		if (num)
			acl->intr_mode = 1;
		else
			acl->intr_mode = 0;
		acl->changed = 1;
		break;
	case PROC_SET_ACL_IP_ADDR:
	{
		int i;
		int n[4];

		i = sscanf(buf, "%u.%u.%u.%u",
			&n[0], &n[1], &n[2], &n[3]);
		if (4 == i) {
			for (i = 0; i < 4; i++)
				acl->ip4_addr[i] = (u8) n[i];
			acl->changed = 1;
		}
		break;
	}
	case PROC_SET_ACL_IP_MASK:
	{
		int i;
		int n[4];

		i = sscanf(buf, "%u.%u.%u.%u",
			&n[0], &n[1], &n[2], &n[3]);
		if (4 == i) {
			for (i = 0; i < 4; i++)
				acl->ip4_mask[i] = (u8) n[i];
			acl->changed = 1;
		}
		break;
	}
	case PROC_SET_ACL_PROTOCOL:
		acl->protocol = (u8) num;
		acl->changed = 1;
		break;
	case PROC_SET_ACL_PORT_MODE:
		if (0 <= num && num < 4) {
			acl->port_mode = num;
			acl->changed = 1;
		}
		break;
	case PROC_SET_ACL_MAX_PORT:
		acl->max_port = (u16) num;
		acl->changed = 1;
		break;
	case PROC_SET_ACL_MIN_PORT:
		acl->min_port = (u16) num;
		acl->changed = 1;
		break;
	case PROC_SET_ACL_SEQNUM:
		acl->seqnum = num;
		acl->changed = 1;
		break;
	case PROC_SET_ACL_TCP_FLAG_ENABLE:
		if (num)
			acl->tcp_flag_enable = 1;
		else
			acl->tcp_flag_enable = 0;
		acl->changed = 1;
		break;
	case PROC_SET_ACL_TCP_FLAG:
		acl->tcp_flag = (u8) num;
		acl->changed = 1;
		break;
	case PROC_SET_ACL_TCP_FLAG_MASK:
		acl->tcp_flag_mask = (u8) num;
		acl->changed = 1;
		break;
	case PROC_SET_ACL_INDEX:
		if (0 <= num && num < ACL_TABLE_ENTRIES) {
			p->acl_index = num;
		}
		break;
	case PROC_SET_ACL_ACTION_INDEX:
		if (0 <= num && num < ACL_TABLE_ENTRIES) {
			p->acl_act_index = num;
		}
		break;
	case PROC_SET_ACL_ACTION:
		mutex_lock(&p->acllock);
		if (num)
			sw_w_acl_action(sw, n, p->acl_act_index, action);
		mutex_unlock(&p->acllock);
		break;
	case PROC_SET_ACL_RULE_INDEX:
		if (0 <= num && num < ACL_TABLE_ENTRIES) {
			p->acl_rule_index = num;
		}
		break;
	case PROC_SET_ACL_INFO:
		if (0 <= num && num < ACL_TABLE_ENTRIES)
			sw_reset_acl(&p->acl_info[num]);
		break;
	case PROC_GET_ACL_TABLE:
		mutex_lock(&p->acllock);
		sw_w_acl_table(sw, n, num, &p->acl_info[num]);
		mutex_unlock(&p->acllock);
		break;
	default:
		processed = false;
		break;
	}
	switch (proc_num) {
	case PROC_SET_ACL_RULESET:
	case PROC_SET_ACL_MODE:
	case PROC_SET_ACL_ACTION:
		if (!acl_on) {
			mutex_lock(&p->acllock);
			port_cfg_acl(sw, n, false);
			mutex_unlock(&p->acllock);
		}
		break;
	}
	return processed;
}  /* sysfs_port_write */

static ssize_t show_sw(struct device *d, struct device_attribute *attr,
	char *buf, unsigned long offset)
{
	struct ksz_device *sw;
	struct semaphore *proc_sem;
	ssize_t len = -EINVAL;
	int cnt;
	int num;
	uint port;

	if (attr->attr.name[1] != '_')
		return len;
	port = attr->attr.name[0] - '0' - 1;

	get_private_data(d, &proc_sem, &sw);
	cnt = TOTAL_PORT_NUM;
	if (port >= cnt)
		return len;
	if (down_interruptible(proc_sem))
		return -ERESTARTSYS;

	len = 0;
	num = offset / sizeof(int);
	len = sysfs_port_read(sw, num, port, len, buf);
	if (len)
		goto ksz_sw_sysfs_show_done;

ksz_sw_sysfs_show_done:
	up(proc_sem);
	return len;
}

static ssize_t store_sw(struct device *d, struct device_attribute *attr,
	const char *buf, size_t count, unsigned long offset)
{
	struct ksz_device *sw;
	struct semaphore *proc_sem;
	ssize_t ret = -EINVAL;
	int cnt;
	int num;
	uint port;
	int proc_num;

	if (attr->attr.name[1] != '_')
		return ret;
	port = attr->attr.name[0] - '0' - 1;
	num = get_num_val(buf);
	get_private_data(d, &proc_sem, &sw);
	cnt = TOTAL_PORT_NUM;
	if (port >= cnt)
		return ret;
	if (down_interruptible(proc_sem))
		return -ERESTARTSYS;

	proc_num = offset / sizeof(int);
	ret = count;

	if (sysfs_port_write(sw, proc_num, port, num, buf))
		goto ksz_sw_sysfs_store_done;

ksz_sw_sysfs_store_done:
	up(proc_sem);
	return ret;
}

#define SW_ATTR(_name, _mode, _show, _store) \
struct device_attribute sw_attr_##_name = \
	__ATTR(0_##_name, _mode, _show, _store)

/* generate a read-only attribute */
#define NETSW_RD_ENTRY(name)						\
static ssize_t show_sw_##name(struct device *d,				\
	struct device_attribute *attr, char *buf)			\
{									\
	return show_sw(d, attr, buf,					\
		offsetof(struct sw_attributes, name));			\
}									\
static SW_ATTR(name, S_IRUGO, show_sw_##name, NULL)

/* generate a write-able attribute */
#define NETSW_WR_ENTRY(name)						\
static ssize_t show_sw_##name(struct device *d,				\
	struct device_attribute *attr, char *buf)			\
{									\
	return show_sw(d, attr, buf,					\
		offsetof(struct sw_attributes, name));			\
}									\
static ssize_t store_sw_##name(struct device *d,			\
	struct device_attribute *attr, const char *buf, size_t count)	\
{									\
	return store_sw(d, attr, buf, count,				\
		offsetof(struct sw_attributes, name));			\
}									\
static SW_ATTR(name, S_IRUGO | S_IWUSR, show_sw_##name, store_sw_##name)


NETSW_WR_ENTRY(linkmd);
NETSW_WR_ENTRY(vlan_member);
NETSW_WR_ENTRY(authen_mode);
NETSW_WR_ENTRY(acl);
NETSW_WR_ENTRY(acl_first_rule);
NETSW_WR_ENTRY(acl_ruleset);
NETSW_WR_ENTRY(acl_mode);
NETSW_WR_ENTRY(acl_enable);
NETSW_WR_ENTRY(acl_src);
NETSW_WR_ENTRY(acl_equal);
NETSW_WR_ENTRY(acl_addr);
NETSW_WR_ENTRY(acl_type);
NETSW_WR_ENTRY(acl_cnt);
NETSW_WR_ENTRY(acl_msec);
NETSW_WR_ENTRY(acl_intr_mode);
NETSW_WR_ENTRY(acl_ip_addr);
NETSW_WR_ENTRY(acl_ip_mask);
NETSW_WR_ENTRY(acl_protocol);
NETSW_WR_ENTRY(acl_seqnum);
NETSW_WR_ENTRY(acl_port_mode);
NETSW_WR_ENTRY(acl_max_port);
NETSW_WR_ENTRY(acl_min_port);
NETSW_WR_ENTRY(acl_tcp_flag_enable);
NETSW_WR_ENTRY(acl_tcp_flag);
NETSW_WR_ENTRY(acl_tcp_flag_mask);
NETSW_WR_ENTRY(acl_prio_mode);
NETSW_WR_ENTRY(acl_prio);
NETSW_WR_ENTRY(acl_vlan_prio_replace);
NETSW_WR_ENTRY(acl_vlan_prio);
NETSW_WR_ENTRY(acl_map_mode);
NETSW_WR_ENTRY(acl_ports);
NETSW_WR_ENTRY(acl_index);
NETSW_WR_ENTRY(acl_act_index);
NETSW_WR_ENTRY(acl_act);
NETSW_WR_ENTRY(acl_rule_index);
NETSW_WR_ENTRY(acl_info);
NETSW_WR_ENTRY(acl_table);

static struct attribute *sw_attrs[] = {
	&sw_attr_linkmd.attr,
	&sw_attr_vlan_member.attr,
	&sw_attr_authen_mode.attr,
	&sw_attr_acl.attr,
	&sw_attr_acl_first_rule.attr,
	&sw_attr_acl_ruleset.attr,
	&sw_attr_acl_mode.attr,
	&sw_attr_acl_enable.attr,
	&sw_attr_acl_src.attr,
	&sw_attr_acl_equal.attr,
	&sw_attr_acl_addr.attr,
	&sw_attr_acl_type.attr,
	&sw_attr_acl_cnt.attr,
	&sw_attr_acl_msec.attr,
	&sw_attr_acl_intr_mode.attr,
	&sw_attr_acl_ip_addr.attr,
	&sw_attr_acl_ip_mask.attr,
	&sw_attr_acl_protocol.attr,
	&sw_attr_acl_seqnum.attr,
	&sw_attr_acl_port_mode.attr,
	&sw_attr_acl_max_port.attr,
	&sw_attr_acl_min_port.attr,
	&sw_attr_acl_tcp_flag_enable.attr,
	&sw_attr_acl_tcp_flag.attr,
	&sw_attr_acl_tcp_flag_mask.attr,
	&sw_attr_acl_prio_mode.attr,
	&sw_attr_acl_prio.attr,
	&sw_attr_acl_vlan_prio_replace.attr,
	&sw_attr_acl_vlan_prio.attr,
	&sw_attr_acl_map_mode.attr,
	&sw_attr_acl_ports.attr,
	&sw_attr_acl_index.attr,
	&sw_attr_acl_act_index.attr,
	&sw_attr_acl_act.attr,
	&sw_attr_acl_rule_index.attr,
	&sw_attr_acl_info.attr,
	&sw_attr_acl_table.attr,
	NULL
};

static struct attribute_group sw_group = {
	.name  = "lan",
	.attrs  = sw_attrs,
};

/* Kernel checking requires the attributes are in data segment. */
#define SW_ATTRS_SIZE		(sizeof(sw_attrs) / sizeof(void *) - 1)

#define MAX_SWITCHES		2

static struct ksz_dev_attr ksz_sw_dev_attrs[(
	SW_ATTRS_SIZE * TOTAL_PORT_NUM) * MAX_SWITCHES];
static struct ksz_dev_attr *ksz_sw_dev_attrs_ptr = ksz_sw_dev_attrs;

void ksz_sw_sysfs_exit(struct ksz_device *sw, struct ksz_sw_sysfs *info,
	struct device *dev)
{
	int i;
	int j;

	for (i = 0; i < sw->mib_port_cnt; i++) {
		sw_group.name = sw_name[i + j];
		sw_group.attrs = info->port_attrs[i];
		sysfs_remove_group(&dev->kobj, &sw_group);
		kfree(info->port_attrs[i]);
		info->port_attrs[i] = NULL;
		info->ksz_port_attrs[i] = NULL;
	}
	ksz_sw_dev_attrs_ptr = ksz_sw_dev_attrs;
}
EXPORT_SYMBOL(ksz_sw_sysfs_exit);

int ksz_sw_sysfs_init(struct ksz_device *sw, struct ksz_sw_sysfs *info,
	struct device *dev)
{
	int err;
	int i;
	uint p;
	char *file;

	info->ksz_port_attrs = devm_kzalloc(sw->dev, 
		sizeof(struct ksz_dev_attr *) * TOTAL_PORT_NUM, 
		GFP_KERNEL);
	info->port_attrs = devm_kzalloc(sw->dev, 
		sizeof(struct attribute **) * TOTAL_PORT_NUM, 
		GFP_KERNEL);

	for (i = 0; i < sw->mib_port_cnt; i++) {
		p = i;
		file = NULL;
		if (i == sw->cpu_port)
			continue;
		err = alloc_dev_attr(sw_attrs,
			sizeof(sw_attrs) / sizeof(void *), i,
			&info->ksz_port_attrs[i], &info->port_attrs[i],
			file, &ksz_sw_dev_attrs_ptr);
		if (err)
			return err;
		sw_group.name = sw_name[i];
		sw_group.attrs = info->port_attrs[i];
		err = sysfs_create_group(&dev->kobj, &sw_group);
		if (err)
			return err;
	}

	sw_init_acl(sw);

	return err;
}
EXPORT_SYMBOL(ksz_sw_sysfs_init);

MODULE_DESCRIPTION("Microchip KSZ9897 I2C Switch Driver");
MODULE_AUTHOR("Tristram Ha <Tristram.Ha@microchip.com>");
MODULE_LICENSE("GPL");
