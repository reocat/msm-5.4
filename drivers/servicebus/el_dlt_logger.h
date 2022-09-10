/********************************************************************************/
/** @file		el_dlt_logger.h
	@brief		event listener dlt logger
	@note		Copyright (C) 2014 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2014/05/22
	@version	1.0
*********************************************************************************/
#if !defined(_EL_DLT_LOGGER__INCLUDED_)
#define _EL_DLT_LOGGER__INCLUDED_

/*--------------------------------------------------------------------------------
								include files
--------------------------------------------------------------------------------*/
#include "event_listener.h"
#include <linux/mutex.h>

/*--------------------------------------------------------------------------------
							   interface define
--------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------
								local define
--------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------
								type rename
--------------------------------------------------------------------------------*/
typedef struct _ELDltLoggerObject							ELDltLoggerObjectType;
typedef struct _MessageQueueObject							ELDltLogger_MQObjectType;
typedef struct _EventDispatcherObject 						ELDltLogger_EventDispatcherObjectType;
typedef struct _WaitNotifyObject							ELDltLogger_WaitNotifyObjectType;

/*--------------------------------------------------------------------------------
							  typedef function
--------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------
							  local typedef struct
--------------------------------------------------------------------------------*/
typedef Result (*ELDltLogger_Init)(ELDltLoggerObjectType*, ELDltLogger_WaitNotifyObjectType*, NotifyUserIdType);
typedef Result (*ELDltLogger_Exit)(ELDltLoggerObjectType*);
typedef Result (*ELDltLogger_Pop)(ELDltLoggerObjectType*, char __user *, int, int __user *);
typedef bool (*ELDltLogger_IS_Start)(ELDltLoggerObjectType*);
typedef Result (*ELDltLogger_SetMsgLog)(ELDltLoggerObjectType*, char*, const char*, int);

/*--------------------------------------------------------------------------------
						Object (ELLoggerObject)
--------------------------------------------------------------------------------*/
typedef struct _ELDltLoggerObject
{
	/* +++ base object +++ */
	EventListenerObjectDefine

	#ifdef _TKERNEL_
	FastLock												mutx_;
	#else
	struct mutex 											mutx_;
	#endif // _TKERNEL_
	ELDltLogger_MQObjectType*								msg_queue_;
	ELDltLogger_WaitNotifyObjectType*						wait_notify_;
	NotifyUserIdType										id_pattern_;
	int														buff_size_;
	char*													buff_;

	/* +++ public member function +++ */
	ELDltLogger_Init										init;				// init method
	ELDltLogger_IS_Start									is_start;			// is start method
	ELDltLogger_IS_Start									is_start_impl;		// is start impl method
	ELDltLogger_Pop											get;				// pop method
	ELDltLogger_SetMsgLog									set_msg_log;		// set msg log method

} ELDltLoggerObject;

/*--------------------------------------------------------------------------------
						interface of ELDltLoggerObject
--------------------------------------------------------------------------------*/
extern Result InitializeELDltLoggerObject(ELDltLoggerObject* obj);

#endif // _EL_DLT_LOGGER__INCLUDED_
