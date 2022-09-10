/********************************************************************************/
/** @file		servicebus_ssystub.c
	@brief		servicebus ssystub
	@note		Copyright (C) 2013 DENSO CORPORATION. All rights reserved.
	@author		H.Kondo
	@date		2013/08/09
	@version	1.0
*********************************************************************************/

/*--------------------------------------------------------------------------------
									include
--------------------------------------------------------------------------------*/
#include <fcntl.h>
#include <stdio.h>
#include "sfw_error.h"
#include "servicebus_if_param.h"
#include "servicebus_if_id.h"

/*--------------------------------------------------------------------------------
								type rename
--------------------------------------------------------------------------------*/
typedef int DevHandle;

/*--------------------------------------------------------------------------------
								Define
--------------------------------------------------------------------------------*/
#define DEVFILE "/dev/service_bus"

/*--------------------------------------------------------------------------------
								Global
--------------------------------------------------------------------------------*/
DevHandle g_handle = -1;

/*--------------------------------------------------------------------------------
	_get_device_handle
--------------------------------------------------------------------------------*/
/**
 *	@fn			static DevHandle _get_device_handle(void)
 *	@brief		get device handle
 *	@param 		なし
 *	@return		DevHandle !-1...OK, -1...NG
 */
static DevHandle _get_device_handle(void)
{
    // non thread safe
    if (g_handle == -1)
    {
    	g_handle = open(DEVFILE, O_RDWR);
    }

	return g_handle;
}

