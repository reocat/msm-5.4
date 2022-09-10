/********************************************************************************/
/** @file		servicebus_entory.c
	@brief		サービスバスのエントリー処理
	@note		Copyright (C) 2013 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2013/09/26
	@version	1.0
*********************************************************************************/
/*--------------------------------------------------------------------------------
	Include
--------------------------------------------------------------------------------*/
#include "servicebus_common.h"													// service bus common include
#include "servicebus_version.h"													// servicebus version information

#ifdef _TKERNEL_
#include "system/smng/smng.h"													// system manager include
#include "resource/component_id.h"
#elif defined(WIN32)
#include "windows.h"
#else
#include "servicebus_if_ioctl.h"												// servicebus if ioctl include
#include <linux/device.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/moduleparam.h>
#endif // !WIN32

/*--------------------------------------------------------------------------------
	Module parameters
--------------------------------------------------------------------------------*/
#ifdef _TKERNEL_
#define QUEUE_MAX_SIZE				(100)
#else
#define QUEUE_MAX_SIZE				(1024)
#endif // _TKERNEL_

#ifdef BUSBUSMNG
#ifdef _TKERNEL_
#define HOST_NAME					("ivs")
#else
#define HOST_NAME					("ive")
#endif // _TKERNEL_
#else
#define HOST_NAME					("localhost")
#endif // BUSBUSMNG

DEC_MODULE_PARAM(qmaxsize, 	int,	QUEUE_MAX_SIZE, "message queue max size [kbyte]. (defalut qmaxsize=1024)");
DEC_MODULE_PARAM(dlt, 	   	bool,	false, 			"enable output to dlt. (defalut dlt=N)");
DEC_MODULE_PARAM(dmesg,    	bool,	true,  			"enable output to dmesg. (defalut dmesg=Y)");
DEC_MODULE_PARAM(maxsvc,	int,	512,   			"max service count at one thread(user group). (defalut maxsvc=512)");
DEC_MODULE_PARAM_STR(hostname,		HOST_NAME, 		"host name [string]. (defalut hostname=ivs)");
DEC_MODULE_PARAM(permission,int,	0606,			"/dev/service_bus permission. (default permission=0606)");

/*--------------------------------------------------------------------------------
	Local Define
--------------------------------------------------------------------------------*/

#ifdef _TKERNEL_
/*--------------------------------------------------------------------------------
	SVCBUS_init
--------------------------------------------------------------------------------*/
/**
 *	@fn			int SVCBUS_init(S_BOOT_FACTOR factor)
 *	@brief		モジュールの初期化時に呼ばれる
 *	@param[in]	S_BOOT_FACTOR factor	: 起動要因
 *	@return		D_SMNG_OK...正常, 負数...異常
 */
int SVCBUS_init(S_BOOT_FACTOR factor)
{
	extern int InitializeServiceBus(void);
	InitializeServiceBus();
	return D_SMNG_OK;
}

/*--------------------------------------------------------------------------------
	SVCBUS_start
--------------------------------------------------------------------------------*/
/**
 *	@fn			int SVCBUS_start(S_SMNG_MODE mode)
 *	@brief		モジュールの起動時に呼ばれる
 *	@param[in]	S_SMNG_MODE mode	: 起動モード
 *	@return		D_SMNG_OK...正常, 負数...異常
 */
int SVCBUS_start(S_SMNG_MODE mode)
{
	SMNG_start_complete(D_COMPONENT_SVCBUS, D_SMNG_OK);
	return D_SMNG_OK;
}

/*--------------------------------------------------------------------------------
	SVCBUS_mode_notify
--------------------------------------------------------------------------------*/
/**
 *	@fn			int SVCBUS_mode_notify(S_SMNG_MODE current, S_SMNG_MODE previous)
 *	@brief
 *	@param[in]	S_SMNG_MODE current	:
 *	@param[in]	S_SMNG_MODE previous	:
 *	@return		D_SMNG_OK...正常, 負数...異常
 */
int SVCBUS_mode_notify(S_SMNG_MODE current, S_SMNG_MODE previous)
{
	SMNG_mode_response(D_COMPONENT_SVCBUS);
	return D_SMNG_OK;
}

#elif defined(WIN32)
/*--------------------------------------------------------------------------------
	DllMain
--------------------------------------------------------------------------------*/
/**
 *	@fn			BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
 *	@brief		DllMain
 *	@param[in]	HMODULE hModule
 *	@param[in]	DWORD ul_reason_for_call
 *	@param[in]	LPVOID lpReserved
 *	@return		ER E_OK...OK, !E_OK...NG
 */
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		{
			extern Result InitializeServiceBus(void);
			Result ret =InitializeServiceBus();
		}
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		{
			extern Result FinalizeServiceBus(void);
			Result ret = FinalizeServiceBus();
		}
		break;
	}
	return TRUE;
}

#else

