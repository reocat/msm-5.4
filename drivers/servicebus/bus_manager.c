/********************************************************************************/
/** @file		bus_manager.c
	@brief		bus manager
	@note		Copyright (C) 2013 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2013/10/01
	@version	1.0
*********************************************************************************/
/*--------------------------------------------------------------------------------
	Include
--------------------------------------------------------------------------------*/
#include "servicebus_common.h"													// service bus common include
#include "bus_manager.h"														// bus manager include
#include "mc_repository.h"														// mc container include
#include "message_container.h"													// message container include
#include "wait_notify.h"														// wait notify include
#include "lock_obj.h"															// lock objetct include
#include <linux/delay.h>
#include <linux/limits.h>

/*--------------------------------------------------------------------------------
	Local Value
--------------------------------------------------------------------------------*/
#ifndef INT_MAX
#define INT_MAX	(2147483647) // 32bit int
#endif

/*----------------------------------------------------------------------------
	external funciton
----------------------------------------------------------------------------*/
extern bool get_dmesg(void);
extern bool get_dlt(void);
extern int get_qmaxsize(void);
extern char* get_hostname(void);

/*--------------------------------------------------------------------------------
	register_service
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result bus_manager_register(BusManagerObject* obj, const char* service, NotifyIdType id, NotifyPatternType pattern, NotifyIdKind notify_id_kind)
 *	@brief		register service
 *	@param[in]	const char* service			: service name
 *	@param[in]	NotifyGroupIdType group_id	: group id
 *	@param[in]	NotifyUserIdType user_id	: user id
 *	@param[in]	NotifyIdKind notify_id_kind	: notify id kind
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result bus_manager_register(BusManagerObject* obj, const char* service, NotifyGroupIdType group_id, NotifyUserIdType user_id, NotifyIdKind notify_id_kind)
{
	Result ret = SFW_S_OK;

	LOCKOBJ_LOCK(obj->mutx_);
	{
		/*
		// restart service
		*/
		MessageContainerObject* mc_obj = obj->mc_repo_.find(&obj->mc_repo_, service);
		if (mc_obj != NULL)
		{
			EventParamServiceNameInfo serivce_info = {
				.name = service,
				.group_id = group_id,
				.user_id = user_id
			};

			obj->event_source_.update(&obj->event_source_, BUS_EVENT_UNREGISTER_SERVICE, (void*)&serivce_info, NULL, NULL, NULL);

			obj->unregister_impl(obj, service, mc_obj);
		}

		/*
		// start service
		*/
		ret = obj->register_impl(obj, service, group_id, user_id, notify_id_kind);
		{
			EventParamServiceNameInfo serivce_info = {
				.name = service,
				.group_id = group_id,
				.user_id = user_id
			};

			obj->event_source_.update(&obj->event_source_, SFW_SUCCEEDED(ret) ? BUS_EVENT_REGISTER_SERVICE : BUS_EVENT_REGISTER_SERVICE_ERROR, (void*)&serivce_info, NULL, NULL, NULL);
		}
	}
	LOCKOBJ_UNLOCK(obj->mutx_);

	return ret;
}

/*--------------------------------------------------------------------------------
	multi_register_service
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result bus_manager_multi_register(BusManagerObject* obj, const char* service, NotifyIdType id, NotifyPatternType pattern)
 *	@brief		register service
 *	@param[in]	const char* service			: service name
 *	@param[in]	NotifyGroupIdType group_id	: group id
 *	@param[in]	NotifyPatternType pattern	: pattern
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result bus_manager_multi_register(BusManagerObject* obj, const char* service, NotifyGroupIdType group_id, NotifyUserIdType user_id)
{
	Result ret = SFW_S_OK;
	MessageContainerObject* mc_obj = NULL;

	LOCKOBJ_LOCK(obj->mutx_);
	{
		if (service == NULL)
		{
			/*
			// restart service
			*/
			mc_obj = obj->el_reg_unreg_.search(&obj->el_reg_unreg_, group_id);
			if (mc_obj != NULL) obj->multi_unregister_impl(obj, service, mc_obj);

			/*
			// start service
			*/
			ret = obj->multi_register_impl(obj, service, group_id, user_id);
		}
		else
		{
			/*
			// restart service
			*/
			mc_obj = obj->mc_repo_.find(&obj->mc_repo_, service);
			if (mc_obj != NULL)
			{
				EventParamServiceNameInfo serivce_info = {
					.name = service,
					.group_id = group_id,
					.user_id = user_id
				};

				obj->event_source_.update(&obj->event_source_, BUS_EVENT_UNREGISTER_SERVICE, (void*)&serivce_info, NULL, NULL, NULL);

				obj->multi_unregister_impl(obj, service, mc_obj);
			}

			/*
			// start service
			*/
			ret = obj->multi_register_impl(obj, service, group_id, user_id);
			{
				EventParamServiceNameInfo serivce_info = {
					.name = service,
					.group_id = group_id,
					.user_id = user_id
				};

				obj->event_source_.update(&obj->event_source_, SFW_SUCCEEDED(ret) ? BUS_EVENT_REGISTER_SERVICE : BUS_EVENT_REGISTER_SERVICE_ERROR, (void*)&serivce_info, NULL, NULL, NULL);
			}
		}
	}
	LOCKOBJ_UNLOCK(obj->mutx_);

	return ret;
}

