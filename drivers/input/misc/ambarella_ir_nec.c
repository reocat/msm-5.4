/*
 * drivers/input/misc/ambarella_ir_nec.c
 *
 * History:
 *      2007/03/28 - [Dragon Chiang] created file
 *	2009/03/10 - [Anthony Ginger] Port to 2.6.28
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

#include <linux/platform_device.h>
#include <linux/interrupt.h>

#include "ambarella_input.h"

/**
 * Pulse-Width-Coded Signals vary the length of pulses to code the information.
 * In this case if the pulse width is short (approximately 550us) it
 * corresponds to a logical zero or a low. If the pulse width is long
 * (approximately 2200us) it corresponds to a logical one or a high.
 *
 *         +--+  +--+  +----+  +----+
 *         |  |  |  |  |    |  |    |
 *      ---+  +--+  +--+    +--+    +---
 *          0     0      1       1
 */

/* NEC - APEX - HITACHI - PIONEER */
#define NEC_DEFAULT_FREQUENCY		36000	/* 36KHz */
#define NEC_DEFAULT_SMALLER_TIME	560	/* T, 560 microseconds. */

/** bit 0 [1120us]
 *      ---+    +----+
 *         |    |    |
 *         +----+    +---
 *           -T   +T
 */

/** bit 1 [2240us]
 *      ---+    +------------+
 *         |    |            |
 *         +----+            +---
 *           -T      +3T
 */

/** start [13.3ms]
 *      ---+                                +---------------+
 *         |                                |               |
 *         +--------------------------------+               +---
 *                     -16T(9ms)              +7.5T(4.2ms)
 */

/** REPEAT Frame [11.3ms]
 *      ---+                                +--------+  +---
 *         |                                |        |  |
 *         +--------------------------------+        +--+
 *                     -16T(9ms)            +4T(2.2ms)
 */

#define NEC_LEADER_LOW_UPBOUND		123	/* default 9ms   */
#define NEC_LEADER_LOW_LOWBOUND		113
#define NEC_LEADER_HIGH_UPBOUND		63	/* default 4.2ms */
#define NEC_LEADER_HIGH_LOWBOUND	52

#define NEC_REPEAT_LOW_UPBOUND		123	/* default 9ms   */
#define NEC_REPEAT_LOW_LOWBOUND		113
#define NEC_REPEAT_HIGH_UPBOUND		33	/* default 2.2ms */
#define NEC_REPEAT_HIGH_LOWBOUND	23

#define NEC_DATA_LOW_UPBOUND		12	/* default 560us  */
#define NEC_DATA_LOW_LOWBOUND		1
#define NEC_DATA_0_HIGH_UPBOUND		12	/* default 560us  */
#define NEC_DATA_0_HIGH_LOWBOUND	1
#define NEC_DATA_1_HIGH_UPBOUND		26	/* default 1680us */
#define NEC_DATA_1_HIGH_LOWBOUND	15

/**
 * Check the waveform data is leader code or not.
 */
static int ambarella_ir_pulse_leader_code(struct ambarella_ir_info *pinfo)
{
	u16 val = ambarella_ir_read_data(pinfo, pinfo->ir_pread);

	if ((val < NEC_LEADER_LOW_UPBOUND)  &&
	    (val > NEC_LEADER_LOW_LOWBOUND)) {
	} else {
		return 0;
	}

	if ((pinfo->ir_pread + 1) >= MAX_IR_BUFFER) {
		val = ambarella_ir_read_data(pinfo, 0);
	} else {
		val = ambarella_ir_read_data(pinfo, pinfo->ir_pread + 1);
	}

	if ((val < NEC_LEADER_HIGH_UPBOUND) &&
	    (val > NEC_LEADER_HIGH_LOWBOUND) )
		return 1;
	else
		return 0;
}

/**
 * Check the waveform data is repeat code or not.
 */
static int ambarella_ir_pulse_repeat_code(struct ambarella_ir_info *pinfo)
{
	u16 val = ambarella_ir_read_data(pinfo, pinfo->ir_pread);

	if ((val < NEC_REPEAT_LOW_UPBOUND)  &&
	    (val > NEC_REPEAT_LOW_LOWBOUND)) {
	} else {
		return 0;
	}

	if ((pinfo->ir_pread + 1) >= MAX_IR_BUFFER) {
		val = ambarella_ir_read_data(pinfo, 0);
	} else {
		val = ambarella_ir_read_data(pinfo, pinfo->ir_pread + 1);
	}

	if ((val < NEC_REPEAT_HIGH_UPBOUND) &&
	    (val > NEC_REPEAT_HIGH_LOWBOUND) )
		return 1;
	else
		return 0;
}