/*--------------------------------------------------------------------------------
	Global
--------------------------------------------------------------------------------*/
struct cdev				ioctl_cdev;								///< cdev
dev_t					ioctl_dev;								///< dev
struct class*			ioctl_class;							///< class
struct file_operations	ioctl_fops;								///< fops
int 					ioctl_major_ver = 1;					///< major version
int 					ioctl_minor_ver = 0;					///< minor version
int 					ioctl_req_num_minor = 0;				///< number of minor numbers required

/*--------------------------------------------------------------------------------
	ioctl_devnode
--------------------------------------------------------------------------------*/
char* ioctl_devnode(struct device *dev, umode_t *mode)
{
	if (!mode) return NULL;
	*mode = permission;
	return NULL;
}
/*--------------------------------------------------------------------------------
	EntryServiceBus
--------------------------------------------------------------------------------*/
/**
 *	@fn		int EntryServiceBus(void)
 *	@brief	IOCTL使用のための準備処理
 *	@param	なし
 *	@return Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static int EntryServiceBus(void)
{
	dev_t dev;
	int ret = 0;

	/*
	// init dev_t dev
	*/
	dev = MKDEV(ioctl_major_ver, 0);

	ret = alloc_chrdev_region(&dev, 0, ioctl_req_num_minor, "service_bus");
	if (ret != 0)
	{
		SFW_DEBUG_OUT("%s alloc_chrdev_region error\n", __func__);

		return SFW_E_FAIL;
	}

	/*
	// init ioctl_fops & ioctl_cdev
	*/
	memset(&ioctl_fops, 0, sizeof(ioctl_fops));

	ioctl_fops.owner = THIS_MODULE;
	ioctl_fops.open = svc_bus_ioctl_open;
	ioctl_fops.release = svc_bus_ioctl_close;
	ioctl_fops.unlocked_ioctl = svc_bus_ioctl;

	cdev_init(&ioctl_cdev, &ioctl_fops);

	ioctl_cdev.owner = THIS_MODULE;
	ioctl_cdev.ops = &ioctl_fops;

	ioctl_major_ver = MAJOR(dev);

	/*
	// add ioctl_cdev
	*/
	ret = cdev_add(&ioctl_cdev, MKDEV(ioctl_major_ver, ioctl_minor_ver), 1);
	if (ret != 0)
	{
		SFW_ERROR_MSG("%s cdev_add error\n", __func__);

		unregister_chrdev_region(dev, ioctl_req_num_minor);

		return SFW_E_FAIL;
	}

	/*
	// registaer class
	*/
	ioctl_class = class_create(THIS_MODULE, "service_bus");
	if (IS_ERR(ioctl_class))
	{
		SFW_ERROR_MSG("%s class_create error\n", __func__);

		unregister_chrdev_region(dev, ioctl_req_num_minor);
		cdev_del(&ioctl_cdev);

		return SFW_E_FAIL;
	}

	ioctl_dev = MKDEV(ioctl_major_ver, ioctl_minor_ver);
	ioctl_class->devnode = ioctl_devnode;

	// new version
	device_create(ioctl_class, NULL, ioctl_dev, "service_bus", "service_bus");

	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	ExitServiceBus
--------------------------------------------------------------------------------*/
/**
 *	@fn		Result ExitServiceBus(void)
 *	@brief	IOCTLの終了処理
 *	@param	なし
 *	@return Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result ExitServiceBus(void)
{
	dev_t dev = MKDEV(ioctl_major_ver, 0);

	/*
	// unregister class
	*/
	device_destroy(ioctl_class, ioctl_dev);
	class_destroy(ioctl_class);

	cdev_del(&ioctl_cdev);

	unregister_chrdev_region(dev, ioctl_req_num_minor);

	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	_init_service_bus
--------------------------------------------------------------------------------*/
/**
 *	@fn		int _init_service_bus(void)
 *	@brief	サービスバスの初期化
 *	@param	なし
 *	@return int 0...OK, -ENOSYS...NG
 */
static int _init_service_bus(void)
{
	extern Result InitializeServiceBus(void);
	Result ret = SFW_E_FAIL;

	SFW_REQINFO_OUT("%s version=\"%s\"\n", __func__, servicebus_src_version);

	ret = EntryServiceBus();

	if (SFW_SUCCEEDED(ret)) ret = InitializeServiceBus();

	return SFW_SUCCEEDED(ret) ? 0 : -ENOSYS;
}

/*--------------------------------------------------------------------------------
	_exit_service_bus
--------------------------------------------------------------------------------*/
/**
 *	@fn		void _exit_service_bus(void)
 *	@brief	サービスバスの終了
 *	@param	なし
 *	@return なし
 */
static void _exit_service_bus(void)
{
	extern Result FinalizeServiceBus(void);
	Result ret = SFW_E_FAIL;

	SFW_DEBUG_OUT("%s\n", __func__);

	ret = FinalizeServiceBus();

	if (SFW_SUCCEEDED(ret)) ret = ExitServiceBus();
}

module_init(_init_service_bus);
module_exit(_exit_service_bus);
MODULE_LICENSE("Dual BSD/GPL");

#endif // !WIN32