/*--------------------------------------------------------------------------------
	unregister
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result bus_manager_unregister(BusManagerObject* obj, const char* service)
 *	@brief		unregister service
 *	@param[in]	const char* service	: service name
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result bus_manager_unregister(BusManagerObject* obj, const char* service)
{
	MessageContainerObject* mc_obj = NULL;

	LOCKOBJ_LOCK(obj->mutx_);
	{
		EventParamServiceNameInfo serivce_info = {
			.name = service,
			.group_id = 0,
			.user_id = 0
		};

		mc_obj = obj->mc_repo_.find(&obj->mc_repo_, service);

		obj->event_source_.update(&obj->event_source_, mc_obj != NULL ? BUS_EVENT_UNREGISTER_SERVICE : BUS_EVENT_UNREGISTER_SERVICE_ERROR, (void*)&serivce_info, NULL, NULL, NULL);

		if (mc_obj != NULL) obj->unregister_impl(obj, service, mc_obj);
	}
	LOCKOBJ_UNLOCK(obj->mutx_);

	return mc_obj != NULL ? SFW_S_OK : SFW_E_ACCESS_DENIED;
}

/*--------------------------------------------------------------------------------
	multi_unregister
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result bus_manager_multi_unregister(BusManagerObject* obj, const char* service, NotifyGroupIdType group_id)
 *	@brief		multi unregister service
 *	@param[in]	const char* service	: service name
 *	@param[in]	NotifyGroupIdType group_id : group id
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result bus_manager_multi_unregister(BusManagerObject* obj, const char* service, NotifyGroupIdType group_id)
{
	MessageContainerObject* mc_obj = NULL;

	LOCKOBJ_LOCK(obj->mutx_);
	{
		if (service == NULL)
		{
			mc_obj = obj->el_reg_unreg_.search(&obj->el_reg_unreg_, group_id);
		}
		else
		{
			EventParamServiceNameInfo serivce_info = {
				.name = service,
				.group_id = group_id,
				.user_id = 0
			};

			mc_obj = obj->mc_repo_.find(&obj->mc_repo_, service);

			obj->event_source_.update(&obj->event_source_, mc_obj != NULL ? BUS_EVENT_UNREGISTER_SERVICE : BUS_EVENT_UNREGISTER_SERVICE_ERROR, (void*)&serivce_info, NULL, NULL, NULL);
		}

		if (mc_obj != NULL) obj->multi_unregister_impl(obj, service, mc_obj);
	}
	LOCKOBJ_UNLOCK(obj->mutx_);

	return mc_obj != NULL ? SFW_S_OK : SFW_E_ACCESS_DENIED;
}

/*--------------------------------------------------------------------------------
	register_impl
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result bus_manager_register_impl(BusManagerObject* obj, const char* service, NotifyIdType id, NotifyPatternType pattern, NotifyIdKind notify_id_kind)
 *	@brief		register service
 *	@param[in]	const char* service			: service name
 *	@param[in]	NotifyGroupIdType group_id	: group id
 *	@param[in]	NotifyUserIdType user_id	: user id
 *	@param[in]	NotifyIdKind notify_id_kind	: notify id kind
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result bus_manager_register_impl(BusManagerObject* obj, const char* service, NotifyGroupIdType group_id, NotifyUserIdType user_id, NotifyIdKind notify_id_kind)
{
	Result ret = SFW_S_OK;
	MessageContainerObject* mc_obj = NULL;
	WaitNotifyObject* wait_notify_obj = NULL;

	/*
	// create/get user event object
	*/
	wait_notify_obj = obj->wn_repo_.get_instance(&obj->wn_repo_, group_id);
	if (wait_notify_obj == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : obj->wn_repo_.create_wait_notify\n", __FUNCTION__);

		return SFW_E_INIT_MEMORY_ALLOCATOR;
	}

	if (notify_id_kind == NOTIFY_ID_KIND_INDEX)
	{
		ret = wait_notify_obj->set_newversion(wait_notify_obj);
		if (SFW_FAILED(ret))
		{
			SFW_ERROR_MSG("[Error] %s : wait_notify_obj->set_newversion\n", __FUNCTION__);
			/* ret = */ obj->wn_repo_.release_instance(&obj->wn_repo_, wait_notify_obj);

			return ret;
		}
	}

	/*
	// create message container
	*/
	mc_obj = obj->mc_repo_.create(&obj->mc_repo_, &obj->event_source_, wait_notify_obj, group_id, user_id);
	if (mc_obj == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : mc_builder_.create\n", __FUNCTION__);
		/* ret = */ obj->wn_repo_.release_instance(&obj->wn_repo_, wait_notify_obj);

		return SFW_E_FAIL;
	}

	ret = mc_obj->init(mc_obj, false);
	if (SFW_FAILED(ret))
	{
		SFW_ERROR_MSG("[Error] %s : mc_obj->init\n", __FUNCTION__);
		/* ret = */ obj->wn_repo_.release_instance(&obj->wn_repo_, wait_notify_obj);

		kfree(mc_obj);

		return ret;
	}

	ret = obj->mc_repo_.add(&obj->mc_repo_, service, mc_obj);
	if (SFW_FAILED(ret))
	{
		SFW_ERROR_MSG("[Error] %s : obj->mc_repo_.add\n", __FUNCTION__);
		/* ret = */ obj->wn_repo_.release_instance(&obj->wn_repo_, wait_notify_obj);

		mc_obj->exit(mc_obj);
		kfree(mc_obj);

		return ret;
	}

	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	unregister_impl
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	void bus_manager_unregister_impl(BusManagerObject* obj, const char* service, MessageContainerObject* mc_obj)
 *	@brief		unregister service
 *	@param[in]	const char* service	: service name
 *	@param[in]	MessageContainerObject* mc_obj : message container
 *	@return		なし
 */
static void bus_manager_unregister_impl(BusManagerObject* obj, const char* service, MessageContainerObject* mc_obj)
{
	Result ret = SFW_S_OK;

	ret = obj->mc_repo_.remove(&obj->mc_repo_, service);
	if (SFW_FAILED(ret)) SFW_ERROR_MSG("[Error] %s : obj->mc_repo_.remove\n", __FUNCTION__);

	ret = obj->wn_repo_.release_instance(&obj->wn_repo_, mc_obj->wait_notify_);
	if (SFW_FAILED(ret)) SFW_ERROR_MSG("[Error] %s : obj->wn_repo_.release_instance\n", __FUNCTION__);

	mc_obj->exit(mc_obj);
	kfree(mc_obj);
}

