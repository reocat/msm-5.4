/********************************************************************************/
/** @file		el_service_status.h
	@brief		event listener service status
	@note		Copyright (C) 2015 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2015/12/24
	@version	1.0
*********************************************************************************/
#if !defined(_EL_SERVICE_STATUS__INCLUDED_)
#define _EL_SERVICE_STATUS__INCLUDED_

/*--------------------------------------------------------------------------------
								include files
--------------------------------------------------------------------------------*/
#include "event_listener.h"
#include "name_parser.h"

/*--------------------------------------------------------------------------------
								type rename
--------------------------------------------------------------------------------*/
typedef struct _ELServiceStatusObject			ELServiceStatusObjectType;
typedef struct _ServiceInfoList					ELServiceStatusServiceInfoListType;
typedef struct _RegInfo							ELServiceStatusRegInfoType;
typedef struct _WaitNotifyObject				ELServiceStatus_WaitNotifyObjectType;

/*--------------------------------------------------------------------------------
							  typedef function
--------------------------------------------------------------------------------*/
typedef Result (*ELServiceStatus_add)(ELServiceStatusObjectType*, ELServiceStatus_WaitNotifyObjectType*, NotifyUserIdType);
typedef Result (*ELServiceStatus_delete)(ELServiceStatusObjectType*, ELServiceStatus_WaitNotifyObjectType*, NotifyUserIdType);
typedef void (*ELServiceStatus_clear)(ELServiceStatusObjectType*);
typedef void (*ELServiceStatus_clear_service_reg_info)(ELServiceStatusObjectType*, ELServiceStatusServiceInfoListType*);
typedef struct _AllServiceInfo* (*ELServiceStatus_search_device)(ELServiceStatusObjectType*, const char*);
typedef ELServiceStatusServiceInfoListType* (*ELServiceStatus_search_service)(ELServiceStatusObjectType*, ELServiceStatusServiceInfoListType*, const char*);
typedef Result (*ELServiceStatus_get_buff_size)(ELServiceStatusObjectType*, const char*, int, char __user*);
typedef Result (*ELServiceStatus_get_status)(ELServiceStatusObjectType*, const char*, int, int, char __user*);
typedef char __user* (*ELServiceStatus_copy_service_reg_info)(ELServiceStatusObjectType*, ELServiceStatusServiceInfoListType*, char __user*, int*);
typedef Result (*ELServiceStatus_create_reg_info)(ELServiceStatusObjectType*, ELServiceStatusRegInfoType*, const char*);
typedef int (*ELServiceStatus_current_buff_size)(ELServiceStatusObjectType*, const char*, int);

/*--------------------------------------------------------------------------------
							  local typedef struct
--------------------------------------------------------------------------------*/
typedef struct _WaitNotifyListInfo
{
	ELServiceStatus_WaitNotifyObjectType*		wait_notify;
	NotifyUserIdType 							id_pattern;
	struct list_head 							list;

} WaitNotifyListInfo;

typedef struct _ServiceInfoList
{
	char 										service_name[FULL_NAME_LEN_MAX];
	int											reg_count;
	int											unreg_count;
	struct list_head							list;

} ServiceInfoList;

typedef struct _RegInfo
{
	ServiceInfoList								service_info_list;
	size_t										total_len;
	int 										count;

} RegInfo;

typedef struct _AllServiceInfo
{
	char 										device_name[HOST_NAME_LEN_MAX];
	RegInfo										services;
	RegInfo										users;
	struct list_head							list;

} AllServiceInfo;

/*--------------------------------------------------------------------------------
						Object (ELServiceStatusObject)
--------------------------------------------------------------------------------*/
typedef struct _ELServiceStatusObject
{
	/* +++ base object +++ */
	EventListenerObjectDefine

	WaitNotifyListInfo							notify_obj_list_;
	AllServiceInfo 								all_service_list_;
	NameParserObject							name_parser_;

	/* +++ public member function +++ */
	ELServiceStatus_add							add;
	ELServiceStatus_delete						del;
	ELServiceStatus_search_device				search_device;
	ELServiceStatus_search_service				search_service;
	ELServiceStatus_clear						clear;
	ELServiceStatus_clear_service_reg_info 		clear_service_reg_info;

	ELServiceStatus_get_buff_size				get_buff_size;
	ELServiceStatus_get_status					get_status;

	ELServiceStatus_current_buff_size			current_buff_size;
	ELServiceStatus_copy_service_reg_info		copy_service_reg_info;
	ELServiceStatus_create_reg_info				create_reg_info;

} ELServiceStatusObject;

/*--------------------------------------------------------------------------------
						interface of ELServiceStatusObject
--------------------------------------------------------------------------------*/
extern Result InitializeELServiceStatusObject(ELServiceStatusObject* obj);

#endif // _EL_SERVICE_STATUS__INCLUDED_
