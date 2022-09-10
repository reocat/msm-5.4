/********************************************************************************/
/** @file		mc_repository.c
	@brief		メッセージコンテナのリポジトリ
	@note		Copyright (C) 2013 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2013/09/30
	@version	1.0
*********************************************************************************/
/*--------------------------------------------------------------------------------
	Include
--------------------------------------------------------------------------------*/
#include "servicebus_common.h"													// service bus common include
#include <linux/list.h>															// linux linked list
#include "mc_repository.h"														// mc container include
#include "message_container.h"													// message container include
#include "name_parser.h"														// name parser include
#include "message_queue.h"														// message queue include
#include "wait_notify.h"														// wait notify include
#include "event_source.h"														// event source include

/*--------------------------------------------------------------------------------
	Local Value
--------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------
	create
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	MessageContainerObject* mc_repository_create(MCRepositoryObject* obj, EventSourceObject* event_source_obj, WaitNotifyObject* wait_notify_obj, NotifyGroupIdType group_id, NotifyUserIdType user_id)
 *	@brief		MC オブジェクトを生成する
 *	@param[in]	EventSourceObject* event_source_obj
 *	@param[in]	WaitNotifyObject* wait_notify_obj
 *	@param[in]	NotifyGroupIdType group_id
 *	@param[in]	NotifyUserIdType user_id
 *	@return		MessageContainerObject* !NULL...OK, NULL...NG
 */
static MessageContainerObject* mc_repository_create(MCRepositoryObject* obj, EventSourceObject* event_source_obj, WaitNotifyObject* wait_notify_obj, NotifyGroupIdType group_id, NotifyUserIdType user_id)
{
	/*
	// create object
	*/
	MessageContainerObject* mc_obj = obj->create_mc(obj);
	MessageQueueObject* 	mq_obj = obj->create_mq(obj);

	/*
	// check object
	*/
	if ((mc_obj == NULL) || (mq_obj == NULL))
	{
		if (mc_obj != NULL)
		{
			mc_obj->exit(mc_obj);
			kfree(mc_obj);
		}

		if (mq_obj != NULL)
		{
			mq_obj->exit(mq_obj);
			kfree(mq_obj);
		}

		return NULL;
	}

	/*
	// set dependency
	*/
	mc_obj->msg_queue_ 		= mq_obj;
	mc_obj->event_source_ 	= event_source_obj;
	mc_obj->wait_notify_ 	= wait_notify_obj;
	mc_obj->group_id_		= group_id;
	mc_obj->user_id_		= user_id;

	return mc_obj;
}

/*--------------------------------------------------------------------------------
	mc_repository_create_mc
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	MessageContainerObjectType* mc_repository_create_mc(MCRepositoryObject* obj)
 *	@brief		create sync wait object
 *	@return		MessageContainerObjectType* !NULL...OK, NULL...NG
 */
static MessageContainerObjectType* mc_repository_create_mc(MCRepositoryObject* obj)
{
	MessageContainerObject* target_obj = NULL;
	Result ret = SFW_S_OK;

	target_obj = kmalloc(sizeof(MessageContainerObjectType), GFP_KERNEL);
	if (target_obj == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : allocate memory\n", __FUNCTION__);
		return NULL;
	}

	ret = InitializeMessageContainerObject(target_obj);
	if (SFW_FAILED(ret))
	{
		SFW_ERROR_MSG("[Error] %s : InitializeMessageContainerObject\n", __FUNCTION__);
		kfree(target_obj);
		return NULL;
	}

	return target_obj;
}

/*--------------------------------------------------------------------------------
	mc_repository_create_mq
--------------------------------------------------------------------------------*/
/**
 *	@fn		 	MessageQueueObject* mc_repository_create_mq(MCRepositoryObject* obj)
 *	@brief		create message queue
 *	@return		MessageQueueObject* !NULL...OK, NULL...NG
 */
static MessageQueueObject* mc_repository_create_mq(MCRepositoryObject* obj)
{
	MessageQueueObject* target_obj = NULL;
	Result ret = SFW_S_OK;

	target_obj = kmalloc(sizeof(MessageQueueObject), GFP_KERNEL);
	if (target_obj == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : allocate memory\n", __FUNCTION__);
		return NULL;
	}

	ret = InitializeMessageQueueObject(target_obj);
	if (SFW_FAILED(ret))
	{
		SFW_ERROR_MSG("[Error] %s : InitializeMessageQueueObject\n", __FUNCTION__);
		kfree(target_obj);
		return NULL;
	}

	return target_obj;
}

