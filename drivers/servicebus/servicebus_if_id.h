/********************************************************************************/
/** @file		servicebus_if_id.h
	@brief		interface id
	@note		Copyright (C) 2013 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2013/09/26
	@version	1.0
*********************************************************************************/
#if !defined(_SERVICEBUS_IF_ID__INCLUDED_)
#define _SERVICEBUS_IF_ID__INCLUDED_

/*--------------------------------------------------------------------------------
	Include
--------------------------------------------------------------------------------*/
#include <linux/ioctl.h>														// ioctl include
#include "servicebus_if_param.h"

/*--------------------------------------------------------------------------------
	Define
--------------------------------------------------------------------------------*/
#define IOC_MAGIC 'b'

#define SVC_BUS_REGISTER			_IOWR(IOC_MAGIC,  1, SvcBusRegisterPara)
#define SVC_BUS_UNREGISTER			_IOWR(IOC_MAGIC,  2, SvcBusUnregisterPara)
#define SVC_BUS_SEND				_IOWR(IOC_MAGIC,  3, SvcBusSendPara)
#define SVC_BUS_SET					_IOWR(IOC_MAGIC,  4, SvcBusSetPara)
#define SVC_BUS_WAIT_RECEIVE		_IOWR(IOC_MAGIC,  5, SvcBusWaitReceivePara)
#define SVC_BUS_RECEIVE				_IOWR(IOC_MAGIC,  6, SvcBusReceivePara)
#define SVC_BUS_CONTROL				_IOWR(IOC_MAGIC,  7, SvcBusControlPara)
#define SVC_BUS_REGISTER_EX			_IOWR(IOC_MAGIC,  8, SvcBusRegisterExPara)
#define SVC_BUS_REGISTER_MULTI		_IOWR(IOC_MAGIC,  9, SvcBusRegisterPara)
#define SVC_BUS_UNREGISTER_MULTI	_IOWR(IOC_MAGIC, 10, SvcBusUnregisterPara)
#define SVC_BUS_RECEIVE_MULTI		_IOWR(IOC_MAGIC, 11, SvcBusReceiveMultiPara)

#endif // _SERVICEBUS_IF_ID__INCLUDED_
