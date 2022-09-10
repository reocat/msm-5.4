/********************************************************************************/
/** @file		message_queue.c
	@brief		message queue
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
#include <asm/uaccess.h>														// copy_from_user
#include "message_queue.h"														// message queue include

/*----------------------------------------------------------------------------
	external funciton
----------------------------------------------------------------------------*/
extern int get_qmaxsize(void);

/*--------------------------------------------------------------------------------
	init
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result message_queue_init(MessageQueueObject* obj)
 *	@brief		init message queue
 *	@param		なし
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result message_queue_init(MessageQueueObject* obj)
{
	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	exit
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result message_queue_exit(MessageQueueObject* obj)
 *	@brief		exit message queue
 *	@param		なし
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
static Result message_queue_exit(MessageQueueObject* obj)
{
	while (!list_empty(&obj->msg_queue_.list))
	{
		MessageType* msg_queue = list_entry(obj->msg_queue_.list.next, struct _MessageType, list);

		if ((msg_queue == NULL) || (msg_queue->buffer_ptr == NULL))
		{
			SFW_ERROR_MSG("[Error] %s \n", __FUNCTION__);
			return SFW_E_FAIL;
		}

		kfree(msg_queue->buffer_ptr);
		list_del(&msg_queue->list);
		kfree(msg_queue);
	}

	obj->total_msg_size_ = 0;
	obj->queue_size_ = 0;

	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	push
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result message_queue_push(MessageQueueObject* obj, const char __user * message_ptr, int msg_size)
 *	@brief		push message
 *	@param[in]	const char __user * message_ptr	: message
 *	@param[in]	int msg_size					: message size
 *	@return		Result SFW_S_OK...OK, SFW_E_INIT_MEMORY_ALLOCATOR/SFW_E_INVALID_SIZELIMIT...NG
 */
static Result message_queue_push(MessageQueueObject* obj, const char __user * message_ptr, int msg_size)
{
	MessageType* mesg_queue = NULL;
	BufferType* message = NULL;

	if (obj->total_msg_limit_size_ < (obj->total_msg_size_ + msg_size))
	{
		// SFW_ERROR_MSG("[Error] %s : limit over. %s\n", __FUNCTION__, message_ptr);
		return SFW_E_INVALID_SIZELIMIT;
	}

	/*
	// create message object
	*/
	mesg_queue = (MessageType*)kmalloc(sizeof(MessageType), GFP_KERNEL);
	if (mesg_queue == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : new queue msg.\n", __FUNCTION__);

		return SFW_E_INIT_MEMORY_ALLOCATOR;
	}

	message = (BufferType*)kmalloc(msg_size, GFP_KERNEL);
	if (message == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : new message buffer %d.\n", __FUNCTION__, msg_size);
		kfree(mesg_queue);

		return SFW_E_INIT_MEMORY_ALLOCATOR;
	}

	mesg_queue->buffer_ptr = message;
	mesg_queue->size = msg_size;
	mesg_queue->pos = 0;

	if (copy_from_user(message, message_ptr, msg_size))
	{
		SFW_ERROR_MSG("[Error] %s : copy_from_user no_copy_size=%d data=%s.\n", __FUNCTION__, msg_size, message_ptr);

		kfree(message);
		kfree(mesg_queue);

		return SFW_E_ACCESS_DENIED;
	}

	list_add_tail(&mesg_queue->list, &obj->msg_queue_.list);

	obj->queue_size_ ++;
	obj->total_msg_size_ += mesg_queue->size;

	return SFW_S_OK;
}

/*
// TODO : 別 KO にしてメッセージプロトコルを意識させないが、暫定でここに書く
*/
const char* TO_STR = "\"to\":\"";
const char* FORM_STR = "\"from\":\"";
const char* API_STR  = "\"api\":{\"name\":\"";

char* to_start_pos;
char* to_end_pos;
char* from_start_pos;
char* form_end_pos;
char* api_start_pos;
char* api_end_pos;

#define	LEN_MAX(a, b) (((a)>(b)) ? (a) : (b))

