/********************************************************************************/
/** @file		servicebus_interface.c
	@brief		サービスバスインタフェース部実体部
	@note		Copyright (C) 2013 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2013/03/07
	@version	1.0
*********************************************************************************/
/*--------------------------------------------------------------------------------
	Include
--------------------------------------------------------------------------------*/
#include "servicebus_common.h"													// service bus common include
#include "bus_manager.h"														// service bus manager include

/*--------------------------------------------------------------------------------
	Interface
--------------------------------------------------------------------------------*/
extern int get_maxsvc(void);

/*--------------------------------------------------------------------------------
	Global
--------------------------------------------------------------------------------*/
static BusManagerObject bus_mng;

/*--------------------------------------------------------------------------------
	InitializeServiceBus
--------------------------------------------------------------------------------*/
/**
 *	@fn		BusResult InitializeServiceBus(void)
 *	@brief	サービスバスの初期
 *	@param	なし
 *	@return BusResult SFW_S_OK...OK, !SFW_S_OK...NG
 */
BusResult InitializeServiceBus(void)
{
	return InitializeBusManagerObject(&bus_mng);
}

/*--------------------------------------------------------------------------------
	FinalizeServiceBus
--------------------------------------------------------------------------------*/
/**
 *	@fn		BusResult FinalizeServiceBus(void)
 *	@brief	サービスバスの終了
 *	@param	なし
 *	@return BusResult SFW_S_OK...OK, !SFW_S_OK...NG
 */
BusResult FinalizeServiceBus(void)
{
	return FinalizeBusManagerObject(&bus_mng);
}

/*--------------------------------------------------------------------------------
	GetServiceMng
--------------------------------------------------------------------------------*/
/**
 *	@fn		svcbs::ServiceMngObj* GetServiceMng(void)
 *	@brief	ServiceBusManager オブジェクトの取得
 *	@param	なし
 *	@return ServiceMngObj*
 */
static inline BusManagerObject* GetServiceMng(void)
{
	return &bus_mng;
}

/*--------------------------------------------------------------------------------
	CopyServiceName
--------------------------------------------------------------------------------*/
/**
 *	@fn			bool CopyServiceName(char* dst, char __user * src)
 *	@brief		サービス名のコピー処理
 *	@param[out]	カーネル空間領域
  *	@param[in]	ユーザ空間領域
 *	@return 	bool true...OK, false...NG
 */
static bool CopyServiceName(char* dst, const char __user * src)
{
	int len = strnlen_user(src, FULL_NAME_LEN_MAX);

	if (len > FULL_NAME_LEN_MAX) len = FULL_NAME_LEN_MAX;

	if (copy_from_user(dst, src, len))
	{
		return false;
	}

	dst[len] = 0;

	return true;
}

/*--------------------------------------------------------------------------------
	svc_bus_register
--------------------------------------------------------------------------------*/
BusResult svc_bus_register(const char __user* serivce, NotifyGroupIdType id, NotifyUserIdType pattern)
{
	char service_name[FULL_NAME_LEN_MAX + STR_TERMINATE_SIZE];
	BusManagerObject* bus_mng = GetServiceMng();

	if (serivce == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : param check_1.\n", __FUNCTION__);
		return SFW_E_INVALIDARG;
	}

	if (CopyServiceName(service_name, serivce) == false)
	{
		SFW_ERROR_MSG("[Error] %s : param check_2.\n", __FUNCTION__);
		return SFW_E_INVALIDARG;
	}

	return bus_mng->register_service(bus_mng, service_name, id, pattern, NOTIFY_ID_KIND_PATTERN);
}

/*--------------------------------------------------------------------------------
	svc_bus_register_ex
--------------------------------------------------------------------------------*/
BusResult svc_bus_register_ex(const char __user* serivce, NotifyGroupIdType id, NotifyIndexType index)
{
	char service_name[FULL_NAME_LEN_MAX + STR_TERMINATE_SIZE];
	BusManagerObject* bus_mng = GetServiceMng();

	if (serivce == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : param check_1.\n", __FUNCTION__);
		return SFW_E_INVALIDARG;
	}

	if (CopyServiceName(service_name, serivce) == false)
	{
		SFW_ERROR_MSG("[Error] %s : param check_2.\n", __FUNCTION__);
		return SFW_E_INVALIDARG;
	}

	if (index > (get_maxsvc() - 1))
	{
		SFW_ERROR_MSG("[Error] %s : param check_3. index=%d max=%d\n", __FUNCTION__, index, get_maxsvc());
		return SFW_E_INVALIDARG;
	}

	return bus_mng->register_service(bus_mng, service_name, id, (NotifyUserIdType)index, NOTIFY_ID_KIND_INDEX);
}

