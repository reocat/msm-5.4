/********************************************************************************/
/** @file		message_queue.h
	@brief		message queue
	@note		Copyright (C) 2013 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2013/09/30
	@version	1.0
*********************************************************************************/
#if !defined(_MESSAGE_QUEUE__INCLUDED_)
#define _MESSAGE_QUEUE__INCLUDED_

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
typedef struct _MessageQueueObject							MessageQueueObjectType;
typedef char 												BufferType;

/*--------------------------------------------------------------------------------
							  typedef function
--------------------------------------------------------------------------------*/
typedef Result (*MessageQueue_Init)(MessageQueueObjectType*);
typedef Result (*MessageQueue_Exit)(MessageQueueObjectType*);
typedef Result (*MessageQueue_Push)(MessageQueueObjectType*, const char __user *, int);
typedef Result (*MessageQueue_Set)(MessageQueueObjectType*, const char __user *, int);
typedef Result (*MessageQueue_Pop)(MessageQueueObjectType*, char __user *, int, int*);
typedef bool (*MessageQueue_Empty)(MessageQueueObjectType*);
typedef unsigned int (*MessageQueue_Count)(MessageQueueObjectType*);
typedef void (*MessageQueue_ChangeMsgLimitSize)(MessageQueueObjectType*, long);

/*--------------------------------------------------------------------------------
							  local typedef struct
--------------------------------------------------------------------------------*/
typedef struct _MessageType
{
	BufferType* buffer_ptr;
	int size;
	int pos;

	struct list_head list;

} MessageType;

/*--------------------------------------------------------------------------------
						Object (MessageQueueObject)
--------------------------------------------------------------------------------*/
typedef struct _MessageQueueObject
{
	/* +++ member value +++ */
	long											total_msg_size_;
	long											total_msg_limit_size_;

	MessageType										msg_queue_;
	int 											queue_size_;

	/* +++ public member function +++ */
	MessageQueue_Init								init;						// init method
	MessageQueue_Exit								exit;						// exit method
	MessageQueue_Push								push;						// push method
	MessageQueue_Set								set;						// set method
	MessageQueue_Set								strictly_set;				// strictly_set method
	MessageQueue_Pop								pop;						// pop method
	MessageQueue_Empty								empty;						// empty method
	MessageQueue_Count								count;						// count method
	MessageQueue_ChangeMsgLimitSize					change_msg_limit_size;		// change msg limit size

} MessageQueueObject;

/*--------------------------------------------------------------------------------
							interface of MessageQueueObject
--------------------------------------------------------------------------------*/
extern Result InitializeMessageQueueObject(MessageQueueObject* obj);

#endif // _MESSAGE_QUEUE__INCLUDED_
