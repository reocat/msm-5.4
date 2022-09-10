/********************************************************************************/
/** @file		servicebus_if_ioctl.c
	@brief		サービスバスシステムコール受信部
	@note		Copyright (C) 2013 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2013/09/26
	@version	1.0
*********************************************************************************/
/*--------------------------------------------------------------------------------
	Include
--------------------------------------------------------------------------------*/
#include "servicebus_common.h"													// service bus common include
#include "servicebus_if_id.h"													// service bus if id

/*--------------------------------------------------------------------------------
	Define
--------------------------------------------------------------------------------*/
#define BUS_PARAM_COPY(type)	type param; \
	if (copy_from_user(&param, (int __user*)arg, sizeof(param))) return SFW_E_ACCESS_DENIED

/*
// svc_bus_ioctl
*/
long svc_bus_ioctl(struct file* filp, unsigned int cmd, unsigned long arg)
{
	int ret = SFW_E_INVALIDARG;

/*
	if (!access_ok(VERIFY_WRITE, (void __user*)arg, _IOC_SIZE(cmd)))
	{
		SFW_ERROR_MSG("error cannot access memory %s() cmd=%d ret=%d", __func__, cmd, ret );
		return SFW_E_POINTER;
	}
*/
	switch (cmd)
	{
	case SVC_BUS_REGISTER:
		{
			BUS_PARAM_COPY(SvcBusRegisterPara);
			ret = svc_bus_register(param.name, param.id, param.pattern);
		}
		break;

	case SVC_BUS_UNREGISTER:
		{
			BUS_PARAM_COPY(SvcBusUnregisterPara);
			ret = svc_bus_unregister(param.name);
		}
		break;

	case SVC_BUS_SEND:
		{
			BUS_PARAM_COPY(SvcBusSendPara);
			ret = svc_bus_send(param.name, param.data, param.data_size);
		}
		break;

	case SVC_BUS_SET:
		{
			BUS_PARAM_COPY(SvcBusSetPara);
			ret = svc_bus_set(param.name, param.data, param.data_size);
		}
		break;

	case SVC_BUS_WAIT_RECEIVE:
		{
			BUS_PARAM_COPY(SvcBusWaitReceivePara);
			ret = svc_bus_wait_receive(param.id, param.buf_ptr);
		}
		break;

	case SVC_BUS_RECEIVE:
		{
			BUS_PARAM_COPY(SvcBusReceivePara);
			ret = svc_bus_receive(param.name, param.buf_ptr, param.buf_len, param.read_len);
		}
		break;

	case SVC_BUS_CONTROL:
		{
			BUS_PARAM_COPY(SvcBusControlPara);
			ret = svc_bus_control(param.id, param.param);
		}
		break;

	case SVC_BUS_REGISTER_EX:
		{
			BUS_PARAM_COPY(SvcBusRegisterExPara);
			ret = svc_bus_register_ex(param.name, param.id, param.index);
		}
		break;

	case SVC_BUS_REGISTER_MULTI:
		{
			BUS_PARAM_COPY(SvcBusRegisterMultiPara);
			ret = svc_bus_register_multi(param.name, param.id, param.pattern);
		}
		break;

	case SVC_BUS_UNREGISTER_MULTI:
		{
			BUS_PARAM_COPY(SvcBusUnregisterMultiPara);
			ret = svc_bus_unregister_multi(param.name, param.id);
		}
		break;

	case SVC_BUS_RECEIVE_MULTI:
		{
			BUS_PARAM_COPY(SvcBusReceiveMultiPara);
			ret = svc_bus_receive_multi(param.id, param.buf_ptr, param.buf_len, param.read_len);
		}
		break;

	default:
		break;
	}

	// if (SFW_FAILED(ret)) SFW_ERROR_MSG("error %s() cmd=%x ret=%d", __func__, cmd, ret);

	return ret;
}

/*
// svc_bus_ioctl_open
*/
int svc_bus_ioctl_open(struct inode* inode, struct file* file_ptr)
{
	return 0;
}

/*
// svc_bus_ioctl_close
*/
int svc_bus_ioctl_close(struct inode* inode, struct file* file_ptr)
{
	return 0;
}