/*--------------------------------------------------------------------------------
	multi_register_impl
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result bus_manager_multi_register_impl(BusManagerObject* obj, const char* service, NotifyIdType id, NotifyUserIdType user_id)
 *	@brief		register service
 *	@param[in]	const char* service			: service name
 *	@param[in]	NotifyGroupIdType group_id	: group id
 *	@param[in]	NotifyUserIdType user_id	: user id
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result bus_manager_multi_register_impl(BusManagerObject* obj, const char* service, NotifyGroupIdType group_id, NotifyUserIdType user_id)
{
	Result ret = SFW_S_OK;
	MessageContainerObject* mc_obj = NULL;
	WaitNotifyObject* wait_notify_obj = NULL;

	/*
	// create/get user event object
	*/
	wait_notify_obj = obj->wn_repo_.get_instance(&obj->wn_repo_, group_id);
	if (wait_notify_obj == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : obj->mc_repo_.create_wait_notify\n", __FUNCTION__);
		return SFW_E_INIT_MEMORY_ALLOCATOR;
	}

	/*
	// create message container
	*/
	mc_obj = obj->el_reg_unreg_.search(&obj->el_reg_unreg_, group_id);
	if (mc_obj != NULL)
	{
		mc_obj->attach(mc_obj);
	}
	else
	{
		mc_obj = obj->mc_repo_.get_instance(&obj->mc_repo_, &obj->event_source_, wait_notify_obj, group_id, user_id);
		if (mc_obj == NULL)
		{
			SFW_ERROR_MSG("[Error] %s : mc_builder_.create\n", __FUNCTION__);
			/* ret = */ obj->wn_repo_.release_instance(&obj->wn_repo_, wait_notify_obj);

			return SFW_E_FAIL;
		}
	}

	if (service == NULL)
	{
		ret = obj->el_reg_unreg_.add(&obj->el_reg_unreg_, mc_obj);
		if (SFW_FAILED(ret))
		{
			SFW_ERROR_MSG("[Error] %s : obj->el_reg_unreg_.add\n", __FUNCTION__);

			/* ret = */ obj->wn_repo_.release_instance(&obj->wn_repo_, wait_notify_obj);
			/* ret = */ obj->mc_repo_.release_instance(&obj->mc_repo_, mc_obj);

			return ret;
		}
	}
	else
	{
		ret = obj->mc_repo_.add(&obj->mc_repo_, service, mc_obj);
		if (SFW_FAILED(ret))
		{
			SFW_ERROR_MSG("[Error] %s : obj->mc_repo_.add\n", __FUNCTION__);

			/* ret = */ obj->wn_repo_.release_instance(&obj->wn_repo_, wait_notify_obj);
			/* ret = */ obj->mc_repo_.release_instance(&obj->mc_repo_, mc_obj);

			return ret;
		}
	}

	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	multi_unregister_impl
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result bus_manager_multi_unregister_impl(BusManagerObject* obj, const char* service, MessageContainerObject* mc_obj)
 *	@brief		unregister service
 *	@param[in]	const char* service			　　: service name
 *	@param[in]	MessageContainerObject* mc_obj : message container
 *	@return		なし
 */
static void bus_manager_multi_unregister_impl(BusManagerObject* obj, const char* service, MessageContainerObject* mc_obj)
{
	Result ret = SFW_S_OK;

	if (service == NULL)
	{
		ret = obj->el_reg_unreg_.del(&obj->el_reg_unreg_, mc_obj);
		if (SFW_FAILED(ret)) SFW_ERROR_MSG("[Error] %s : obj->el_reg_unreg_.del\n", __FUNCTION__);
	}
	else
	{
		ret = obj->mc_repo_.remove(&obj->mc_repo_, service);
		if (SFW_FAILED(ret)) SFW_ERROR_MSG("[Error] %s : obj->mc_repo_.remove\n", __FUNCTION__);
	}

	ret = obj->wn_repo_.release_instance(&obj->wn_repo_, mc_obj->wait_notify_);
	if (SFW_FAILED(ret)) SFW_ERROR_MSG("[Error] %s : obj->wn_repo_.release_instance\n", __FUNCTION__);

	ret = obj->mc_repo_.release_instance(&obj->mc_repo_, mc_obj);
	if (SFW_FAILED(ret)) SFW_ERROR_MSG("[Error] %s : obj->mc_repo_.release_instance\n", __FUNCTION__);
}

/*--------------------------------------------------------------------------------
	send_message
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result bus_manager_send_message(BusManagerObject* obj, const char* service, const char __user * msg, int msg_len)
 *	@brief		send message
 *	@param[in]	const char* name		: dst service name
 *	@param[in]	const char __user * msg	: data
 *	@param[in]	unsigned int data_size	: data size
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result bus_manager_send_message(BusManagerObject* obj, const char* service, const char __user * msg, int msg_len)
{
	Result ret = SFW_S_OK;
	MCArrary* mc_array = NULL;
	int i;

	LOCKOBJ_LOCK(obj->mutx_);

	mc_array = obj->mc_repo_.get(&obj->mc_repo_, service);

	if ((mc_array == NULL) || (mc_array->tbl_cnt == 0))
	{
		/*SFW_DEBUG_OUT("[Warning] %s : not register service=%s\n", __FUNCTION__, service);*/
		LOCKOBJ_UNLOCK(obj->mutx_);
		return SFW_E_ACCESS_DENIED;
	}

	for (i = 0; i < mc_array->tbl_cnt; i ++)
	{
		MessageContainerObject* mc_obj = mc_array->tbl[i];

		if (mc_obj != NULL)
		{
			ret |= mc_obj->push(mc_obj, msg, msg_len);
		}
	}

	LOCKOBJ_UNLOCK(obj->mutx_);

	return ret;
}

/*--------------------------------------------------------------------------------
	set_message
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result bus_manager_set_message(BusManagerObject* obj, const char* service, const char __user * msg, int msg_len)
 *	@brief		set message
 *	@param[in]	const char* name		: dst service name
 *	@param[in]	const char __user * msg	: data
 *	@param[in]	unsigned int data_size	: data size
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result bus_manager_set_message(BusManagerObject* obj, const char* service, const char __user * msg, int msg_len)
{
	Result ret = SFW_S_OK;
	MCArrary* mc_array = NULL;
	int i;

	LOCKOBJ_LOCK(obj->mutx_);

	mc_array = obj->mc_repo_.get(&obj->mc_repo_, service);

	if ((mc_array == NULL) || (mc_array->tbl_cnt == 0))
	{
		/*SFW_DEBUG_OUT("[Warning] %s : not register service=%s\n", __FUNCTION__, service);*/
		LOCKOBJ_UNLOCK(obj->mutx_);
		return SFW_E_ACCESS_DENIED;
	}

	for (i = 0; i < mc_array->tbl_cnt; i ++)
	{
		MessageContainerObject* mc_obj = mc_array->tbl[i];

		if (mc_obj != NULL)
		{
			ret |= mc_obj->set(mc_obj, msg, msg_len);
		}
	}

	LOCKOBJ_UNLOCK(obj->mutx_);

	return ret;
}

