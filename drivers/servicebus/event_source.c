/********************************************************************************/
/** @file		event_source.c
	@brief		event soruce
	@note		Copyright (C) 2013 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2013/02/25
	@version	1.0
*********************************************************************************/
/*--------------------------------------------------------------------------------
	Include
--------------------------------------------------------------------------------*/
#include "servicebus_common.h"													// service bus common include
#include "event_source.h"														// event source include
#include "event_listener.h"														// event listener include

/*--------------------------------------------------------------------------------
	Local Value
--------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------
	init
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result event_source_init(EventSourceObject* obj)
 *	@brief		init event source
 *	@param		なし
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result event_source_init(EventSourceObject* obj)
{
	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	exit
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result event_source_exit(EventSourceObject* obj)
 *	@brief		exit event source
 *	@param		なし
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result event_source_exit(EventSourceObject* obj)
{
	while (!list_empty(&obj->event_listener_list_.list))
	{
		EventListenerListInfo* listener_obj_list = list_entry(obj->event_listener_list_.list.next, struct _EventListenerListInfo, list);

		if (listener_obj_list != NULL)
		{
			// call virtual destructor
			if (listener_obj_list->obj != NULL) listener_obj_list->obj->exit(listener_obj_list->obj);

			list_del(&listener_obj_list->list);
			kfree(listener_obj_list);
		}
	}

	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	register
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result event_source_register(EventSourceObject* obj, EventListenerObject* listener)
 *	@brief		add listener
 *	@param[in]	EventListenerObject* listener
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result event_source_register(EventSourceObject* obj, EventListenerObject* listener)
{
	EventListenerListInfo* listener_obj_list = NULL;
	struct list_head* listptr = NULL;

	if (listener == NULL) return SFW_E_INVALIDARG;

	list_for_each(listptr, &obj->event_listener_list_.list)
	{
		listener_obj_list = list_entry(listptr, struct _EventListenerListInfo, list);

		if ((listener_obj_list != NULL) && (listener_obj_list->obj == listener))
		{
			return SFW_S_OK;
		}
 	}

	listener_obj_list = kmalloc(sizeof(EventListenerListInfo), GFP_KERNEL);
	if (listener_obj_list == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : new EventListenerListInfo.\n", __FUNCTION__);
		return SFW_E_INIT_MEMORY_ALLOCATOR;
	}

	listener_obj_list->obj  = listener;

	list_add(&listener_obj_list->list, &obj->event_listener_list_.list);

	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	unregister
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result event_source_unregister(EventSourceObject* obj, EventListenerObject* listener)
 *	@brief		remove listener
 *	@param[in]	EventListenerObject* listener
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result event_source_unregister(EventSourceObject* obj, EventListenerObject* listener)
{
	Result ret = SFW_E_FAIL;
	struct list_head* listptr = NULL;

	if (listener == NULL) return SFW_E_INVALIDARG;

	list_for_each(listptr, &obj->event_listener_list_.list)
	{
		EventListenerListInfo* listener_obj_list = list_entry(listptr, struct _EventListenerListInfo, list);

		if ((listener_obj_list != NULL) && (listener_obj_list->obj == listener))
		{
			ret = SFW_S_OK;
			list_del(&listener_obj_list->list);
			kfree(listener_obj_list);

			break;
		}
 	}

	return ret;
}

/*--------------------------------------------------------------------------------
	update
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result event_source_update(EventSourceObject* obj, BusEventType event_type, void* param_1, void* param_2, void* param_3, void* param_4)
 *	@brief		change notify
 *	@param[in]	BusEventType event_type
 *	@param[in]	void* data_1
 *	@param[in]	void* data_2
 *	@param[in]	void* data_3
 *	@param[in]	void* data_4
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result event_source_update(EventSourceObject* obj, BusEventType event_type, void* param_1, void* param_2, void* param_3, void* param_4)
{
	Result ret = SFW_S_OK;
	struct list_head* listptr = NULL;

	list_for_each(listptr, &obj->event_listener_list_.list)
	{
		EventListenerListInfo* listener_obj_list = list_entry(listptr, struct _EventListenerListInfo, list);

		if ((listener_obj_list != NULL) && (listener_obj_list->obj != NULL))
		{
			ESMessage es_msg =
			{
				.event_type = event_type,
				.data_1 = param_1,
				.data_2 = param_2,
				.data_3 = param_3,
				.data_4 = param_4,
			};

			ret = listener_obj_list->obj->dispatch(listener_obj_list->obj, &es_msg);
		}
 	}

	return ret;
}

/*--------------------------------------------------------------------------------
	InitializeEventSourceObject
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result InitializeEventSourceObject(EventSourceObject* pObj)
 *	@brief		constructor
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
Result InitializeEventSourceObject(EventSourceObject* obj)
{
	if (obj == NULL) return SFW_E_INVALIDARG;

	memset(&obj->event_listener_list_, 0, sizeof(obj->event_listener_list_));
	INIT_LIST_HEAD(&obj->event_listener_list_.list);

	obj->init					= event_source_init;
	obj->exit					= event_source_exit;
	obj->register_obj 			= event_source_register;
	obj->unregister_obj 		= event_source_unregister;
	obj->update					= event_source_update;

	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	FinalizeEventSourceObject
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result FinalizeEventSourceObject(EventSourceObject* pObj)
 *	@brief		destructor
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
Result FinalizeEventSourceObject(EventSourceObject* obj)
{
	if (obj == NULL) return SFW_E_INVALIDARG;

	obj->exit(obj);

	return SFW_S_OK;
}
