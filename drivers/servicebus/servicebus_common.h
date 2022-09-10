/********************************************************************************/
/** @file		servicebus_common.h
	@brief		bus common
	@note		Copyright (C) 2013 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2013/02/25
	@version	1.0
*********************************************************************************/
#if !defined(_SERVICE_BUS_COMMON__INCLUDED_)
#define _SERVICE_BUS_COMMON__INCLUDED_

/*--------------------------------------------------------------------------------
									include
--------------------------------------------------------------------------------*/
#include "servicebus_interface.h"

/*--------------------------------------------------------------------------------
									option
--------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------
								include files
--------------------------------------------------------------------------------*/
#include <linux/slab.h>
#include <linux/syscalls.h>
#include "sfw_error.h"

/*--------------------------------------------------------------------------------
								common define
--------------------------------------------------------------------------------*/
#ifdef _TKERNEL_

extern int LOGFS_Regist(unsigned char*, void*, int);
extern char servicebus_src_version[];

#define FUNC_LOG()
typedef struct { char* a; char* b; } 		BusLogInfo;
#define STRINGIFY(x)						#x
#define TOSTRING(x)							STRINGIFY(x)
#define THIS_ORIGIN							(__FILE__ " : " TOSTRING(__LINE__))
#define SFW_ERROR_MSG(a, ...)				{ BusLogInfo log = {servicebus_src_version, THIS_ORIGIN}; LOGFS_Regist("/logfs/ServiceBusError.log", &log, strlen(servicebus_src_version) + strlen(THIS_ORIGIN)); }
#define SFW_DEBUG_OUT(a, ...)
#define SFW_REQINFO_OUT(a, ...)

#elif defined(__KERNEL__)

#define FUNC_LOG()
#define PID_FMT(fmt)						"[%5d] " fmt
#define SFW_ERROR_MSG(fmt, ...)				printk(KERN_ERR PID_FMT(fmt), task_tgid_vnr(current), ##__VA_ARGS__)
#define SFW_DEBUG_OUT(fmt, ...)				printk(KERN_DEBUG PID_FMT(fmt), task_tgid_vnr(current), ##__VA_ARGS__)
#define SFW_REQINFO_OUT(a, ...)				printk(KERN_ERR a, ##__VA_ARGS__)

#else

#define FUNC_LOG()
#define SFW_ERROR_MSG(a, ...)				printk(KERN_ERR a, ##__VA_ARGS__)
#define SFW_DEBUG_OUT(a, ...)				printk(KERN_DEBUG a, ##__VA_ARGS__)
#define SFW_REQINFO_OUT(a, ...)				printk(KERN_ERR a, ##__VA_ARGS__)

#endif // _TKERNEL_

/*--------------------------------------------------------------------------------
							module param helper for linux
--------------------------------------------------------------------------------*/
#define DEC_MODULE_PARAM(paraname, type, default_value, msg)	\
	static type paraname = default_value;						\
	module_param(paraname, type, S_IRUGO | S_IWUSR);			\
	MODULE_PARM_DESC(paraname, msg);							\
	type get_##paraname(void) { return paraname; }

#define DEC_MODULE_PARAM_STR(paraname, default_value, msg)		\
	static char* paraname = default_value;						\
	module_param(paraname, charp, S_IRUGO | S_IWUSR);			\
	MODULE_PARM_DESC(paraname, msg);							\
	char* get_##paraname(void) { return paraname; }

#endif // _SERVICE_BUS_COMMON__INCLUDED_
