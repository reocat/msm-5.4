/********************************************************************************/
/** @file		el_bus_status.c
	@brief		event listener bus status
	@note		Copyright (C) 2014 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2014/05/20
	@version	1.0
*********************************************************************************/
/*--------------------------------------------------------------------------------
	Include
--------------------------------------------------------------------------------*/
#include "servicebus_common.h"													// service bus common include
#include "el_bus_status.h"														// event listener bus status include
#include "wait_notify.h"														// wait notify

/*--------------------------------------------------------------------------------
	Local Value
--------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------
	add
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result el_bus_status_add(ELBusStatusObject* obj, WaitNotifyObject* wait_notify, NotifyUserIdType id_pattern)
 *	@brief		WaitNotifyObject* と NotifyUserIdType を管理する
 *	@param[in]	WaitNotifyObject* wait_notify
 *	@param[in]	NotifyUserIdType id_pattern
 *	@return		Result SFW_S_OK...OK, SFW_E_INVALIDARG/SFW_E_INIT_MEMORY_ALLOCATOR...NG
 */
static Result el_bus_status_add(ELBusStatusObject* obj, WaitNotifyObject* wait_notify, NotifyUserIdType id_pattern)
{
	NotifyListInfo* notify_obj_list = NULL;
	struct list_head* listptr = NULL;

	if (wait_notify == NULL) return SFW_E_INVALIDARG;

	list_for_each(listptr, &obj->notify_obj_list_.list)
	{
		notify_obj_list = list_entry(listptr, struct _NotifyListInfo, list);

		if ((notify_obj_list->wait_notify == wait_notify) && (notify_obj_list->id_pattern == id_pattern))
		{
			return SFW_S_OK;
		}
 	}

	notify_obj_list = kmalloc(sizeof(NotifyListInfo), GFP_KERNEL);
	if (notify_obj_list == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : new EventObjListInfo.\n", __FUNCTION__);
		return SFW_E_INIT_MEMORY_ALLOCATOR;
	}

	notify_obj_list->wait_notify  = wait_notify;
	notify_obj_list->id_pattern = id_pattern;

	list_add(&notify_obj_list->list, &obj->notify_obj_list_.list);

	return SFW_S_OK;
}

/*----------------------------------------------------------------------------
	del
----------------------------------------------------------------------------*/
/**
 *	@fn			Result el_bus_status_del(ELBusStatusObject* obj, WaitNotifyObject* wait_notify, NotifyUserIdType id_pattern)
 *	@brief		WaitNotifyObject* と NotifyUserIdType を管理から外す
 *	@param[in]	WaitNotifyObject* wait_notify
 *	@param[in]	NotifyUserIdType id_pattern
 *	@return		Result SFW_S_OK...OK, SFW_E_INVALIDARG/SFW_E_FAIL...NG
 */
