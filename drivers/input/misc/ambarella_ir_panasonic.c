/*
 * drivers/input/misc/ambarella_ir_panasonic.c
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

#define PANASONIC_DEFAULT_FREQUENCY	38000	/* 38KHz */

/* T = 420 £gs to approx 424 £gs in the USA and Canada */
/* T = 454 £gs to approx 460 £gs in Europe and others  */
#define PANASONIC_DEFAULT_SMALLER_TIME	454	/* T, 450 microseconds. */

/** bit 0
 *         +----+    +---
 *         |    |    |
 *      ---+    +----+
 *           2T   2T
 */

/** bit 1
 *         +----+            +---
 *         |    |            |
 *      ---+    +------------+
 *           2T       6T
 */

/** start
 *         +--------------------------------+                +---
 *         |                                |                |
 *      ---+                                +----------------+
 *                         16T                      8T
 */

#define PANASONIC_LEADER_HIGH_UPBOUND	50
#define PANASONIC_LEADER_HIGH_LOWBOUND	42
#define PANASONIC_LEADER_LOW_UPBOUND	26
#define PANASONIC_LEADER_LOW_LOWBOUND	18

#define PANASONIC_DATA_HIGH_UPBOUND	9
#define PANASONIC_DATA_HIGH_LOWBOUND	1
#define PANASONIC_DATA_0_LOW_UPBOUND	9
#define PANASONIC_DATA_0_LOW_LOWBOUND	1
#define PANASONIC_DATA_1_LOW_UPBOUND	20
#define PANASONIC_DATA_1_LOW_LOWBOUND	12

static int ambarella_ir_pulse_leader_code(struct ambarella_ir_info *pinfo)
{
	u16 val = ambarella_ir_read_data(pinfo, pinfo->ir_pread);

	if ((val < PANASONIC_LEADER_HIGH_UPBOUND)  &&
	    (val > PANASONIC_LEADER_HIGH_LOWBOUND)) {
	} else {
		return 0;
	}

	if ((pinfo->ir_pread + 1) >= MAX_IR_BUFFER) {
		val = ambarella_ir_read_data(pinfo, 0);
	} else {
		val = ambarella_ir_read_data(pinfo, pinfo->ir_pread + 1);
	}

	if ((val < PANASONIC_LEADER_LOW_UPBOUND) &&
	    (val > PANASONIC_LEADER_LOW_LOWBOUND) )
		return 1;
	else
		return 0;
}

static int ambarella_ir_pulse_code_0(struct ambarella_ir_info *pinfo)
{
	u16 val = ambarella_ir_read_data(pinfo, pinfo->ir_pread);
	if ((val < PANASONIC_DATA_HIGH_UPBOUND)  &&
	    (val > PANASONIC_DATA_HIGH_LOWBOUND)) {
	} else {
		return 0;
	}

	if ((pinfo->ir_pread + 1) >= MAX_IR_BUFFER) {
		val = ambarella_ir_read_data(pinfo, 0);
	} else {
		val = ambarella_ir_read_data(pinfo, pinfo->ir_pread + 1);
	}

	if ((val < PANASONIC_DATA_0_LOW_UPBOUND) &&
	    (val > PANASONIC_DATA_0_LOW_LOWBOUND) )
		return 1;
	else
		return 0;
}

static int ambarella_ir_pulse_code_1(struct ambarella_ir_info *pinfo)
{
	u16 val = ambarella_ir_read_data(pinfo, pinfo->ir_pread);

	if ((val < PANASONIC_DATA_HIGH_UPBOUND)  &&
	    (val > PANASONIC_DATA_HIGH_LOWBOUND)) {
	} else {
		return 0;
	}

	if ((pinfo->ir_pread + 1) >= MAX_IR_BUFFER) {
		val = ambarella_ir_read_data(pinfo, 0);
	} else {
		val = ambarella_ir_read_data(pinfo, pinfo->ir_pread + 1);
	}

	if ((val < PANASONIC_DATA_1_LOW_UPBOUND) &&
	    (val > PANASONIC_DATA_1_LOW_LOWBOUND) )
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
			ambi_dbg("%d ERROR, the waveform can't match",
				  pinfo->ir_pread);
			return -1;
		}
	}

	return 0;
}

static int ambarella_ir_pulse_decode(struct ambarella_ir_info *pinfo, u32 *uid)
{
	u8 addr0 = 0, addr1 = 0, data0 = 0, data1 = 0, data2 = 0, data3 = 0;
	int rval;

	/* Then follows 22 bits of data, broken down in 4 bytes of 8 bits. */

	/* The first 8 bits is the Address 0. */
	rval = ambarella_ir_pulse_data_translate(pinfo, &addr0);
	if (rval < 0)
		return rval;

	/* The second 8 bits is the Address 1. */
	rval = ambarella_ir_pulse_data_translate(pinfo, &addr1);
	if (rval < 0)
		return rval;

	/* The third 8 bits is the data 0. */
	rval = ambarella_ir_pulse_data_translate(pinfo, &data0);
	if (rval < 0)
		return rval;

	/* The third 8 bits is the data 1. */
	rval = ambarella_ir_pulse_data_translate(pinfo, &data1);
	if (rval < 0)
		return rval;

	/* The third 8 bits is the data 2. */
	rval = ambarella_ir_pulse_data_translate(pinfo, &data2);
	if (rval < 0)
		return rval;

	/* The third 8 bits is the data 3. */
	rval = ambarella_ir_pulse_data_translate(pinfo, &data3);
	if (rval < 0)
		return rval;

	ambi_dbg("\taddr0\taddr1\tdata0\tdata1\tdata2\tdata3");
	ambi_dbg("\t0x%x\t0x%x\t0x%x\t0x%x\t0x%x\t0x%x\n",
			addr0, addr1, data0, data1, data2, data3);

	*uid = (data0 << 24) | (data1 << 16) | (data2 << 8) | data3;

	return 0;
}

int ambarella_ir_panasonic_parse(struct ambarella_ir_info *pinfo, u32 *uid)
{
	int				rval;
	int				cur_ptr = pinfo->ir_pread;

	if (ambarella_ir_pulse_leader_code(pinfo)) {
		ambi_dbg("%d  find leader code", cur_ptr);

		rval = ambarella_ir_pulse_decode(pinfo, uid);

		if (rval >= 0) {
			ambi_dbg("%d  mornal key", cur_ptr);

			ambarella_ir_move_read_ptr(pinfo, 2);

			return 0;
		}
	}

	*uid = 0xff;

	return (-1);
}

