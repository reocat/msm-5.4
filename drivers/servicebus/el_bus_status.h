/********************************************************************************/
/** @file		el_bus_status.h
	@brief		event listener bus status
	@note		Copyright (C) 2014 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2014/05/20
	@version	1.0
*********************************************************************************/
#if !defined(_EL_BUS_STATUS__INCLUDED_)
#define _EL_BUS_STATUS__INCLUDED_

/*--------------------------------------------------------------------------------
								include files
--------------------------------------------------------------------------------*/
#include "event_listener.h"
#include "name_parser.h"

/*--------------------------------------------------------------------------------
								type rename
--------------------------------------------------------------------------------*/
typedef struct _ELBusStatusObject				ELBusStatusObjectType;
typedef struct _WaitNotifyObject				ELBusStatus_WaitNotifyObjectType;

/*--------------------------------------------------------------------------------
							  typedef function
--------------------------------------------------------------------------------*/
typedef Result (*ELBusStatus_add)(ELBusStatusObjectType*, ELBusStatus_WaitNotifyObjectType*, NotifyUserIdType);
typedef Result (*ELBusStatus_delete)(ELBusStatusObjectType*, ELBusStatus_WaitNotifyObjectType*, NotifyUserIdType);
typedef struct _ServiceRegInfo* (*ELBusStatus_search)(ELBusStatusObjectType*, const char*);
typedef void (*ELBusStatus_clear)(ELBusStatusObjectType*);
typedef Result (*ELBusStatus_get_status_count)(ELBusStatusObjectType*, char*);
typedef Result (*ELBusStatus_get_status)(ELBusStatusObjectType*, int, char*);

/*--------------------------------------------------------------------------------
							  local typedef struct
--------------------------------------------------------------------------------*/
typedef struct _NotifyListInfo
{
	ELBusStatus_WaitNotifyObjectType*			wait_notify;
	NotifyUserIdType 							id_pattern;
	struct list_head 							list;

} NotifyListInfo;

typedef struct _ServiceRegInfo
{
	char 										service_name[FULL_NAME_LEN_MAX];
	int 										reg_count;
	int 										unreg_count;
	struct list_head							list;

} ServiceRegInfo;

/*--------------------------------------------------------------------------------
						Object (ELBusStatusObject)
--------------------------------------------------------------------------------*/
typedef struct _ELBusStatusObject
{
	/* +++ base object +++ */
	EventListenerObjectDefine

	NotifyListInfo								notify_obj_list_;
	ServiceRegInfo								service_reg_list_;
	NameParserObject							name_parser_;

	/* +++ public member function +++ */
	ELBusStatus_add								add;
	ELBusStatus_delete							del;
	ELBusStatus_search							search;
	ELBusStatus_clear							clear;
	ELBusStatus_get_status_count				get_status_count;
	ELBusStatus_get_status						get_status;

} ELBusStatusObject;

/*--------------------------------------------------------------------------------
						interface of ELBusStatusObject
--------------------------------------------------------------------------------*/
extern Result InitializeELBusStatusObject(ELBusStatusObject* obj);

#endif // _EL_BUS_STATUS__INCLUDED_
