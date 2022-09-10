/********************************************************************************/
/** @file		event_listener_reg_unreg.h
	@brief		event listener logger
	@note		Copyright (C) 2015 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2015/08/26
	@version	1.0
*********************************************************************************/
#if !defined(_EL_REGUNREG__INCLUDED_)
#define _EL_REGUNREG__INCLUDED_

/*--------------------------------------------------------------------------------
								include files
--------------------------------------------------------------------------------*/
#include "event_listener.h"

/*--------------------------------------------------------------------------------
							   interface define
--------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------
								local define
--------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------
								type rename
--------------------------------------------------------------------------------*/
typedef struct _ELRegUnregObject							ELRegUnregObjectType;
typedef struct _MessageContainerObject						ELRegUnregMCObjectType;

/*--------------------------------------------------------------------------------
							  typedef function
--------------------------------------------------------------------------------*/
typedef Result (*ELRegUnreg_add)(ELRegUnregObjectType*, ELRegUnregMCObjectType*);
typedef Result (*ELRegUnreg_delete)(ELRegUnregObjectType*, ELRegUnregMCObjectType*);
typedef ELRegUnregMCObjectType* (*ELRegUnreg_search)(ELRegUnregObjectType*, NotifyGroupIdType);

/*--------------------------------------------------------------------------------
							  local typedef struct
--------------------------------------------------------------------------------*/
typedef struct _MCObjListInfo
{
	ELRegUnregMCObjectType*									mc_obj;
	struct list_head 										list;

} MCObjListInfo;

/*--------------------------------------------------------------------------------
						Object (ELRegUnregObject)
--------------------------------------------------------------------------------*/
typedef struct _ELRegUnregObject
{
	/* +++ base object +++ */
	EventListenerObjectDefine

	MCObjListInfo											mc_obj_list_;
	unsigned int											counter_;

	/* +++ public member function +++ */
	ELRegUnreg_add											add;
	ELRegUnreg_delete										del;
	ELRegUnreg_search										search;

} ELRegUnregObject;

/*--------------------------------------------------------------------------------
						interface of ELRegUnregObject
--------------------------------------------------------------------------------*/
extern Result InitializeELRegUnregObject(ELRegUnregObject* obj);

#endif // _EL_REGUNREG__INCLUDED_
