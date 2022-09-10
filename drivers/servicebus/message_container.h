/********************************************************************************/
/** @file		message_container.h
	@brief		message container
	@note		Copyright (C) 2013 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2013/09/30
	@version	1.0
*********************************************************************************/
#if !defined(_MESSAGE_CONTAINER__INCLUDED_)
#define _MESSAGE_CONTAINER__INCLUDED_

/*--------------------------------------------------------------------------------
								include files
--------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------
							   interface define
--------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------
								local define
--------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------
								type rename
--------------------------------------------------------------------------------*/
typedef struct _MessageContainerObject						MessageContainerObjectType;
typedef struct _MessageQueueObject							MessageContainer_MQObjectType;
typedef struct _WaitNotifyObject							MessageContainer_WaitNotifyObjectType;
typedef struct _EventSourceObject							MessageContainer_EventSourceObjectType;

/*--------------------------------------------------------------------------------
							  typedef function
--------------------------------------------------------------------------------*/
typedef Result (*MessageContainer_Init)(MessageContainerObjectType*, bool);
typedef Result (*MessageContainer_Exit)(MessageContainerObjectType*);
typedef Result (*MessageContainer_Push)(MessageContainerObjectType*, const char __user *, int);
typedef Result (*MessageContainer_Pop)(MessageContainerObjectType*, char __user *, int, int*);
typedef bool (*MessageContainer_Empty)(MessageContainerObjectType*);
typedef unsigned int (*MessageContainer_Count)(MessageContainerObjectType*);
typedef int (*MessageContainer_Attach)(MessageContainerObjectType*);
typedef int (*MessageContainer_Detach)(MessageContainerObjectType*);

/*--------------------------------------------------------------------------------
						Object (MessageContainerObject)
--------------------------------------------------------------------------------*/
typedef struct _MessageContainerObject
{
	/* +++ member value +++ */
	MessageContainer_MQObjectType*					msg_queue_;
	MessageContainer_WaitNotifyObjectType*			wait_notify_;
	MessageContainer_EventSourceObjectType*			event_source_;
	NotifyGroupIdType								group_id_;
	NotifyUserIdType								user_id_;
	int												ref_count_;
	bool											strictly_overwrite_;
	bool											dummy_;

	/* +++ public member function +++ */
	MessageContainer_Init							init;						// init method
	MessageContainer_Exit							exit;						// exit method
	MessageContainer_Push							push;						// push method
	MessageContainer_Push							set;						// set method
	MessageContainer_Pop							pop;						// pop method
	MessageContainer_Empty							empty;						// empty method
	MessageContainer_Count							count;						// count method

	MessageContainer_Attach							attach;						// attach method
	MessageContainer_Detach							detach;						// detach method

} MessageContainerObject;

/*--------------------------------------------------------------------------------
							interface of MessageContainerObject
--------------------------------------------------------------------------------*/
extern Result InitializeMessageContainerObject(MessageContainerObject* obj);

#endif // _MESSAGE_CONTAINER__INCLUDED_