char * strnstr_len (const char *str1, const char *str2, int str1_len)
{
	const char *check = str1;
	size_t str2_len = strlen(str2);
	int count = 0;

	if (str1_len < str2_len) return NULL;

	while(count < str1_len)
	{
		if ((count + (int)str2_len) > str1_len) return NULL;
		if (*check == *str2)
		{
			if (memcmp(check, str2, str2_len) == 0) return (char*) check;
		}
		check++;
		count++;
	}
	return NULL;
}

static int _set_overwrite_message(const char* message, int msg_size)
{
	from_start_pos = strnstr_len(message, FORM_STR, msg_size);
	if (from_start_pos == NULL) return -1;
	from_start_pos += strlen(FORM_STR);

	form_end_pos = strchr(from_start_pos, '"');
	if (form_end_pos == NULL) return -1;

	api_start_pos = strnstr_len(message, API_STR, msg_size);
	if (api_start_pos == NULL) return -1;
	api_start_pos += strlen(API_STR);

	api_end_pos = strchr(api_start_pos, '"');
	if (api_end_pos == NULL) return -1;

	return 0;
}

static int _search_overwrite_message(const char* target_message, int msg_size)
{
	char* target_from_start_pos = NULL;
	char* target_from_end_pos   = NULL;

	char* target_api_start_pos  = NULL;
	char* target_api_end_pos    = NULL;

	target_from_start_pos = strnstr_len(target_message, FORM_STR, msg_size);
	if (target_from_start_pos == NULL) return -1;
	target_from_start_pos += strlen(FORM_STR);

	target_from_end_pos = strchr(target_from_start_pos, '"');
	if (target_from_end_pos == NULL) return -1;

	target_api_start_pos = strnstr_len(target_message, API_STR, msg_size);
	if (target_api_start_pos == NULL) return -1;
	target_api_start_pos += strlen(API_STR);

	target_api_end_pos = strchr(target_api_start_pos, '"');
	if (target_api_end_pos == NULL) return -1;

	if (memcmp(from_start_pos, target_from_start_pos, LEN_MAX(form_end_pos - from_start_pos, target_from_end_pos - target_from_start_pos)) != 0) return -1;
	if (memcmp(api_start_pos,  target_api_start_pos,  LEN_MAX(api_end_pos - api_start_pos, target_api_end_pos - target_api_start_pos)) != 0) return -1;

	return 0;
}

static int _strictly_set_overwrite_message(const char* message, int msg_size)
{
	to_start_pos = strnstr_len(message, TO_STR, msg_size);
	if (to_start_pos == NULL) return -1;
	to_start_pos += strlen(TO_STR);

	to_end_pos = strchr(to_start_pos, '"');
	if (to_end_pos == NULL) return -1;

	return _set_overwrite_message(message, msg_size);
}

static int _strictly_search_overwrite_message(const char* target_message, int msg_size)
{
	char* target_to_start_pos = NULL;
	char* target_to_end_pos   = NULL;

	target_to_start_pos = strnstr_len(target_message, TO_STR, msg_size);
	if (target_to_start_pos == NULL) return -1;
	target_to_start_pos += strlen(TO_STR);

	target_to_end_pos = strchr(target_to_start_pos, '"');
	if (target_to_end_pos == NULL) return -1;

	if (memcmp(to_start_pos, target_to_start_pos, LEN_MAX(to_end_pos - to_start_pos, target_to_end_pos - target_to_start_pos)) != 0) return -1;

	return _search_overwrite_message(target_message, msg_size);
}

/*--------------------------------------------------------------------------------
	set
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result message_queue_set(MessageQueueObject* obj, const char __user * message_ptr, int msg_size)
 *	@brief		set message
 *	@param[in]	const char __user * message_ptr	: message
 *	@param[in]	int msg_size					: message size
 *	@return		Result SFW_S_OK/SFW_S_UPDATE...OK, SFW_E_INIT_MEMORY_ALLOCATOR/SFW_E_INVALID_SIZELIMIT...NG
 */
