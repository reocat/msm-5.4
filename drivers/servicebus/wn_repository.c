/********************************************************************************/
/** @file		wn_repository.c
	@brief		wn repository
	@note		Copyright (C) 2015 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2015/08/20
	@version	1.0
*********************************************************************************/
/*--------------------------------------------------------------------------------
	Include
--------------------------------------------------------------------------------*/
#include "servicebus_common.h"													// service bus common include
#include "wn_repository.h"														// wn repository include
#include "wait_notify.h"														// wait notify include

/*--------------------------------------------------------------------------------
	Local Value
--------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------
	get_instance
--------------------------------------------------------------------------------*/
/**
 *	@fn			WaitNotifyObject* wn_repository_get_instance(WNRepositoryObject* obj, NotifyGroupIdType id)
 *	@brief		IDからWaitNotifyObjectを取得する、無い場合は作成する
 *	@param[in]	NotifyGroupIdType id
 *	@return		WaitNotifyObject* !NULL...OK, NULL...NG
 */
static WaitNotifyObject* wn_repository_get_instance(WNRepositoryObject* obj, NotifyGroupIdType id)
{
	WaitNotifyObject* wait_notify_obj = obj->find(obj, id);

	if (wait_notify_obj != NULL)
	{
		wait_notify_obj->attach(wait_notify_obj);
		return wait_notify_obj;
	}

	return obj->create(obj, id);;
}

/*--------------------------------------------------------------------------------
	release_instance
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result wn_repository_release_instance(WNRepositoryObject* obj, WaitNotifyObject* wait_notify)
 *	@brief		WaitNotifyObject を開放する、最後の１つの場合はオブジェクトを削除する
 *	@param[in]	WaitNotifyObject* wait_notify
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result wn_repository_release_instance(WNRepositoryObject* obj, WaitNotifyObject* wait_notify)
{
	Result ret = SFW_S_OK;

	if (wait_notify == NULL) return SFW_E_INVALIDARG;
	if (wait_notify->detach(wait_notify) != 0) return SFW_S_OK;

	/*
	// wait 中どうか聞く、wait でない場合はオブジェクトを消し、
	// wait 中であれば終了要求をしかける
	*/
	if (wait_notify->is_wait(wait_notify))
	{
		wait_notify->request_finish(wait_notify);
	}
	else
	{
		ret = obj->destroy(obj, wait_notify);
	}

	return ret;
}

/*--------------------------------------------------------------------------------
	create
--------------------------------------------------------------------------------*/
/**
 *	@fn			WaitNotifyObject* wn_repository_create(WNRepositoryObject* obj, NotifyGroupIdType id)
 *	@brief		IDからWaitNotifyObjectを生成する
 *	@param[in]	NotifyGroupIdType id
 *	@return		WaitNotifyObject* !NULL...OK, NULL...NG
 */
static WaitNotifyObject* wn_repository_create(WNRepositoryObject* obj, NotifyGroupIdType id)
{
	WaitNotifyList* waitnotify_list = NULL;
	WaitNotifyObject* wait_notify_obj = NULL;
	Result ret = SFW_S_OK;

	waitnotify_list = kmalloc(sizeof(WaitNotifyList), GFP_KERNEL);
	if (waitnotify_list == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : new WaitNotifyList.\n", __FUNCTION__);
		return NULL;
	}

	wait_notify_obj = kmalloc(sizeof(WaitNotifyObject), GFP_KERNEL);
	if (wait_notify_obj == NULL)
	{
		kfree(waitnotify_list);
		SFW_ERROR_MSG("[Error] %s : new WaitNotifyObject.\n", __FUNCTION__);
		return NULL;
	}

	ret = InitializeWaitNotifyObject(wait_notify_obj, id);
	if (SFW_FAILED(ret))
	{
		kfree(wait_notify_obj);
		kfree(waitnotify_list);
		SFW_ERROR_MSG("[Error] %s : InitializeWaitNotifyObject.\n", __FUNCTION__);
		return NULL;
	}

	waitnotify_list->wait_notity = wait_notify_obj;

	list_add(&waitnotify_list->list, &obj->waitnotify_rep_.list);

	return wait_notify_obj;
}

