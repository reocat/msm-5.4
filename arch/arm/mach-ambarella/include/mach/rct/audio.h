/*
 * ambhw/rct/rct.h
 *
 * History:
 *	2006/12/27 - [Charles Chiou] created file
 *	2008/02/19 - [Allen Wang] changed to use capabilities and chip ID
 *	2008/05/07 - [Allen Wang] moved to separate files at directory 'rct'	
 *	2008/05/13 - [Allen Wang] added capabilities of A2S and A2M silicons  
 *
 * Copyright (C) 2006-2008, Ambarella, Inc.
 */

#ifndef __AMBHW__RCT_AUDIO_H__
#define __AMBHW__RCT_AUDIO_H__

/****************************************************/
/* Capabilities based on chip revision              */
/****************************************************/

#define RCT_SUPPORT_CG_SETTING_CHANGED	1

#if (CHIP_REV == A1)
#define RCT_AUDIO_PLL_CONF_MODE		0
#elif (CHIP_REV == A2) || (CHIP_REV == A3)
#define RCT_AUDIO_PLL_CONF_MODE		1
#else
#define RCT_AUDIO_PLL_CONF_MODE         2
#endif

#define PLL_LOCK_AUDIO  	(0x1 << 7)

#endif
