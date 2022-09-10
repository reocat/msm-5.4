/********************************************************************************/
/** @file		lock_obj.h 
	@brief		lock class helper
	@note		Copyright (C) 2013 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2013/02/25
	@version	1.0
*********************************************************************************/
#if !defined(_LOCK_OBJ__INCLUDED_)
#define _LOCK_OBJ__INCLUDED_

/*--------------------------------------------------------------------------------
								include files
--------------------------------------------------------------------------------*/
#include <linux/mutex.h>

/*--------------------------------------------------------------------------------
							   interface define
--------------------------------------------------------------------------------*/
#define LOCKOBJ(obj)									DEFINE_MUTEX(obj)
#define LOCKOBJ_LOCK(obj)								mutex_lock(&(obj))
#define LOCKOBJ_UNLOCK(obj)								mutex_unlock(&(obj))

#endif // _LOCK_OBJ__INCLUDED_