/*--------------------------------------------------------------------------------
	receive_message
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result bus_manager_receive_message(BusManagerObject* obj, const char* service, char __user * buf_ptr, int buf_len, int __user * read_len)
 *	@brief		get message
 *	@param[in]	const char* serivce		: service name
 *	@param[out]	char __user * buf_ptr	: receive buff
 *	@param[in]	int buf_len				: receive buff size
 *	@param[out]	int __user * read_len	: read size
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result bus_manager_receive_message(BusManagerObject* obj, const char* service, char __user * buf_ptr, int buf_len, int __user * read_len)
{
	Result ret = SFW_S_OK;

	LOCKOBJ_LOCK(obj->mutx_);
	{
		int len = 0;
		MessageContainerObject* mc_obj = obj->mc_repo_.find(&obj->mc_repo_, service);

		if (mc_obj == NULL)
		{
			SFW_DEBUG_OUT("[Warning] %s : not register service=%s\n", __FUNCTION__, service);
			LOCKOBJ_UNLOCK(obj->mutx_);
			return SFW_E_ACCESS_DENIED;
		}

		ret = mc_obj->pop(mc_obj, buf_ptr, buf_len, &len);

		if (SFW_FAILED(ret))
		{
			SFW_ERROR_MSG("[Error] %s : pop error [%x] readlen = %d.\n", __FUNCTION__, ret, len);
		}
		put_user((int)len, read_len);

	}
	LOCKOBJ_UNLOCK(obj->mutx_);

	return ret;
}

/*--------------------------------------------------------------------------------
	receive_message
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result bus_manager_multi_receive_message(BusManagerObject* obj, const char* service, char __user * buf_ptr, int buf_len, int __user * read_len)
 *	@brief		get message
 *	@param[in]	const char* serivce		: service name
 *	@param[out]	char __user * buf_ptr	: receive buff
 *	@param[in]	int buf_len				: receive buff size
 *	@param[out]	int __user * read_len	: read size
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result bus_manager_multi_receive_message(BusManagerObject* obj, NotifyGroupIdType group_id, char __user * buf_ptr, int buf_len, int __user * read_len)
{
	Result ret = SFW_S_OK;

	LOCKOBJ_LOCK(obj->mutx_);
	{
		int len = 0;

		// マルチの場合、el_reg_unreg_ か　mc_repo_　の両方もしくは片方に入っている、
		// まずは最初は登録数の少ない、el_reg_unreg_ から探し、無かったら mc_repo_ から探す。
		MessageContainerObject* mc_obj = obj->el_reg_unreg_.search(&obj->el_reg_unreg_, group_id);
		if (mc_obj == NULL)
		{
			// 次は mc_repo_ から探す
			mc_obj = obj->mc_repo_.find_from_id(&obj->mc_repo_, group_id);
			if (mc_obj == NULL)
			{
				SFW_ERROR_MSG("[Error] %s : not register pid=%d tid=%d\n", __FUNCTION__, (int)(group_id >> 16), (int)(group_id & 0x0000ffff));

				LOCKOBJ_UNLOCK(obj->mutx_);
				return SFW_E_ACCESS_DENIED;
			}
		}

		ret = mc_obj->pop(mc_obj, buf_ptr, buf_len, &len);

		if (SFW_FAILED(ret))
		{
			SFW_ERROR_MSG("[Error] %s : pop error [%x] readlen = %d.\n", __FUNCTION__, ret, len);
		}
		put_user((int)len, read_len);

	}
	LOCKOBJ_UNLOCK(obj->mutx_);

	return ret;
}


/*--------------------------------------------------------------------------------
	wait_message
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result bus_manager_wait_message(BusManagerObject* obj, NotifyGroupIdType group_id, void __user * ptr)
 *	@brief		wait message
 *	@param[in]	NotifyGroupIdType group_id	: group id
 *	@param[out]	void __user * ptr
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result bus_manager_wait_message(BusManagerObject* obj, NotifyGroupIdType group_id, void __user * ptr)
{
	Result ret = SFW_S_OK;
	WaitNotifyObject* wait_notify_obj = NULL;

	LOCKOBJ_LOCK(obj->mutx_);
	{
		wait_notify_obj = obj->wn_repo_.find(&obj->wn_repo_, group_id);

		if (wait_notify_obj == NULL)
		{
			SFW_ERROR_MSG("[Error] %s : obj->mc_repo_.get_wait_notify (not registered pid=%d tid=%d)\n", __FUNCTION__, (int)(group_id >> 16), (int)(group_id & 0x0000ffff));
			LOCKOBJ_UNLOCK(obj->mutx_);
			return SFW_E_ACCESS_DENIED;
		}

		wait_notify_obj->set_wait_state(wait_notify_obj, true);
	}
	LOCKOBJ_UNLOCK(obj->mutx_);

	ret = wait_notify_obj->wait(wait_notify_obj, ptr);

	LOCKOBJ_LOCK(obj->mutx_);
	{
		/*
		// 終了要求があった場合は、ここで wait_notiy を削除し終了の戻りを返す
		*/
		wait_notify_obj->set_wait_state(wait_notify_obj, false);

		if (wait_notify_obj->is_finish_request(wait_notify_obj))
		{
			ret = obj->wn_repo_.release_instance(&obj->wn_repo_, wait_notify_obj);
			if (SFW_FAILED(ret)) SFW_ERROR_MSG("[Error] %s : obj->mc_repo_.destroy_wait_notify\n", __FUNCTION__);

			// set finish code
			ret = SFW_S_INTERRUPT_QUIT;
		}
	}
	LOCKOBJ_UNLOCK(obj->mutx_);

	return ret;
}

/*--------------------------------------------------------------------------------
	subscribe_bus_status
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result bus_manager_subscribe_bus_status(BusManagerObject* obj, void __user* status)
 *	@brief		subscribe_service
 *	@param[in]	void __user* status(BusControlEventInfo*)
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result bus_manager_subscribe_bus_status(BusManagerObject* obj, void __user* status)
{
	Result ret = SFW_S_OK;
	WaitNotifyObject* wait_notify_obj = NULL;

	LOCKOBJ_LOCK(obj->mutx_);
	{
		BusControlSubscribeInfo event_info = {0};

		if (copy_from_user(&event_info, status, sizeof(BusControlSubscribeInfo)))
		{
			SFW_ERROR_MSG("[Error] %s : copy_from_user\n", __FUNCTION__);
			LOCKOBJ_UNLOCK(obj->mutx_);
			return SFW_E_FAIL;
		}

		wait_notify_obj = obj->wn_repo_.get_instance(&obj->wn_repo_, event_info.id);
		if (wait_notify_obj == NULL)
		{
			SFW_ERROR_MSG("[Error] %s : obj->mc_repo_.create_wait_notify\n", __FUNCTION__);
			LOCKOBJ_UNLOCK(obj->mutx_);
			return SFW_E_INIT_MEMORY_ALLOCATOR;
		}

		ret = obj->el_bus_status_.add(&obj->el_bus_status_, wait_notify_obj, event_info.pattern);
		if (SFW_FAILED(ret))
		{
			SFW_ERROR_MSG("[Error] %s : obj->el_bus_status_.del\n", __FUNCTION__);
			/* ret = */ obj->wn_repo_.release_instance(&obj->wn_repo_, wait_notify_obj);
		}
	}
	LOCKOBJ_UNLOCK(obj->mutx_);

	return ret;
}

