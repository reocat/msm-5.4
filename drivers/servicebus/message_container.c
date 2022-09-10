/********************************************************************************/
/** @file		message_container.c
	@brief		message container
	@note		Copyright (C) 2013 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2013/10/04
	@version	1.0
*********************************************************************************/
/*--------------------------------------------------------------------------------
	Include
--------------------------------------------------------------------------------*/
#include "servicebus_common.h"													// service bus common include
#include "message_container.h"													// message container include
#include "message_queue.h"														// message queue include
#include "event_source.h"														// event source include
#include "wait_notify.h"														// wait notify include

/*----------------------------------------------------------------------------
	external funciton
----------------------------------------------------------------------------*/
extern int get_qmaxsize(void);

/*--------------------------------------------------------------------------------
	init
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result message_container_init(MessageContainerObject* obj, bool strictly_overwrite)
 *	@brief		init message container
 *	@param[in]	bool strictly_overwrite
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result message_container_init(MessageContainerObject* obj, bool strictly_overwrite)
{
	obj->strictly_overwrite_ = strictly_overwrite;

	return (obj->msg_queue_ != NULL) ? obj->msg_queue_->init(obj->msg_queue_) : SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	exit
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result message_container_exit(MessageContainerObject* obj)
 *	@brief		exit message queue
 *	@param		なし
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result message_container_exit(MessageContainerObject* obj)
{
	if (obj->msg_queue_ != NULL)
	{
		obj->msg_queue_->exit(obj->msg_queue_);
		kfree(obj->msg_queue_);
		obj->msg_queue_ = NULL;
	}

	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	push
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result message_container_push(MessageContainerObject* obj, const char __user * message_ptr, int msg_size)
 *	@brief		push message
 *	@param[in]	const char __user * message_ptr	: message
 *	@param[in]	int msg_size					: message size
 *	@return		Result SFW_S_OK...OK, SFW_E_INIT_MEMORY_ALLOCATOR/SFW_E_INVALID_SIZELIMIT...NG
 */
static Result message_container_push(MessageContainerObject* obj, const char __user * message_ptr, int msg_size)
{
	Result ret = SFW_S_OK;

	if (obj->msg_queue_ == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : obj->msg_queue_ == NULL || obj->event_source_ == NULL\n", __FUNCTION__);
		return SFW_E_FAIL;
	}

	ret = obj->msg_queue_->push(obj->msg_queue_, message_ptr, msg_size);

	if (obj->event_source_ != NULL)
	{
		EventParamActionResultInfo action_info = {
			.result = ret
		};

		EventParamMsgInfo msg_info = {
			.msg = message_ptr,
			.msg_size = msg_size,
			.queue_count = obj->count(obj)
		};

		EventParamNotifyInfo event_info = {
			.wait_notify = obj->wait_notify_,
			.id_pattern = obj->user_id_
		};

		obj->event_source_->update(obj->event_source_, SFW_SUCCEEDED(ret) ? BUS_EVENT_MESSAGE_PUSH : BUS_EVENT_MESSAGE_PUSH_ERROR, NULL, (void*)&action_info, (void*)&msg_info, (void*)&event_info);
	}

	return ret;
}

/*--------------------------------------------------------------------------------
	set
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result message_container_set(MessageContainerObject* obj, const char __user * message_ptr, int msg_size)
 *	@brief		set message
*	@param[in]	const char __user * message_ptr	: message
 *	@param[in]	int msg_size					: message size
 *	@return		Result SFW_S_OK...OK, SFW_E_INIT_MEMORY_ALLOCATOR/SFW_E_INVALID_SIZELIMIT...NG
 */
static Result message_container_set(MessageContainerObject* obj, const char __user * message_ptr, int msg_size)
{
	Result ret = SFW_S_OK;

	if (obj->msg_queue_ == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : obj->msg_queue_ == NULL || obj->event_source_ == NULL\n", __FUNCTION__);
		return SFW_E_FAIL;
	}

	ret = obj->strictly_overwrite_ ? obj->msg_queue_->strictly_set(obj->msg_queue_, message_ptr, msg_size) : obj->msg_queue_->set(obj->msg_queue_, message_ptr, msg_size);

	if (obj->event_source_ != NULL)
	{
		EventParamActionResultInfo action_info = {
			.result = ret
		};

		EventParamMsgInfo msg_info = {
			.msg = message_ptr,
			.msg_size = msg_size,
			.queue_count = obj->count(obj)
		};

		EventParamNotifyInfo event_info = {
			.wait_notify = obj->wait_notify_,
			.id_pattern = obj->user_id_
		};

		obj->event_source_->update(obj->event_source_, SFW_SUCCEEDED(ret) ? BUS_EVENT_MESSAGE_SET : BUS_EVENT_MESSAGE_SET_ERROR, NULL, (void*)&action_info, (void*)&msg_info, (void*)&event_info);
	}

	return ret;
}

