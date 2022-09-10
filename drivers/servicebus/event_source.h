/********************************************************************************/
/** @file		event_source.h
	@brief		event soruce
	@note		Copyright (C) 2013 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2013/02/25
	@version	1.0
*********************************************************************************/
#if !defined(_EVENT_SOURCE__INCLUDED_)
#define _EVENT_SOURCE__INCLUDED_

/*--------------------------------------------------------------------------------
								include files
--------------------------------------------------------------------------------*/
#include "event_listener.h"

/*--------------------------------------------------------------------------------
							   interface define
--------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------
								local define
--------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------
								type rename
--------------------------------------------------------------------------------*/
typedef struct _EventSourceObject					EventSourceObjectType;

/*--------------------------------------------------------------------------------
							  typedef function
--------------------------------------------------------------------------------*/
typedef Result (*EventSource_init)(EventSourceObjectType*);
typedef Result (*EventSource_exit)(EventSourceObjectType*);
typedef Result (*EventSource_register)(EventSourceObjectType*, EventListenerObject*);
typedef Result (*EventSource_unregister)(EventSourceObjectType*, EventListenerObject*);
typedef Result (*EventSource_update)(EventSourceObjectType*, BusEventType event_type, void* param_1, void* param_2, void* param_3, void* param_4);

/*--------------------------------------------------------------------------------
							  local typedef struct
--------------------------------------------------------------------------------*/
typedef struct _EventListenerListInfo
{
	EventListenerObject*							obj;
	struct list_head 								list;

} EventListenerListInfo;

/*--------------------------------------------------------------------------------
						Object (EventSourceObject)
--------------------------------------------------------------------------------*/
typedef struct _EventSourceObject
{
	/* +++ member value +++ */
	EventListenerListInfo							event_listener_list_;		///< event listener obj list

	/* +++ public member function +++ */
	EventSource_init								init;						// init method
	EventSource_exit								exit;						// exit method
	EventSource_register							register_obj;				// register method
	EventSource_unregister							unregister_obj;				// unregister method
	EventSource_update								update;						// update method

} EventSourceObject;

/*--------------------------------------------------------------------------------
						interface of EventSourceObject
--------------------------------------------------------------------------------*/
extern Result InitializeEventSourceObject(EventSourceObject* pObj);
extern Result FinalizeEventSourceObject(EventSourceObject* obj);

#endif // _EVENT_SOURCE__INCLUDED_