/*--------------------------------------------------------------------------------
	add
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result mc_repository_add(MCRepositoryObject* obj, const char* service_name, MessageContainerObjectType* mc_obj)
 *	@brief		MCを追加する、既存の場合はエラーとする
 *	@param[in]	const char* service_name
 *	@param[in]	MessageContainerObjectType* mc_obj
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result mc_repository_add(MCRepositoryObject* obj, const char* service_name, MessageContainerObjectType* mc_obj)
{
	NameParserObject* name_obj = &obj->name_parser_;

	ServiceInfo* service_info = NULL;
	HostInfo* host_info = NULL;
	UserInfo* user_info = NULL;

	if (mc_obj == NULL) return SFW_E_INVALIDARG;

	if (name_obj->parse(name_obj, service_name) == false)
	{
		SFW_ERROR_MSG("[Error] %s : fail service name.\n", __FUNCTION__);
		return SFW_E_INVALIDARG;
	}

	service_info = obj->get_service_info(obj, &obj->service_rep_.list, name_obj->get_service_name(name_obj));
	if (service_info == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : fail get_service_info.\n", __FUNCTION__);
		return SFW_E_FAIL;
	}

	host_info = obj->get_host_info(obj, &service_info->host_info.list, name_obj->get_host_name(name_obj));
	if (host_info == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : fail get_host_info.\n", __FUNCTION__);
		return SFW_E_FAIL;
	}

	user_info = obj->get_user_info(obj, &host_info->user_info.list, name_obj->get_user_name(name_obj));
	if (user_info == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : fail get_user_info.\n", __FUNCTION__);
		return SFW_E_FAIL;
	}

	if (user_info->mc_obj != NULL)
	{
		SFW_ERROR_MSG("[Error] %s : user_info->mc_obj != NULL.\n", __FUNCTION__);
		return SFW_E_ALREADY_EXISTS;
	}

	user_info->mc_obj = mc_obj;
	obj->service_count_++;

	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	find
--------------------------------------------------------------------------------*/
/**
 *	@fn			MessageContainerObjectType* mc_repository_find(MCRepositoryObject* obj, const char* service_name)
 *	@brief		メッセージキューが存在するかどうかをチェックする
 *	@param[in]	const char* service_name
 *	@return		MessageContainerObjectType* !NULL...OK, NULL...NG
 */
static MessageContainerObjectType* mc_repository_find(MCRepositoryObject* obj, const char* service_name)
{
	NameParserObject* name_obj = &obj->name_parser_;
	MessageContainerObjectType* mc_obj = NULL;

	ServiceInfo* service_info = NULL;
	HostInfo* host_info = NULL;
	UserInfo* user_info = NULL;

	// if (service_name == NULL) return false;

	if (name_obj->parse(name_obj, service_name) == false)
	{
		SFW_ERROR_MSG("[Error] %s : fail service name.\n", __FUNCTION__);
		return NULL;
	}

	service_info = obj->search_service_info(obj, &obj->service_rep_.list, name_obj->get_service_name(name_obj));
	if (service_info == NULL) return NULL;

	/*
	// サービス名が見つかった場合で、さらに名前に指定があった場合、ホスト名とユーザ名を探しに行く
	// ↓
	// 名前指定があった場合ではななく、常にホスト名とユーザ名を探す(Add時に使ってるのでデフォの振舞いはやめる)
	*/

	// if (name_obj->assign_host_name(name_obj))
	{
		host_info = obj->search_host_info(obj, &service_info->host_info.list, name_obj->get_host_name(name_obj));
		if (host_info == NULL) return NULL;
	}

	// if (name_obj->assign_user_name(name_obj))
	{
		user_info = obj->search_user_info(obj, &host_info->user_info.list, name_obj->get_user_name(name_obj));
		if (user_info == NULL) return NULL;
	}

	mc_obj = user_info->mc_obj;

	return mc_obj;
}

/*--------------------------------------------------------------------------------
	get_instance
--------------------------------------------------------------------------------*/
/**
 *	@fn			MessageContainerObject* mc_repository_get_instance(MCRepositoryObject* obj, EventSourceObject* event_source_obj, WaitNotifyObject* wait_notify_obj, NotifyGroupIdType group_id, NotifyUserIdType user_id)
 *	@brief		IDからWaitNotifyObjectを取得する、無い場合は作成する
 *	@param[in]	EventSourceObject* event_source_obj
 *	@param[in]	WaitNotifyObject* wait_notify_obj
 *	@param[in]	NotifyGroupIdType group_id
 *	@param[in]	NotifyUserIdType user_id
 *	@return		MessageContainerObject* !NULL...OK, NULL...NG
 */
static MessageContainerObject* mc_repository_get_instance(MCRepositoryObject* obj, EventSourceObject* event_source_obj, WaitNotifyObject* wait_notify_obj, NotifyGroupIdType group_id, NotifyUserIdType user_id)
{
	MessageContainerObject* mc_obj = obj->find_from_id(obj, group_id);

	if (mc_obj != NULL)
	{
		mc_obj->attach(mc_obj);
		return mc_obj;
	}

	mc_obj = obj->create(obj, event_source_obj, wait_notify_obj, group_id, user_id);

	/*
	// init mc object
	*/
	if (mc_obj != NULL)
	{
		Result ret = mc_obj->init(mc_obj, true);
		if (SFW_FAILED(ret))
		{
			SFW_ERROR_MSG("[Error] %s : mc_obj->init\n", __FUNCTION__);
			kfree(mc_obj);

			return NULL;
		}
	}

	return mc_obj;
}

/*--------------------------------------------------------------------------------
	release_instance
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result mc_repository_release_instance(MCRepositoryObject* obj, MessageContainerObject* mc_obj)
 *	@brief		MessageContainerObject を開放する、最後の１つの場合はオブジェクトを削除する
 *	@param[in]	MessageContainerObject* mc_obj
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result mc_repository_release_instance(MCRepositoryObject* obj, MessageContainerObject* mc_obj)
{
	Result ret = SFW_S_OK;

	if (mc_obj == NULL) return SFW_E_INVALIDARG;
	if (mc_obj->detach(mc_obj) != 0) return SFW_S_OK;

	ret = mc_obj->exit(mc_obj);
	kfree(mc_obj);

	return ret;
}

/*--------------------------------------------------------------------------------
	find_from_id
--------------------------------------------------------------------------------*/
/**
 *	@fn			MessageContainerObjectType* mc_repository_find_from_id(MCRepositoryObject* obj, NotifyGroupIdType group_id)
 *	@brief		グループIDをから MC オブジェクトを探す
 *	@param[in]	NotifyGroupIdType group_id
 *	@return		MessageContainerObjectType* !NULL...OK, NULL...NG
 */