/*----------------------------------------------------------------------------
	pop
----------------------------------------------------------------------------*/
/**
 *	@fn			Result message_container_pop(MessageContainerObject* obj, char __user * message_ptr, int msg_size, int* read_size)
 *	@brief		pop message
 *	@param[out]	char __user * message_ptr	: out buff
 *	@param[in]  int msg_size				: out buff size
 *	@param[out] int* read_size				: read size
 *	@return		Result SFW_S_OK/SFW_S_SIZELIMIT...OK, SFW_E_BAD_LENGTH...NG
 */
static Result message_container_pop(MessageContainerObject* obj, char __user * message_ptr, int msg_size, int* read_size)
{
	Result ret = SFW_S_OK;

	if (obj->msg_queue_ == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : obj->msg_queue_ == NULL\n", __FUNCTION__);
		return SFW_E_FAIL;
	}

	ret = obj->msg_queue_->pop(obj->msg_queue_, message_ptr, msg_size, read_size);

	if (obj->event_source_ != NULL)
	{
		EventParamActionResultInfo action_info = {
			.result = ret
		};

		EventParamMsgInfo msg_info = {
			.msg = message_ptr,
			.msg_size = *read_size,
			.queue_count = obj->count(obj)
		};

		EventParamNotifyInfo event_info = {
			.wait_notify = obj->wait_notify_,
			.id_pattern = obj->user_id_
		};

		obj->event_source_->update(obj->event_source_, SFW_SUCCEEDED(ret) ? BUS_EVENT_MESSAGE_POP : BUS_EVENT_MESSAGE_POP_ERROR, NULL, (void*)&action_info, (void*)&msg_info, (void*)&event_info);
	}

	return ret;
}

/*----------------------------------------------------------------------------
	empty
----------------------------------------------------------------------------*/
/**
 *	@fn			bool message_container_empty(MessageContainerObject* obj)
 *	@brief		empty
 *	@param		nothing
 *	@return		bool true...empty, false...!empty
 */
static bool message_container_empty(MessageContainerObject* obj)
{
	if (obj->msg_queue_ == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : obj->msg_queue_ == NULL\n", __FUNCTION__);
		return true;
	}

	return obj->msg_queue_->empty(obj->msg_queue_);
}

/*----------------------------------------------------------------------------
	count
----------------------------------------------------------------------------*/
/**
 *	@fn			unsigned int message_container_count(MessageContainerObject* obj)
 *	@brief		queue count
 *	@param		nothing
 *	@return		int 0...empty, !0...!empty
 */
static unsigned int message_container_count(MessageContainerObject* obj)
{
	if (obj->msg_queue_ == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : obj->msg_queue_ == NULL\n", __FUNCTION__);
		return 0;
	}

	return obj->msg_queue_->count(obj->msg_queue_);
}

/*----------------------------------------------------------------------------
	attach
----------------------------------------------------------------------------*/
/**
 *	@fn			void message_container_attach(MessageContainerObject* obj)
 *	@brief		参照を追加
 *	@param		なし
 *	@return		int 現在の参照カウンタ
 */
static int message_container_attach(MessageContainerObject* obj)
{
	// 参照カウンタをアップし、参照カウンタに基づいてキューのサイズを設定
	int counter = obj->ref_count_ ++;

	obj->msg_queue_->change_msg_limit_size(obj->msg_queue_, get_qmaxsize() * 1024 * counter);

	return counter;
}

/*----------------------------------------------------------------------------
	detach
----------------------------------------------------------------------------*/
/**
 *	@fn			void message_container_detach(MessageContainerObject* obj)
 *	@brief		参照を削除
 *	@param		なし
 *	@return		int 現在の参照カウンタ
 */
static int message_container_detach(MessageContainerObject* obj)
{
	// 参照カウンタをダウンし、参照カウンタに基づいてキューのサイズを設定
	int counter = obj->ref_count_ > 0 ? (-- obj->ref_count_) : 0;

	obj->msg_queue_->change_msg_limit_size(obj->msg_queue_, get_qmaxsize() * 1024 * counter);

	return counter;
}

/*--------------------------------------------------------------------------------
	InitializeMessageContainerObject
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result InitializeMessageContainerObject(MessageContainerObject* obj)
 *	@brief		constructor
 *	@param		nothing
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
Result InitializeMessageContainerObject(MessageContainerObject* obj)
{
	if (obj == NULL) return SFW_E_INVALIDARG;

	obj->msg_queue_				= NULL;
	obj->wait_notify_			= NULL;
	obj->event_source_			= NULL;
	obj->group_id_				= 0;
	obj->user_id_				= 0;
	obj->ref_count_				= 1;
	obj->strictly_overwrite_	= false;

	obj->init					= message_container_init;
	obj->exit					= message_container_exit;
	obj->push					= message_container_push;
	obj->set					= message_container_set;
	obj->pop					= message_container_pop;
	obj->empty					= message_container_empty;
	obj->count					= message_container_count;

	obj->attach					= message_container_attach;
	obj->detach					= message_container_detach;

	return SFW_S_OK;
}
