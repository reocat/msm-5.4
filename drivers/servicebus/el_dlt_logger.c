/********************************************************************************/
/** @file		el_dlt_logger.c
	@brief		event listener dlt logger
	@note		Copyright (C) 2014 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2014/05/22
	@version	1.0
*********************************************************************************/
/*--------------------------------------------------------------------------------
	Include
--------------------------------------------------------------------------------*/
#include "servicebus_common.h"													// service bus common include
#include "el_dlt_logger.h"														// event listener dlt logger include
#include "message_queue.h"														// message queue include
#include "wait_notify.h"														// wait notify include
#include "lock_obj.h"															// lock objetct include

/*--------------------------------------------------------------------------------
	Local Value
--------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
	init
----------------------------------------------------------------------------*/
/**
 *	@fn			Result el_dlt_logger_init(ELDltLoggerObject* obj)
 *	@brief		init dlt logger
 *	@param[in]	WaitNotifyObject* wait_notify
 *	@param[in]	NotifyUserIdType id_pattern
 *	@return		Result SFW_S_OK...OK, SFW_E_FAIL...NG
 */
static Result el_dlt_logger_init(ELDltLoggerObject* obj, ELDltLogger_WaitNotifyObjectType* wait_notify, NotifyUserIdType id_pattern)
{
	Result ret = SFW_S_OK;

	LOCKOBJ_LOCK(obj->mutx_);

	if (obj->msg_queue_ == NULL)
	{
		obj->msg_queue_ = kmalloc(sizeof(MessageQueueObject), GFP_KERNEL);
		if (obj->msg_queue_ == NULL)
		{
			SFW_ERROR_MSG("[Error] %s : allocate memory\n", __FUNCTION__);

			LOCKOBJ_UNLOCK(obj->mutx_);

			return SFW_E_INIT_MEMORY_ALLOCATOR;
		}

		ret = InitializeMessageQueueObject(obj->msg_queue_);
		if (SFW_FAILED(ret))
		{
			SFW_ERROR_MSG("[Error] %s : InitializeMessageQueueObject\n", __FUNCTION__);
			kfree(obj->msg_queue_);
			obj->msg_queue_ = NULL;

			LOCKOBJ_UNLOCK(obj->mutx_);

			return SFW_E_FAIL;
		}

		ret = obj->msg_queue_->init(obj->msg_queue_);
		if (SFW_FAILED(ret))
		{
			SFW_ERROR_MSG("[Error] %s : obj->msg_queue_->init\n", __FUNCTION__);
			kfree(obj->msg_queue_);
			obj->msg_queue_ = NULL;

			LOCKOBJ_UNLOCK(obj->mutx_);

			return SFW_E_FAIL;
		}
	}

	if (obj->buff_ == NULL)
	{
		obj->buff_size_ = 1024;
		obj->buff_ = kmalloc(obj->buff_size_, GFP_KERNEL);

		if (obj->buff_ == NULL)
		{
			SFW_ERROR_MSG("[Error] %s : allocate memory obj->buff_\n", __FUNCTION__);

			LOCKOBJ_UNLOCK(obj->mutx_);

			return SFW_E_INIT_MEMORY_ALLOCATOR;
		}
	}

	obj->wait_notify_ = wait_notify;
	obj->id_pattern_ = id_pattern;

	LOCKOBJ_UNLOCK(obj->mutx_);

	return ret;
}

/*----------------------------------------------------------------------------
	exit
----------------------------------------------------------------------------*/
/**
 *	@fn			void el_dlt_logger_exit(ELDltLoggerObject* obj)
 *	@brief		exit dlt logger
 *	@param		なし
 *	@return		なし
 */
static void el_dlt_logger_exit(ELDltLoggerObject* obj)
{
	LOCKOBJ_LOCK(obj->mutx_);

	if (obj->msg_queue_ != NULL)
	{
		obj->msg_queue_->exit(obj->msg_queue_);
		kfree(obj->msg_queue_);
		obj->msg_queue_ = NULL;
	}

	if (obj->buff_ != NULL)
	{
		obj->buff_size_ = 0;
		kfree(obj->buff_);
		obj->buff_ = NULL;
	}

	obj->wait_notify_ = NULL;
	obj->id_pattern_ = 0;

	LOCKOBJ_UNLOCK(obj->mutx_);
}

/*----------------------------------------------------------------------------
	is_start
----------------------------------------------------------------------------*/
/**
 *	@fn			bool el_dlt_logger_is_start(ELDltLoggerObject* obj)
 *	@brief		is start
 *	@param		なし
 *	@return		bool start...true/stop...false
 */