static Result message_queue_set(MessageQueueObject* obj, const char __user * message_ptr, int msg_size)
{
	MessageType* mesg_queue = NULL;
	BufferType* message = NULL;

	if (obj->total_msg_limit_size_ < (obj->total_msg_size_ + msg_size))
	{
		// SFW_ERROR_MSG("[Error] %s : limit over. %s\n", __FUNCTION__, message_ptr);
		return SFW_E_INVALID_SIZELIMIT;
	}

	/*
	// create message object
	*/
	mesg_queue = (MessageType*)kmalloc(sizeof(MessageType), GFP_KERNEL);
	if (mesg_queue == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : new queue msg.\n", __FUNCTION__);

		return SFW_E_INIT_MEMORY_ALLOCATOR;
	}

	message = (BufferType*)kmalloc(msg_size, GFP_KERNEL);
	if (message == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : new message buffer %d.\n", __FUNCTION__, msg_size);
		kfree(mesg_queue);

		return SFW_E_INIT_MEMORY_ALLOCATOR;
	}

	mesg_queue->buffer_ptr = message;
	mesg_queue->size = msg_size;
	mesg_queue->pos = 0;

	if (copy_from_user(message, message_ptr, msg_size))
	{
		SFW_ERROR_MSG("[Error] %s : copy_from_user no_copy_size=%d.\n", __FUNCTION__, msg_size);

		kfree(message);
		kfree(mesg_queue);

		return SFW_E_ACCESS_DENIED;
	}

	/*
	// データ上書き処理の開始、探すメッセージを解析する
	//
	// １．キューにあるデータを先頭からを検索する、検索条件は、送信元とパラメータが同じメッセージ(送信先は固定されているので探す必要なし)
	// ２．キューがない場合は後ろへ追加する
	// ３．キューがある場合で、読み込み途中でない場合(msg.posが0であること)のデータに上書きする
	// ４．さらにそれ以降の同じデータを消す(オプション：現状なくてもよいはず)
	*/
	if (_set_overwrite_message(message, msg_size) == 0)
	{
		struct list_head* msg_que_ptr = NULL;

		list_for_each(msg_que_ptr, &obj->msg_queue_.list)
		{
			MessageType* itr = list_entry(msg_que_ptr, struct _MessageType, list);

			if (itr == NULL)
			{
				SFW_ERROR_MSG("[Error] %s : list_for_each(msg_que_ptr, &obj->msg_queue_.list).\n", __FUNCTION__);

				kfree(message);
				kfree(mesg_queue);

				return SFW_E_ACCESS_DENIED;
			}
			else if ((itr->pos == 0) && (_search_overwrite_message(itr->buffer_ptr, itr->size) == 0))
			{
				// 既存のデータを破棄
				kfree(itr->buffer_ptr);
				obj->total_msg_size_ -= itr->size;

				// 新しいデータをセット
				itr->buffer_ptr = mesg_queue->buffer_ptr;
				itr->size = mesg_queue->size;

				obj->total_msg_size_ += mesg_queue->size;

				kfree(mesg_queue);

				return SFW_S_UPDATE;
			}
		}
	}

	list_add_tail(&mesg_queue->list, &obj->msg_queue_.list);

	obj->queue_size_ ++;
	obj->total_msg_size_ += mesg_queue->size;

	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	strictly_set
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result message_queue_strictly_set(MessageQueueObject* obj, const char __user * message_ptr, int msg_size)
 *	@brief		set message
 *	@param[in]	const char __user * message_ptr	: message
 *	@param[in]	int msg_size					: message size
 *	@return		Result SFW_S_OK/SFW_S_UPDATE...OK, SFW_E_INIT_MEMORY_ALLOCATOR/SFW_E_INVALID_SIZELIMIT...NG
 */
static Result message_queue_strictly_set(MessageQueueObject* obj, const char __user * message_ptr, int msg_size)
{
	MessageType* mesg_queue = NULL;
	BufferType* message = NULL;

	if (obj->total_msg_limit_size_ < (obj->total_msg_size_ + msg_size))
	{
		// SFW_ERROR_MSG("[Error] %s : limit over. %s\n", __FUNCTION__, message_ptr);
		return SFW_E_INVALID_SIZELIMIT;
	}

	/*
	// create message object
	*/
	mesg_queue = (MessageType*)kmalloc(sizeof(MessageType), GFP_KERNEL);
	if (mesg_queue == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : new queue msg.\n", __FUNCTION__);

		return SFW_E_INIT_MEMORY_ALLOCATOR;
	}

	message = (BufferType*)kmalloc(msg_size, GFP_KERNEL);
	if (message == NULL)
	{
		SFW_ERROR_MSG("[Error] %s : new message buffer %d.\n", __FUNCTION__, msg_size);
		kfree(mesg_queue);

		return SFW_E_INIT_MEMORY_ALLOCATOR;
	}

	mesg_queue->buffer_ptr = message;
	mesg_queue->size = msg_size;
	mesg_queue->pos = 0;

	if (copy_from_user(message, message_ptr, msg_size))
	{
		SFW_ERROR_MSG("[Error] %s : copy_from_user no_copy_size=%d.\n", __FUNCTION__, msg_size);

		kfree(message);
		kfree(mesg_queue);

		return SFW_E_ACCESS_DENIED;
	}

	/*
	// データ上書き処理の開始、探すメッセージを解析する
	//
	// ※strictly_set はキューが１つしかないため from, api に加えて to の情報も一致させる必要がある、
	//   また、サービス情報の場合は後ろから検索し、サービス情報が出た場合は上書き、それ以外の場合は追加する
	//
	// １．データ元が通常メッセージの場合は、キューにあるデータを先頭からを検索する、検索条件は、送信元とパラメータが同じメッセージ(送信先は固定されているので探す必要なし)
	// ２．キューがない場合は後ろへ追加する
	// ３．キューがある場合で、読み込み途中でない場合(msg.posが0であること)のデータに上書きする
	// ４．データ元がゴキちゃんの場青は、キューにあるデータを後ろから検索する、検索条件は先頭文字が'G'
	// ５．検索先が通常メッセージだった場合は追加する
	*/
	if ((*message == '{') && (_strictly_set_overwrite_message(message, msg_size) == 0))
	{
		struct list_head* msg_que_ptr = NULL;

		list_for_each(msg_que_ptr, &obj->msg_queue_.list)
		{
			MessageType* itr = list_entry(msg_que_ptr, struct _MessageType, list);

			if (itr == NULL)
			{
				SFW_ERROR_MSG("[Error] %s : list_for_each(msg_que_ptr, &obj->msg_queue_.list).\n", __FUNCTION__);

				kfree(message);
				kfree(mesg_queue);

				return SFW_E_ACCESS_DENIED;
			}
			else if ((itr->pos == 0) && (itr->size > 0) && (*itr->buffer_ptr == '{') && (_strictly_search_overwrite_message(itr->buffer_ptr, itr->size) == 0))
			{
				// 既存のデータを破棄
				kfree(itr->buffer_ptr);
				obj->total_msg_size_ -= itr->size;

				// 新しいデータをセット
				itr->buffer_ptr = mesg_queue->buffer_ptr;
				itr->size = mesg_queue->size;

				obj->total_msg_size_ += mesg_queue->size;

				kfree(mesg_queue);

				return SFW_S_UPDATE;
			}
		}
	}
	else
	{
		if (!obj->empty(obj))
		{
			// サービス情報メッセージ
			MessageType* itr = list_entry(obj->msg_queue_.list.prev, struct _MessageType, list);

			if (itr == NULL)
			{
				SFW_ERROR_MSG("[Error] %s : list_last_entry(msg_que_ptr, struct _MessageType, list).\n", __FUNCTION__);

				kfree(message);
				kfree(mesg_queue);

				return SFW_E_ACCESS_DENIED;
			}
			else if ((itr->pos == 0) && (*itr->buffer_ptr == 'G'))
			{
				// 既存のデータを破棄
				kfree(itr->buffer_ptr);
				obj->total_msg_size_ -= itr->size;

				// 新しいデータをセット
				itr->buffer_ptr = mesg_queue->buffer_ptr;
				itr->size = mesg_queue->size;

				obj->total_msg_size_ += mesg_queue->size;

				kfree(mesg_queue);

				return SFW_S_UPDATE;
			}
		}
	}

	list_add_tail(&mesg_queue->list, &obj->msg_queue_.list);

	obj->queue_size_ ++;
	obj->total_msg_size_ += mesg_queue->size;

	return SFW_S_OK;
}

/*----------------------------------------------------------------------------
	pop
----------------------------------------------------------------------------*/
/**
 *	@fn			Result message_queue_pop(MessageQueueObject* obj, char __user * message_ptr, int msg_size, int* read_size)
 *	@brief		pop message
 *	@param[out]	char __user * message_ptr	: out buff
 *	@param[in]  int msg_size				: out buff size
 *	@param[out] int* read_size				: read size
 *	@return		Result SFW_S_OK/SFW_S_SIZELIMIT...OK, SFW_E_BAD_LENGTH...NG
 */
static Result message_queue_pop(MessageQueueObject* obj, char __user * message_ptr, int msg_size, int* read_size)
{
	MessageType* msg_queue = NULL;

	if (obj->empty(obj))
	{
		*read_size = 0;
		return SFW_S_OK;
	}

	msg_queue = list_entry(obj->msg_queue_.list.next, struct _MessageType, list);

	if ((msg_queue == NULL) || (msg_queue->buffer_ptr == NULL) || (msg_queue->size <= 0))
	{
		SFW_ERROR_MSG("[Error] %s \n", __FUNCTION__);
		return SFW_E_FAIL;
	}

	{
		int remain_size = msg_queue->size - msg_queue->pos;
		int copy_size = (remain_size > msg_size) ? msg_size : remain_size;

		if (copy_to_user(message_ptr, msg_queue->buffer_ptr + msg_queue->pos, copy_size))
		{
			SFW_ERROR_MSG("[Error] %s : copy_to_user no_copy_size = %d.\n", __FUNCTION__, copy_size);
			return SFW_E_ACCESS_DENIED;
		}

		*read_size = copy_size;
		//put_user(copy_size, read_size);

		obj->total_msg_size_ -= copy_size;

		#ifdef WIN32_NATIVE
		::Sleep(2);
		#endif // WIN32_NATIVE

		if (copy_size != remain_size)
		{
			msg_queue->pos += copy_size;
			return SFW_S_SIZELIMIT;
		}
	}

	kfree(msg_queue->buffer_ptr);
	list_del(&msg_queue->list);
    kfree(msg_queue);

    obj->queue_size_ --;

	return SFW_S_OK;
}

/*----------------------------------------------------------------------------
	empty
----------------------------------------------------------------------------*/
/**
 *	@fn			bool message_queue_empty(MessageQueueObject* obj)
 *	@brief		empty
 *	@param		nothing
 *	@return		bool true...empty, false...!empty
 */
static bool message_queue_empty(MessageQueueObject* obj)
{
	return obj->queue_size_ == 0;
}

/*----------------------------------------------------------------------------
	count
----------------------------------------------------------------------------*/
/**
 *	@fn			unsigned int message_queue_count(MessageQueueObject* obj)
 *	@brief		queue count
 *	@param		nothing
 *	@return		int 0...empty, !0...!empty
 */
static unsigned int message_queue_count(MessageQueueObject* obj)
{
	return obj->queue_size_;
}

/*----------------------------------------------------------------------------
	change_msg_limit_size
----------------------------------------------------------------------------*/
/**
 *	@fn			void message_queue_change_msg_limit_size(MessageQueueObject* obj, long)
 *	@brief		queue count
 *	@param[in]  long total_msg_limit_size : total msg limit size
 *	@return		なし
 */
static void message_queue_change_msg_limit_size(MessageQueueObject* obj, long total_msg_limit_size)
{
	obj->total_msg_limit_size_ = total_msg_limit_size;
}

/*--------------------------------------------------------------------------------
	InitializeMessageQueueObject
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result InitializeMessageQueueObject(MessageQueueObject* obj)
 *	@brief		constructor
 *	@param		nothing
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
Result InitializeMessageQueueObject(MessageQueueObject* obj)
{
	if (obj == NULL) return SFW_E_INVALIDARG;

	obj->total_msg_size_		= 0;
	obj->total_msg_limit_size_	= get_qmaxsize() * 1024;
	obj->queue_size_			= 0;

	// init deq object
	memset(&obj->msg_queue_, 0, sizeof(obj->msg_queue_));
	INIT_LIST_HEAD(&obj->msg_queue_.list);

	obj->init					= message_queue_init;
	obj->exit					= message_queue_exit;
	obj->push					= message_queue_push;
	obj->set					= message_queue_set;
	obj->strictly_set			= message_queue_strictly_set;
	obj->pop					= message_queue_pop;
	obj->empty					= message_queue_empty;
	obj->count					= message_queue_count;
	obj->change_msg_limit_size	= message_queue_change_msg_limit_size;

	return SFW_S_OK;
}
