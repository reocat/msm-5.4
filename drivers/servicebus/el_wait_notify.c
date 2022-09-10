/********************************************************************************/
/** @file		el_wait_notify.c
	@brief		wait notify
	@note		Copyright (C) 2014 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2014/05/08
	@version	1.0
*********************************************************************************/
/*--------------------------------------------------------------------------------
	Include
--------------------------------------------------------------------------------*/
#include "servicebus_common.h"													// service bus common include
#include "el_wait_notify.h"														// el wait notify include
#include "wait_notify.h"														// wait notify

/*--------------------------------------------------------------------------------
	Local Value
--------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------
	dispatch
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result el_wait_notify_dispatch(ELWaitNotifyObject* obj, ESMessageType* es_msg)
 *	@brief		change handler
 *	@param[in]	const char* data
 *	@param[in]	BusStatus status
 *	@param[in]	int data_size
 *	@param[in]	int queue_size
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result el_wait_notify_dispatch(ELWaitNotifyObject* obj, ESMessageType* es_msg)
{
//	EvnetParamServiceNameInfo* service_info = es_msg->data_1;
	EventParamActionResultInfo* action_info = es_msg->data_2;
	EventParamMsgInfo* msg_info 			= es_msg->data_3;
	EventParamNotifyInfo* event_info		= es_msg->data_4;

	if ((es_msg->event_type == BUS_EVENT_MESSAGE_PUSH) ||
		((es_msg->event_type == BUS_EVENT_MESSAGE_SET) && (action_info->result != SFW_S_UPDATE)) ||
		((es_msg->event_type == BUS_EVENT_MESSAGE_POP) && (msg_info->queue_count > 0)))
	{
		event_info->wait_notify->notify(event_info->wait_notify, event_info->id_pattern);
	}

	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	InitializeELWaitNotifyObject
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result InitializeELWaitNotifyObject(ELWaitNotifyObject* obj)
 *	@brief		constructor
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
Result InitializeELWaitNotifyObject(ELWaitNotifyObject* obj)
{
	Result ret = SFW_S_OK;

	if (obj == NULL) return SFW_E_INVALIDARG;

	ret = InitializeEventListenerObject((EventListenerObject*)obj);

	if (SFW_FAILED(ret)) return SFW_E_FAIL;

	obj->dispatch = (EventListener_dispatch)el_wait_notify_dispatch;

	return SFW_S_OK;
}