/*--------------------------------------------------------------------------------
	svc_bus_register_multi
--------------------------------------------------------------------------------*/
BusResult svc_bus_register_multi(const char __user* serivce, NotifyGroupIdType id, NotifyUserIdType pattern)
{
	char service_name[FULL_NAME_LEN_MAX + STR_TERMINATE_SIZE];
	BusManagerObject* bus_mng = GetServiceMng();

	if (serivce != NULL)
	{
		if (CopyServiceName(service_name, serivce) == false)
		{
			SFW_ERROR_MSG("[Error] %s : param check_2.\n", __FUNCTION__);
			return SFW_E_INVALIDARG;
		}
	}

	return bus_mng->multi_register_service(bus_mng, serivce != NULL ? service_name : NULL, id, pattern);
}

/*--------------------------------------------------------------------------------
	svc_bus_unregister
--------------------------------------------------------------------------------*/
BusResult svc_bus_unregister(const char __user* serivce)
{
	char service_name[FULL_NAME_LEN_MAX + STR_TERMINATE_SIZE];
	BusManagerObject* bus_mng = GetServiceMng();

	if (serivce == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : param check_1.\n", __FUNCTION__);
		return SFW_E_INVALIDARG;
	}

	if (CopyServiceName(service_name, serivce) == false)
	{
		SFW_ERROR_MSG("[Error] %s : param check_2.\n", __FUNCTION__);
		return SFW_E_INVALIDARG;
	}

	return bus_mng->unregister_service(bus_mng, service_name);
}

/*--------------------------------------------------------------------------------
	svc_bus_unregister_multi
--------------------------------------------------------------------------------*/
BusResult svc_bus_unregister_multi(const char __user* serivce, NotifyGroupIdType id)
{
	char service_name[FULL_NAME_LEN_MAX + STR_TERMINATE_SIZE];
	BusManagerObject* bus_mng = GetServiceMng();

	if (serivce != NULL)
	{
		if (CopyServiceName(service_name, serivce) == false)
		{
			SFW_ERROR_MSG("[Error] %s : param check_2.\n", __FUNCTION__);
			return SFW_E_INVALIDARG;
		}
	}

	return bus_mng->multi_unregister_service(bus_mng, serivce != NULL ? service_name : serivce, id);
}

/*--------------------------------------------------------------------------------
	svc_bus_send
--------------------------------------------------------------------------------*/
BusResult svc_bus_send(const char __user* serivce, const char __user* data, unsigned int data_size)
{
	char service_name[FULL_NAME_LEN_MAX + STR_TERMINATE_SIZE];
	BusManagerObject* bus_mng = GetServiceMng();

	if ((serivce == NULL) || (data == NULL) || (data_size == 0))
	{
		SFW_ERROR_MSG("[Error] %s : param check_1.\n", __FUNCTION__);
		return SFW_E_INVALIDARG;
	}

	if (CopyServiceName(service_name, serivce) == false)
	{
		SFW_ERROR_MSG("[Error] %s : param check_2.\n", __FUNCTION__);
		return SFW_E_INVALIDARG;
	}

	return bus_mng->send_message(bus_mng, service_name, data, data_size);
}

/*--------------------------------------------------------------------------------
	svc_bus_set(上書き)
--------------------------------------------------------------------------------*/
BusResult svc_bus_set(const char __user* serivce, const char __user* data, unsigned int data_size)
{
	char service_name[FULL_NAME_LEN_MAX + STR_TERMINATE_SIZE];
	BusManagerObject* bus_mng = GetServiceMng();

	if ((serivce == NULL) || (data == NULL) || (data_size == 0))
	{
		SFW_ERROR_MSG("[Error] %s : param check_1.\n", __FUNCTION__);
		return SFW_E_INVALIDARG;
	}

	if (CopyServiceName(service_name, serivce) == false)
	{
		SFW_ERROR_MSG("[Error] %s : param check_2.\n", __FUNCTION__);
		return SFW_E_INVALIDARG;
	}

	return bus_mng->set_message(bus_mng, service_name, data, data_size);
}

