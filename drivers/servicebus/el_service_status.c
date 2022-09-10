/********************************************************************************/
/** @file		el_service_status.c
	@brief		event listener service status
	@note		Copyright (C) 2015 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2015/12/24
	@version	1.0
*********************************************************************************/
/*--------------------------------------------------------------------------------
	Include
--------------------------------------------------------------------------------*/
#include "servicebus_common.h"													// service bus common include
#include "el_service_status.h"													// event listener bus status include
#include "wait_notify.h"														// wait notify

/*--------------------------------------------------------------------------------
	add
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result el_service_status_add(ELServiceStatusObject* obj, WaitNotifyObject* wait_notify, NotifyUserIdType id_pattern)
 *	@brief		WaitNotifyObject* と NotifyUserIdType を管理する
 *	@param[in]	WaitNotifyObject* wait_notify
 *	@param[in]	NotifyUserIdType id_pattern
 *	@return		Result SFW_S_OK...OK, SFW_E_INVALIDARG/SFW_E_INIT_MEMORY_ALLOCATOR...NG
 */
static Result el_service_status_add(ELServiceStatusObject* obj, WaitNotifyObject* wait_notify, NotifyUserIdType id_pattern)
{
	WaitNotifyListInfo* notify_obj_list = NULL;
	struct list_head* listptr = NULL;

	if (wait_notify == NULL) return SFW_E_INVALIDARG;

	list_for_each(listptr, &obj->notify_obj_list_.list)
	{
		notify_obj_list = list_entry(listptr, struct _WaitNotifyListInfo, list);

		if ((notify_obj_list->wait_notify == wait_notify) && (notify_obj_list->id_pattern == id_pattern))
		{
			return SFW_S_OK;
		}
	}

	notify_obj_list = kmalloc(sizeof(WaitNotifyListInfo), GFP_KERNEL);
	if (notify_obj_list == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : new EventObjListInfo.\n", __FUNCTION__);
		return SFW_E_INIT_MEMORY_ALLOCATOR;
	}
	memset(notify_obj_list, 0, sizeof(WaitNotifyListInfo));

	notify_obj_list->wait_notify  = wait_notify;
	notify_obj_list->id_pattern = id_pattern;

	list_add(&notify_obj_list->list, &obj->notify_obj_list_.list);

	return SFW_S_OK;
}

/*----------------------------------------------------------------------------
	del
----------------------------------------------------------------------------*/
/**
 *	@fn			Result el_service_status_del(ELServiceStatusObject* obj, WaitNotifyObject* wait_notify, NotifyUserIdType id_pattern)
 *	@brief		WaitNotifyObject* と NotifyUserIdType を管理から外す
 *	@param[in]	WaitNotifyObject* wait_notify
 *	@param[in]	NotifyUserIdType id_pattern
 *	@return		Result SFW_S_OK...OK, SFW_E_INVALIDARG/SFW_E_FAIL...NG
 */
static Result el_service_status_del(ELServiceStatusObject* obj, WaitNotifyObject* wait_notify, NotifyUserIdType id_pattern)
{
	Result ret = SFW_E_FAIL;
	struct list_head* listptr = NULL;

	if (wait_notify == NULL) return SFW_E_INVALIDARG;

	list_for_each(listptr, &obj->notify_obj_list_.list)
	{
		WaitNotifyListInfo* notify_obj_list = list_entry(listptr, struct _WaitNotifyListInfo, list);

		if ((notify_obj_list->wait_notify == wait_notify) && (notify_obj_list->id_pattern == id_pattern))
		{
			ret = SFW_S_OK;

			list_del(&notify_obj_list->list);

			kfree(notify_obj_list);

			break;
		}
 	}

	return ret;
}

/*----------------------------------------------------------------------------
	exit
----------------------------------------------------------------------------*/
/**
 *	@fn			void el_service_status_exit(ELServiceStatusObject* obj)
 *	@brief		exit bus status
 *	@param		なし
 *	@return		なし
 */
static void el_service_status_exit(ELServiceStatusObject* obj)
{
	obj->clear(obj);
}

