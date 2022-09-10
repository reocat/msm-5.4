/********************************************************************************/
/** @file		service_bus_syscall.c
	@brief		サービスバスシステムコール受信部
	@note		Copyright (C) 2013 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2013/02/28
	@version	1.0
*********************************************************************************/
/*--------------------------------------------------------------------------------
	Include
--------------------------------------------------------------------------------*/
#include "servicebus_interface.h"												// service bus interface

/*
// svc_bus_register
*/
SYSCALL_DEFINE3(svc_bus_register, const char __user*, name, int, id, unsigned long long, pattern)
{
	return svc_bus_register(name, id, pattern);
}

/*
// svc_bus_unregister
*/
SYSCALL_DEFINE1(svc_bus_unregister, const char __user*, name)
{
	return svc_bus_unregister(name);
}

/*
// svc_bus_send
*/
SYSCALL_DEFINE3(svc_bus_send, const char __user*, name, const char __user*, data, unsigned int, data_size)
{
	return svc_bus_send(name, data, data_size);
}

/*
// svc_bus_set
*/
SYSCALL_DEFINE3(svc_bus_set, const char __user*, name, const char __user*, data, unsigned int, data_size)
{
	return svc_bus_set(name, data, data_size);
}

/*
// svc_bus_wait
*/
SYSCALL_DEFINE2(svc_bus_wait, int, id, unsigned long long __user*, pattern)
{
	return svc_bus_wait(id, pattern);
}

/*
// svc_bus_get
*/
SYSCALL_DEFINE4(svc_bus_get, const char __user*, name, char __user*, buf_ptr, int, buf_len, int __user*, read_len)
{
	return svc_bus_get(name, buf_ptr, buf_len, read_len);
}

/*
// svc_bus_control
*/
SYSCALL_DEFINE2(svc_bus_control, int, id, void __user*, param)
{
	return svc_bus_control(id, param);
}
