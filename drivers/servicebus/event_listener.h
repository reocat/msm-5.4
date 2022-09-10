/********************************************************************************/
/** @file		event_listener.h
	@brief		base event listener
	@note		Copyright (C) 2013 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2013/02/25
	@version	1.0
*********************************************************************************/
#if !defined(_EVENT_LISTENER__INCLUDED_)
#define _EVENT_LISTENER__INCLUDED_

/*--------------------------------------------------------------------------------
								include files
--------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------
							   interface define
--------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------
								local define
--------------------------------------------------------------------------------*/
typedef enum _BusEventType
{
	BUS_EVENT_REGISTER_SERVICE = 0,
	BUS_EVENT_UNREGISTER_SERVICE,
	BUS_EVENT_MESSAGE_PUSH,
	BUS_EVENT_MESSAGE_SET,
	BUS_EVENT_MESSAGE_POP,

	BUS_EVENT_REGISTER_SERVICE_ERROR = 1000,
	BUS_EVENT_UNREGISTER_SERVICE_ERROR,
	BUS_EVENT_MESSAGE_PUSH_ERROR,
	BUS_EVENT_MESSAGE_SET_ERROR,
	BUS_EVENT_MESSAGE_POP_ERROR

} BusEventType;

/*--------------------------------------------------------------------------------
								type rename
--------------------------------------------------------------------------------*/
typedef struct _EventListenerObject								EventListenerObjectType;
typedef struct _ESMessage										ESMessageType;
typedef struct _WaitNotifyObject								ESWaitNotifyObjectType;

/*--------------------------------------------------------------------------------
							  typedef function
--------------------------------------------------------------------------------*/
typedef Result (*EventListener_dispatch)(EventListenerObjectType*, ESMessageType*);
typedef void (*EventListener_exit)(EventListenerObjectType*);

/*--------------------------------------------------------------------------------
							  local typedef struct
--------------------------------------------------------------------------------*/
typedef struct _EventParamServiceNameInfo
{
	const char*										name;
	NotifyGroupIdType								group_id;
	NotifyUserIdType								user_id;

} EventParamServiceNameInfo;

typedef struct _EventParamActionResultInfo
{
	Result 											result;

} EventParamActionResultInfo;

typedef struct _EventParamMsgInfo
{
	const char*										msg;
	int 											msg_size;
	unsigned int									queue_count;

} EventParamMsgInfo;

typedef struct _EventParamNotifyInfo
{
	ESWaitNotifyObjectType* 						wait_notify;
	NotifyUserIdType								id_pattern;

} EventParamNotifyInfo;

/*--------------------------------------------------------------------------------
							  local typedef struct
--------------------------------------------------------------------------------*/
typedef struct _ESMessage
{
	BusEventType 									event_type;

	void*											data_1;
	void*											data_2;
	void*											data_3;
	void*											data_4;

} ESMessage;

/*--------------------------------------------------------------------------------
						 	local define
--------------------------------------------------------------------------------*/
#define EventListenerObjectDefine												/* base object real */			\
																												\
	/* +++ public member function +++ */																		\
	EventListener_dispatch							dispatch;					/* dispatch method */			\
	EventListener_exit								exit;						/* exit method */

/*--------------------------------------------------------------------------------
						Object (EventSourceObject)
--------------------------------------------------------------------------------*/
typedef struct _EventListenerObject
{
	/* ::::: base event listener object ::::: */
	EventListenerObjectDefine

} EventListenerObject;

/*--------------------------------------------------------------------------------
						interface of EventListenerObject
--------------------------------------------------------------------------------*/
extern Result InitializeEventListenerObject(EventListenerObject* pObj);

#endif // _EVENT_LISTENER__INCLUDED_
