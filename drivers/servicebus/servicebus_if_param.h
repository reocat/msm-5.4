/********************************************************************************/
/** @file		servicebus_if_param.h
	@brief		interface param
	@note		Copyright (C) 2013 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2013/09/26
	@version	1.0
*********************************************************************************/
#if !defined(_SERVICEBUS_IF_PARAM__INCLUDED_)
#define _SERVICEBUS_IF_PARAM__INCLUDED_

/*--------------------------------------------------------------------------------
	Parameter Struct
--------------------------------------------------------------------------------*/
typedef struct _SvcBusRegisterPara
{
	const char*			name;
	int					id;
	unsigned long long	pattern;

} SvcBusRegisterPara;

typedef struct _SvcBusRegisterExPara
{
	const char*			name;
	int					id;
	unsigned int		index;

} SvcBusRegisterExPara;

typedef struct _SvcBusRegisterMultiPara
{
	const char*			name;
	int					id;
	unsigned long long	pattern;

} SvcBusRegisterMultiPara;

typedef struct _SvcBusUnregisterPara
{
	const char*			name;

} SvcBusUnregisterPara;

typedef struct _SvcBusUnregisterMultiPara
{
	const char*			name;
	int					id;

} SvcBusUnregisterMultiPara;

typedef struct _SvcBusSendPara
{
	const char*			name;
	const char*			data;
	unsigned int		data_size;

} SvcBusSendPara;

typedef struct _SvcBusSetPara
{
	const char*			name;
	const char*			data;
	unsigned int		data_size;

} SvcBusSetPara;

typedef struct _SvcBusWaitReceivePara
{
	int					id;
	void*				buf_ptr;

} SvcBusWaitReceivePara;

typedef struct _SvcBusReceivePara
{
	const char*			name;
	char*				buf_ptr;
	int					buf_len;
	int*				read_len;

} SvcBusReceivePara;

typedef struct _SvcBusReceiveMultiPara
{
	int					id;
	char*				buf_ptr;
	int					buf_len;
	int*				read_len;

} SvcBusReceiveMultiPara;

typedef struct _SvcBusControlPara
{
	int					id;
	void*				param;

} SvcBusControlPara;

#endif  /* _SERVICEBUS_IF_PARAM__INCLUDED_ */