/*--------------------------------------------------------------------------------
	svc_bus_register
--------------------------------------------------------------------------------*/
/**
 *	@fn			int svc_bus_register(const char *name, int id, NotifyPatternType pattern)
 *	@brief		register service
 *	@param[in]	const char* name			: service name
 *	@param[in]	int id						: ID(PID)
 *	@param[in]	unsigned long long pattern	: event pattern
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
long svc_bus_register(const char* name, int id, unsigned long long pattern)
{
	SvcBusRegisterPara	para;
	DevHandle			handle = _get_device_handle();

	if (handle == -1)
	{
		printf("Error Cannot open %s (%s)\n", DEVFILE, __func__ );
		return SFW_E_FAIL;
	}

	para.name = (char*)name;
	para.id = id;
	para.pattern = pattern;

	return ioctl(handle, SVC_BUS_REGISTER, &para);
}

/*--------------------------------------------------------------------------------
	svc_bus_register_ex
--------------------------------------------------------------------------------*/
/**
 *	@fn			int svc_bus_register_ex(const char *name, int id, unsigned int index)
 *	@brief		register service
 *	@param[in]	const char* name			: service name
 *	@param[in]	int id						: ID(PID)
 *	@param[in]	unsigned int index			: index
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
long svc_bus_register_ex(const char* name, int id, unsigned int index)
{
	SvcBusRegisterExPara para;
	DevHandle			 handle = _get_device_handle();

	if (handle == -1)
	{
		printf("Error Cannot open %s (%s)\n", DEVFILE, __func__ );
		return SFW_E_FAIL;
	}

	para.name = (char*)name;
	para.id = id;
	para.index = index;

	return ioctl(handle, SVC_BUS_REGISTER_EX, &para);
}

/*--------------------------------------------------------------------------------
	svc_bus_register_multi
--------------------------------------------------------------------------------*/
/**
 *	@fn			int svc_bus_register_multi(const char *name, int id, NotifyPatternType pattern)
 *	@brief		register service
 *	@param[in]	const char* name			: service name
 *	@param[in]	int id						: ID(PID)
 *	@param[in]	unsigned long long pattern	: event pattern
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
long svc_bus_register_multi(const char* name, int id, unsigned long long pattern)
{
	SvcBusRegisterMultiPara	para;
	DevHandle				handle = _get_device_handle();

	if (handle == -1)
	{
		printf("Error Cannot open %s (%s)\n", DEVFILE, __func__ );
		return SFW_E_FAIL;
	}

	para.name = (char*)name;
	para.id = id;
	para.pattern = pattern;

	return ioctl(handle, SVC_BUS_REGISTER_MULTI, &para);
}

/*--------------------------------------------------------------------------------
	svc_bus_unregister
--------------------------------------------------------------------------------*/
/**
 *	@fn			int svc_bus_unregister(const char* name)
 *	@brief		unregister service
 *	@param[in]	const char* name	: service name
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
long svc_bus_unregister(const char* name)
{
	SvcBusUnregisterPara	para;
	DevHandle				handle = _get_device_handle();

	if (handle == -1)
	{
		printf("Error Cannot open %s (%s)\n", DEVFILE, __func__);
		return SFW_E_FAIL;
	}

	para.name = (char*)name;

	return ioctl(handle, SVC_BUS_UNREGISTER, &para);
}

/*--------------------------------------------------------------------------------
	svc_bus_unregister_multi
--------------------------------------------------------------------------------*/
/**
 *	@fn			int svc_bus_unregister_multi(const char* name, int id)
 *	@brief		unregister service
 *	@param[in]	const char* name	: service name
 *	@param[in]	int id						: ID(PID)
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
long svc_bus_unregister_multi(const char* name, int id)
{
	SvcBusUnregisterMultiPara	para;
	DevHandle					handle = _get_device_handle();

	if (handle == -1)
	{
		printf("Error Cannot open %s (%s)\n", DEVFILE, __func__);
		return SFW_E_FAIL;
	}

	para.name = (char*)name;
	para.id = id;

	return ioctl(handle, SVC_BUS_UNREGISTER_MULTI, &para);
}

/*--------------------------------------------------------------------------------
	svc_bus_send
--------------------------------------------------------------------------------*/
/**
 *	@fn			int svc_bus_send(const char* name, const char* data, unsigned int data_size)
 *	@brief		send message
 *	@param[in]	const char* name		: dst service name
 *	@param[in]	const char* data		: data
 *	@param[in]	unsigned int data_size	: data size
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
long svc_bus_send(const char* name, const char* data, unsigned int data_size)
{
	SvcBusSendPara	para;
	DevHandle		handle = _get_device_handle();

	if (handle == -1)
	{
		printf("Error Cannot open %s (%s)\n", DEVFILE, __func__);
		return SFW_E_FAIL;
	}

	para.name = (char*)name;
	para.data = (char*)data;
	para.data_size = data_size;

	return ioctl(handle, SVC_BUS_SEND, &para);
}

/*--------------------------------------------------------------------------------
	svc_bus_set
--------------------------------------------------------------------------------*/
/**
 *	@fn			int svc_bus_set(const char* name, const char* data, unsigned int data_size)
 *	@brief		set message
 *	@param[in]	const char* name		: dst service name
 *	@param[in]	const char* data		: data
 *	@param[in]	unsigned int data_size	: data size
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
long svc_bus_set(const char* name, const char* data, unsigned int data_size)
{
	SvcBusSetPara	para;
	DevHandle		handle = _get_device_handle();

	if (handle == -1)
	{
		printf("Error Cannot open %s (%s)\n", DEVFILE, __func__);
		return SFW_E_FAIL;
	}

	para.name = (char*)name;
	para.data = (char*)data;
	para.data_size = data_size;

	return ioctl(handle, SVC_BUS_SET, &para);
}

/*--------------------------------------------------------------------------------
	svc_bus_wait_receive
--------------------------------------------------------------------------------*/
/**
 *	@fn			int svc_bus_wait_receive(int id, void* buf_ptr)
 *	@brief		wait bus message
 *	@param[in]	int id
 *	@param[in]	void* buf_ptr
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
long svc_bus_wait_receive(int id, void* buf_ptr)
{
	SvcBusWaitReceivePara	para;
	DevHandle				handle = _get_device_handle();

	if (handle == -1)
	{
		printf("Error Cannot open %s (%s)\n", DEVFILE, __func__);
		return SFW_E_FAIL;
	}

	para.id = id;
	para.buf_ptr = buf_ptr;

	return ioctl(handle, SVC_BUS_WAIT_RECEIVE, &para);
}

/*--------------------------------------------------------------------------------
	svc_bus_receive
--------------------------------------------------------------------------------*/
/**
 *	@fn			int svc_bus_receive(const char* name, const char* xml_msg)
 *	@brief		get message
 *	@param[in]	const char* serivce	: service name
 *	@param[out]	char* buf_ptr		: receive buff
 *	@param[in]	int buf_len			: receive buff size
 *	@param[out]	int* read_len		: read size
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
long svc_bus_receive(const char* name, char* buf_ptr, int buf_len, int *read_len)
{
	SvcBusReceivePara		para;
	DevHandle				handle = _get_device_handle();

	if (handle == -1)
	{
		printf("Error Cannot open %s (%s)\n", DEVFILE, __func__);
		return SFW_E_FAIL;
	}

	para.name = (char*)name;
	para.buf_ptr = buf_ptr;
	para.buf_len = buf_len;
	para.read_len = read_len;

	return ioctl(handle, SVC_BUS_RECEIVE, &para);
}

/*--------------------------------------------------------------------------------
	svc_bus_receive_multi
--------------------------------------------------------------------------------*/
/**
 *	@fn			int svc_bus_receive_multi(int id, char* buf_ptr, int buf_len, int *read_len)
 *	@brief		get message
 *	@param[in]	int id				: ID(PID)
 *	@param[out]	char* buf_ptr		: receive buff
 *	@param[in]	int buf_len			: receive buff size
 *	@param[out]	int* read_len		: read size
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
long svc_bus_receive_multi(int id, char* buf_ptr, int buf_len, int *read_len)
{
	SvcBusReceiveMultiPara	para;
	DevHandle				handle = _get_device_handle();

	if (handle == -1)
	{
		printf("Error Cannot open %s (%s)\n", DEVFILE, __func__);
		return SFW_E_FAIL;
	}

	para.id = id;
	para.buf_ptr = buf_ptr;
	para.buf_len = buf_len;
	para.read_len = read_len;

	return ioctl(handle, SVC_BUS_RECEIVE_MULTI, &para);
}

/*--------------------------------------------------------------------------------
	svc_bus_control
--------------------------------------------------------------------------------*/
/**
 *	@fn			int svc_bus_control(int id, void* param)
 *	@brief		bus control
 *	@param[in]	int id						: exec id
 *	@param[in]	void __user* param 			: control data
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
long svc_bus_control(int id, void* param)
{
	SvcBusControlPara	para;
	DevHandle			handle = _get_device_handle();

	if (handle == -1)
	{
		printf("Error Cannot open %s (%s)\n", DEVFILE, __func__);
		return SFW_E_FAIL;
	}

	para.id = id;
	para.param = param;

	return ioctl(handle, SVC_BUS_CONTROL, &para);
}