static MessageContainerObjectType* mc_repository_find_from_id(MCRepositoryObject* obj, NotifyGroupIdType group_id)
{
	struct list_head* service_listptr = NULL;
	MessageContainerObjectType* mc_obj = NULL;

	list_for_each(service_listptr, &obj->service_rep_.list)
	{
		ServiceInfo* service_rep = list_entry(service_listptr, struct _ServiceInfo, list);

		if (service_rep == NULL) continue;

		struct list_head* host_listptr = NULL;

		list_for_each(host_listptr, &service_rep->host_info.list)
		{
			HostInfo* host_rep = list_entry(host_listptr, struct _HostInfo, list);
			if (host_rep == NULL) continue;

			struct list_head* user_listptr = NULL;

			list_for_each(user_listptr, &host_rep->user_info.list)
			{
				UserInfo* user_rep = list_entry(user_listptr, struct _UserInfo, list);

				if ((user_rep == NULL) || (user_rep->mc_obj == NULL)) continue;

				if (user_rep->mc_obj->group_id_ == group_id)
				{
					mc_obj = user_rep->mc_obj;
					break;
				}
			}
		}
	}

	return mc_obj;
}

/*--------------------------------------------------------------------------------
	remove
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result mc_repository_add(MCRepositoryObject* obj, const char* service_name)
 *	@brief		MCを削除する(サービス名の指定のフォーマットは、省略時はデフォルトとする)
 *	@param[in]	const char* service_name
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result mc_repository_remove(MCRepositoryObject* obj, const char* service_name)
{
	NameParserObject* name_obj = &obj->name_parser_;

	ServiceInfo* service_remove_target = NULL;
	HostInfo* host_remove_target = NULL;
	UserInfo* user_remove_target = NULL;

	// if (service_name == NULL) return SFW_E_INVALIDARG;

	if (name_obj->parse(name_obj, service_name) == false)
	{
		SFW_ERROR_MSG("[Error] %s : fail service name.\n", __FUNCTION__);
		return SFW_E_INVALIDARG;
	}

	service_remove_target = obj->search_service_info(obj, &obj->service_rep_.list, name_obj->get_service_name(name_obj));
	if (service_remove_target == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : search_service_info. = %s\n", __FUNCTION__, name_obj->get_service_name(name_obj));
		return SFW_E_FAIL;
	}

	host_remove_target = obj->search_host_info(obj, &service_remove_target->host_info.list, name_obj->get_host_name(name_obj));
	if (host_remove_target == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : search_host_info. = %s\n", __FUNCTION__, name_obj->get_host_name(name_obj));
		return SFW_E_FAIL;
	}

	user_remove_target = obj->search_user_info(obj, &host_remove_target->user_info.list, name_obj->get_user_name(name_obj));
	if (user_remove_target == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : search_user_info. = %s\n", __FUNCTION__, name_obj->get_user_name(name_obj));
		return SFW_E_FAIL;
	}

	// user
	list_del(&user_remove_target->list);
	kfree(user_remove_target);

	// host
	if (list_empty(&host_remove_target->user_info.list))
	{
		list_del(&host_remove_target->list);
		kfree(host_remove_target);
	}

	// service
	if (list_empty(&service_remove_target->host_info.list))
	{
		list_del(&service_remove_target->list);
		kfree(service_remove_target);
	}

	if (obj->service_count_ > 0) obj->service_count_--;

	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	get
--------------------------------------------------------------------------------*/
/**
 *	@fn			MCArrary* mc_repository_get(MCRepositoryObject* obj, const char* service_name)
 *	@brief		サービス名よりMCを取得する
 *	@param[in]	const char* service_name
 *	@return		MCArrary* !NULL...OK, NULL...NG
 */
