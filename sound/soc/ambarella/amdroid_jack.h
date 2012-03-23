/*
 * sound/soc/amdroid_jack.h
 *
 * History:
 *	2012/03/23 - [Cao Rongrong] created file
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


#ifndef __AMDROID_JACK_H_
#define __AMDROID_JACK_H_

#define MAX_ZONE_LIMIT		10
#define DET_CHECK_TIME_MS	200		/* 200ms */
#define WAKE_LOCK_TIME		(HZ * 5)	/* 5 sec */

enum {
	AMDROID_JACK_NO_DEVICE		= 0x0,
	AMDROID_HEADSET_4POLE		= 0x01 << 0,	// Headset
	AMDROID_HEADSET_3POLE		= 0x01 << 1,	// Headphone
	AMDROID_TTY_DEVICE		= 0x01 << 2,
	AMDROID_FM_HEADSET		= 0x01 << 3,
	AMDROID_FM_SPEAKER		= 0x01 << 4,
	AMDROID_TVOUT_DEVICE		= 0x01 << 5,
	AMDROID_EXTRA_DOCK_SPEAKER	= 0x01 << 6,
	AMDROID_EXTRA_CAR_DOCK_SPEAKER	= 0x01 << 7,
	AMDROID_UNKNOWN_DEVICE		= 0x01 << 8,
};


struct amdroid_jack_zone {
	unsigned int adc_high;
	unsigned int delay_ms;
	unsigned int check_count;
	unsigned int jack_type;
};

struct amdroid_jack_platform_data {
	struct amdroid_jack_zone *zones;
	int	num_zones;
	int	detect_gpio;
	bool	active_high;
	int	adc_channel;
	void	(*set_micbias_state)(void *, bool);
	void	*private_data;
};

#endif

