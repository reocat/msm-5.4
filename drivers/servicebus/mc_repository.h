/********************************************************************************/
/** @file		mc_repository.h
	@brief		mc repository
	@note		Copyright (C) 2013 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2013/09/30
	@version	1.0
*********************************************************************************/
#if !defined(_MC_REPOSITORY__INCLUDED_)
#define _MC_REPOSITORY__INCLUDED_

/*--------------------------------------------------------------------------------
								include files
--------------------------------------------------------------------------------*/
#include "name_parser.h"

/*--------------------------------------------------------------------------------
							   interface define
--------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------
								local define
--------------------------------------------------------------------------------*/
#define MCOBJ_ARRAY_CNT_MAX									(32)

/*--------------------------------------------------------------------------------
								type rename
--------------------------------------------------------------------------------*/
typedef struct _MCRepositoryObject							MCRepositoryObjectType;
typedef struct _MessageContainerObject						MCRepositoryMCObjectType;
typedef struct _ServiceInfo									ServiceInfoType;
typedef struct _HostInfo									HostInfoType;
typedef struct _UserInfo									UserInfoType;
typedef struct _MCArrary 									MCArrayType;
typedef struct _MessageQueueObject							MCRepositoryMessageQueueObjectType;
typedef struct _EventSourceObject							MCRepositoryEventSourceObjectType;
typedef struct _WaitNotifyObject							MCRepositoryWaitNotifyObjectType;

/*--------------------------------------------------------------------------------
							  typedef function
--------------------------------------------------------------------------------*/
typedef MCRepositoryMCObjectType* (*MCRepository_Create)(MCRepositoryObjectType*, MCRepositoryEventSourceObjectType*, MCRepositoryWaitNotifyObjectType*, NotifyGroupIdType, NotifyUserIdType);
typedef Result (*MCRepository_Add)(MCRepositoryObjectType*, const char*, MCRepositoryMCObjectType*);
typedef MCRepositoryMCObjectType* (*MCRepository_Find)(MCRepositoryObjectType*, const char*);
typedef MCRepositoryMCObjectType* (*MCRepository_FindFromId)(MCRepositoryObjectType*, NotifyGroupIdType);
typedef Result (*MCRepository_Remove)(MCRepositoryObjectType*, const char*);
typedef MCArrayType* (*MCRepository_Get)(MCRepositoryObjectType*, const char*);
typedef void (*MCRepository_Show)(MCRepositoryObjectType*);
typedef Result (*MCRepository_GetServiceName)(MCRepositoryObjectType*, int, char*);
typedef Result (*MCRepository_GetServiceCount)(MCRepositoryObjectType*, char*);
typedef Result (*MCRepository_GetBusInfo)(MCRepositoryObjectType*, int, char*);
typedef MCRepositoryMCObjectType* (*MCRepository_GetInstance)(MCRepositoryObjectType*, MCRepositoryEventSourceObjectType*, MCRepositoryWaitNotifyObjectType*, NotifyGroupIdType, NotifyUserIdType);
typedef Result (*MCRepository_ReleaseInstance)(MCRepositoryObjectType*, MCRepositoryMCObjectType*);
typedef Result (*MCRepository_Exit)(MCRepositoryObjectType*);

typedef MCRepositoryMCObjectType* (*MCRepository_CreateMC)(MCRepositoryObjectType*);
typedef MCRepositoryMessageQueueObjectType* (*MCRepository_CreateMQ)(MCRepositoryObjectType*);

typedef ServiceInfoType* (*MCRepository_GetServiceInfo)(MCRepositoryObjectType*, struct list_head* search_list, const char*);
typedef HostInfoType* (*MCRepository_GetHostInfo)(MCRepositoryObjectType*, struct list_head* search_list, const char*);
typedef UserInfoType* (*MCRepository_GetUserInfo)(MCRepositoryObjectType*, struct list_head* search_list, const char*);

typedef bool (*MCRepository_AddMCArray)(MCRepositoryObjectType*, MCRepositoryMCObjectType*);
typedef void (*MCRepository_ClearMCArray)(MCRepositoryObjectType*);

/*--------------------------------------------------------------------------------
							  local typedef struct
--------------------------------------------------------------------------------*/
/** user info */
typedef struct _UserInfo
{
	char 						name[USER_NAME_LEN_MAX + STR_TERMINATE_SIZE];
	MCRepositoryMCObjectType*	mc_obj;
	struct list_head			list;

} UserInfo;

/** host info */
typedef struct _HostInfo
{
	char 						name[HOST_NAME_LEN_MAX + STR_TERMINATE_SIZE];
	UserInfo 					user_info;
	struct list_head			list;

} HostInfo;

/** service info */
typedef struct _ServiceInfo
{
	char 						name[SERVICE_NAME_LEN_MAX + STR_TERMINATE_SIZE];
	HostInfo 					host_info;
	struct list_head			list;

} ServiceInfo;

/** mc arrary */
typedef struct _MCArrary
{
	MCRepositoryMCObjectType*	tbl[MCOBJ_ARRAY_CNT_MAX];
	int 						tbl_cnt;

} MCArrary;

/*--------------------------------------------------------------------------------
						Object (MCRepositoryObject)
--------------------------------------------------------------------------------*/
typedef struct _MCRepositoryObject
{
	/* +++ member value +++ */
	NameParserObject								name_parser_;
	ServiceInfo										service_rep_;
	MCArrary										mc_array_;
	int												service_count_;

	/* +++ public member function +++ */
	MCRepository_Create								create;						// create mc object
	MCRepository_Add								add;						// add mc object
	MCRepository_Find								find;						// find mc object
	MCRepository_FindFromId							find_from_id;				// find mc object from id
	MCRepository_Remove								remove;						// remove mc object
	MCRepository_Get								get;						// get mc object
	MCRepository_Show								show;						// show service
	MCRepository_GetServiceName						get_service;				// get service
	MCRepository_GetServiceCount					get_service_count;			// get service count
	MCRepository_GetBusInfo							get_bus_info;				// get bus info
	MCRepository_GetInstance						get_instance;				// get wait notify object
	MCRepository_ReleaseInstance					release_instance;			// release wait notify object
	MCRepository_Exit								exit;						// exit mc object

	/* +++ private member function +++ */
	MCRepository_CreateMC							create_mc;					///< create mc object
	MCRepository_CreateMQ							create_mq;					///< create mq object

	MCRepository_GetServiceInfo						get_service_info;			// get service info
	MCRepository_GetHostInfo						get_host_info;				// get host info
	MCRepository_GetUserInfo						get_user_info;				// get user info

	MCRepository_GetServiceInfo						search_service_info;		// search service info
	MCRepository_GetHostInfo						search_host_info;			// search host info
	MCRepository_GetUserInfo						search_user_info;			// search user info

	MCRepository_AddMCArray 						add_mc_array;				// add mc array
	MCRepository_ClearMCArray 						clear_mc_array;				// clear mc array

} MCRepositoryObject;

/*--------------------------------------------------------------------------------
						interface of MCRepositoryObject
--------------------------------------------------------------------------------*/
extern Result InitializeMCRepositoryObject(MCRepositoryObject* obj);
extern Result FinalizeMCRepositoryObject(MCRepositoryObject* obj);

#endif // _MC_REPOSITORY__INCLUDED_
