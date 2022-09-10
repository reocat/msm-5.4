/********************************************************************************/
/** @file		servicebus_interface.h
	@brief		interface
	@note		Copyright (C) 2013 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2013/02/25
	@version	1.0
*********************************************************************************/
#if !defined(_SERVICEBUS_INTERFACE__INCLUDED_)
#define _SERVICEBUS_INTERFACE__INCLUDED_

/*--------------------------------------------------------------------------------
	Include
--------------------------------------------------------------------------------*/
#ifdef __KERNEL__
#include <linux/types.h>
#else
#define __user
#endif // __KERNEL__

#ifdef linux
#define IMPORT
#endif // linux

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/*--------------------------------------------------------------------------------
								interface
--------------------------------------------------------------------------------*/
#define SERVICE_NAME_LEN_MAX				(64)
#define HOST_NAME_LEN_MAX					(32)
#define USER_NAME_LEN_MAX					(64)
#define FULL_NAME_LEN_MAX					(SERVICE_NAME_LEN_MAX + HOST_NAME_LEN_MAX + USER_NAME_LEN_MAX)
#define STR_TERMINATE_SIZE					(4)

#define SERVICE_STATUS_KIND_SERVICE			(0x1)
#define SERVICE_STATUS_KIND_USER			(0x2)
#define SERVICE_STATUS_KIND_SELF_DEVICE		(0x4)

/** service handle */
typedef int BusResult;

/** service handle */
typedef int	SvcHandle;

/** notify group id */
typedef int NotifyGroupIdType;

/** notify user id */
#ifdef _TKERNEL_
typedef int NotifyUserIdType;
#else
typedef unsigned long long NotifyUserIdType;
#endif // _TKERNEL_

/** notify index */
typedef unsigned int NotifyIndexType;

/** bus control subscribe */
typedef struct _BusControlSubscribeInfo
{
	NotifyGroupIdType id;
	NotifyUserIdType pattern;

} BusControlSubscribeInfo;

/** bus control log read */
typedef struct _BusControlLogReadInfo
{
	char __user* buf_ptr;
	int buf_len;
	int __user* read_len;

} BusControlLogReadInfo;

/** bus control bus status */
typedef struct _BusControlBusStatus
{
	int max_service_cnt;
	char __user* buf_ptr;

} BusControlBusStatus;

/** bus control service status */
typedef struct _BusControlSerivceStatus
{
	int status_kind;
	int max_buff_len;
	char __user* buf_ptr;

} BusControlSerivceStatus;

/** bus control id */
typedef enum _BusControlId
{
	BUS_CONTROL_SUBSCRIBE = 0,					// XXX // subscribe for BUS_CONTROL_GET_BUS_STATUS/BUS_CONTROL_GET_BUS_INFO/BUS_CONTROL_GET_BUS_STATUS_LIMIT/BUS_CONTROL_STORE_SERVICE_STATUS
	BUS_CONTROL_UNSUBSCRIBE,					// XXX // unsubscribe
	BUS_CONTROL_GET_BUS_STATUS, 				// XXX // get service name list
	BUS_CONTROL_LOG_START,						// --- // start log
	BUS_CONTROL_LOG_STOP,						// --- // stop log
	BUS_CONTROL_LOG_CLEAR,						// --- // clear log
	BUS_CONTROL_LOG_GET,						// --- // get log
	BUS_CONTROL_GET_BUS_INFO,					// XXX // bus inside information
	BUS_CONTROL_GET_SERVICE_CNT, 				// --- // get service count
	BUS_CONTROL_GET_BUS_STATUS_LIMIT,			// XXX // get service name list(limit version)
	BUS_CONTROL_STORE_SERVICE_CNT,				// XXX // get service status count from store
	BUS_CONTROL_STORE_SERVICE_STATUS,			// XXX // get service status form store
	BUS_CONTROL_GET_BUS_INFO_LIMIT,				// --- // bus inside information(limit version)
	BUS_CONTROL_GET_DEVICE_NAME,				// --- // get device name
	BUS_CONTROL_GET_MAX_QUEUE_SIZE,				// --- // get max queue size
	BUS_CONTROL_SUBSCRIBE_SERVICE_STATUS,		// OOO // subscribe for BUS_CONTROL_GET_SELF_DEVICE_SERVICE/BUS_CONTROL_GET_ALL_SERVICE
	BUS_CONTROL_UNSUBSCRIBE_SERVICE_STATUS,		// OOO // unsubscribe
	BUS_CONTROL_GET_SERVICE_STATUS_BUFF_SIZE,	// OOO // services(only service) count
	BUS_CONTROL_GET_SERVICE_STATUS				// OOO // services(only service) status

} BusControlId;

/*--------------------------------------------------------------------------------
	svc_bus_register
--------------------------------------------------------------------------------*/
/**
 *	@fn			BusResult svc_bus_register(const char *name, NotifyGroupIdType id, NotifyPatternType pattern)
 *	@brief		register service
 *	@param[in]	const char* name			: service name
 *	@param[in]	NotifyGroupIdType id		: ID
 *	@param[in]	NotifyUserIdType pattern	: event pattern
 *	@return		BusResult SFW_S_OK...OK, !SFW_S_OK...NG
 */
IMPORT BusResult svc_bus_register(const char __user* name, NotifyGroupIdType id, NotifyUserIdType pattern);

/*--------------------------------------------------------------------------------
	svc_bus_register_ex
--------------------------------------------------------------------------------*/
/**
 *	@fn			BusResult svc_bus_register_ex(const char *name, NotifyGroupIdType id, NotifyIndexType index)
 *	@brief		register service
 *	@param[in]	const char* name			: service name
 *	@param[in]	NotifyGroupIdType id		: ID
 *	@param[in]	NotifyIndexType index		: index
 *	@return		BusResult SFW_S_OK...OK, !SFW_S_OK...NG
 */