static MCArrary* mc_repository_get(MCRepositoryObject* obj, const char* service_name)
{
	NameParserObject* name_obj = &obj->name_parser_;

	ServiceInfo* service_info = NULL;
	HostInfo* host_info = NULL;
	UserInfo* user_info = NULL;

	struct list_head* listptr = NULL;

	// if (service_name == NULL) return NULL;

	if (name_obj->parse(name_obj, service_name) == false)
	{
		SFW_ERROR_MSG("[Error] %s : fail service name.\n", __FUNCTION__);
		return NULL;
	}

	// clear mc array
	obj->clear_mc_array(obj);

	service_info = obj->search_service_info(obj, &obj->service_rep_.list, name_obj->get_service_name(name_obj));
	if (service_info != NULL)
	{
		// ホスト名が指定されているかどうか
		// ホスト名が指定されている場合、ホスト名を探す
		if (name_obj->assign_host_name(name_obj))
		{
			host_info = obj->search_host_info(obj, &service_info->host_info.list, name_obj->get_host_name(name_obj));

			// ホスト名が合ってない場合は処理しない
			if (host_info != NULL)
			{
				// ユーザ名が指定有無
				if (name_obj->assign_user_name(name_obj))
				{
					/*
					// ユーザ名指定なので、一人のユーザを探す
					*/
					user_info = obj->search_user_info(obj, &host_info->user_info.list, name_obj->get_user_name(name_obj));

					if ((user_info != NULL) && (user_info->mc_obj != NULL))
					{
						/* bool ret = */ obj->add_mc_array(obj, user_info->mc_obj);
					}
				}
				else
				{
					/*
					// ユーザ名を指定していないので、ホスト以下すべてのユーザ
					*/
					list_for_each(listptr, &host_info->user_info.list)
					{
						user_info = list_entry(listptr, struct _UserInfo, list);

						if ((user_info != NULL) && (user_info->mc_obj != NULL))
						{
							/* bool ret = */ obj->add_mc_array(obj, user_info->mc_obj);
						}
					}
				}
			}
		}
		else
		{
			if (name_obj->assign_user_name(name_obj))
			{
				// 複数のホストから１つのユーザ
				list_for_each(listptr, &service_info->host_info.list)
				{
					host_info = list_entry(listptr, struct _HostInfo, list);

					if (host_info != NULL)
					{
						user_info = obj->search_user_info(obj, &host_info->user_info.list, name_obj->get_user_name(name_obj));

	 					if ((user_info != NULL) && (user_info->mc_obj != NULL))
						{
							/* bool ret = */ obj->add_mc_array(obj, user_info->mc_obj);
						}
					}
				}
			}
			else
			{
				// 複数のホスト以下すべて
				list_for_each(listptr, &service_info->host_info.list)
				{
					struct list_head* user_listptr = NULL;

					host_info = list_entry(listptr, struct _HostInfo, list);

					if (host_info != NULL)
					{
						list_for_each(user_listptr, &host_info->user_info.list)
						{
							user_info = list_entry(user_listptr, struct _UserInfo, list);

							if ((user_info != NULL) && (user_info->mc_obj != NULL))
							{
								/* bool ret = */ obj->add_mc_array(obj, user_info->mc_obj);
							}
						}
					}
				}
			}
		}
	}

	return &obj->mc_array_;
}

/*--------------------------------------------------------------------------------
	show
--------------------------------------------------------------------------------*/
/**
 *	@fn			void mc_repository_show(MCRepositoryObject* obj)
 *	@brief		デバッグ用、中身を表示する
 *	@param		なし
 *	@return		なし
 */
static void mc_repository_show(MCRepositoryObject* obj)
{
	struct list_head* service_listptr = NULL;

	list_for_each(service_listptr, &obj->service_rep_.list)
	{
		ServiceInfo* service_rep = list_entry(service_listptr, struct _ServiceInfo, list);
		SFW_DEBUG_OUT("show service=%s\n", service_rep->name);
		{
			struct list_head* host_listptr = NULL;
			list_for_each(host_listptr, &service_rep->host_info.list)
			{
				HostInfo* host_rep = list_entry(host_listptr, struct _HostInfo, list);
				SFW_DEBUG_OUT("show host=%s\n", host_rep->name);
				{
					struct list_head* user_listptr = NULL;
					list_for_each(user_listptr, &host_rep->user_info.list)
					{
						UserInfo* user_rep = list_entry(user_listptr, struct _UserInfo, list);
						SFW_DEBUG_OUT("show user=%s\n", user_rep->name);
					}
				}
			}
		}
	}
}

/*--------------------------------------------------------------------------------
    get_service_count
 --------------------------------------------------------------------------------*/
/**
 *	@fn			Result mc_repository_get_service_count(MCRepositoryObject* obj, char __user* dst_service_count)
 *	@brief		登録されているサービス数の取得
 *	@param[out]	char __user* dst_service_count
 *	@return		Result SFW_S_OK...OK, SFW_E_ACCESS_DENIED...NG
 */