static Result el_bus_status_del(ELBusStatusObject* obj, WaitNotifyObject* wait_notify, NotifyUserIdType id_pattern)
{
	Result ret = SFW_E_FAIL;
	struct list_head* listptr = NULL;

	if (wait_notify == NULL) return SFW_E_INVALIDARG;

	list_for_each(listptr, &obj->notify_obj_list_.list)
	{
		NotifyListInfo* notify_obj_list = list_entry(listptr, struct _NotifyListInfo, list);

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
 *	@fn			void el_bus_status_exit(ELDltLoggerObject* obj)
 *	@brief		exit bus status
 *	@param		なし
 *	@return		なし
 */
static void el_bus_status_exit(ELBusStatusObject* obj)
{
	obj->clear(obj);
}

/*--------------------------------------------------------------------------------
	dispatch
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result el_bus_status_dispatch(ELBusStatusObject* obj, ESMessageType* es_msg)
 *	@brief		change handler
 *	@param[in]	const char* data
 *	@param[in]	int data_size
 *	@param[in]	BusStatus status
 *	@param[in]	int queue_size
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result el_bus_status_dispatch(ELBusStatusObject* obj, ESMessageType* es_msg)
{
	EventParamServiceNameInfo* service_info = es_msg->data_1;
//	EventParamActionResultInfo* action_info = es_msg->data_2;
//	EventParamMsgInfo* msg_info 			= es_msg->data_3;
//	EventParamNotifyInfo* event_info		= es_msg->data_4;

	if ((es_msg->event_type == BUS_EVENT_REGISTER_SERVICE) || (es_msg->event_type == BUS_EVENT_UNREGISTER_SERVICE))
	{
		const char* service_name = NULL;
		ServiceRegInfo* target = NULL;

		/*
		// search or create target
		*/
		NameParserObject* name_obj = &obj->name_parser_;
		if (name_obj->parse(name_obj, service_info->name) == false)
		{
			SFW_ERROR_MSG("[Error] %s : fail service name.\n", __FUNCTION__);
			return SFW_E_FAIL;
		}

		service_name = name_obj->get_full_name(name_obj);

		target = obj->search(obj, service_name);
		if (target != NULL)
		{
			if (es_msg->event_type == BUS_EVENT_REGISTER_SERVICE)
			{
				target->reg_count++;
			}
			else
			{
				target->unreg_count++;
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

			target = kmalloc(sizeof(ServiceRegInfo), GFP_KERNEL);
			if (target == NULL)
			{
				SFW_ERROR_MSG("[Error] %s : new ServiceRegInfo.\n", __FUNCTION__);
				return SFW_E_INIT_MEMORY_ALLOCATOR;
			}

			strncpy(target->service_name, service_name, FULL_NAME_LEN_MAX);
			target->reg_count = 1;
			target->unreg_count = 0;

			list_add(&target->list, &obj->service_reg_list_.list);
		}

		/*
		// notify listener
		*/
		{
			struct list_head* listptr = NULL;

			list_for_each(listptr, &obj->notify_obj_list_.list)
			{
				NotifyListInfo* notify_obj_list = list_entry(listptr, struct _NotifyListInfo, list);
				if (notify_obj_list->wait_notify != NULL) notify_obj_list->wait_notify->notify(notify_obj_list->wait_notify, notify_obj_list->id_pattern);
			}
		}
	}

	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	search
--------------------------------------------------------------------------------*/
/**
 *	@fn			ServiceRegInfo* el_bus_status_search(ELBusStatusObject* obj, const char* name)
 *	@brief		サービス登録情報の取得
 *	@param[in]	const char* name
 *	@return		ServiceRegInfo* !NULL...OK, NULL...NG
 */
static ServiceRegInfo* el_bus_status_search(ELBusStatusObject* obj, const char* name)
{
	struct list_head* listptr = NULL;
	ServiceRegInfo* result = NULL;

	list_for_each(listptr, &obj->service_reg_list_.list)
	{
		ServiceRegInfo* target = list_entry(listptr, struct _ServiceRegInfo, list);

		if (strncmp(target->service_name, name, FULL_NAME_LEN_MAX) == 0)
		{
			result = target;
			break;
		}
	}

	return result;
}

/*--------------------------------------------------------------------------------
	clear
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result el_bus_status_clear(ELBusStatusObject* obj)
 *	@brief		登録情報のクリア
 *	@param 		なし
 *	@return		なし
 */
static void el_bus_status_clear(ELBusStatusObject* obj)
{
	while (!list_empty(&obj->service_reg_list_.list))
	{
		ServiceRegInfo* target = list_entry(obj->service_reg_list_.list.next, struct _ServiceRegInfo, list);

		if (target != NULL)
		{
			list_del(&target->list);
			kfree(target);
		}
	}
}

/*--------------------------------------------------------------------------------
 	get_status_count
 --------------------------------------------------------------------------------*/
/**
 *	@fn			Result el_bus_status_get_status_count(ELBusStatusObject* obj, char __user* dst_service_count)
 *	@brief		登録されているサービス数の取得
 *	@param[out]	char __user* dst_service_count
 *	@return		Result SFW_S_OK...OK, SFW_E_ACCESS_DENIED...NG
 */
static Result el_bus_status_get_status_count(ELBusStatusObject* obj, char __user* dst_service_count)
{
	struct list_head* listptr = NULL;
	int service_cnt = 0;

	list_for_each(listptr, &obj->service_reg_list_.list)
	{
		ServiceRegInfo* target = list_entry(listptr, struct _ServiceRegInfo, list);
		if (target == NULL) continue;
		service_cnt ++;
	}

	if (copy_to_user(dst_service_count, &service_cnt, sizeof(service_cnt)))
	{
		SFW_ERROR_MSG("[Error] %s : copy_to_user sizeof(service_cnt) = %u.\n", __FUNCTION__, sizeof(service_cnt));
		return SFW_E_ACCESS_DENIED;
	}

	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
    get_status
 --------------------------------------------------------------------------------*/
/**
 *	@fn			Result el_bus_status_get_status(ELBusStatusObject* obj, int max_service_cnt, char __user* dst_service_tbl)
 *	@brief		すべてのサービス名の取得
 *	@param[in]	int max_service_cnt
 *	@param[out]	char __user* dst_service_tbl
 *	@return		Result SFW_S_OK/SFW_S_SIZELIMIT...OK, SFW_E_ACCESS_DENIED...NG
 */
static Result el_bus_status_get_status(ELBusStatusObject* obj, int max_service_cnt, char __user* dst_service_tbl)
{
	char __user* dst_service_cnt = dst_service_tbl;
	struct list_head* listptr = NULL;
	int service_cnt = 0;

	// format
	// +---------------+----------+--------------------+---------+-----------+----------+--------------------+--------+-----+
	// | service count | name len | service name       | reg_cnt | unreg_cnt | name len | service name       | status | ... |
	// +---------------+----------+--------------------+---------+-----------+----------+--------------------+--------+-----+

	dst_service_tbl += sizeof(service_cnt);

	list_for_each(listptr, &obj->service_reg_list_.list)
	{
		size_t name_len = 0;

		ServiceRegInfo* target = list_entry(listptr, struct _ServiceRegInfo, list);
		if (target == NULL) continue;

		name_len = strlen(target->service_name);
		if (name_len == 0) continue;

		// check output size(max_service_cnt)
		if ((max_service_cnt - 1) < service_cnt)
		{
			SFW_ERROR_MSG("[Info] %s : SFW_S_SIZELIMIT max=%d cnt=%d\n", __FUNCTION__, max_service_cnt, service_cnt);
			return SFW_S_SIZELIMIT;
		}

		// copy len
		if (copy_to_user(dst_service_tbl, &name_len, sizeof(name_len)))
		{
			SFW_ERROR_MSG("[Error] %s : copy_to_user sizeof(name_len) = %u.\n", __FUNCTION__, sizeof(name_len));
			return SFW_E_ACCESS_DENIED;
		}

		dst_service_tbl += sizeof(name_len);

		// copy service name
		if (copy_to_user(dst_service_tbl, target->service_name, name_len))
		{
			SFW_ERROR_MSG("[Error] %s : copy_to_user(dst_service_tbl, target->service_name, name_len) len = %d.\n", __FUNCTION__, (int)name_len);
			return SFW_E_ACCESS_DENIED;
		}

		dst_service_tbl += name_len;

		// copy padding
		if (name_len & 1)
		{
			const char zero = 0;
			if (copy_to_user(dst_service_tbl, &zero, sizeof(zero)))
			{
				SFW_ERROR_MSG("[Error] %s : copy_to_user sizeof(zero) = %u.\n", __FUNCTION__, sizeof(zero));
				return SFW_E_ACCESS_DENIED;
			}

			dst_service_tbl += 1;
		}

		// copy reg_count
		if (copy_to_user(dst_service_tbl, &target->reg_count, sizeof(target->reg_count)))
		{
			SFW_ERROR_MSG("[Error] %s : copy_to_user sizeof(target->reg_count) = %u.\n", __FUNCTION__, sizeof(target->reg_count));
			return SFW_E_ACCESS_DENIED;
		}

		dst_service_tbl += sizeof(target->reg_count);

		// copy unreg_count
		if (copy_to_user(dst_service_tbl, &target->unreg_count, sizeof(target->unreg_count)))
		{
			SFW_ERROR_MSG("[Error] %s : copy_to_user sizeof(target->unreg_count) = %u.\n", __FUNCTION__, sizeof(target->unreg_count));
			return SFW_E_ACCESS_DENIED;
		}

		dst_service_tbl += sizeof(target->unreg_count);

        service_cnt ++;
	}

	if (copy_to_user(dst_service_cnt, &service_cnt, sizeof(service_cnt)))
	{
		SFW_ERROR_MSG("[Error] %s : copy_to_user sizeof(service_cnt) = %u.\n", __FUNCTION__, sizeof(service_cnt));
		return SFW_E_ACCESS_DENIED;
	}

    return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	ELBusStatusObject
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result InitializeELBusStatusObject(ELBusStatusObject* obj)
 *	@brief		constructor
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
Result InitializeELBusStatusObject(ELBusStatusObject* obj)
{
	Result ret = SFW_S_OK;

	if (obj == NULL) return SFW_E_INVALIDARG;

	ret = InitializeEventListenerObject((EventListenerObject*)obj);

	if (SFW_FAILED(ret)) return SFW_E_FAIL;

	memset(&obj->notify_obj_list_, 0, sizeof(obj->notify_obj_list_));
	INIT_LIST_HEAD(&obj->notify_obj_list_.list);

	memset(&obj->service_reg_list_, 0, sizeof(obj->service_reg_list_));
	INIT_LIST_HEAD(&obj->service_reg_list_.list);

	ret = InitializeNameParserObject(&obj->name_parser_);
	if (SFW_FAILED(ret)) return SFW_E_FAIL;

	obj->dispatch 			= (EventListener_dispatch)el_bus_status_dispatch;
	obj->exit				= (EventListener_exit)el_bus_status_exit;

	obj->add				= el_bus_status_add;
	obj->del				= el_bus_status_del;
	obj->search   			= el_bus_status_search;
	obj->clear    			= el_bus_status_clear;
	obj->get_status_count	= el_bus_status_get_status_count;
	obj->get_status 		= el_bus_status_get_status;

	return SFW_S_OK;
}