IMPORT BusResult svc_bus_register_ex(const char __user* name, NotifyGroupIdType id, NotifyIndexType index);

/*--------------------------------------------------------------------------------
	svc_bus_register_multi
--------------------------------------------------------------------------------*/
/**
 *	@fn			BusResult svc_bus_register_multi(const char *name, NotifyGroupIdType id, NotifyPatternType pattern)
 *	@brief		register service
 *	@param[in]	const char* name			: service name
 *	@param[in]	NotifyGroupIdType id		: ID
 *	@param[in]	NotifyUserIdType pattern	: event pattern
 *	@return		BusResult SFW_S_OK...OK, !SFW_S_OK...NG
 */
IMPORT BusResult svc_bus_register_multi(const char __user* name, NotifyGroupIdType id, NotifyUserIdType pattern);

/*--------------------------------------------------------------------------------
	svc_bus_unregister
--------------------------------------------------------------------------------*/
/**
 *	@fn			BusResult svc_bus_unregister(const char* name)
 *	@brief		unregister service
 *	@param[in]	const char* name	: service name
 *	@return		BusResult SFW_S_OK...OK, !SFW_S_OK...NG
 */
IMPORT BusResult svc_bus_unregister(const char __user* name);

/*--------------------------------------------------------------------------------
	svc_bus_unregister_multi
--------------------------------------------------------------------------------*/
/**
 *	@fn			BusResult svc_bus_unregister_multi(const char* name, NotifyGroupIdType id)
 *	@brief		unregister service
 *	@param[in]	const char* name		: service name
 *	@param[in]	NotifyGroupIdType id	: ID
 *	@return		BusResult SFW_S_OK...OK, !SFW_S_OK...NG
 */
IMPORT BusResult svc_bus_unregister_multi(const char __user* name, NotifyGroupIdType id);

/*--------------------------------------------------------------------------------
	svc_bus_send
--------------------------------------------------------------------------------*/
/**
 *	@fn			BusResult svc_bus_send(const char* name, const char* data, unsigned int data_size)
 *	@brief		send message
 *	@param[in]	const char* name		: dst service name
 *	@param[in]	const char* data		: data
 *	@param[in]	unsigned int data_size	: data size
 *	@return		BusResult SFW_S_OK...OK, !SFW_S_OK...NG
 */
IMPORT BusResult svc_bus_send(const char __user* name, const char __user* data, unsigned int data_size);

/*--------------------------------------------------------------------------------
	svc_bus_set
--------------------------------------------------------------------------------*/
/**
 *	@fn			BusResult svc_bus_set(const char* name, const char* data, unsigned int data_size)
 *	@brief		set message
 *	@param[in]	const char* name		: dst service name
 *	@param[in]	const char* data		: data
 *	@param[in]	unsigned int data_size	: data size
 *	@return		BusResult SFW_S_OK...OK, !SFW_S_OK...NG
 */
IMPORT BusResult svc_bus_set(const char __user* name, const char __user* data, unsigned int data_size);

/*--------------------------------------------------------------------------------
	svc_bus_wait_receive
--------------------------------------------------------------------------------*/
/**
 *	@fn			int svc_bus_wait_receive(NotifyGroupIdType id, NotifyUserIdType __user* pattern)
 *	@brief		wait bus message
 *	@param[in]	NotifyGroupIdType id
 *	@param[out]	NotifyUserIdType __user* pattern
 *	@return		BusResult SFW_S_OK...OK, !SFW_S_OK...NG
 */
IMPORT BusResult svc_bus_wait_receive(NotifyGroupIdType id, NotifyUserIdType __user* pattern);

/*--------------------------------------------------------------------------------
	svc_bus_receive
--------------------------------------------------------------------------------*/
/**
 *	@fn			int svc_bus_receive(const char __user* name, char __user* buf_ptr, int buf_len, int *read_len)
 *	@brief		get message
 *	@param[in]	const char* serivce	: service name
 *	@param[out]	char* buf_ptr		: receive buff
 *	@param[in]	int buf_len			: receive buff size
 *	@param[out]	int* read_len		: read size
 *	@return		BusResult SFW_S_OK...OK, !SFW_S_OK...NG
 */
IMPORT BusResult svc_bus_receive(const char __user* name, char __user* buf_ptr, int buf_len, int *read_len);

/*--------------------------------------------------------------------------------
	svc_bus_receive_multi
--------------------------------------------------------------------------------*/
/**
 *	@fn			int svc_bus_receive_multi(NotifyGroupIdType id, char __user* buf_ptr, int buf_len, int *read_len)
 *	@brief		get message
 *	@param[in]	NotifyGroupIdType id: group id
 *	@param[out]	char* buf_ptr		: receive buff
 *	@param[in]	int buf_len			: receive buff size
 *	@param[out]	int* read_len		: read size
 *	@return		BusResult SFW_S_OK...OK, !SFW_S_OK...NG
 */
IMPORT BusResult svc_bus_receive_multi(NotifyGroupIdType id, char __user* buf_ptr, int buf_len, int *read_len);

/*--------------------------------------------------------------------------------
	svc_bus_control
--------------------------------------------------------------------------------*/
/**
 *	@fn			BusResult svc_bus_control(BusControlId id, void __user* param)
 *	@brief		add bus_status handler
 *	@param[in]	BusControlId id				: exec id
 *	@param[in]	void __user* param 			: control data
 *	@return		BusResult SFW_S_OK...OK, !SFW_S_OK...NG
 */
IMPORT BusResult svc_bus_control(BusControlId id, void __user* param);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // _SERVICE_BUS_INTERFACE_INCLUDED_
