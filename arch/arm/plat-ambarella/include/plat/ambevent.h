/*
 * arch/arm/plat-ambarella/include/plat/ambevent.h
 *
 * History:
 *    2010/07/23 - [Zhenwu Xue] Create
 *
 * Copyright (C) 2004-2010, Ambarella, Inc.
 *
 * All rights reserved. No Part of this file may be reproduced, stored
 * in a retrieval system, or transmitted, in any form, or by any means,
 * electronic, mechanical, photocopying, recording, or otherwise,
 * without the prior consent of Ambarella, Inc.
 *
 */

#ifndef __AMB_EVENT_H
#define __AMB_EVENT_H

/* ==========================================================================*/
#ifndef __ASSEMBLER__

/* Copy from build/include/ambas_event.h */
enum amb_event_type {
	/* No Event */
	AMB_EV_NONE				= 0x00000000,

	/* VIN Event */
	AMB_EV_VIN_DECODER_SOURCE_PLUG		= 0x00010000,
	AMB_EV_VIN_DECODER_SOURCE_REMOVE,

	/* VOUT Event */
	AMB_EV_VOUT_CVBS_PLUG			= 0x00020000,
	AMB_EV_VOUT_CVBS_REMOVE,
	AMB_EV_VOUT_YPBPR_PLUG,
	AMB_EV_VOUT_YPBPR_REMOVE,
	AMB_EV_VOUT_HDMI_PLUG,
	AMB_EV_VOUT_HDMI_REMOVE,
};

struct amb_event {
	unsigned int		sno;		//sequential number
	unsigned int		time_code;
	enum amb_event_type	type;
};

struct amb_event_pool {
	struct mutex			op_mutex;
	struct amb_event		events[256];
	unsigned int			ev_sno;
	unsigned char			ev_index;
};

extern int amb_event_pool_init(struct amb_event_pool *pool);
extern int amb_event_pool_affuse(struct amb_event_pool *pool,
	enum amb_event_type ev_type);
extern int amb_event_pool_query_index(struct amb_event_pool *pool);
extern int amb_event_pool_query_event(struct amb_event_pool *pool,
	struct amb_event *event, unsigned char index);

#endif /* __ASSEMBLER__ */
/* ==========================================================================*/

#endif

