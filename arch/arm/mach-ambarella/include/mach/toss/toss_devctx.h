/*
 * system/src/toss/toss_devctx.h
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
#define MAX_TOSS_DEVCTX_SD_HOST		2
#define MAX_TOSS_DEVCTX_SD_CARD		2
	__ARMCC_PACK__ struct {
		unsigned int state;
#define TOSS_SDCTX_CHANGE	0x1	/* card state change event */
					/* Set this bit to validate the reamin bits fields. */
#define TOSS_SDCTX_INSERT	0x2	/* card eject event */
#define TOSS_SDCTX_EJECT	0x4	/* card insert event */
#define TOSS_SDCTX_WRITE	0x8	/* card write event */
	} __ATTRIB_PACK__ card[MAX_TOSS_DEVCTX_SD_HOST][MAX_TOSS_DEVCTX_SD_CARD];
} __ATTRIB_PACK__;


__ARMCC_PACK__ struct toss_devctx_dsp_s
{
#define DSP_CMD_SIZE			128
#define MAX_TOSS_DEVCTX_DSP_CMDS	20
	__ARMCC_PACK__ struct dsp_cmd_s
	{
		unsigned int	code;
		unsigned char	param[DSP_CMD_SIZE - 4];
	} __ATTRIB_PACK__ cmd[MAX_TOSS_DEVCTX_DSP_CMDS];

	int	filtered_cmds;	/* number of cmds are filtered. */
} __ATTRIB_PACK__;

/*
 * Device context of each personality.
 */
__ARMCC_PACK__ struct toss_devctx_s
{
	struct toss_devctx_time_s	time;
	struct toss_devctx_sd_card_s	sd;
	struct toss_devctx_dsp_s	dsp;
} __ATTRIB_PACK__;

#endif