static Result mc_repository_get_service_count(MCRepositoryObject* obj, char __user* dst_service_count)
{
	if (copy_to_user(dst_service_count, &obj->service_count_, sizeof(obj->service_count_)))
	{
		SFW_ERROR_MSG("[Error] %s : copy_to_user obj->service_count_ = %d.\n", __FUNCTION__, obj->service_count_);
		return SFW_E_ACCESS_DENIED;
	}

	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
    get_service
 --------------------------------------------------------------------------------*/
/**
 *	@fn			Result mc_repository_get_service(MCRepositoryObject* obj, int max_service_cnt, char __user* dst_service_tbl)
 *	@brief		すべてのサービス名の取得
 *	@param[in]	int max_service_cnt
 *	@param[out]	char __user* dst_service_tbl
 *	@return		Result SFW_S_OK/SFW_S_SIZELIMIT...OK, SFW_E_ACCESS_DENIED...NG
 */
static Result mc_repository_get_service(MCRepositoryObject* obj, int max_service_cnt, char __user* dst_service_tbl)
{
	// format
	// +--------------------+--------------------+--------------------+--------------------+--------------------+-----+
	// | service name count | service name len   | service name       | service name len   | service name       | ... |
	// +--------------------+--------------------+--------------------+--------------------+--------------------+-----+

	Result ret = SFW_S_OK;
	char* dst_service_cnt = dst_service_tbl;
	struct list_head* service_listptr = NULL;
	int service_cnt = 0;

	dst_service_tbl += sizeof(service_cnt);

	list_for_each(service_listptr, &obj->service_rep_.list)
	{
		ServiceInfo* service_rep = list_entry(service_listptr, struct _ServiceInfo, list);

		if (service_rep == NULL) continue;

		size_t service_len = strlen(service_rep->name);
		if (service_len == 0) continue;

		struct list_head* host_listptr = NULL;

		list_for_each(host_listptr, &service_rep->host_info.list)
		{
			HostInfo* host_rep = list_entry(host_listptr, struct _HostInfo, list);

			if (host_rep == NULL) continue;
			#ifndef BUSBUSMNG
			if (strncmp(SERVICE_BUS_DEFAULT_HOST_NAME, host_rep->name, strlen(SERVICE_BUS_DEFAULT_HOST_NAME)) != 0) continue;
			#endif // BUSBUSMNG

			#ifdef BUSBUSMNG
			size_t host_len = strlen(host_rep->name);
			if (host_len == 0) continue;
			#endif // BUSBUSMNG
			struct list_head* user_listptr = NULL;

			list_for_each(user_listptr, &host_rep->user_info.list)
			{
				UserInfo* user_rep = list_entry(user_listptr, struct _UserInfo, list);

				if (user_rep == NULL) continue;

				size_t user_len = strlen(user_rep->name);
				if (user_len == 0) continue;

				// check output size(max_service_cnt)
				if ((max_service_cnt - 1) < service_cnt)
				{
					ret = SFW_S_SIZELIMIT;
					break;
				}

				/*
				// copy full service name (username@servicename)
				*/
				#ifdef BUSBUSMNG
				int total_len = (int)service_len + 1 /* @ */ + (int)host_len + 1 /* / */ + (int)user_len;
				#else
				int total_len = (int)service_len + 1 /* @ */ + /* (int)host_len + 1 */ (int)user_len;
				#endif // BUSBUSMNG

				// copy len
				if (copy_to_user(dst_service_tbl, &total_len, sizeof(total_len)))
				{
					SFW_ERROR_MSG("[Error] %s : copy_to_user sizeof(total_len) = %u.\n", __FUNCTION__, sizeof(total_len));
					return SFW_E_ACCESS_DENIED;
				}
				dst_service_tbl += sizeof(total_len);

				// copy user name
				if (copy_to_user(dst_service_tbl, user_rep->name, user_len))
				{
					SFW_ERROR_MSG("[Error] %s : copy_to_user(dst_service_tbl, user_rep->name, user_len) len = %d.\n", __FUNCTION__, (int)user_len);
					return SFW_E_ACCESS_DENIED;
				}
				dst_service_tbl += user_len;

				if (copy_to_user(dst_service_tbl, "@", 1))
				{
					SFW_ERROR_MSG("[Error] %s : copy_to_user(dst_service_tbl, \"@\", 1)\n", __FUNCTION__);
					return SFW_E_ACCESS_DENIED;
				}
				dst_service_tbl += 1;

				#ifdef BUSBUSMNG
				// copy host name
				if (copy_to_user(dst_service_tbl, host_rep->name, host_len))
				{
					SFW_ERROR_MSG("[Error] %s : copy_to_user(dst_service_tbl, host_rep->name, host_len) len = %zu.\n", __FUNCTION__, host_len);
					return SFW_E_ACCESS_DENIED;
				}
				dst_service_tbl += host_len;

				if (copy_to_user(dst_service_tbl, "/", 1))
				{
					SFW_ERROR_MSG("[Error] %s : copy_to_user(dst_service_tbl, \"/\", 1)\n", __FUNCTION__);
					return SFW_E_ACCESS_DENIED;
				}
				dst_service_tbl += 1;
				#endif // BUSBUSMNG

				// copy service name
				if (copy_to_user(dst_service_tbl, service_rep->name, service_len))
				{
					SFW_ERROR_MSG("[Error] %s : copy_to_user(dst_service_tbl, service_rep->name, service_len) len = %d.\n", __FUNCTION__, (int)service_len);
					return SFW_E_ACCESS_DENIED;
				}
				dst_service_tbl += service_len;

				if (total_len & 1)
				{
					// *dst_service_tbl++ = 0;
					const char zero = 0;
					if (copy_to_user(dst_service_tbl, &zero, sizeof(zero)))
					{
						SFW_ERROR_MSG("[Error] %s : copy_to_user sizeof(zero) = %u.\n", __FUNCTION__, sizeof(zero));
						return SFW_E_ACCESS_DENIED;
					}
					dst_service_tbl += 1;
				}

				service_cnt ++;
			}
		}
	}

	if (copy_to_user(dst_service_cnt, &service_cnt, sizeof(service_cnt)))
	{
		SFW_ERROR_MSG("[Error] %s : copy_to_user sizeof(service_cnt) = %u.\n", __FUNCTION__, sizeof(service_cnt));
		return SFW_E_ACCESS_DENIED;
	}

	return ret;
}

/*--------------------------------------------------------------------------------
    get_bus_info
 --------------------------------------------------------------------------------*/
/**
 *	@fn			Result mc_repository_get_bus_info(MCRepositoryObject* obj, int max_service_cnt, char __user* dst_service_tbl)
 *	@brief		登録されているサービス状態を取得
 *	@param[in]	int max_service_cnt
 *	@param[out]	char __user* dst_service_tbl
 *	@return		Result SFW_S_OK/SFW_S_SIZELIMIT...OK, SFW_E_ACCESS_DENIED...NG
 */
static Result mc_repository_get_bus_info(MCRepositoryObject* obj, int max_service_cnt, char __user* dst_service_tbl)
{
	// format
	// +---------------------+------------------------+-------------+--------------+-------------+--------------+--------------------+
	// | record count (4byte)| service name (132byte) | GID (4byte) | WCNT (4byte) | UID (8byte) | QCnt (4byte) | MsgAllSize (4byte) |
	// +---------------------+------------------------+-------------+--------------+-------------+--------------+--------------------+

	Result ret = SFW_S_OK;
	char* dst_service_cnt = dst_service_tbl;
	struct list_head* service_listptr = NULL;
	int service_cnt = 0;

	dst_service_tbl += sizeof(service_cnt);

	list_for_each(service_listptr, &obj->service_rep_.list)
	{
		ServiceInfo* service_rep = list_entry(service_listptr, struct _ServiceInfo, list);

		if (service_rep == NULL) continue;

		size_t service_len = strlen(service_rep->name);
		if (service_len == 0) continue;

		struct list_head* host_listptr = NULL;

		list_for_each(host_listptr, &service_rep->host_info.list)
		{
			HostInfo* host_rep = list_entry(host_listptr, struct _HostInfo, list);

			if (host_rep == NULL) continue;

			size_t host_len = strlen(host_rep->name);
			if (host_len == 0) continue;

			struct list_head* user_listptr = NULL;

			list_for_each(user_listptr, &host_rep->user_info.list)
			{
				UserInfo* user_rep = list_entry(user_listptr, struct _UserInfo, list);

				if (user_rep == NULL) continue;

				size_t user_len = strlen(user_rep->name);
				if (user_len == 0) continue;

				// check mc obj
				if (user_rep->mc_obj == NULL) continue;
				if (user_rep->mc_obj->msg_queue_ == NULL) continue;
				if (user_rep->mc_obj->wait_notify_ == NULL) continue;

				// check output size(max_service_cnt)
				if ((max_service_cnt - 1) < service_cnt)
				{
					ret = SFW_S_SIZELIMIT;
					break;
				}

				/*
				// copy service information
				*/
				{
					// klockworkで怒られてもここのサイズは大丈夫。もし直す場合は、bus_proxy の BUS_CONTROL_GET_BUS_INFO_XXX を使っている箇所を直す必要がある
					char full_name[FULL_NAME_LEN_MAX + STR_TERMINATE_SIZE] = {0};

					sprintf(full_name, "%s@%s/%s", user_rep->name, host_rep->name, service_rep->name);

					if (copy_to_user(dst_service_tbl, full_name, sizeof(full_name)))
					{
						SFW_ERROR_MSG("[Error] %s : copy_to_user(full_name)\n", __FUNCTION__);
						return SFW_E_ACCESS_DENIED;
					}

					dst_service_tbl += sizeof(full_name);
				}

				/*
				// copy id information
				*/

				// for gid
				{
					NotifyGroupIdType gid = user_rep->mc_obj->wait_notify_->id_;

					if (copy_to_user(dst_service_tbl, &gid, sizeof(gid)))
					{
						SFW_ERROR_MSG("[Error] %s : copy_to_user(gid)\n", __FUNCTION__);
						return SFW_E_ACCESS_DENIED;
					}

					dst_service_tbl += sizeof(gid);
				}

				// for wait notify counter
				{
					int counter = user_rep->mc_obj->wait_notify_->ref_count_;

					if (copy_to_user(dst_service_tbl, &counter, sizeof(counter)))
					{
						SFW_ERROR_MSG("[Error] %s : copy_to_user(counter)\n", __FUNCTION__);
						return SFW_E_ACCESS_DENIED;
					}

					dst_service_tbl += sizeof(counter);
				}

				// for user id
				{
					NotifyUserIdType uid = user_rep->mc_obj->user_id_;

					if (copy_to_user(dst_service_tbl, &uid, sizeof(uid)))
					{
						SFW_ERROR_MSG("[Error] %s : copy_to_user(uid)\n", __FUNCTION__);
						return SFW_E_ACCESS_DENIED;
					}

					dst_service_tbl += sizeof(uid);
				}

				/*
				// copy msg information
				*/

				// queue count
				{
					unsigned int queue_cnt = user_rep->mc_obj->count(user_rep->mc_obj);

					if (copy_to_user(dst_service_tbl, &queue_cnt, sizeof(queue_cnt)))
					{
						SFW_ERROR_MSG("[Error] %s : copy_to_user(queue_cnt)\n", __FUNCTION__);
						return SFW_E_ACCESS_DENIED;
					}

					dst_service_tbl += sizeof(queue_cnt);
				}

				// all msg size
				{
					long total_msg_size = user_rep->mc_obj->msg_queue_->total_msg_size_;

					if (copy_to_user(dst_service_tbl, &total_msg_size, sizeof(total_msg_size)))
					{
						SFW_ERROR_MSG("[Error] %s : copy_to_user(queue_cnt)\n", __FUNCTION__);
						return SFW_E_ACCESS_DENIED;
					}

					dst_service_tbl += sizeof(total_msg_size);
				}

				// counter up
				service_cnt ++;
			}
		}
	}

	if (copy_to_user(dst_service_cnt, &service_cnt, sizeof(service_cnt)))
	{
		SFW_ERROR_MSG("[Error] %s : copy_to_user sizeof(service_cnt) = %u.\n", __FUNCTION__, sizeof(service_cnt));
		return SFW_E_ACCESS_DENIED;
	}

	return ret;
}

/*--------------------------------------------------------------------------------
	search_service_info
--------------------------------------------------------------------------------*/
/**
 *	@fn			ServiceInfo* mc_repository_search_service_info(MCRepositoryObject* obj, struct list_head* search_list, const char* name)
 *	@brief		サービス情報の取得
 *	@param[out]	struct list_head* search_list
 *	@param[in]	const char* name
 *	@return		ServiceInfo* !NULL...OK, NULL...NG
 */
static ServiceInfo* mc_repository_search_service_info(MCRepositoryObject* obj, struct list_head* search_list, const char* name)
{
	struct list_head* listptr = NULL;
	ServiceInfo* result = NULL;

	list_for_each(listptr, search_list)
	{
		ServiceInfo* target = list_entry(listptr, struct _ServiceInfo, list);
		if (strncmp(target->name, name, SERVICE_NAME_LEN_MAX) == 0)
		{
			result = target;
			break;
		}
	}

	return result;
}

/*--------------------------------------------------------------------------------
	search_host_info
--------------------------------------------------------------------------------*/
/**
 *	@fn			HostInfo* mc_repository_search_host_info(MCRepositoryObject* obj, struct list_head* search_list, const char* name)
 *	@brief		ホスト情報の取得
 *	@param[out]	struct list_head* search_list
 *	@param[in]	const char* name
 *	@return		HostInfo* !NULL...OK, NULL...NG
 */
static HostInfo* mc_repository_search_host_info(MCRepositoryObject* obj, struct list_head* search_list, const char* name)
{
	struct list_head* listptr = NULL;
	HostInfo* result = NULL;

	list_for_each(listptr, search_list)
	{
		HostInfo* target = list_entry(listptr, struct _HostInfo, list);
		if (strncmp(target->name, name, HOST_NAME_LEN_MAX) == 0)
		{
			result = target;
			break;
		}
	}

	return result;
}

/*--------------------------------------------------------------------------------
	search_user_info
--------------------------------------------------------------------------------*/
/**
 *	@fn			UserInfo* mc_repository_search_user_info(MCRepositoryObject* obj, struct list_head* search_list, const char* name)
 *	@brief		ユーザ情報の取得
 *	@param[out]	struct list_head* search_list
 *	@param[in]	const char* name
 *	@return		UserInfo* !NULL...OK, NULL...NG
 */
static UserInfo* mc_repository_search_user_info(MCRepositoryObject* obj, struct list_head* search_list, const char* name)
{
	struct list_head* listptr = NULL;
	UserInfo* result = NULL;

	list_for_each(listptr, search_list)
	{
		UserInfo* target = list_entry(listptr, struct _UserInfo, list);
		if (strncmp(target->name, name, USER_NAME_LEN_MAX) == 0)
		{
			result = target;
			break;
		}
	}

	return result;
}

/*--------------------------------------------------------------------------------
	get_service_info
--------------------------------------------------------------------------------*/
/**
 *	@fn			ServiceInfo* mc_repository_get_service_info(MCRepositoryObject* obj, struct list_head* search_list, const char* name)
 *	@brief		サービス情報の取得
 *	@param[out]	struct list_head* search_list
 *	@param[in]	const char* name
 *	@return		ServiceInfo* !NULL...OK, NULL...NG
 */
static ServiceInfo* mc_repository_get_service_info(MCRepositoryObject* obj, struct list_head* search_list, const char* name)
{
	size_t name_len = 0;
	ServiceInfo* target = obj->search_service_info(obj, search_list, name);

	if (target == NULL)
	{
		target = kmalloc(sizeof(ServiceInfo), GFP_KERNEL);
		if (target == NULL)
		{
			SFW_ERROR_MSG("[Error] %s : new ServiceInfo.\n", __FUNCTION__);
			return NULL;
		}

		name_len = strlen(name);
		memcpy(target->name, name, name_len);
		target->name[name_len] = 0;

		INIT_LIST_HEAD(&target->host_info.list);

		list_add(&target->list, search_list);
	}

	return target;
}

/*--------------------------------------------------------------------------------
	mc_repository_get_host_info
--------------------------------------------------------------------------------*/
/**
 *	@fn			HostInfo* mc_repository_get_host_info(MCRepositoryObject* obj, struct list_head* search_list, const char* name)
 *	@brief		ホスト情報の取得
 *	@param[out]	struct list_head* search_list
 *	@param[in]	const char* name
 *	@return		HostInfo* !NULL...OK, NULL...NG
 */
static HostInfo* mc_repository_get_host_info(MCRepositoryObject* obj, struct list_head* search_list, const char* name)
{
	size_t name_len = 0;
	HostInfo* target = obj->search_host_info(obj, search_list, name);

	if (target == NULL)
	{
		target = kmalloc(sizeof(HostInfo), GFP_KERNEL);
		if (target == NULL)
		{
			SFW_ERROR_MSG("[Error] %s : new HostInfo.\n", __FUNCTION__);
			return NULL;
		}

		name_len = strlen(name);
		memcpy(target->name, name, name_len);
		target->name[name_len] = 0;

		INIT_LIST_HEAD(&target->user_info.list);

		list_add(&target->list, search_list);
	}

	return target;
}

/*--------------------------------------------------------------------------------
	mc_repository_get_user_info
--------------------------------------------------------------------------------*/
/**
 *	@fn			UserInfo* mc_repository_get_user_info(MCRepositoryObject* obj, struct list_head* search_list, const char* name)
 *	@brief		ユーザ情報の取得
 *	@param[out]	struct list_head* search_list
 *	@param[in]	const char* name
 *	@return		UserInfo* !NULL...OK, NULL...NG
 */
static UserInfo* mc_repository_get_user_info(MCRepositoryObject* obj, struct list_head* search_list, const char* name)
{
	size_t name_len = 0;
	UserInfo* target = obj->search_user_info(obj, search_list, name);

	if (target == NULL)
	{
		target = kmalloc(sizeof(UserInfo), GFP_KERNEL);
		if (target == NULL)
		{
			SFW_ERROR_MSG("[Error] %s : new UserInfo.\n", __FUNCTION__);
			return NULL;
		}

		name_len = strlen(name);
		memcpy(target->name, name, name_len);
		target->name[name_len] = 0;

		target->mc_obj = NULL;

		list_add(&target->list, search_list);
	}

	return target;
}

/*--------------------------------------------------------------------------------
	add_mc_array
--------------------------------------------------------------------------------*/
/**
 *	@fn			UserInfo* mc_repository_add_mc_array(MCRepositoryObject* obj, MessageContainerObjectType* mc)
 *	@brief		MC管理領域へ追加
 *	@param[in]	MessageContainerObjectType* mc
 *	@return		bool true...OK, false...NG
 */
static bool mc_repository_add_mc_array(MCRepositoryObject* obj, MessageContainerObjectType* mc)
{
	MCArrary* mc_array = &obj->mc_array_;

	if (mc_array->tbl_cnt < MCOBJ_ARRAY_CNT_MAX)
	{
		mc_array->tbl[mc_array->tbl_cnt] = mc;
		obj->mc_array_.tbl_cnt ++;
		return true;
	}

	return false;
}

/*--------------------------------------------------------------------------------
	clear_mc_array
--------------------------------------------------------------------------------*/
/**
 *	@fn			void mc_repository_clear_mc_array(MCRepositoryObject* obj)
 *	@brief		MC管理領域をクリア
 *	@param		なし
 *	@return 	なし
 */
static void mc_repository_clear_mc_array(MCRepositoryObject* obj)
{
	obj->mc_array_.tbl_cnt = 0;
}

/*--------------------------------------------------------------------------------
	exit
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result mc_repository_exit(MCRepositoryObject* obj)
 *	@brief		exit mc repository
 *	@param		なし
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result mc_repository_exit(MCRepositoryObject* obj)
{
	while (!list_empty(&obj->service_rep_.list))
	{
		ServiceInfo* service_rep = list_entry(obj->service_rep_.list.next, struct _ServiceInfo, list);
		if (service_rep != NULL)
		{
			while (!list_empty(&service_rep->host_info.list))
			{
				HostInfo* host_rep = list_entry(service_rep->host_info.list.next, struct _HostInfo, list);
				if (host_rep != NULL)
				{
					while (!list_empty(&host_rep->user_info.list))
					{
						UserInfo* user_rep = list_entry(host_rep->user_info.list.next, struct _UserInfo, list);
						if (user_rep != NULL)
						{
							/*Result ret =*/ obj->release_instance(obj, user_rep->mc_obj);

							list_del(&user_rep->list);
							kfree(user_rep);
						}
					}
					list_del(&host_rep->list);
					kfree(host_rep);
				}
			}
			list_del(&service_rep->list);
			kfree(service_rep);
		}
	}

	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	InitializeMCRepositoryObject
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result InitializeMCRepositoryObject(MCRepositoryObject* obj)
 *	@brief		コンストラクタ
 *	@param		なし
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
Result InitializeMCRepositoryObject(MCRepositoryObject* obj)
{
	Result ret = SFW_S_OK;

	if (obj == NULL) return SFW_E_INVALIDARG;

	ret = InitializeNameParserObject(&obj->name_parser_);
	if (SFW_FAILED(ret)) return SFW_E_FAIL;

	memset(&obj->service_rep_, 0, sizeof(obj->service_rep_));
	INIT_LIST_HEAD(&obj->service_rep_.list);

	memset(&obj->mc_array_, 0, sizeof(obj->mc_array_));

	obj->service_count_ = 0;

	// mc
	obj->create					= mc_repository_create;
	obj->add					= mc_repository_add;
	obj->find					= mc_repository_find;
	obj->find_from_id			= mc_repository_find_from_id;
	obj->remove					= mc_repository_remove;
	obj->get					= mc_repository_get;
	obj->show					= mc_repository_show;
	obj->get_service			= mc_repository_get_service;
	obj->get_service_count		= mc_repository_get_service_count;
	obj->get_bus_info			= mc_repository_get_bus_info;
	obj->get_instance			= mc_repository_get_instance;
	obj->release_instance		= mc_repository_release_instance;
	obj->exit					= mc_repository_exit;

	obj->create_mc				= mc_repository_create_mc;
	obj->create_mq				= mc_repository_create_mq;

	obj->get_service_info		= mc_repository_get_service_info;
	obj->get_host_info			= mc_repository_get_host_info;
	obj->get_user_info			= mc_repository_get_user_info;
	obj->search_service_info	= mc_repository_search_service_info;
	obj->search_host_info 		= mc_repository_search_host_info;
	obj->search_user_info 		= mc_repository_search_user_info;

	obj->add_mc_array			= mc_repository_add_mc_array;
	obj->clear_mc_array			= mc_repository_clear_mc_array;

	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	FinalizeMCRepositoryObject
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result FinalizeMCRepositoryObject(MCRepositoryObject* pObj)
 *	@brief		destructor
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
Result FinalizeMCRepositoryObject(MCRepositoryObject* obj)
{
	if (obj == NULL) return SFW_E_INVALIDARG;
	return obj->exit(obj);
}
