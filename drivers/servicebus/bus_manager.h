/********************************************************************************/
/** @file		bus_manager.h
	@brief		service bus manager
	@note		Copyright (C) 2013 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2013/10/01
	@version	1.0
*********************************************************************************/
#if !defined(_BUS_MANAGER__INCLUDED_)
#define _BUS_MANAGER__INCLUDED_

/*--------------------------------------------------------------------------------
								include files
--------------------------------------------------------------------------------*/
#include "mc_repository.h"														// mc repository include
#include "wn_repository.h"														// wn repository include
#include "event_source.h"														// event source include
#include <linux/mutex.h>														// mutex
/*------------------------------------------------------------------------------*/
#include "el_logger.h"															// event listener logger
#include "el_dlt_logger.h"														// event listener dlt logger
#include "el_wait_notify.h"														// event listener wait notify
#include "el_bus_status.h"														// event listener bus status
#include "el_reg_unreg.h"														// event listener reg & unreg
#include "el_service_status.h"													// event listener service status

/*--------------------------------------------------------------------------------
							   interface define
--------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------
								local define
--------------------------------------------------------------------------------*/
typedef enum _NotifyIdKind { NOTIFY_ID_KIND_PATTERN = 0, NOTIFY_ID_KIND_INDEX } NotifyIdKind;

/*--------------------------------------------------------------------------------
								type rename
--------------------------------------------------------------------------------*/
typedef struct _BusManagerObject					BusManagerObjectType;
typedef struct _WaitNotifyObject					BusManagerObject_WaitNotifyObjectType;
typedef struct _MessageContainerObject				BusManagerObject_MessageContainerObjectType;

/*--------------------------------------------------------------------------------
							  typedef function
--------------------------------------------------------------------------------*/
typedef Result (*BusManager_Register)(BusManagerObjectType*, const char*, NotifyGroupIdType, NotifyUserIdType, NotifyIdKind);
typedef Result (*BusManager_Unregister)(BusManagerObjectType*, const char*);
typedef void (*BusManager_UnregisterImpl)(BusManagerObjectType*, const char*, BusManagerObject_MessageContainerObjectType*);
typedef Result (*BusManager_MultiRegister)(BusManagerObjectType*, const char*, NotifyGroupIdType, NotifyUserIdType);
typedef Result (*BusManager_MultiUnregister)(BusManagerObjectType*, const char*, NotifyGroupIdType);
typedef void (*BusManager_MultiUnregisterImpl)(BusManagerObjectType*, const char*, BusManagerObject_MessageContainerObjectType*);
typedef Result (*BusManager_Send)(BusManagerObjectType*, const char*, const char __user *, int);
typedef Result (*BusManager_Receive)(BusManagerObjectType*, const char*, char __user *, int, int __user *);
typedef Result (*BusManager_MultiReceive)(BusManagerObjectType*, NotifyGroupIdType, char __user *, int, int __user *);
typedef Result (*BusManager_Wait)(BusManagerObjectType*, NotifyGroupIdType, void __user *);
typedef Result (*BusManager_BusControl)(BusManagerObjectType*, void __user *);
typedef Result (*BusManager_UpdateWaitNotify)(BusManagerObjectType*, NotifyGroupIdType);

/*--------------------------------------------------------------------------------
						Object (BusManagerObject)
--------------------------------------------------------------------------------*/
typedef struct _BusManagerObject
{
	/* +++ member value +++ */
	#ifdef _TKERNEL_
	FastLock								mutx_;
	#else
	struct mutex 							mutx_;
	#endif // _TKERNEL_

	MCRepositoryObject						mc_repo_;							///< mc repository
	WNRepositoryObject						wn_repo_;							///< wn repository

	EventSourceObject						event_source_;						///< event source

	ELLoggerObject							el_logger_;							///< el logger
	ELDltLoggerObject						el_dlt_logger_;						///< el dlt_logger
	ELWaitNotifyObject						el_wait_notify_;					///< el wait notify
	ELBusStatusObject						el_bus_status_;						///< el bus status
	ELRegUnregObject						el_reg_unreg_;						///< el reg & unreg
	ELServiceStatusObject					el_service_status_;					///< el service status

	/* +++ private member function +++ */
	BusManager_Register						register_impl;						///< register impl method
	BusManager_UnregisterImpl				unregister_impl;					///< unregister impl method

	BusManager_MultiRegister				multi_register_impl;				///< multi register impl method
	BusManager_MultiUnregisterImpl			multi_unregister_impl;				///< multi unregister impl method

	/* +++ public member function +++ */
	BusManager_Register						register_service;					///< register method
	BusManager_Unregister					unregister_service;					///< unregister method

	BusManager_MultiRegister				multi_register_service;				///< multi register method
	BusManager_MultiUnregister				multi_unregister_service;			///< multi unregister method

	BusManager_Send							send_message;						///< send_async_message method
	BusManager_Send							set_message;						///< set_async_message method
	BusManager_Receive						receive_message;					///< receive_async_message method
	BusManager_MultiReceive					multi_receive_message;				///< multi receive_async_message method
	BusManager_Wait							wait_message;						///< wait receive msg

	BusManager_BusControl					subscribe_bus_status;				///< subscribe change service
	BusManager_BusControl					unsubscribe_bus_status;				///< unsubscribe change service
	BusManager_BusControl					get_service;						///< get service name
	BusManager_BusControl					get_service_count;					///< get service count
	BusManager_BusControl					get_service_limit;					///< get service name (limit version)
	BusManager_BusControl					get_bus_info;						///< get service information
	BusManager_BusControl					get_bus_info_limit;					///< get service information (limit version)

	BusManager_BusControl					get_store_service_count;			///< get service count from store
	BusManager_BusControl					get_store_service_status;			///< get service status from store

	BusManager_BusControl					start_log;							///< start log
	BusManager_BusControl					stop_log;							///< stop log
	BusManager_BusControl					get_log;							///< get log

	BusManager_BusControl					subscribe_service_status;			///< subscribe change service
	BusManager_BusControl					unsubscribe_service_status;			///< unsubscribe change service
	BusManager_BusControl					get_service_status_buff_size;		///< get servie status buff size
	BusManager_BusControl					get_service_status;					///< get servie status

	BusManager_BusControl					get_device_name;					///< get host(device) name
	BusManager_BusControl					get_max_queue_size;					///< get max queue size

	BusManager_UpdateWaitNotify				update_waitnotify;					///< update userid

} BusManagerObject;

/*--------------------------------------------------------------------------------
						interface of BusManagerObject
--------------------------------------------------------------------------------*/
extern Result InitializeBusManagerObject(BusManagerObject* obj);
extern Result FinalizeBusManagerObject(BusManagerObject* obj);

#endif // _BUS_MANAGER__INCLUDED_
