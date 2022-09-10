/********************************************************************************/
/** @file		event_listener.c
	@brief		base event listener
	@note		Copyright (C) 2013 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2013/02/25
	@version	1.0
*********************************************************************************/
/*--------------------------------------------------------------------------------
	Include
--------------------------------------------------------------------------------*/
#include "servicebus_common.h"													// service bus common include
#include "event_listener.h"														// event listener include

/*--------------------------------------------------------------------------------
	Local Value
--------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------
	dispatch
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result event_listener_dispatch(EventListenerObject* obj, ESMessageType* es_msg)
 *	@brief		change handler
 *	@param[in]	ESMessageType* es_msg
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result event_listener_dispatch(EventListenerObject* obj, ESMessageType* es_msg)
{
	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	exit
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	void event_listener_exit(EventListenerObject* obj)
 *	@brief		virtual destructor
 *	@param		なし
 *	@return		なし
 */
static void event_listener_exit(EventListenerObject* obj)
{
}

/*--------------------------------------------------------------------------------
	InitializeEventListenerObject
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result InitializeEventListenerObject(EventListenerObject* obj)
 *	@brief		constructor
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
Result InitializeEventListenerObject(EventListenerObject* obj)
{
	if (obj == NULL) return SFW_E_INVALIDARG;

	obj->dispatch = event_listener_dispatch;
	obj->exit 	  = event_listener_exit;

	return SFW_S_OK;
}
