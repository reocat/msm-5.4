/********************************************************************************/
/** @file		event_listener_reg_unreg.c
	@brief		event listener logger
	@note		Copyright (C) 2015 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2015/08/26
	@version	1.0
*********************************************************************************/
/*--------------------------------------------------------------------------------
	Include
--------------------------------------------------------------------------------*/
#include "servicebus_common.h"													// service bus common include
#include "el_reg_unreg.h"														// event listener reg & unreg include
#include "message_container.h"													// message container include

/*--------------------------------------------------------------------------------
	Local Value
--------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------
	dispatch
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result el_reg_unreg_dispatch(ELRegUnregObject* obj, ESMessageType* es_msg)
 *	@brief		change handler
 *	@param[in]	const char* data
 *	@param[in]	int data_size
 *	@param[in]	BusStatus status
 *	@param[in]	int queue_size
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result el_reg_unreg_dispatch(ELRegUnregObject* obj, ESMessageType* es_msg)
{
// 	EventParamServiceNameInfo* service_info = es_msg->data_1;
//	EventParamActionResultInfo* action_info = es_msg->data_2;
//	EventParamMsgInfo* msg_info 			= es_msg->data_3;
//	EventParamNotifyInfo* event_info		= es_msg->data_4;

	if ((es_msg->event_type == BUS_EVENT_REGISTER_SERVICE) || (es_msg->event_type == BUS_EVENT_UNREGISTER_SERVICE))
	{
		char msg[16] = {0};

		obj->counter_ ++;

		sprintf(msg, "G%d", obj->counter_);

		{
			struct list_head* listptr = NULL;

			#ifdef linux
			mm_segment_t old_fs = get_fs();
			set_fs(KERNEL_DS);
			#endif // linux

			list_for_each(listptr, &obj->mc_obj_list_.list)
			{
				MCObjListInfo* mc_obj_list = list_entry(listptr, struct _MCObjListInfo, list);

				if ((mc_obj_list == NULL) || (mc_obj_list->mc_obj == NULL)) continue;

				mc_obj_list->mc_obj->set(mc_obj_list->mc_obj, msg, strlen(msg));
		 	}

			#ifdef linux
			set_fs(old_fs);
			#endif // linux
		}
	}

	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	add
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result el_reg_unreg_add(ELRegUnregObject* obj, MessageContainerObject* mc_obj)
 *	@brief		MessageContainerObject* を管理する
 *	@param[in]	MessageContainerObject* mc_obj
 *	@return		Result SFW_S_OK...OK, SFW_E_INVALIDARG/SFW_E_INIT_MEMORY_ALLOCATOR...NG
 */
static Result el_reg_unreg_add(ELRegUnregObject* obj, MessageContainerObject* mc_obj)
{
	MCObjListInfo* mc_obj_list = NULL;
	struct list_head* listptr = NULL;

	if (mc_obj == NULL) return SFW_E_INVALIDARG;

	list_for_each(listptr, &obj->mc_obj_list_.list)
	{
		mc_obj_list = list_entry(listptr, struct _MCObjListInfo, list);

		if (mc_obj_list == NULL) continue;

		if (mc_obj_list->mc_obj == mc_obj)
		{
			return SFW_S_OK;
		}
 	}

	mc_obj_list = kmalloc(sizeof(MCObjListInfo), GFP_KERNEL);
	if (mc_obj_list == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : new MCObjListInfo.\n", __FUNCTION__);
		return SFW_E_INIT_MEMORY_ALLOCATOR;
	}

	mc_obj_list->mc_obj = mc_obj;

	list_add(&mc_obj_list->list, &obj->mc_obj_list_.list);

	return SFW_S_OK;
}

/*----------------------------------------------------------------------------
	del
----------------------------------------------------------------------------*/
/**
 *	@fn			Result el_reg_unreg_del(ELRegUnregObject* obj, MessageContainerObject* mc_obj)
 *	@brief		MessageContainerObject* を削除する
 *	@param[in]	MessageContainerObject* mc_obj
 *	@return		Result SFW_S_OK...OK, SFW_E_INVALIDARG/SFW_E_FAIL...NG
 */
static Result el_reg_unreg_del(ELRegUnregObject* obj, MessageContainerObject* mc_obj)
{
	Result ret = SFW_E_FAIL;
	struct list_head* listptr = NULL;

	if (mc_obj == NULL) return SFW_E_INVALIDARG;

	list_for_each(listptr, &obj->mc_obj_list_.list)
	{
		MCObjListInfo* mc_obj_list = list_entry(listptr, struct _MCObjListInfo, list);

		if (mc_obj_list == NULL) continue;

		if (mc_obj_list->mc_obj == mc_obj)
		{
			ret = SFW_S_OK;

			list_del(&mc_obj_list->list);

			kfree(mc_obj_list);

			break;
		}
 	}

	return ret;
}

/*--------------------------------------------------------------------------------
	search
--------------------------------------------------------------------------------*/
/**
 *	@fn			MessageContainerObject* el_reg_unreg_search(ELRegUnregObject* obj, NotifyGroupIdType group_id)
 *	@brief		登録した、MessageContainerObject* の中からグループ ID と同じものを探す
 *	@param[in]	NotifyGroupIdType group_id
 *	@return		MessageContainerObject* !NULL...OK, NULL...NG
 */
static MessageContainerObject* el_reg_unreg_search(ELRegUnregObject* obj, NotifyGroupIdType group_id)
{
	struct list_head* listptr = NULL;

	list_for_each(listptr, &obj->mc_obj_list_.list)
	{
		MCObjListInfo* mc_obj_list = list_entry(listptr, struct _MCObjListInfo, list);

		if ((mc_obj_list == NULL) || (mc_obj_list->mc_obj == NULL)) continue;

		if (mc_obj_list->mc_obj->group_id_ == group_id)
		{
			return mc_obj_list->mc_obj;
		}
 	}

	return NULL;
}

/*--------------------------------------------------------------------------------
	InitializeELRegUnregObject
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result InitializeELRegUnregObject(ELRegUnregObject* obj)
 *	@brief		constructor
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
Result InitializeELRegUnregObject(ELRegUnregObject* obj)
{
	Result ret = SFW_S_OK;

	if (obj == NULL) return SFW_E_INVALIDARG;

	ret = InitializeEventListenerObject((EventListenerObject*)obj);
	if (SFW_FAILED(ret)) return SFW_E_FAIL;

	memset(&obj->mc_obj_list_, 0, sizeof(obj->mc_obj_list_));
	INIT_LIST_HEAD(&obj->mc_obj_list_.list);

	obj->counter_	= 0;

	obj->dispatch 	= (EventListener_dispatch)el_reg_unreg_dispatch;
	obj->add		= el_reg_unreg_add;
	obj->del		= el_reg_unreg_del;
	obj->search   	= el_reg_unreg_search;

	return SFW_S_OK;
}
