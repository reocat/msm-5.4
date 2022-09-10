/********************************************************************************/
/** @file		wait_notify.c
	@brief		ユーザイベント通知オブジェクト
	@note		Copyright (C) 2013 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2013/10/04
	@version	1.0
*********************************************************************************/
/*--------------------------------------------------------------------------------
	Include
--------------------------------------------------------------------------------*/
#include "servicebus_common.h"													// service bus common include
#include "wait_notify.h"														// wait notify include
#include "lock_obj.h"															// lock objetct include

/*----------------------------------------------------------------------------
	external funciton
----------------------------------------------------------------------------*/
extern int get_maxsvc(void);

/*----------------------------------------------------------------------------
	wait
----------------------------------------------------------------------------*/
/**
 *	@fn			Result wait_notify_wait(WaitNotifyObject* obj, void __user * out_ptr)
 *	@brief		作業完了待ち
 *	@param[out]	void __user * out_ptr	: 出力先のバッファ
 *	@return		Result SFW_S_OK/SFW_S_INTERRUPT_QUIT...OK, SFW_E_ACCESS_DENIED...NG
 */
static Result wait_notify_wait(WaitNotifyObject* obj, void __user * out_ptr)
{
	// GDB でアタッチ出来る様にタイムアウト付きの API にする

	/*
	// [wait_for_completion_killable_timeout]
	// Return: -ERESTARTSYS if interrupted, 0 if timed out,
	// positive (at least 1, or number of jiffies left till timeout) if completed.
	*/
	Result ret = SFW_S_OK;
	long wait_ret = wait_for_completion_killable_timeout(&obj->comp_, obj->jiffies_);

	if (wait_ret > 0)
	{
		// positive (at least 1, or number of jiffies left till timeout) if completed
		LOCKOBJ_LOCK(obj->mutx_);

		if (out_ptr != NULL) ret = copy_to_user(out_ptr, obj->notify_ptr_, obj->notify_value_size_) ? SFW_E_ACCESS_DENIED : SFW_S_OK;
		memset(obj->notify_ptr_, 0, obj->notify_value_size_);

		LOCKOBJ_UNLOCK(obj->mutx_);
	}
	else if (wait_ret < 0)
	{
		// -ERESTARTSYS if interrupted
		ret = SFW_S_INTERRUPT_QUIT;
	}
	else
	{
		// 0 if timed out
		ret = SFW_S_OK;
	}

	return obj->is_finish_request(obj) ? SFW_S_INTERRUPT_QUIT : ret;
}

/*----------------------------------------------------------------------------
	notify
----------------------------------------------------------------------------*/
/**
 *	@fn			void wait_notify_notify(WaitNotifyObject* obj, NotifyUserIdType index)
 *	@brief		作業完了通知
 *	@param[in]	NotifyUserIdType index
 *	@return		なし
 */
static void wait_notify_notify(WaitNotifyObject* obj, NotifyUserIdType pattern_index)
{
	LOCKOBJ_LOCK(obj->mutx_);
	{
		if (obj->new_version_)
		{
			*(obj->notify_ptr_ + (pattern_index / 8)) |= (unsigned char)(1 << (pattern_index % 8));
		}
		else
		{
			obj->notify_value_ |= pattern_index;
		}
	}
	LOCKOBJ_UNLOCK(obj->mutx_);

	if (!completion_done(&obj->comp_))
	{
		complete(&obj->comp_);
	}
}

/*----------------------------------------------------------------------------
	attach
----------------------------------------------------------------------------*/
/**
 *	@fn			void wait_notify_attach(WaitNotifyObject* obj)
 *	@brief		参照を追加
 *	@param		なし
 *	@return		int 現在の参照カウンタ
 */
static int wait_notify_attach(WaitNotifyObject* obj)
{
	return ++ obj->ref_count_;
}

/*----------------------------------------------------------------------------
	detach
----------------------------------------------------------------------------*/
/**
 *	@fn			void wait_notify_detach(WaitNotifyObject* obj)
 *	@brief		参照を削除
 *	@param		なし
 *	@return		int 現在の参照カウンタ
 */
static int wait_notify_detach(WaitNotifyObject* obj)
{
	return obj->ref_count_ > 0 ? (-- obj->ref_count_) : 0;
}

/*----------------------------------------------------------------------------
	is_wait
----------------------------------------------------------------------------*/
/**
 *	@fn			bool wait_notify_is_wait(WaitNotifyObject* obj)
 *	@brief		待ち中かどうか
 *	@param		なし
 *	@return		bool true...待ち中, false...待ってない
 */
static bool wait_notify_is_wait(WaitNotifyObject* obj)
{
	return obj->wait_; //!completion_done(&obj->comp_);
}

