/********************************************************************************/
/** @file		wn_repository.h
	@brief		wn repository
	@note		Copyright (C) 2015 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2015/08/20
	@version	1.0
*********************************************************************************/
#if !defined(_WN_REPOSITORY__INCLUDED_)
#define _WN_REPOSITORY__INCLUDED_

/*--------------------------------------------------------------------------------
								include files
--------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------
							   interface define
--------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------
								local define
--------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------
								type rename
--------------------------------------------------------------------------------*/
typedef struct _WNRepositoryObject							WNRepositoryObjectType;
typedef struct _WaitNotifyObject							WNRepositoryWaitNotifyObjectType;

/*--------------------------------------------------------------------------------
							  typedef function
--------------------------------------------------------------------------------*/
typedef WNRepositoryWaitNotifyObjectType* (*WNRepository_GetInstance)(WNRepositoryObjectType*, NotifyGroupIdType);
typedef Result (*WNRepository_ReleaseInstance)(WNRepositoryObjectType*, WNRepositoryWaitNotifyObjectType*);
typedef WNRepositoryWaitNotifyObjectType* (*WNRepository_CreateInstance)(WNRepositoryObjectType*, NotifyGroupIdType);
typedef Result (*WNRepository_DestroyInstance)(WNRepositoryObjectType*, WNRepositoryWaitNotifyObjectType*);
typedef WNRepositoryWaitNotifyObjectType* (*WNRepository_Find)(WNRepositoryObjectType*, NotifyGroupIdType);
typedef Result (*WNRepository_Exit)(WNRepositoryObjectType*);

/*--------------------------------------------------------------------------------
							  local typedef struct
--------------------------------------------------------------------------------*/
typedef struct _WaitNotifyList
{
	WNRepositoryWaitNotifyObjectType* wait_notity;
	struct list_head list;

} WaitNotifyList;

/*--------------------------------------------------------------------------------
						Object (WNRepositoryObject)
--------------------------------------------------------------------------------*/
typedef struct _WNRepositoryObject
{
	/* +++ member value +++ */
	WaitNotifyList 									waitnotify_rep_;

	/* +++ public member function +++ */
	WNRepository_GetInstance						get_instance;				// get wait notify object
	WNRepository_ReleaseInstance					release_instance;			// release wait notify object
	WNRepository_Find								find;						// find wait notify object
	WNRepository_Exit								exit;						// exit wait notify object

	/* +++ private member function +++ */
	WNRepository_CreateInstance						create;						// create wait notify object
	WNRepository_DestroyInstance					destroy;					// destroy wait notify object

} WNRepositoryObject;

/*--------------------------------------------------------------------------------
						interface of WNRepositoryObject
--------------------------------------------------------------------------------*/
extern Result InitializeWNRepositoryObject(WNRepositoryObject* obj);
extern Result FinalizeWNRepositoryObject(WNRepositoryObject* obj);

#endif // _WN_REPOSITORY__INCLUDED_