/*----------------------------------------------------------------------------
	create_reg_info
----------------------------------------------------------------------------*/
/**
 *	@fn			void el_service_status_create_reg_info(ELServiceStatusObject* obj, RegInfo* reg_info, const char* full_name)
 *	@brief		ServiceInfoList を生成して、引数に追加する
 *	@param[in]	RegInfo* reg_info
 *	@param[in]	const char* full_name
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result el_service_status_create_reg_info(ELServiceStatusObject* obj, RegInfo* reg_info, const char* full_name)
{
	ServiceInfoList* service_info_list = kmalloc(sizeof(ServiceInfoList), GFP_KERNEL);
	if (service_info_list == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : new ServiceInfoList.\n", __FUNCTION__);
		return SFW_E_INIT_MEMORY_ALLOCATOR;
	}
	memset(service_info_list, 0, sizeof(ServiceInfoList));

	strncpy(service_info_list->service_name, full_name, FULL_NAME_LEN_MAX);
	service_info_list->reg_count = 1;
	service_info_list->unreg_count = 0;

	list_add(&service_info_list->list, &reg_info->service_info_list.list);

	/*
	// サービス長、サービス数の更新
	*/
	size_t len = strlen(full_name);
	reg_info->total_len += (len + (len & 1));
	reg_info->count ++;

	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	dispatch
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result el_service_status_dispatch(ELServiceStatusObject* obj, ESMessageType* es_msg)
 *	@brief		change handler
 *	@param[in]	const char* data
 *	@param[in]	int data_size
 *	@param[in]	BusStatus status
 *	@param[in]	int queue_size
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result el_service_status_dispatch(ELServiceStatusObject* obj, ESMessageType* es_msg)
{
	EventParamServiceNameInfo* p 			= es_msg->data_1;
//	EventParamActionResultInfo* action_info = es_msg->data_2;
//	EventParamMsgInfo* msg_info 			= es_msg->data_3;
//	EventParamNotifyInfo* event_info		= es_msg->data_4;

	if ((es_msg->event_type == BUS_EVENT_REGISTER_SERVICE) || (es_msg->event_type == BUS_EVENT_UNREGISTER_SERVICE))
	{
		Result ret = SFW_S_OK;

		const char* user_name = NULL;
		const char* device_name = NULL;
		const char* service_name = NULL;
		const char* full_name = NULL;

		ServiceInfoList* service_info_list = NULL;
		RegInfo* reg_info = NULL;
		AllServiceInfo* device_service = NULL;

		/*
		// search or create target
		*/
		NameParserObject* name_obj = &obj->name_parser_;
		if (name_obj->parse(name_obj, p->name) == false)
		{
			SFW_ERROR_MSG("[Error] %s : fail service name. %s\n", __FUNCTION__, p->name);
			return SFW_E_FAIL;
		}

		user_name    = name_obj->get_user_name(name_obj);
		device_name  = name_obj->get_host_name(name_obj);
		service_name = name_obj->get_service_name(name_obj);
		full_name    = name_obj->get_full_name(name_obj);

		device_service = obj->search_device(obj, device_name);

		if (device_service != NULL)
		{
			reg_info = strncmp(user_name, service_name, SERVICE_NAME_LEN_MAX/*(==USER_NAME_LEN_MAX)*/) == 0 ? &device_service->services : &device_service->users;
			service_info_list = obj->search_service(obj, &reg_info->service_info_list, full_name);

			if (service_info_list != NULL)
			{
				if (es_msg->event_type == BUS_EVENT_REGISTER_SERVICE)
				{
					service_info_list->reg_count++;
				}
				else
				{
					service_info_list->unreg_count++;
				}
			}
			else
			{
				/*
				// 新規サービスで unregister はないので、状態異常として処理しない
				*/
				if (es_msg->event_type == BUS_EVENT_UNREGISTER_SERVICE)
				{
					SFW_ERROR_MSG("[Error] %s : es_msg->event_type == BUS_EVENT_UNREGISTER_SERVICE.\n", __FUNCTION__);
					return SFW_E_FAIL;
				}

				ret = obj->create_reg_info(obj, reg_info, full_name);
				if (SFW_FAILED(ret))
				{
					SFW_ERROR_MSG("[Error] %s : add_reg_info.\n", __FUNCTION__);
					return ret;
				}
			}
		}
		else
		{
			/*
			// 新規サービスで unregister はないので、状態異常として処理しない
			*/
			if (es_msg->event_type == BUS_EVENT_UNREGISTER_SERVICE)
			{
				SFW_ERROR_MSG("[Error] %s : es_msg->event_type == BUS_EVENT_UNREGISTER_SERVICE2.\n", __FUNCTION__);
				return SFW_E_FAIL;
			}

			device_service = kmalloc(sizeof(AllServiceInfo), GFP_KERNEL);
			if (device_service == NULL)
			{
				SFW_ERROR_MSG("[Error] %s : new AllServiceInfo.\n", __FUNCTION__);
				return SFW_E_INIT_MEMORY_ALLOCATOR;
			}
			memset(device_service, 0, sizeof(AllServiceInfo));

			/*
			// init member
			*/
			strncpy(device_service->device_name, device_name, HOST_NAME_LEN_MAX);

			INIT_LIST_HEAD(&device_service->services.service_info_list.list);
			device_service->services.total_len = 0;
			device_service->services.count = 0;

			INIT_LIST_HEAD(&device_service->users.service_info_list.list);
			device_service->users.total_len = 0;
			device_service->users.count = 0;

			/*
			// add service info
			*/
			reg_info = strncmp(user_name, service_name, SERVICE_NAME_LEN_MAX/*(==USER_NAME_LEN_MAX)*/) == 0 ? &device_service->services : &device_service->users;
			ret = obj->create_reg_info(obj, reg_info, full_name);
			if (SFW_FAILED(ret))
			{
				SFW_ERROR_MSG("[Error] %s : add_reg_info2.\n", __FUNCTION__);
				kfree(device_service);
				return ret;
			}

			list_add(&device_service->list, &obj->all_service_list_.list);
		}

		/*
		// notify listener
		*/
		{
			struct list_head* listptr = NULL;

			list_for_each(listptr, &obj->notify_obj_list_.list)
			{
				WaitNotifyListInfo* notify_obj_list = list_entry(listptr, struct _WaitNotifyListInfo, list);
				if (notify_obj_list->wait_notify != NULL) notify_obj_list->wait_notify->notify(notify_obj_list->wait_notify, notify_obj_list->id_pattern);
			}
		}
	}

	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	search_device
--------------------------------------------------------------------------------*/
/**
 *	@fn			AllServiceInfo* el_service_status_search_device(ELServiceStatusObject* obj, const char* name)
 *	@brief		サービス登録情報の取得
 *	@param[in]	const char* name
 *	@return		AllServiceInfo* !NULL...OK, NULL...NG
 */
static AllServiceInfo* el_service_status_search_device(ELServiceStatusObject* obj, const char* name)
{
	struct list_head* listptr = NULL;
	AllServiceInfo* result = NULL;

	if (name == NULL) return NULL;

	list_for_each(listptr, &obj->all_service_list_.list)
	{
		AllServiceInfo* target = list_entry(listptr, struct _AllServiceInfo, list);

		if (strncmp(target->device_name, name, HOST_NAME_LEN_MAX) == 0)
		{
			result = target;
			break;
		}
	}

	return result;
}

/*--------------------------------------------------------------------------------
	search_service
--------------------------------------------------------------------------------*/
/**
 *	@fn			ServiceInfoList* el_service_status_search_service(ELServiceStatusObject* obj, ServiceInfoList* service_info_list, const char* name)
 *	@brief		サービス登録情報の取得
 *	@param[in]	ServiceInfoList* service_info_list
 *	@param[in]	const char* name
 *	@return		ServiceInfoList* !NULL...OK, NULL...NG
 */
static ServiceInfoList* el_service_status_search_service(ELServiceStatusObject* obj, ServiceInfoList* service_info_list, const char* name)
{
	struct list_head* listptr = NULL;
	ServiceInfoList* result = NULL;

	if ((service_info_list == NULL) || (name == NULL)) return NULL;

	list_for_each(listptr, &service_info_list->list)
	{
		ServiceInfoList* target = list_entry(listptr, struct _ServiceInfoList, list);

		if (strncmp(target->service_name, name, FULL_NAME_LEN_MAX) == 0)
		{
			result = target;
			break;
		}
	}

	return result;
}

/*--------------------------------------------------------------------------------
	clear_service_reg_info
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result el_service_status_clear_service_reg_info(ELServiceStatusObject* obj, ServiceInfoList* service_info_list)
 *	@brief		登録情報のクリア
 *	@param[in]	ServiceInfoList* service_info_list
 *	@return		なし
 */
static void el_service_status_clear_service_reg_info(ELServiceStatusObject* obj, ServiceInfoList* service_info_list)
{
	while (!list_empty(&service_info_list->list))
	{
		ServiceInfoList* target = list_entry(service_info_list->list.next, struct _ServiceInfoList, list);
		if (target != NULL)
		{
			list_del(&target->list);
			kfree(target);
		}
	}
}

/*--------------------------------------------------------------------------------
	clear
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result el_service_status_clear(ELServiceStatusObject* obj)
 *	@brief		登録情報のクリア
 *	@param 		なし
 *	@return		なし
 */
static void el_service_status_clear(ELServiceStatusObject* obj)
{
	while (!list_empty(&obj->all_service_list_.list))
	{
		AllServiceInfo* target = list_entry(obj->all_service_list_.list.next, struct _AllServiceInfo, list);
		if (target != NULL)
		{
			obj->clear_service_reg_info(obj, &target->services.service_info_list);
			target->services.total_len = 0;
			target->services.count = 0;

			obj->clear_service_reg_info(obj, &target->users.service_info_list);
			target->users.total_len = 0;
			target->users.count = 0;

			list_del(&target->list);
			kfree(target);
		}
	}
}

/*--------------------------------------------------------------------------------
	get_buff_size
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result el_service_status_get_buff_size(ELServiceStatusObject* obj, const char* target_device, int status_kind, char __user* dst_len)
 *	@brief		自身のデバイスのサービス名の名前長の取得
 *	@param[in]	const char* target_device
 *	@param[in]	int status_kind
 *	@param[out]	char __user* dst_len
 *	@return		Result SFW_S_OK...OK, SFW_E_ACCESS_DENIED...NG
 */
static Result el_service_status_get_buff_size(ELServiceStatusObject* obj, const char* target_device, int status_kind, char __user* dst_len)
{
	int service_name_len = obj->current_buff_size(obj, target_device, status_kind);

	if (copy_to_user(dst_len, &service_name_len, sizeof(service_name_len)))
	{
		SFW_ERROR_MSG("[Error] %s : copy_to_user sizeof(service_name_len) = %u.\n", __FUNCTION__, sizeof(service_name_len));
		return SFW_E_ACCESS_DENIED;
	}

	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	get_status
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result el_service_status_get_status(ELServiceStatusObject* obj, const char* target_device, int status_kind, int max_buff_len, char __user* dst_service_tbl)
 *	@brief		サービスの取得
 *	@param[in]	const char* target_device
 *	@param[in]	int status_kind
 *	@param[in]	int max_buff_len
 *	@param[out]	char __user* dst_service_tbl
 *	@return		Result SFW_S_OK/SFW_S_SIZELIMIT...OK, SFW_E_ACCESS_DENIED...NG
 */
static Result el_service_status_get_status(ELServiceStatusObject* obj, const char* target_device, int status_kind, int max_buff_len, char __user* dst_service_tbl)
{
	char __user* dst_service_cnt = dst_service_tbl;
	struct list_head* listptr = NULL;
	int service_cnt = 0;

	/*
	// 最初に長さチェック
	*/
	if (obj->current_buff_size(obj, target_device, status_kind) > max_buff_len) return SFW_S_SIZELIMIT;

	// format
	// +---------------+----------+--------------------+---------+-----------+----------+--------------------+--------+-----+
	// | service count | name len | service name       | reg_cnt | unreg_cnt | name len | service name       | status | ... |
	// +---------------+----------+--------------------+---------+-----------+----------+--------------------+--------+-----+

	dst_service_tbl += sizeof(service_cnt);

	list_for_each(listptr, &obj->all_service_list_.list)
	{
		AllServiceInfo* service_info = list_entry(listptr, struct _AllServiceInfo, list);
		if (service_info == NULL) continue;

		if (target_device == NULL)
		{
			if (status_kind & SERVICE_STATUS_KIND_SERVICE)
			{
				int copy_service_cnt = 0;
				dst_service_tbl = obj->copy_service_reg_info(obj, &service_info->services.service_info_list, dst_service_tbl, &copy_service_cnt);
				if (dst_service_tbl == NULL) continue;
				service_cnt += copy_service_cnt;
			}

			if (status_kind & SERVICE_STATUS_KIND_USER)
			{
				int copy_user_cnt = 0;
				dst_service_tbl = obj->copy_service_reg_info(obj, &service_info->users.service_info_list, dst_service_tbl, &copy_user_cnt);
				if (dst_service_tbl == NULL) continue;
				service_cnt += copy_user_cnt;
			}
		}
		else if (strncmp(service_info->device_name, target_device, HOST_NAME_LEN_MAX) == 0)
		{
			if (status_kind & SERVICE_STATUS_KIND_SERVICE)
			{
				int copy_service_cnt = 0;
				dst_service_tbl = obj->copy_service_reg_info(obj, &service_info->services.service_info_list, dst_service_tbl, &copy_service_cnt);
				if (dst_service_tbl == NULL) continue;
				service_cnt += copy_service_cnt;
			}

			if (status_kind & SERVICE_STATUS_KIND_USER)
			{
				int copy_user_cnt = 0;
				dst_service_tbl = obj->copy_service_reg_info(obj, &service_info->users.service_info_list, dst_service_tbl, &copy_user_cnt);
				if (dst_service_tbl == NULL) continue;
				service_cnt += copy_user_cnt;
			}

			// ターゲット指定時はここで終了
			break;
		}
	}

	if (copy_to_user(dst_service_cnt, &service_cnt, sizeof(service_cnt)))
	{
		SFW_ERROR_MSG("[Error] %s : copy_to_user sizeof(service_cnt) = %u.\n", __FUNCTION__, sizeof(service_cnt));
		return SFW_E_ACCESS_DENIED;
	}

	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	current_buff_size
--------------------------------------------------------------------------------*/
/**
 *	@fn			int el_service_status_current_buff_size(ELServiceStatusObject* obj, const char* target_device, int status_kind)
 *	@brief		自身のデバイスのサービス名の名前長の取得
 *	@param[in]	const char* target_device
 *	@param[in]	int status_kind
 *	@return		int buff size
 */
static int el_service_status_current_buff_size(ELServiceStatusObject* obj, const char* target_device, int status_kind)
{
	struct list_head* listptr = NULL;
	int service_name_len = 0;

	list_for_each(listptr, &obj->all_service_list_.list)
	{
		int service_len = 0, user_len = 0;
		AllServiceInfo* target = list_entry(listptr, struct _AllServiceInfo, list);
		if (target == NULL) continue;

		if (target_device == NULL)
		{
			if ((status_kind & SERVICE_STATUS_KIND_SERVICE) && (target->services.count != 0)) service_len = target->services.total_len + ((sizeof(size_t) + sizeof(int) + sizeof(int)) * target->services.count);
			if ((status_kind & SERVICE_STATUS_KIND_USER) && (target->users.count != 0)) user_len = target->users.total_len + ((sizeof(size_t) + sizeof(int) + sizeof(int)) * target->users.count);

			service_name_len += (service_len + user_len);
		}
		else if (strncmp(target->device_name, target_device, HOST_NAME_LEN_MAX) == 0)
		{
			if ((status_kind & SERVICE_STATUS_KIND_SERVICE) && (target->services.count != 0)) service_len = target->services.total_len + ((sizeof(size_t) + sizeof(int) + sizeof(int)) * target->services.count);
			if ((status_kind & SERVICE_STATUS_KIND_USER) && (target->users.count != 0)) user_len = target->users.total_len + ((sizeof(size_t) + sizeof(int) + sizeof(int)) * target->users.count);

			service_name_len = service_len + user_len;

			// ターゲット指定時はここで終了
			break;
		}
	}

	service_name_len += sizeof(int);

	return service_name_len;
}

/*--------------------------------------------------------------------------------
 	copy_service_reg_info
 --------------------------------------------------------------------------------*/
/**
 *	@fn			char __user* el_service_status_copy_service_reg_info(ELServiceStatusObject* obj, ServiceInfoList* service_info_list, char __user* dst_service_tbl, int* dst_service_cnt)
 *	@brief		サービス情報のコピー
 *	@param[in]	ServiceInfoList* service_info_list
 *	@param[out]	char __user* dst_service_tbl
 *	@param[out]	int* dst_service_cnt
 *	@return		char __user* dst_service_tbl
 */
static char __user* el_service_status_copy_service_reg_info(ELServiceStatusObject* obj, ServiceInfoList* service_info_list, char __user* dst_service_tbl, int* dst_service_cnt)
{
	struct list_head* listptr = NULL;
	int service_cnt = 0;

	// format
	// +---------------+----------+--------------------+---------+-----------+----------+--------------------+--------+-----+
	// | service count | name len | service name       | reg_cnt | unreg_cnt | name len | service name       | status | ... |
	// +---------------+----------+--------------------+---------+-----------+----------+--------------------+--------+-----+

	list_for_each(listptr, &service_info_list->list)
	{
		size_t name_len = 0;

		ServiceInfoList* target = list_entry(listptr, struct _ServiceInfoList, list);
		if (target == NULL) continue;

		name_len = strlen(target->service_name);
		if (name_len == 0) continue;

		// padding name len
		name_len += name_len & 1;

		// copy len
		if (copy_to_user(dst_service_tbl, &name_len, sizeof(name_len)))
		{
			SFW_ERROR_MSG("[Error] %s : copy_to_user sizeof(name_len) = %zu.\n", __FUNCTION__, sizeof(name_len));
			return NULL;
		}

		dst_service_tbl += sizeof(name_len);

		// copy service name
		if (copy_to_user(dst_service_tbl, target->service_name, name_len))
		{
			SFW_ERROR_MSG("[Error] %s : copy_to_user(dst_service_tbl, target->service_name, name_len) len = %d.\n", __FUNCTION__, (int)name_len);
			return NULL;
		}

		dst_service_tbl += name_len;

		// copy reg_count
		if (copy_to_user(dst_service_tbl, &target->reg_count, sizeof(target->reg_count)))
		{
			SFW_ERROR_MSG("[Error] %s : copy_to_user sizeof(target->reg_count) = %d.\n", __FUNCTION__, sizeof(target->reg_count));
			return NULL;
		}

		dst_service_tbl += sizeof(target->reg_count);

		// copy unreg_count
		if (copy_to_user(dst_service_tbl, &target->unreg_count, sizeof(target->unreg_count)))
		{
			SFW_ERROR_MSG("[Error] %s : copy_to_user sizeof(target->unreg_count) = %d.\n", __FUNCTION__, sizeof(target->unreg_count));
			return NULL;
		}

		dst_service_tbl += sizeof(target->unreg_count);

		service_cnt ++;
	}

	if (dst_service_cnt != NULL) *dst_service_cnt = service_cnt;

	return dst_service_tbl;
}

/*--------------------------------------------------------------------------------
	ELBusStatusObject
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result InitializeELServiceStatusObject(ELServiceStatusObject* obj)
 *	@brief		constructor
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
Result InitializeELServiceStatusObject(ELServiceStatusObject* obj)
{
	Result ret = SFW_S_OK;

	if (obj == NULL) return SFW_E_INVALIDARG;

	ret = InitializeEventListenerObject((EventListenerObject*)obj);

	if (SFW_FAILED(ret)) return SFW_E_FAIL;

	memset(&obj->notify_obj_list_, 0, sizeof(obj->notify_obj_list_));
	INIT_LIST_HEAD(&obj->notify_obj_list_.list);

	memset(&obj->all_service_list_, 0, sizeof(obj->all_service_list_));
	INIT_LIST_HEAD(&obj->all_service_list_.list);

	ret = InitializeNameParserObject(&obj->name_parser_);
	if (SFW_FAILED(ret)) return SFW_E_FAIL;

	obj->dispatch 						= (EventListener_dispatch)el_service_status_dispatch;
	obj->exit							= (EventListener_exit)el_service_status_exit;

	obj->add							= el_service_status_add;
	obj->del							= el_service_status_del;

	obj->search_device					= el_service_status_search_device;
	obj->search_service					= el_service_status_search_service;

	obj->clear							= el_service_status_clear;
	obj->clear_service_reg_info			= el_service_status_clear_service_reg_info;

	obj->get_buff_size					= el_service_status_get_buff_size;
	obj->get_status						= el_service_status_get_status;
	obj->current_buff_size				= el_service_status_current_buff_size;
	obj->copy_service_reg_info			= el_service_status_copy_service_reg_info;
	obj->create_reg_info				= el_service_status_create_reg_info;

	return SFW_S_OK;
}