static bool el_dlt_logger_is_start(ELDltLoggerObject* obj)
{
	bool ret = false;

	LOCKOBJ_LOCK(obj->mutx_);
	ret = obj->is_start_impl(obj);
	LOCKOBJ_UNLOCK(obj->mutx_);

	return ret;
}

/*----------------------------------------------------------------------------
	is_start_impl
----------------------------------------------------------------------------*/
/**
 *	@fn			bool el_dlt_logger_is_start_impl(ELDltLoggerObject* obj)
 *	@brief		is start
 *	@param		なし
 *	@return		bool start...true/stop...false
 */
static bool el_dlt_logger_is_start_impl(ELDltLoggerObject* obj)
{
	return (obj->msg_queue_ != NULL) && (obj->buff_ != NULL);
}

/*----------------------------------------------------------------------------
	pop
----------------------------------------------------------------------------*/
/**
 *	@fn			Result el_dlt_logger_pop(ELDltLoggerObject* obj, char __user * message_ptr, int msg_size, int __user * read_size)
 *	@brief		pop message
 *	@param[out]	char __user * message_ptr	: out buff
 *	@param[in]  int msg_size				: out buff size
 *	@param[out] int* read_size				: read size
 *	@return		Result SFW_S_OK...OK, SFW_E_FAIL...NG
 */
static Result el_dlt_logger_pop(ELDltLoggerObject* obj, char __user * message_ptr, int msg_size, int __user * read_size)
{
	Result ret = SFW_S_OK;

	LOCKOBJ_LOCK(obj->mutx_);

	if (obj->msg_queue_ != NULL)
	{
		int len = 0;
		ret = obj->msg_queue_->pop(obj->msg_queue_, message_ptr, msg_size, &len);
		if (SFW_SUCCEEDED(ret))
		{
			put_user(len, read_size);
		}

		if (!obj->msg_queue_->empty(obj->msg_queue_))
		{
			/* not empty, notify.*/
			if (obj->wait_notify_ != NULL) obj->wait_notify_->notify(obj->wait_notify_, obj->id_pattern_);
		}
	}

	LOCKOBJ_UNLOCK(obj->mutx_);

	return ret;
}

/*--------------------------------------------------------------------------------
	set_msg_log
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	void el_dlt_logger_set_msg_log(ELDltLoggerObject* obj, char* prefix_msg, const char* msg, int msg_size)
 *	@brief		set message data
 *	@param[in]	char* prefix_msg
 *	@param[in]	const char* msg
 *	@param[in]	int msg_size
 *	@return		Result SFW_S_OK...OK, SFW_E_FAIL...NG
 */
static Result el_dlt_logger_set_msg_log(ELDltLoggerObject* obj, char* prefix_msg, const char* msg, int msg_size)
{
	Result ret = SFW_S_OK;
	size_t prefix_msg_len = strlen(prefix_msg);

	// 内部メソッドであるため、引数のチェックはしない

	if ((obj->buff_size_ - prefix_msg_len - 1/*null*/) < msg_size)
	{
		kfree(obj->buff_);

		obj->buff_size_ = (((prefix_msg_len + msg_size) / 1024) + 1) * 1024;
		obj->buff_ = kmalloc(obj->buff_size_, GFP_KERNEL);

		if (obj->buff_ == NULL)
		{
			obj->buff_size_ = 0;
			SFW_ERROR_MSG("[Error] %s : kmalloc error size=%d\n", __FUNCTION__, obj->buff_size_);
			return SFW_E_INIT_MEMORY_ALLOCATOR;
		}
	}

	memcpy(obj->buff_, prefix_msg, prefix_msg_len);

	#ifdef linux
	// msg がユーザ空間の領域であることがあるので、ユーザ空間の場合はコピー方法を変更する
	if (access_ok(msg, msg_size))
	{
		if (copy_from_user(&obj->buff_[prefix_msg_len], msg, msg_size))
		{
			SFW_ERROR_MSG("[Error] %s : copy_from_user\n", __FUNCTION__);
			msg_size = 0;
		}
	}
	else
	#endif // linex
	{
		memcpy(&obj->buff_[prefix_msg_len], msg, msg_size);
	}

	obj->buff_[prefix_msg_len + msg_size] = 0;

	return ret;
}