/*--------------------------------------------------------------------------------
	destroy
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result wn_repository_destroy(WNRepositoryObject* obj, WaitNotifyObject* wait_notify)
 *	@brief		destroy
 *	@param[in]	WaitNotifyObject* wait_notify
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result wn_repository_destroy(WNRepositoryObject* obj, WaitNotifyObject* wait_notify)
{
	Result ret = SFW_E_FAIL;
	struct list_head* listptr = NULL;

	// パラメータのチェックは呼び出し元で確認済み
	// if (wait_notify == NULL) return SFW_E_INVALIDARG;

	list_for_each(listptr, &obj->waitnotify_rep_.list)
	{
		WaitNotifyList* waitnotify_list = list_entry(listptr, struct _WaitNotifyList, list);

		if (waitnotify_list->wait_notity == wait_notify)
		{
			ret = SFW_S_OK;
			list_del(&waitnotify_list->list);

			FinalizeWaitNotifyObject(waitnotify_list->wait_notity);

			kfree(waitnotify_list->wait_notity);
			kfree(waitnotify_list);

			break;
		}
	}

	return ret;
}

/*--------------------------------------------------------------------------------
	exit
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result wn_repository_exit(WNRepositoryObject* obj)
 *	@brief		exit
 *	@param		なし
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result wn_repository_exit(WNRepositoryObject* obj)
{
	while (!list_empty(&obj->waitnotify_rep_.list))
	{
		WaitNotifyList* waitnotify_list = list_entry(obj->waitnotify_rep_.list.next, struct _WaitNotifyList, list);

		if (waitnotify_list->wait_notity != NULL)
		{
			list_del(&waitnotify_list->list);

			FinalizeWaitNotifyObject(waitnotify_list->wait_notity);
			kfree(waitnotify_list->wait_notity);

			kfree(waitnotify_list);
		}
	}

	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	find
--------------------------------------------------------------------------------*/
/**
 *	@fn			WaitNotifyObject* wn_repository_destroy(WNRepositoryObject* obj, NotifyGroupIdType id)
 *	@brief		id 2 WaitNotifyObject
 *	@param[in]	NotifyGroupIdType id
 *	@return		WaitNotifyObject* !NULL...OK, NULL...NG
 */
static WaitNotifyObject* wn_repository_find(WNRepositoryObject* obj, NotifyGroupIdType id)
{
	WaitNotifyObject* wait_notity_obj = NULL;
	struct list_head* listptr = NULL;

	list_for_each(listptr, &obj->waitnotify_rep_.list)
	{
		WaitNotifyList* waitnotify_list = list_entry(listptr, struct _WaitNotifyList, list);

		if (waitnotify_list->wait_notity->id_ == id)
		{
			wait_notity_obj = waitnotify_list->wait_notity;
			break;
		}
	}

	return wait_notity_obj;
}

/*--------------------------------------------------------------------------------
	InitializeWNRepositoryObject
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result InitializeWNRepositoryObject(WNRepositoryObject* obj)
 *	@brief		constructor
 *	@param		なし
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
Result InitializeWNRepositoryObject(WNRepositoryObject* obj)
{
	if (obj == NULL) return SFW_E_INVALIDARG;

	memset(&obj->waitnotify_rep_, 0, sizeof(obj->waitnotify_rep_));
	INIT_LIST_HEAD(&obj->waitnotify_rep_.list);

	obj->get_instance		= wn_repository_get_instance;
	obj->release_instance	= wn_repository_release_instance;
	obj->find				= wn_repository_find;
	obj->exit				= wn_repository_exit;

	obj->create				= wn_repository_create;
	obj->destroy			= wn_repository_destroy;

	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	FinalizeWNRepositoryObject
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result FinalizeWNRepositoryObject(WNRepositoryObject* pObj)
 *	@brief		destructor
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
Result FinalizeWNRepositoryObject(WNRepositoryObject* obj)
{
	if (obj == NULL) return SFW_E_INVALIDARG;
	return obj->exit(obj);
}
