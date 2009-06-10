/*
 * drivers/input/misc/ambarella_ir_philips.c
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
 * Shift-Coded Signals (RC-5)vary the order of pulse space to code the
 * information. In this case if the space width is short (approximately 550us)
 * and the pulse width is long (approximately 1100us) the signal corresponds to
 * a logical one or a high. If the space is long and the pulse is short the
 * signal corresponds to a logical zero or a low.
 *
 *           |     |     |
 *        +--|  +--|--+  |  +---
 *        |  |  |  |  |  |  |
 *     ---+  |--+  |  +--|--+
 *        1  |  1  |  0  |  1
 */

/* Philips (RC-5) */
#define PHILIPS_DEFAULT_FREQUENCY	36000	/* 36KHz */
#define PHILIPS_DEFAULT_SMALLER_TIME	1728	/* T, 1728 microseconds. */

/**       |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
 *     ---+-+ +-+ +-+ +-+ +-+ +-+ +-+ +-+ +-+ +-+ +-+ +-+ +-+ +-+ +---
 *        | | | | | | | | | | | | | | | | | | | | | | | | | | | | |
 *        | +-+ +-+ +-+ +-+ +-+ +-+ +-+ +-+ +-+ +-+ +-+ +-+ +-+ +-+
 *        |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
 *        |  AGC  |TOG|<     address     >|<       command       >|
 *
 *                 |        |
 */
#define PHILIPS_LEADER_UPBOUND		0xffff	/* default don't care */
#define PHILIPS_LEADER_LOWBOUND		30

#define PHILIPS_SHC_SIG_CYC_UPBOUND	14	/* default 10~ 12 */
#define PHILIPS_SHC_SIG_CYC_LOWBOUND	8

#define PHILIPS_SHC_DOB_CYC_UPBOUND	25	/* default 12~ 23 */
#define PHILIPS_SHC_DOB_CYC_LOWBOUND	20

static int ambarella_ir_shift_leader_code(struct ambarella_ir_info *pinfo)
{
/**       |                 |                 |
 *     ---|--------+        +--------+        |
 *        |        |        |        |        |
 *        |        +--------+        +--------|---
 *        |                 |                 |
 *        |                AGC                |
 *
 *      Leader     | Leader | Leader |
 */
	u16 val_1, val_0 = ambarella_ir_read_data(pinfo, pinfo->ir_pread);

#if 0
	if ((val_0 <= PHILIPS_LEADER_UPBOUND) &&
	    (val_0 > PHILIPS_LEADER_LOWBOUND)) {
#else
	if (val_0 > PHILIPS_LEADER_LOWBOUND) {
#endif
	} else {
		return 0;
	}

	if ((pinfo->ir_pread + 1) >= MAX_IR_BUFFER) {
		val_0 = ambarella_ir_read_data(pinfo, 0);
		val_1 = ambarella_ir_read_data(pinfo, 1);
	} else if ((pinfo->ir_pread + 2) == MAX_IR_BUFFER) {
		val_0 = ambarella_ir_read_data(pinfo, pinfo->ir_pread + 1);
		val_1 = ambarella_ir_read_data(pinfo, 0);
	} else {
		val_0 = ambarella_ir_read_data(pinfo, pinfo->ir_pread + 1);
		val_1 = ambarella_ir_read_data(pinfo, pinfo->ir_pread + 2);
	}

	if ((val_0 < PHILIPS_SHC_SIG_CYC_UPBOUND)  &&
	    (val_0 > PHILIPS_SHC_SIG_CYC_LOWBOUND) &&
	    (val_1 < PHILIPS_SHC_SIG_CYC_UPBOUND)  &&
	    (val_1 > PHILIPS_SHC_SIG_CYC_LOWBOUND))
		return 1;
	else
		return 0;
}

static int ambarella_ir_shift_invert_code(struct ambarella_ir_info *pinfo)
{
	u16 val = ambarella_ir_read_data(pinfo, pinfo->ir_pread);

	if ((val < PHILIPS_SHC_DOB_CYC_UPBOUND) &&
	    (val > PHILIPS_SHC_DOB_CYC_LOWBOUND))
		return 1;
	else
		return 0;
}

static int ambarella_ir_shift_repeat_code(struct ambarella_ir_info *pinfo)
{
        int rval;

	u16 val = ambarella_ir_read_data(pinfo, pinfo->ir_pread);

	if ((val < PHILIPS_SHC_SIG_CYC_UPBOUND) &&
	    (val > PHILIPS_SHC_SIG_CYC_LOWBOUND)) {
		rval = ambarella_ir_inc_read_ptr(pinfo);
		if (rval < 0)
			return rval;

		val = ambarella_ir_read_data(pinfo, pinfo->ir_pread);
		if ((val < PHILIPS_SHC_SIG_CYC_UPBOUND) &&
		    (val > PHILIPS_SHC_SIG_CYC_LOWBOUND))
			return 1;
		else
			return 0;
	} else
		return 0;
}

static int ambarella_ir_shift_decode(struct ambarella_ir_info *pinfo, u32 *uid)
{
	int i, val = 0, rval;
	u8 addr = 0, data = 0;

	rval = ambarella_ir_move_read_ptr(pinfo, 3);
	if (rval < 0)
	       return rval;

	/* Get toggle code and Initialize start value */
	if (ambarella_ir_shift_invert_code(pinfo))
		val = 1;
	else if (ambarella_ir_shift_repeat_code(pinfo))
		val = 0;
	else {
		ambi_dbg("Err->toggle code doesn't match");
		return (-1);
	}

	ambi_dbg("toggle code:%d", val);

	/* address */
	for (i = 0; i < 5; i++) {

		rval = ambarella_ir_inc_read_ptr(pinfo);
		if (rval < 0)
		      return rval;    /* data is poor */

		if (ambarella_ir_shift_invert_code(pinfo)) {
			val = 1 - val;
		} else if (ambarella_ir_shift_repeat_code(pinfo)) {
		}else {
			ambi_dbg("Err->addr code(%d) doesn't match", i);
			return (-1);
		}
		addr = (addr << 1) | val;
	}

	/* data */
	for (i = 0; i < 6; i++) {

		rval = ambarella_ir_inc_read_ptr(pinfo);
		if (rval < 0)
		      return rval;    /* data is poor */

		if (ambarella_ir_shift_invert_code(pinfo)) {
			val = 1 - val;
		} else if (ambarella_ir_shift_repeat_code(pinfo)) {
		}else {
			ambi_dbg("Err->data code(%d) doesn't match", i);
			return (-1);
		}
		data = (data << 1) | val;
	}

	*uid = (addr << 16) | data;

	return 0;
}

int ambarella_ir_philips_parse(struct ambarella_ir_info *pinfo, u32 *uid)
{
	int rval;

	if (ambarella_ir_shift_leader_code(pinfo)) {
		ambi_dbg("%d  find leader code", pinfo->ir_pread);

		rval = ambarella_ir_shift_decode(pinfo, uid);

		if (rval >= 0)
			return 0;
	}

	*uid = 0xff;

	return (-1);
}