/*--------------------------------------------------------------------------------
	unsubscribe_bus_status
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result bus_manager_unsubscribe_bus_status(BusManagerObject* obj, void __user* status)
 *	@brief		unsubscribe_service
 *	@param[in]	NULL
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result bus_manager_unsubscribe_bus_status(BusManagerObject* obj, void __user* status)
{
	Result ret = SFW_S_OK;
	WaitNotifyObject* wait_notify_obj = NULL;

	LOCKOBJ_LOCK(obj->mutx_);
	{
		BusControlSubscribeInfo event_info = {0};

		if (copy_from_user(&event_info, status, sizeof(BusControlSubscribeInfo)))
		{
			SFW_ERROR_MSG("[Error] %s : copy_from_user\n", __FUNCTION__);
			LOCKOBJ_UNLOCK(obj->mutx_);
			return SFW_E_FAIL;
		}

		wait_notify_obj = obj->wn_repo_.find(&obj->wn_repo_, event_info.id);
		if (wait_notify_obj == NULL)
		{
			SFW_ERROR_MSG("[Error] %s : obj->wn_repo_.find\n", __FUNCTION__);
			LOCKOBJ_UNLOCK(obj->mutx_);
			return SFW_E_FAIL;
		}

		ret = obj->el_bus_status_.del(&obj->el_bus_status_, wait_notify_obj, event_info.pattern);
		if (SFW_FAILED(ret))
		{
			SFW_ERROR_MSG("[Error] %s : obj->el_bus_status_.del\n", __FUNCTION__);
			// LOCKOBJ_UNLOCK(obj->mutx_);
			// return SFW_E_FAIL;
		}

		ret = obj->wn_repo_.release_instance(&obj->wn_repo_, wait_notify_obj);
		if (SFW_FAILED(ret))
		{
			SFW_ERROR_MSG("[Error] %s : obj->wn_repo_.release_instance\n", __FUNCTION__);
			// LOCKOBJ_UNLOCK(obj->mutx_);
			// return SFW_E_FAIL;
		}
	}
 	LOCKOBJ_UNLOCK(obj->mutx_);

	return ret;
}

/*--------------------------------------------------------------------------------
	bus_manager_get_service
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result bus_manager_get_service(BusManagerObject* obj, void __user* status)
 *	@brief		get service name
 *	@param[out]	void __user* status(char*)
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result bus_manager_get_service(BusManagerObject* obj, void __user* status)
{
	Result ret = SFW_S_OK;

	LOCKOBJ_LOCK(obj->mutx_);
	{
		ret = obj->mc_repo_.get_service(&obj->mc_repo_, INT_MAX, (char*)status);
	}
	LOCKOBJ_UNLOCK(obj->mutx_);

	return ret;
}

/*--------------------------------------------------------------------------------
	bus_manager_get_service_count
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result bus_manager_get_service_count(BusManagerObject* obj, void __user* status)
 *	@brief		get service count
 *	@param[out]	void __user* status(char*)
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result bus_manager_get_service_count(BusManagerObject* obj, void __user* status)
{
	Result ret = SFW_S_OK;

	LOCKOBJ_LOCK(obj->mutx_);
	{
		ret = obj->mc_repo_.get_service_count(&obj->mc_repo_, (char*)status);
	}
 	LOCKOBJ_UNLOCK(obj->mutx_);

	return ret;
}

/*--------------------------------------------------------------------------------
    bus_manager_get_service_limit
 --------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result bus_manager_get_service_limit(BusManagerObject* obj, void __user* status)
 *	@brief		get service name (limit version)
 *	@param[out]	void __user* status(char*)
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result bus_manager_get_service_limit(BusManagerObject* obj, void __user* status)
{
	Result ret = SFW_S_OK;

	LOCKOBJ_LOCK(obj->mutx_);
	{
		BusControlBusStatus param = {0};

		if (copy_from_user(&param, status, sizeof(BusControlBusStatus)))
		{
			SFW_ERROR_MSG("[Error] %s : copy_from_user\n", __FUNCTION__);
			LOCKOBJ_UNLOCK(obj->mutx_);
			return SFW_E_FAIL;
		}

		ret = obj->mc_repo_.get_service(&obj->mc_repo_, param.max_service_cnt, param.buf_ptr);
	}
	LOCKOBJ_UNLOCK(obj->mutx_);

	return ret;
}

/*--------------------------------------------------------------------------------
	bus_manager_get_store_service_count
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result bus_manager_get_store_service_count(BusManagerObject* obj, void __user* status)
 *	@brief		get service count from store
 *	@param[out]	void __user* status(char*)
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result bus_manager_get_store_service_count(BusManagerObject* obj, void __user* status)
{
	Result ret = SFW_S_OK;

	LOCKOBJ_LOCK(obj->mutx_);
	{
		ret = obj->el_bus_status_.get_status_count(&obj->el_bus_status_, (char*)status);
	}
	LOCKOBJ_UNLOCK(obj->mutx_);

	return ret;
}

/*--------------------------------------------------------------------------------
	bus_manager_get_store_service_status
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result bus_manager_get_store_service_statu(BusManagerObject* obj, void __user* status)
 *	@brief		get service status form store
 *	@param[out]	void __user* status(char*)
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result bus_manager_get_store_service_status(BusManagerObject* obj, void __user* status)
{
	Result ret = SFW_S_OK;

	LOCKOBJ_LOCK(obj->mutx_);
	{
		BusControlBusStatus param = {0};

		if (copy_from_user(&param, status, sizeof(BusControlBusStatus)))
		{
			SFW_ERROR_MSG("[Error] %s : copy_from_user\n", __FUNCTION__);
			LOCKOBJ_UNLOCK(obj->mutx_);
			return SFW_E_FAIL;
		}

		ret = obj->el_bus_status_.get_status(&obj->el_bus_status_, param.max_service_cnt, param.buf_ptr);
	}
	LOCKOBJ_UNLOCK(obj->mutx_);

	return ret;
}

/*--------------------------------------------------------------------------------
	bus_manager_get_bus_info
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result bus_manager_get_bus_info(BusManagerObject* obj, void __user* status)
 *	@brief		get service name
 *	@param[out]	void __user* status(char*)
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result bus_manager_get_bus_info(BusManagerObject* obj, void __user* status)
{
	Result ret = SFW_S_OK;

	LOCKOBJ_LOCK(obj->mutx_);
	{
		ret = obj->mc_repo_.get_bus_info(&obj->mc_repo_, INT_MAX, (char*)status);
	}
	LOCKOBJ_UNLOCK(obj->mutx_);

	return ret;
}

/*--------------------------------------------------------------------------------
	bus_manager_get_bus_info_limit
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result bus_manager_get_bus_info_limit(BusManagerObject* obj, void __user* status)
 *	@brief		get service name
 *	@param[out]	void __user* status(char*)
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result bus_manager_get_bus_info_limit(BusManagerObject* obj, void __user* status)
{
	Result ret = SFW_S_OK;

	LOCKOBJ_LOCK(obj->mutx_);
	{
		BusControlBusStatus param = {0};

		if (copy_from_user(&param, status, sizeof(BusControlBusStatus)))
		{
			SFW_ERROR_MSG("[Error] %s : copy_from_user\n", __FUNCTION__);
			LOCKOBJ_UNLOCK(obj->mutx_);
			return SFW_E_FAIL;
		}

		ret = obj->mc_repo_.get_bus_info(&obj->mc_repo_, param.max_service_cnt, param.buf_ptr);
	}
	LOCKOBJ_UNLOCK(obj->mutx_);

	return ret;
}

/*--------------------------------------------------------------------------------
	bus_manager_get_device_name
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result bus_manager_get_device_name(BusManagerObject* obj, void __user* status)
 *	@brief		get device name
 *	@note 		NULL 終端はコピーしない、NULL 終端は含めず最大 HOST_NAME_LEN_MAX 分コピーするので、
 *				０フィルされていることと、アプリから渡されるバッファは HOST_NAME_LEN_MAX + 1 以上であることを期待する。
 *	@param[out]	void __user* status(char*)
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result bus_manager_get_device_name(BusManagerObject* obj, void __user* status)
{
	Result ret = SFW_S_OK;

	LOCKOBJ_LOCK(obj->mutx_);
	{
		const char* device_name = get_hostname();
		int len = strlen(device_name) < HOST_NAME_LEN_MAX ? strlen(device_name) : HOST_NAME_LEN_MAX;

		if (copy_to_user(status, device_name, len))
		{
			SFW_ERROR_MSG("[Error] %s : copy_to_user\n", __FUNCTION__);
			LOCKOBJ_UNLOCK(obj->mutx_);
			return SFW_E_FAIL;
		}
	}
	LOCKOBJ_UNLOCK(obj->mutx_);

	return ret;
}

/*--------------------------------------------------------------------------------
	bus_manager_get_max_queue_size
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result bus_manager_get_max_queue_size(BusManagerObject* obj, void __user* status)
 *	@brief		get max queue size
 *	@param[out]	void __user* status(char*)
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result bus_manager_get_max_queue_size(BusManagerObject* obj, void __user* status)
{
	Result ret = SFW_S_OK;

	LOCKOBJ_LOCK(obj->mutx_);
	{
		long max_queue_size = get_qmaxsize() * 1024;
		if (copy_to_user(status, &max_queue_size, sizeof(max_queue_size)))
		{
			SFW_ERROR_MSG("[Error] %s : copy_to_user\n", __FUNCTION__);
			LOCKOBJ_UNLOCK(obj->mutx_);
			return SFW_E_FAIL;
		}
	}
	LOCKOBJ_UNLOCK(obj->mutx_);

	return ret;
}

/*--------------------------------------------------------------------------------
	bus_manager_start_log
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result bus_manager_start_log(BusManagerObject* obj, void __user* status)
 *	@brief		start_log
 *	@param[in]	void __user* status(BusControlSubscribeInfo*)
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result bus_manager_start_log(BusManagerObject* obj, void __user* status)
{
	Result ret = SFW_S_OK;
	WaitNotifyObject* wait_notify_obj = NULL;
	BusControlSubscribeInfo event_info = {0};

	LOCKOBJ_LOCK(obj->mutx_);

	if (obj->el_dlt_logger_.is_start(&obj->el_dlt_logger_))
	{
		obj->el_dlt_logger_.exit((EventListenerObject*)&obj->el_dlt_logger_);
	}

	if (copy_from_user(&event_info, status, sizeof(BusControlSubscribeInfo)))
	{
		SFW_ERROR_MSG("[Error] %s : copy_from_user\n", __FUNCTION__);
		LOCKOBJ_UNLOCK(obj->mutx_);
		return SFW_E_FAIL;
	}

	wait_notify_obj = obj->wn_repo_.get_instance(&obj->wn_repo_, event_info.id);
	if (wait_notify_obj == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : obj->wn_repo_.get_instance\n", __FUNCTION__);
		LOCKOBJ_UNLOCK(obj->mutx_);
		return SFW_E_INIT_MEMORY_ALLOCATOR;
	}

	ret = obj->el_dlt_logger_.init(&obj->el_dlt_logger_, wait_notify_obj, event_info.pattern);
	if (SFW_FAILED(ret))
	{
		SFW_ERROR_MSG("[Error] %s : obj->el_dlt_logger_.init\n", __FUNCTION__);
		/* ret = */ obj->wn_repo_.release_instance(&obj->wn_repo_, wait_notify_obj);
		// LOCKOBJ_UNLOCK(obj->mutx_);
		// return SFW_E_FAIL;
	}

	LOCKOBJ_UNLOCK(obj->mutx_);

 	return ret;
}