/*----------------------------------------------------------------------------
	is_finish_request
----------------------------------------------------------------------------*/
/**
 *	@fn			bool wait_notify_is_finish_request(WaitNotifyObject* obj)
 *	@brief		終了要求が来ているかどうか
 *	@param		なし
 *	@return		bool true...要求あり, false...要求なし
 */
static bool wait_notify_is_finish_request(WaitNotifyObject* obj)
{
	#ifdef IMX_ZANTEI
	return test_bit(1, &obj->finish_request_);
	#else
	return constant_test_bit(1, &obj->finish_request_);
	#endif // IMX_ZANTEI
}

/*----------------------------------------------------------------------------
	set_wait_state
----------------------------------------------------------------------------*/
/**
 *	@fn			void wait_notify_set_wait_state(WaitNotifyObject* obj, bool state)
 *	@brief		待ち状態の設定
 *	@param[in]	状態
 *	@return		なし
 */
static void wait_notify_set_wait_state(WaitNotifyObject* obj, bool state)
{
	obj->wait_ = state;
}

/*----------------------------------------------------------------------------
	request_finish
----------------------------------------------------------------------------*/
/**
 *	@fn			void wait_notify_request_finish(WaitNotifyObject* obj)
 *	@brief		終了要求
 *	@param		なし
 *	@return		なし
 */
static void wait_notify_request_finish(WaitNotifyObject* obj)
{
	set_bit(1, &obj->finish_request_);

	if (!completion_done(&obj->comp_))
	{
		complete(&obj->comp_);
	}
}

/*----------------------------------------------------------------------------
	set_newversion
----------------------------------------------------------------------------*/
/**
 *	@fn			void wait_notify_set_newversion(WaitNotifyObject* obj)
 *	@brief		新しいバージョンにする
 *	@param		なし
 *	@return		Result SFW_S_OK...OK, SFW_E_INIT_MEMORY_ALLOCATOR...NG
 */
static Result wait_notify_set_newversion(WaitNotifyObject* obj)
{
	LOCKOBJ_LOCK(obj->mutx_);

	if (obj->new_version_ == false)
	{
		int new_buff_size = (ALIGN(get_maxsvc(), 8) / 8) * sizeof(char);
		unsigned char* new_buff = kmalloc(new_buff_size, GFP_KERNEL);

		if (new_buff == NULL)
		{
			SFW_ERROR_MSG("[Error] %s : new obj->notify_value_.\n", __FUNCTION__);
			LOCKOBJ_UNLOCK(obj->mutx_);
			return SFW_E_INIT_MEMORY_ALLOCATOR;
		}

		memset(new_buff, 0, new_buff_size);

		// update new version
		obj->notify_ptr_ = new_buff;
		obj->notify_value_size_ = new_buff_size;
		obj->new_version_ = true;
	}

	LOCKOBJ_UNLOCK(obj->mutx_);

	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	InitializeWaitNotifyObject
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result InitializeWaitNotifyObject(WaitNotifyObject* obj)
 *	@brief		コンストラクタ
 *	@param		なし
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
Result InitializeWaitNotifyObject(WaitNotifyObject* obj, NotifyGroupIdType id)
{
	if (obj == NULL) return SFW_E_INVALIDARG;

	mutex_init(&obj->mutx_);
	init_completion(&obj->comp_);

	obj->notify_value_size_		= sizeof(obj->notify_value_);
	obj->notify_value_          = 0;
	obj->notify_ptr_			= (unsigned char*)&obj->notify_value_;

	obj->id_					= id;
	obj->ref_count_				= 1;
	obj->wait_ 					= false;
	obj->new_version_ 			= false;
	obj->jiffies_				= msecs_to_jiffies(100);
 	obj->finish_request_ 		= 0;

	obj->wait					= wait_notify_wait;
	obj->notify					= wait_notify_notify;
	obj->attach					= wait_notify_attach;
	obj->detach					= wait_notify_detach;
	obj->is_wait				= wait_notify_is_wait;
	obj->is_finish_request		= wait_notify_is_finish_request;
	obj->set_wait_state			= wait_notify_set_wait_state;
	obj->request_finish			= wait_notify_request_finish;
	obj->set_newversion			= wait_notify_set_newversion;

	return SFW_S_OK;
}

/*--------------------------------------------------------------------------------
	FinalizeWaitNotifyObject
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result FinalizeWaitNotifyObject(WaitNotifyObject* obj)
 *	@brief		デストラクタ
 *	@param		なし
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
Result FinalizeWaitNotifyObject(WaitNotifyObject* obj)
{
	if (obj == NULL) return SFW_E_INVALIDARG;

	LOCKOBJ_LOCK(obj->mutx_);

	if (obj->new_version_ && (obj->notify_ptr_ != NULL)) kfree(obj->notify_ptr_);

	LOCKOBJ_UNLOCK(obj->mutx_);

	return SFW_S_OK;
}
