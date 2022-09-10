/********************************************************************************/
/** @file		wait_notify.h
	@brief		ユーザイベント通知オブジェクト
	@note		Copyright (C) 2013 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2013/10/04
	@version	1.0
*********************************************************************************/
#if !defined(_WAIT_NOTIFY__INCLUDED_)
#define _WAIT_NOTIFY__INCLUDED_

/*--------------------------------------------------------------------------------
								include files
--------------------------------------------------------------------------------*/
#include <linux/completion.h>													// completion
#include <linux/mutex.h>

/*--------------------------------------------------------------------------------
							   interface define
--------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------
								local define
--------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------
								type rename
--------------------------------------------------------------------------------*/
typedef struct _WaitNotifyObject								WaitNotifyObjectType;

/*--------------------------------------------------------------------------------
							  typedef function
--------------------------------------------------------------------------------*/
typedef Result (*WaitNotify_wait)(WaitNotifyObjectType*, void __user *);
typedef void (*WaitNotify_notify)(WaitNotifyObjectType*, NotifyUserIdType pattern);
typedef int (*WaitNotify_attach)(WaitNotifyObjectType*);
typedef int (*WaitNotify_detach)(WaitNotifyObjectType*);
typedef bool (*WaitNotify_is_wait)(WaitNotifyObjectType*);
typedef void (*WaitNotify_set_wait_state)(WaitNotifyObjectType*, bool);
typedef bool (*WaitNotify_is_finish_request)(WaitNotifyObjectType*);
typedef void (*WaitNotify_request_finish)(WaitNotifyObjectType*);
typedef Result (*WaitNotify_set_newversion)(WaitNotifyObjectType*);

/*--------------------------------------------------------------------------------
							  local typedef struct
--------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------
						Object (WaitNotifyObject)
--------------------------------------------------------------------------------*/
typedef struct _WaitNotifyObject
{
	/* +++ member value +++ */
	struct mutex 									mutx_;
	struct completion								comp_;
	NotifyUserIdType								notify_value_;
	unsigned char*									notify_ptr_;
	int												notify_value_size_;

	NotifyGroupIdType								id_;
	int												ref_count_;
	unsigned long									jiffies_;
	volatile long									finish_request_;
	bool 											wait_;
	bool											new_version_;

	/* +++ public member function +++ */
	WaitNotify_wait									wait;
	WaitNotify_notify								notify;
	WaitNotify_attach								attach;
	WaitNotify_detach								detach;
	WaitNotify_is_wait								is_wait;
	WaitNotify_set_wait_state						set_wait_state;
	WaitNotify_is_finish_request					is_finish_request;
	WaitNotify_request_finish						request_finish;
	WaitNotify_set_newversion						set_newversion;

} WaitNotifyObject;

/*--------------------------------------------------------------------------------
					interface of WaitNotifyObject
--------------------------------------------------------------------------------*/
extern Result InitializeWaitNotifyObject(WaitNotifyObject* obj, NotifyGroupIdType);
extern Result FinalizeWaitNotifyObject(WaitNotifyObject* obj);

#endif // _WAIT_NOTIFY__INCLUDED_