/*--------------------------------------------------------------------------------
	bus_manager_stop_log
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result bus_manager_stop_log(BusManagerObject* obj, void __user* status)
 *	@brief		stop log
 *	@param[in]	NULL
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result bus_manager_stop_log(BusManagerObject* obj, void __user* status/* NULL */)
{
	Result ret = SFW_S_OK;

	LOCKOBJ_LOCK(obj->mutx_);

	if (obj->el_dlt_logger_.is_start(&obj->el_dlt_logger_))
	{
		ret = obj->wn_repo_.release_instance(&obj->wn_repo_, obj->el_dlt_logger_.wait_notify_);
		if (SFW_FAILED(ret))
		{
			SFW_ERROR_MSG("[Error] %s : obj->wn_repo_.release_instance\n", __FUNCTION__);
			// LOCKOBJ_UNLOCK(obj->mutx_);
			// return SFW_E_FAIL;
		}

		obj->el_dlt_logger_.exit((EventListenerObject*)&obj->el_dlt_logger_);
	}

	LOCKOBJ_UNLOCK(obj->mutx_);

	return ret;
}

/*--------------------------------------------------------------------------------
	bus_manager_get_log
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result bus_manager_get_log(BusManagerObject* obj, void __user* status)
 *	@brief		stop log
 *	@param[in]	NULL
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result bus_manager_get_log(BusManagerObject* obj, void __user* status)
{
	Result ret = SFW_S_OK;

	LOCKOBJ_LOCK(obj->mutx_);
	{
		BusControlLogReadInfo log_param = {0};

		if (copy_from_user(&log_param, status, sizeof(BusControlLogReadInfo)))
		{
			SFW_ERROR_MSG("[Error] %s : copy_from_user\n", __FUNCTION__);
			LOCKOBJ_UNLOCK(obj->mutx_);
			return SFW_E_FAIL;
		}

		ret = obj->el_dlt_logger_.get(&obj->el_dlt_logger_, log_param.buf_ptr, log_param.buf_len, log_param.read_len);
	}
	LOCKOBJ_UNLOCK(obj->mutx_);

	return ret;
}

/*--------------------------------------------------------------------------------
	update_userid
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result bus_manager_update_waitnotify(BusManagerObject* obj, NotifyGroupIdType group_id)
 *	@brief		update userid
 *	@param[in]	NotifyGroupIdType group_id	: group id
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result bus_manager_update_waitnotify(BusManagerObject* obj, NotifyGroupIdType group_id)
{
	Result ret = SFW_S_OK;
	WaitNotifyObject* wait_notify_obj = NULL;

	LOCKOBJ_LOCK(obj->mutx_);
	{
		wait_notify_obj = obj->wn_repo_.find(&obj->wn_repo_, group_id);
		if (wait_notify_obj == NULL)
		{
			SFW_ERROR_MSG("[Error] %s : obj->mc_repo_.get_wait_notify (not registered pid=%d tid=%d)\n", __FUNCTION__, (int)(group_id >> 16), (int)(group_id & 0x0000ffff));
			LOCKOBJ_UNLOCK(obj->mutx_);
			return SFW_E_ACCESS_DENIED;
		}

		wait_notify_obj->set_newversion(wait_notify_obj);
	}
	LOCKOBJ_UNLOCK(obj->mutx_);

	return ret;
}

/*--------------------------------------------------------------------------------
	subscribe_service_status
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result bus_manager_subscribe_service_status(BusManagerObject* obj, void __user* status)
 *	@brief		subscribe_service_status
 *	@param[in]	void __user* status(BusControlEventInfo*)
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result bus_manager_subscribe_service_status(BusManagerObject* obj, void __user* status)
{
	Result ret = SFW_S_OK;
	WaitNotifyObject* wait_notify_obj = NULL;

	LOCKOBJ_LOCK(obj->mutx_);
	{
		BusControlSubscribeInfo event_info = {0};

		if (copy_from_user(&event_info, status, sizeof(BusControlSubscribeInfo)))
		{
			SFW_ERROR_MSG("[Error] %s : copy_from_user\n", __FUNCTION__);
			LOCKOBJ_UNLOCK(obj->mutx_);
			return SFW_E_FAIL;
		}

		wait_notify_obj = obj->wn_repo_.get_instance(&obj->wn_repo_, event_info.id);
		if (wait_notify_obj == NULL)
		{
			SFW_ERROR_MSG("[Error] %s : obj->mc_repo_.create_wait_notify\n", __FUNCTION__);
			LOCKOBJ_UNLOCK(obj->mutx_);
			return SFW_E_INIT_MEMORY_ALLOCATOR;
		}

		ret = obj->el_service_status_.add(&obj->el_service_status_, wait_notify_obj, event_info.pattern);
		if (SFW_FAILED(ret))
		{
			SFW_ERROR_MSG("[Error] %s : obj->el_service_status_.del\n", __FUNCTION__);
			/* ret = */ obj->wn_repo_.release_instance(&obj->wn_repo_, wait_notify_obj);
		}
	}
	LOCKOBJ_UNLOCK(obj->mutx_);

	return ret;
}

