/********************************************************************************/
/** @file		event_listener_logger.c
	@brief		event listener logger
	@note		Copyright (C) 2013 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2013/02/25
	@version	1.0
*********************************************************************************/
/*--------------------------------------------------------------------------------
	Include
--------------------------------------------------------------------------------*/
#include "servicebus_common.h"													// service bus common include
#include "el_logger.h"															// event listener logger include

/*--------------------------------------------------------------------------------
	Local Value
--------------------------------------------------------------------------------*/
// #define EL_LOGGER_FULL_OUTPUT

/*--------------------------------------------------------------------------------
	dispatch
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result el_logger_dispatch(ELLoggerObject* obj, ESMessageType* es_msg)
 *	@brief		change handler
 *	@param[in]	const char* data
 *	@param[in]	int data_size
 *	@param[in]	BusStatus status
 *	@param[in]	int queue_size
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result el_logger_dispatch(ELLoggerObject* obj, ESMessageType* es_msg)
{
	EventParamServiceNameInfo* service_info = es_msg->data_1;
//	EventParamActionResultInfo* action_info = es_msg->data_2;
	EventParamMsgInfo* msg_info 			= es_msg->data_3;
//	EventParamNotifyInfo* event_info		= es_msg->data_4;
	char* msg_ptr							= NULL;

	#ifndef EL_LOGGER_FULL_OUTPUT
	/*
	// ユーザ空間からカーネル空間にコピーする処理が走るが、通常はでは送信のログは出さないので、
	// ここで弾くようにする、送信のログを出したい場合は、このファイルの上部の EL_LOGGER_FULL_OUTPUT の define を有効にする
	*/
	if ((es_msg->event_type == BUS_EVENT_MESSAGE_PUSH) || (es_msg->event_type == BUS_EVENT_MESSAGE_SET))
	{
		return SFW_S_OK;
	}
	#endif // !EL_LOGGER_FULL_OUTPUT

	if ((msg_info != NULL) && (msg_info->msg_size > 0))
	{
		/*
		// msg_info->msg がユーザ空間の領域である場合があるため、
		// ユーザ空間の場合はカーネル空間へコピーする
		*/
		#ifdef linux
		if (access_ok(msg_info->msg, msg_info->msg_size))
		{
			int copy_len = msg_info->msg_size > (EL_LOGGER_BUFF_LEN_MAX - 1) ? (EL_LOGGER_BUFF_LEN_MAX - 1) : msg_info->msg_size;

			if (copy_from_user(obj->buff_, msg_info->msg, copy_len))
			{
				SFW_ERROR_MSG("[Error] %s : copy_from_user\n", __FUNCTION__);
				return SFW_E_FAIL;
			}

			obj->buff_[copy_len] = 0;

			msg_ptr = obj->buff_;
		}
		else
		#endif // linux
		{
			msg_ptr = (char*)msg_info->msg;
		}
	}

	switch (es_msg->event_type)
	{
	case BUS_EVENT_REGISTER_SERVICE:
		SFW_DEBUG_OUT("[BUS] register : %s pid=%d tid=%d id=%lld\n", service_info->name, (int)(service_info->group_id >> 16), (int)(service_info->group_id & 0x0000ffff), service_info->user_id);
		break;
	case BUS_EVENT_UNREGISTER_SERVICE:
		SFW_DEBUG_OUT("[BUS] unregister : %s\n", service_info->name);
		break;
	case BUS_EVENT_MESSAGE_PUSH:
		SFW_DEBUG_OUT("[BUS] send %.*s\n", EL_LOGGER_BUFF_LEN_MAX, msg_ptr);
		break;
	case BUS_EVENT_MESSAGE_SET:
		SFW_DEBUG_OUT("[BUS] set %.*s\n", EL_LOGGER_BUFF_LEN_MAX, msg_ptr);
		break;
	case BUS_EVENT_MESSAGE_POP:
		SFW_DEBUG_OUT("[BUS] receive %.*s\n", EL_LOGGER_BUFF_LEN_MAX, msg_info->msg_size == 0 ? "an empty message." : msg_ptr);
		break;
	case BUS_EVENT_REGISTER_SERVICE_ERROR:
		SFW_DEBUG_OUT("[BUS] Error! register : %s pid=%d tid=%d id=%lld\n", service_info->name, (int)(service_info->group_id >> 16), (int)(service_info->group_id & 0x0000ffff), service_info->user_id);
		break;
	case BUS_EVENT_UNREGISTER_SERVICE_ERROR:
		SFW_DEBUG_OUT("[BUS] Error! unregister : %s\n", service_info->name);
		break;
	case BUS_EVENT_MESSAGE_PUSH_ERROR:
		SFW_DEBUG_OUT("[BUS] Error! send %.*s\n", EL_LOGGER_BUFF_LEN_MAX, msg_ptr);
		break;
	case BUS_EVENT_MESSAGE_SET_ERROR:
		SFW_DEBUG_OUT("[BUS] Error! set %.*s\n", EL_LOGGER_BUFF_LEN_MAX, msg_ptr);
		break;
	case BUS_EVENT_MESSAGE_POP_ERROR:
		SFW_DEBUG_OUT("[BUS] Error! receive %.*s\n", EL_LOGGER_BUFF_LEN_MAX, msg_info->msg_size == 0 ? "an empty message." : msg_ptr);
		break;
	default:
		return SFW_E_FAIL;
	}

	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	ELLoggerObject
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result InitializeELLoggerObject(ELLoggerObject* obj)
 *	@brief		constructor
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
Result InitializeELLoggerObject(ELLoggerObject* obj)
{
	Result ret = SFW_S_OK;

	if (obj == NULL) return SFW_E_INVALIDARG;

	ret = InitializeEventListenerObject((EventListenerObject*)obj);

	if (SFW_FAILED(ret)) return SFW_E_FAIL;

	obj->dispatch = (EventListener_dispatch)el_logger_dispatch;

	return SFW_S_OK;
}