/*--------------------------------------------------------------------------------
	dispatch
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result el_dlt_logger_dispatch(ELDltLoggerObject* obj, ESMessageType* es_msg)
 *	@brief		change handler
 *	@param[in]	const char* data
 *	@param[in]	int data_size
 *	@param[in]	BusStatus status
 *	@param[in]	int queue_size
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result el_dlt_logger_dispatch(ELDltLoggerObject* obj, ESMessageType* es_msg)
{
	Result ret = SFW_S_OK;
	#ifdef linux
	mm_segment_t old_fs;
	#endif // linux

	EventParamServiceNameInfo* service_info = es_msg->data_1;
//	EventParamActionResultInfo* action_info = es_msg->data_2;
	EventParamMsgInfo* msg_info 			= es_msg->data_3;
//	EventParamNotifyInfo* event_info		= es_msg->data_4;

	LOCKOBJ_LOCK(obj->mutx_);

	if (!obj->is_start_impl(obj))
	{
		LOCKOBJ_UNLOCK(obj->mutx_);
		return SFW_S_OK;
	}

	switch (es_msg->event_type)
	{
	case BUS_EVENT_REGISTER_SERVICE:
		ret = obj->set_msg_log(obj, "REG:", service_info->name, strlen(service_info->name));
		break;
	case BUS_EVENT_REGISTER_SERVICE_ERROR:
		ret = obj->set_msg_log(obj, "ERR:REG:", service_info->name, strlen(service_info->name));
		break;
	case BUS_EVENT_UNREGISTER_SERVICE:
		ret = obj->set_msg_log(obj, "UNR:", service_info->name, strlen(service_info->name));
		break;
	case BUS_EVENT_UNREGISTER_SERVICE_ERROR:
		ret = obj->set_msg_log(obj, "ERR:UNR:", service_info->name, strlen(service_info->name));
		break;
	case BUS_EVENT_MESSAGE_PUSH:
		ret = obj->set_msg_log(obj, "SND:", msg_info->msg, msg_info->msg_size);
		break;
	case BUS_EVENT_MESSAGE_PUSH_ERROR:
		ret =obj->set_msg_log(obj, "ERR:SND:", msg_info->msg, msg_info->msg_size);
		break;
	case BUS_EVENT_MESSAGE_SET:
		ret = obj->set_msg_log(obj, "SET:", msg_info->msg, msg_info->msg_size);
		break;
	case BUS_EVENT_MESSAGE_SET_ERROR:
		ret = obj->set_msg_log(obj, "ERR:SET:", msg_info->msg, msg_info->msg_size);
		break;
	case BUS_EVENT_MESSAGE_POP:
		if (msg_info->msg_size > 0) ret = obj->set_msg_log(obj, "RCV:", msg_info->msg, msg_info->msg_size);
		break;
	case BUS_EVENT_MESSAGE_POP_ERROR:
		if (msg_info->msg_size > 0) ret = obj->set_msg_log(obj, "ERR:RCV:", msg_info->msg, msg_info->msg_size);
		break;
	default:
		ret = SFW_E_FAIL;
		break;
	}

	if (SFW_SUCCEEDED(ret))
	{
		#ifdef linux
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		#endif // linux

		ret = obj->msg_queue_->push(obj->msg_queue_, obj->buff_, strlen(obj->buff_));

		#ifdef linux
		set_fs(old_fs);
		#endif // linux

		if (SFW_SUCCEEDED(ret) && (obj->wait_notify_ != NULL)) obj->wait_notify_->notify(obj->wait_notify_, obj->id_pattern_);
	}

	LOCKOBJ_UNLOCK(obj->mutx_);

	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	InitializeELDltLoggerObject
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result InitializeELDltLoggerObject(ELDltLoggerObject* obj)
 *	@brief		constructor
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
Result InitializeELDltLoggerObject(ELDltLoggerObject* obj)
{
	Result ret = SFW_S_OK;

	if (obj == NULL) return SFW_E_INVALIDARG;

	ret = InitializeEventListenerObject((EventListenerObject*)obj);

	if (SFW_FAILED(ret)) return SFW_E_FAIL;

	obj->dispatch 				= (EventListener_dispatch)el_dlt_logger_dispatch;
	obj->exit					= (EventListener_exit)el_dlt_logger_exit;

	mutex_init(&obj->mutx_);
	obj->msg_queue_				= NULL;
	obj->wait_notify_			= NULL;
	obj->id_pattern_			= 0;
	obj->buff_size_				= 0;
	obj->buff_ 					= NULL;

	obj->init					= el_dlt_logger_init;
	obj->is_start				= el_dlt_logger_is_start;
	obj->is_start_impl			= el_dlt_logger_is_start_impl;
	obj->get					= el_dlt_logger_pop;
	obj->set_msg_log			= el_dlt_logger_set_msg_log;

	return SFW_S_OK;
}