/*--------------------------------------------------------------------------------
	unsubscribe_service_status
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result bus_manager_unsubscribe_service_status(BusManagerObject* obj, void __user* status)
 *	@brief		unsubscribe_service
 *	@param[in]	NULL
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result bus_manager_unsubscribe_service_status(BusManagerObject* obj, void __user* status)
{
	Result ret = SFW_S_OK;
	WaitNotifyObject* wait_notify_obj = NULL;

	LOCKOBJ_LOCK(obj->mutx_);
	{
		BusControlSubscribeInfo event_info = {0};

		if (copy_from_user(&event_info, status, sizeof(BusControlSubscribeInfo)))
		{
			SFW_ERROR_MSG("[Error] %s : copy_from_user\n", __FUNCTION__);
			LOCKOBJ_UNLOCK(obj->mutx_);
			return SFW_E_FAIL;
		}

		wait_notify_obj = obj->wn_repo_.find(&obj->wn_repo_, event_info.id);
		if (wait_notify_obj == NULL)
		{
			SFW_ERROR_MSG("[Error] %s : obj->wn_repo_.find\n", __FUNCTION__);
			LOCKOBJ_UNLOCK(obj->mutx_);
			return SFW_E_FAIL;
		}

		ret = obj->el_service_status_.del(&obj->el_service_status_, wait_notify_obj, event_info.pattern);
		if (SFW_FAILED(ret))
		{
			SFW_ERROR_MSG("[Error] %s : obj->el_service_status_.del\n", __FUNCTION__);
			// LOCKOBJ_UNLOCK(obj->mutx_);
			// return SFW_E_FAIL;
		}

		ret = obj->wn_repo_.release_instance(&obj->wn_repo_, wait_notify_obj);
		if (SFW_FAILED(ret))
		{
			SFW_ERROR_MSG("[Error] %s : obj->wn_repo_.release_instance\n", __FUNCTION__);
			// LOCKOBJ_UNLOCK(obj->mutx_);
			// return SFW_E_FAIL;
		}
	}
	LOCKOBJ_UNLOCK(obj->mutx_);

	return ret;
}

/*--------------------------------------------------------------------------------
	get_self_device_service_buff_size
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result bus_manager_get_service_status_buff_size(BusManagerObject* obj, void __user* status)
 *	@brief		get service count from store
 *	@param[out]	void __user* status(char*)
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result bus_manager_get_service_status_buff_size(BusManagerObject* obj, void __user* status)
{
	Result ret = SFW_S_OK;

	LOCKOBJ_LOCK(obj->mutx_);
	{
		BusControlSerivceStatus param = {0};

		if (copy_from_user(&param, status, sizeof(BusControlSerivceStatus)))
		{
			SFW_ERROR_MSG("[Error] %s : copy_from_user\n", __FUNCTION__);
			LOCKOBJ_UNLOCK(obj->mutx_);
			return SFW_E_FAIL;
		}

		ret = obj->el_service_status_.get_buff_size(&obj->el_service_status_, param.status_kind & SERVICE_STATUS_KIND_SELF_DEVICE ? get_hostname() : NULL, param.status_kind, param.buf_ptr);
	}
	LOCKOBJ_UNLOCK(obj->mutx_);

	return ret;
}

/*--------------------------------------------------------------------------------
	bus_manager_get_service_status
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	Result bus_manager_get_service_status(BusManagerObject* obj, void __user* status)
 *	@brief		get service status form store
 *	@param[out]	void __user* status(char*)
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result bus_manager_get_service_status(BusManagerObject* obj, void __user* status)
{
	Result ret = SFW_S_OK;

	LOCKOBJ_LOCK(obj->mutx_);
	{
		BusControlSerivceStatus param = {0};

		if (copy_from_user(&param, status, sizeof(BusControlSerivceStatus)))
		{
			SFW_ERROR_MSG("[Error] %s : copy_from_user\n", __FUNCTION__);
			LOCKOBJ_UNLOCK(obj->mutx_);
			return SFW_E_FAIL;
		}

		ret = obj->el_service_status_.get_status(&obj->el_service_status_, param.status_kind & SERVICE_STATUS_KIND_SELF_DEVICE ? get_hostname() : NULL, param.status_kind, param.max_buff_len, param.buf_ptr);
	}
	LOCKOBJ_UNLOCK(obj->mutx_);

	return ret;
}

/*--------------------------------------------------------------------------------
	InitializeBusManagerObject
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result InitializeBusManagerObject(BusManagerObject* obj)
 *	@brief		constructor
 *	@param		なし
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
Result InitializeBusManagerObject(BusManagerObject* obj)
{
	Result ret = SFW_S_OK;

	if (obj == NULL) return SFW_E_INVALIDARG;

	mutex_init(&obj->mutx_);

	ret = InitializeMCRepositoryObject(&obj->mc_repo_);
	if (SFW_FAILED(ret)) return SFW_E_FAIL;

	ret = InitializeWNRepositoryObject(&obj->wn_repo_);
	if (SFW_FAILED(ret)) return SFW_E_FAIL;

	ret = InitializeEventSourceObject(&obj->event_source_);
	if (SFW_FAILED(ret)) return SFW_E_FAIL;

	obj->register_service					= bus_manager_register;
	obj->unregister_service					= bus_manager_unregister;
	obj->multi_register_service				= bus_manager_multi_register;
	obj->multi_unregister_service			= bus_manager_multi_unregister;

	obj->register_impl						= bus_manager_register_impl;
	obj->unregister_impl					= bus_manager_unregister_impl;
	obj->multi_register_impl				= bus_manager_multi_register_impl;
	obj->multi_unregister_impl				= bus_manager_multi_unregister_impl;

	obj->send_message						= bus_manager_send_message;
	obj->set_message 						= bus_manager_set_message;
	obj->receive_message					= bus_manager_receive_message;
	obj->multi_receive_message				= bus_manager_multi_receive_message;
	obj->wait_message						= bus_manager_wait_message;

	obj->subscribe_bus_status				= bus_manager_subscribe_bus_status;
	obj->unsubscribe_bus_status				= bus_manager_unsubscribe_bus_status;
	obj->get_service						= bus_manager_get_service;
	obj->get_service_limit					= bus_manager_get_service_limit;
	obj->get_service_count					= bus_manager_get_service_count;
	obj->get_bus_info						= bus_manager_get_bus_info;
	obj->get_bus_info_limit					= bus_manager_get_bus_info_limit;

	obj->get_store_service_count			= bus_manager_get_store_service_count;
	obj->get_store_service_status			= bus_manager_get_store_service_status;

	obj->get_device_name					= bus_manager_get_device_name;
	obj->get_max_queue_size					= bus_manager_get_max_queue_size;

	obj->start_log							= bus_manager_start_log;
	obj->stop_log							= bus_manager_stop_log;
	obj->get_log							= bus_manager_get_log;

	obj->subscribe_service_status			= bus_manager_subscribe_service_status;
	obj->unsubscribe_service_status			= bus_manager_unsubscribe_service_status;
	obj->get_service_status_buff_size		= bus_manager_get_service_status_buff_size;
	obj->get_service_status					= bus_manager_get_service_status;

	obj->update_waitnotify					= bus_manager_update_waitnotify;

	/*
	// setup event source & event listener
	*/
	ret = InitializeELLoggerObject(&obj->el_logger_);
	if (SFW_FAILED(ret)) return SFW_E_FAIL;

	ret = InitializeELDltLoggerObject(&obj->el_dlt_logger_);
	if (SFW_FAILED(ret)) return SFW_E_FAIL;

	ret = InitializeELWaitNotifyObject(&obj->el_wait_notify_);
	if (SFW_FAILED(ret)) return SFW_E_FAIL;

	ret = InitializeELBusStatusObject(&obj->el_bus_status_);
	if (SFW_FAILED(ret)) return SFW_E_FAIL;

	ret = InitializeELRegUnregObject(&obj->el_reg_unreg_);
	if (SFW_FAILED(ret)) return SFW_E_FAIL;

	ret = InitializeELServiceStatusObject(&obj->el_service_status_);
	if (SFW_FAILED(ret)) return SFW_E_FAIL;

	// register listener
	if (get_dmesg()) obj->event_source_.register_obj(&obj->event_source_, (EventListenerObject*)&obj->el_logger_);
	if (get_dlt())	 obj->event_source_.register_obj(&obj->event_source_, (EventListenerObject*)&obj->el_dlt_logger_);
	obj->event_source_.register_obj(&obj->event_source_, (EventListenerObject*)&obj->el_wait_notify_);
	obj->event_source_.register_obj(&obj->event_source_, (EventListenerObject*)&obj->el_bus_status_);
	obj->event_source_.register_obj(&obj->event_source_, (EventListenerObject*)&obj->el_reg_unreg_);
	obj->event_source_.register_obj(&obj->event_source_, (EventListenerObject*)&obj->el_service_status_);

	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	FinalizeBusManagerObject
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result FinalizeBusManagerObject(BusManagerObject* obj)
 *	@brief		destructor
 *	@param		なし
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
Result FinalizeBusManagerObject(BusManagerObject* obj)
{
	Result ret = SFW_S_OK;

	if (obj == NULL) return SFW_E_INVALIDARG;

	LOCKOBJ_LOCK(obj->mutx_);
	{
		FinalizeEventSourceObject(&obj->event_source_);
		FinalizeWNRepositoryObject(&obj->wn_repo_);
		FinalizeMCRepositoryObject(&obj->mc_repo_);
	}
	LOCKOBJ_UNLOCK(obj->mutx_);

	return ret;
}
