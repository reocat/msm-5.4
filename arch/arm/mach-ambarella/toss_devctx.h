/*
 * toss/toss_devctx.h
 *
 * Terminal Operating Systerm Scheduler
 *
 * History:
 *    2009/11/10 - [Charles Chiou] created file
 *
 * Copyright (C) 2004-2009, Ambarella, Inc.
 */

#ifndef __TOSS_DEVCTX_H__
#define __TOSS_DEVCTX_H__

/*
 * Time.
 */
__ARMCC_PACK__ struct toss_devctx_time_s
{
	unsigned int uptime_ms;		/* Uptime for OS */
	unsigned int last_elapsed_ms;	/* Elapsed time since last switch */
	unsigned int wall_time_ms;	/* Time of day */
} __ATTRIB_PACK__ ;

/*
 * SD Card.
 */
__ARMCC_PACK__ struct toss_devctx_sd_card_s
{
#define MAX_TOSS_DEVCTX_SD_BUS	2
#define MAX_TOSS_DEVCTX_SD_SLOT	2
	__ARMCC_PACK__ struct {
		unsigned int changed;	/* Insert/eject event occured */
	} __ATTRIB_PACK__ card[MAX_TOSS_DEVCTX_SD_BUS][MAX_TOSS_DEVCTX_SD_SLOT];
} __ATTRIB_PACK__;

/*
 * Device context of each personality.
 */
__ARMCC_PACK__ struct toss_devctx_s
{
	struct toss_devctx_time_s	time;
	struct toss_devctx_sd_card_s	sd_card;
} __ATTRIB_PACK__;

#endif