/*--------------------------------------------------------------------------------
	svc_bus_wait_receive
--------------------------------------------------------------------------------*/
BusResult svc_bus_wait_receive(NotifyGroupIdType id, NotifyUserIdType __user* buf_ptr)
{
	BusManagerObject* bus_mng = GetServiceMng();

	return bus_mng->wait_message(bus_mng, id, (void*)buf_ptr);
}

/*--------------------------------------------------------------------------------
	svc_bus_receive
--------------------------------------------------------------------------------*/
BusResult svc_bus_receive(const char __user* serivce, char __user* buf_ptr, int buf_len, int __user* read_len)
{
	char service_name[FULL_NAME_LEN_MAX + STR_TERMINATE_SIZE];
	BusManagerObject* bus_mng = GetServiceMng();

	if ((serivce == NULL) || (buf_ptr == NULL) || (buf_len <= 0) || (read_len == NULL))
	{
		SFW_ERROR_MSG("[Error] %s : param check_1.\n", __FUNCTION__);
		return SFW_E_INVALIDARG;
	}

	if (CopyServiceName(service_name, serivce) == false)
	{
		SFW_ERROR_MSG("[Error] %s : param check_2.\n", __FUNCTION__);
		return SFW_E_INVALIDARG;
	}

	return bus_mng->receive_message(bus_mng, service_name, buf_ptr, buf_len, read_len);
}

/*--------------------------------------------------------------------------------
	svc_bus_receive_multi
--------------------------------------------------------------------------------*/
BusResult svc_bus_receive_multi(NotifyGroupIdType id, char __user* buf_ptr, int buf_len, int __user* read_len)
{
	BusManagerObject* bus_mng = GetServiceMng();

	if ((buf_ptr == NULL) || (buf_len <= 0) || (read_len == NULL))
	{
		SFW_ERROR_MSG("[Error] %s : param check_1.\n", __FUNCTION__);
		return SFW_E_INVALIDARG;
	}

	return bus_mng->multi_receive_message(bus_mng, id, buf_ptr, buf_len, read_len);
}