/**
 * Check the waveform data is 0 bit or not.
 */
static int ambarella_ir_pulse_code_0(struct ambarella_ir_info *pinfo)
{
	u16 val = ambarella_ir_read_data(pinfo, pinfo->ir_pread);
	if ((val < NEC_DATA_LOW_UPBOUND)  &&
	    (val > NEC_DATA_LOW_LOWBOUND)) {
	} else {
		return 0;
	}

	if ((pinfo->ir_pread + 1) >= MAX_IR_BUFFER) {
		val = ambarella_ir_read_data(pinfo, 0);
	} else {
		val = ambarella_ir_read_data(pinfo, pinfo->ir_pread + 1);
	}

	if ((val < NEC_DATA_0_HIGH_UPBOUND) &&
	    (val > NEC_DATA_0_HIGH_LOWBOUND) )
		return 1;
	else
		return 0;
}

/**
 * Check the waveform data is 1 bit or not.
 */
static int ambarella_ir_pulse_code_1(struct ambarella_ir_info *pinfo)
{
	u16 val = ambarella_ir_read_data(pinfo, pinfo->ir_pread);

	if ((val < NEC_DATA_LOW_UPBOUND)  &&
	    (val > NEC_DATA_LOW_LOWBOUND)) {
	} else {
		return 0;
	}

	if ((pinfo->ir_pread + 1) >= MAX_IR_BUFFER) {
		val = ambarella_ir_read_data(pinfo, 0);
	} else {
		val = ambarella_ir_read_data(pinfo, pinfo->ir_pread + 1);
	}

	if ((val < NEC_DATA_1_HIGH_UPBOUND) &&
	    (val > NEC_DATA_1_HIGH_LOWBOUND) )
		return 1;
	else
		return 0;
}

static int ambarella_ir_pulse_data_translate(struct ambarella_ir_info *pinfo, u8 * data)
{
	int i, rval;

	*data = 0;

	for (i = 7; i >= 0; i--) {
		rval = ambarella_ir_move_read_ptr(pinfo, 2);
		if (rval < 0) {
		      return rval;
		}

		if (ambarella_ir_pulse_code_0(pinfo)) {

		} else if (ambarella_ir_pulse_code_1(pinfo)) {
			*data |= 1 << i;
		} else {
			ambi_dbg("%d ERROR, the waveform can't match\n", i);

			return -1;
		}
	}

	return 0;
}

static int ambarella_ir_pulse_decode(struct ambarella_ir_info *pinfo, u32 *uid)
{
	u8 addr = 0, inv_addr = 0, data = 0, inv_data = 0;
	int rval;

	/* Then follows 32 bits of data, broken down in 4 bytes of 8 bits. */

	/* The first 8 bits is the Address. */
	rval = ambarella_ir_pulse_data_translate(pinfo, &addr);
	if (rval < 0)
		return rval;

	/* The second 8 bits is the Address Complement. */
	rval = ambarella_ir_pulse_data_translate(pinfo, &inv_addr);
	if (rval < 0)
		return rval;

	/* The third 8 bits is the data. */
	rval = ambarella_ir_pulse_data_translate(pinfo, &data);
	if (rval < 0)
		return rval;

	/* The fourth 8 bits is the data Complement. */
	rval = ambarella_ir_pulse_data_translate(pinfo, &inv_data);
	if (rval < 0)
		return rval;

	ambi_dbg("addr\tinv_addr\tdata\tinv_data\n");
	ambi_dbg("0x%x\t0x%x\t\t0x%x\t0x%x\n", addr, inv_addr, data, inv_data);

	*uid = (addr << 24) | (inv_addr << 16) | (data << 8) | inv_data;

	return 0;
}

int ambarella_ir_nec_parse(struct ambarella_ir_info *pinfo, u32 *uid)
{
	int				rval;
	int				cur_ptr = pinfo->ir_pread;

	if (ambarella_ir_pulse_leader_code(pinfo)) {
		ambi_dbg("%d  find leader code\n", cur_ptr);

		rval = ambarella_ir_pulse_decode(pinfo, uid);

		if (rval >= 0) {
			ambi_dbg("%d  mornal key\n", cur_ptr);

			ambarella_ir_move_read_ptr(pinfo, 2);

			return 0;
		}
	} else if (ambarella_ir_pulse_repeat_code(pinfo)) {
		*uid = 0x00;

		ambi_dbg("%d  continuous key\n", cur_ptr);

		ambarella_ir_move_read_ptr(pinfo, 2);

		return 1;
	}

	*uid = 0xff;

	return (-1);
}