/*--------------------------------------------------------------------------------
	svc_bus_control
--------------------------------------------------------------------------------*/
BusResult svc_bus_control(BusControlId id, void __user* param)
{
	BusResult ret = SFW_S_OK;
	BusManagerObject* bus_mng = GetServiceMng();

	switch (id)
	{
	case BUS_CONTROL_SUBSCRIBE:
		// for ServceiManager & BusProxy (subscribe service)
		if (param == NULL)
		{
			SFW_ERROR_MSG("[Error] %s : param error. id = %d.\n", __FUNCTION__, id);
			return SFW_E_INVALIDARG;
		}
		ret = bus_mng->subscribe_bus_status(bus_mng, param);
		break;

	case BUS_CONTROL_UNSUBSCRIBE:
		// for ServceiManager & BusProxy (unsubscribe service)
		if (param == NULL)
		{
			SFW_ERROR_MSG("[Error] %s : param error. id = %d.\n", __FUNCTION__, id);
			return SFW_E_INVALIDARG;
		}
		ret = bus_mng->unsubscribe_bus_status(bus_mng, param);
		break;

	case BUS_CONTROL_GET_BUS_STATUS:
		// for ServceiManager & BusProxy (get service)
		if (param == NULL)
		{
			SFW_ERROR_MSG("[Error] %s : param error. id = %d.\n", __FUNCTION__, id);
			return SFW_E_INVALIDARG;
		}
		ret = bus_mng->get_service(bus_mng, param);
		break;

	case BUS_CONTROL_GET_SERVICE_CNT:
		// for ServceiManager & BusProxy (get service)
		if (param == NULL)
		{
			SFW_ERROR_MSG("[Error] %s : param error. id = %d.\n", __FUNCTION__, id);
			return SFW_E_INVALIDARG;
		}
		ret = bus_mng->get_service_count(bus_mng, param);
		break;

	case BUS_CONTROL_GET_BUS_STATUS_LIMIT:
		// for ServceiManager & BusProxy (get service)
		if (param == NULL)
		{
			SFW_ERROR_MSG("[Error] %s : param error. id = %d.\n", __FUNCTION__, id);
			return SFW_E_INVALIDARG;
		}
		ret = bus_mng->get_service_limit(bus_mng, param);
		break;

	case BUS_CONTROL_STORE_SERVICE_CNT:
		// for ServceiManager & BusProxy (get service count)
		if (param == NULL)
		{
			SFW_ERROR_MSG("[Error] %s : param error. id = %d.\n", __FUNCTION__, id);
			return SFW_E_INVALIDARG;
		}
		ret = bus_mng->get_store_service_count(bus_mng, param);
		break;

	case BUS_CONTROL_STORE_SERVICE_STATUS:
		// for ServceiManager & BusProxy (get service status)
		if (param == NULL)
		{
			SFW_ERROR_MSG("[Error] %s : param error. id = %d.\n", __FUNCTION__, id);
			return SFW_E_INVALIDARG;
		}
		ret = bus_mng->get_store_service_status(bus_mng, param);
		break;

	case BUS_CONTROL_LOG_START:
		if (param == NULL)
		{
			SFW_ERROR_MSG("[Error] %s : param error. id = %d.\n", __FUNCTION__, id);
			return SFW_E_INVALIDARG;
		}
		ret = bus_mng->start_log(bus_mng, param);
		break;

	case BUS_CONTROL_LOG_STOP:
		ret = bus_mng->stop_log(bus_mng, param);
		break;

	case BUS_CONTROL_LOG_CLEAR:
		// ret = bus_mng->clear_log(bus_mng, NULL);
		break;

	case BUS_CONTROL_LOG_GET:
		if (param == NULL)
		{
			SFW_ERROR_MSG("[Error] %s : param error. id = %d.\n", __FUNCTION__, id);
			return SFW_E_INVALIDARG;
		}
		ret = bus_mng->get_log(bus_mng, param);
		break;

	case BUS_CONTROL_GET_BUS_INFO:
		if (param == NULL)
		{
			SFW_ERROR_MSG("[Error] %s : param error. id = %d.\n", __FUNCTION__, id);
			return SFW_E_INVALIDARG;
		}
		ret = bus_mng->get_bus_info(bus_mng, param);
		break;

	case BUS_CONTROL_GET_BUS_INFO_LIMIT:
		if (param == NULL)
		{
			SFW_ERROR_MSG("[Error] %s : param error. id = %d.\n", __FUNCTION__, id);
			return SFW_E_INVALIDARG;
		}
		ret = bus_mng->get_bus_info_limit(bus_mng, param);
		break;

	case BUS_CONTROL_GET_DEVICE_NAME:
		if (param == NULL)
		{
			SFW_ERROR_MSG("[Error] %s : param error. id = %d.\n", __FUNCTION__, id);
			return SFW_E_INVALIDARG;
		}
		ret = bus_mng->get_device_name(bus_mng, param);
		break;

	case BUS_CONTROL_GET_MAX_QUEUE_SIZE:
		if (param == NULL)
		{
			SFW_ERROR_MSG("[Error] %s : param error. id = %d.\n", __FUNCTION__, id);
			return SFW_E_INVALIDARG;
		}
		ret = bus_mng->get_max_queue_size(bus_mng, param);
		break;

	case BUS_CONTROL_SUBSCRIBE_SERVICE_STATUS:
		if (param == NULL)
		{
			SFW_ERROR_MSG("[Error] %s : param error. id = %d.\n", __FUNCTION__, id);
			return SFW_E_INVALIDARG;
		}
		ret = bus_mng->subscribe_service_status(bus_mng, param);
		break;

	case BUS_CONTROL_UNSUBSCRIBE_SERVICE_STATUS:
		if (param == NULL)
		{
			SFW_ERROR_MSG("[Error] %s : param error. id = %d.\n", __FUNCTION__, id);
			return SFW_E_INVALIDARG;
		}
		ret = bus_mng->unsubscribe_service_status(bus_mng, param);
		break;

	case BUS_CONTROL_GET_SERVICE_STATUS_BUFF_SIZE:
		if (param == NULL)
		{
			SFW_ERROR_MSG("[Error] %s : param error. id = %d.\n", __FUNCTION__, id);
			return SFW_E_INVALIDARG;
		}
		ret = bus_mng->get_service_status_buff_size(bus_mng, param);
		break;

	case BUS_CONTROL_GET_SERVICE_STATUS:
		if (param == NULL)
		{
			SFW_ERROR_MSG("[Error] %s : param error. id = %d.\n", __FUNCTION__, id);
			return SFW_E_INVALIDARG;
		}
		ret = bus_mng->get_service_status(bus_mng, param);
		break;

	default:
		SFW_ERROR_MSG("[Error] %s : id error. id = %d \n", __FUNCTION__, id);
		ret = SFW_E_INVALIDARG;
		break;
	}

	return ret;
}
