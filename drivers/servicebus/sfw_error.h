/********************************************************************************/
/** @file		sfw_error.h
	@brief		サービスフレームワーク エラー
	@note		Copyright (C) 2014 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2014/01/23
	@version	1.0
*********************************************************************************/
#if !defined(_SERVICE_FRAMEWORK_ERROR_H__INCLUDED_)
#define _SERVICE_FRAMEWORK_ERROR_H__INCLUDED_

typedef int Result;
typedef int SFW_RESULT;

// Severity values
#define SFW_SEVERITY_SUCCESS						(0)
#define SFW_SEVERITY_ERROR							(1)

// Generic test for success on any status value (non-negative numbers indicate success).
#define SFW_SUCCEEDED(Status)						((SFW_RESULT)(Status) >= 0)
#define SFW_FAILED(Status)							((SFW_RESULT)(Status) < 0)

// Return the code
#define SFW_RESULT_CODE(hr)							((hr) & 0xFFFF)

// Generic test for error on any status value.
#define SFW_IS_ERROR(Status)						((unsigned long)(Status) >> 31 == SEVERITY_ERROR)

// Create an HRESULT value from component pieces
#define SFW_MAKE_RESULT(sev, fac, code)				((SFW_RESULT) (((unsigned long)(sev)<<31) | ((unsigned long)(fac)<<16) | ((unsigned long)(code))))

// The operation completed successfully.
#define SFW_ERROR_SUCCESS							((SFW_RESULT)0)				///< The operation completed successfully
#define SFW_ERROR_INVALID_FUNCTION					((SFW_RESULT)1)				///< Incorrect function
#define SFW_ERROR_FILE_NOT_FOUND					((SFW_RESULT)2)				///< The system cannot find the file specified
#define SFW_ERROR_PATH_NOT_FOUND					((SFW_RESULT)3)				///< The system cannot find the path specified
#define SFW_ERROR_TOO_MANY_OPEN_FILES				((SFW_RESULT)4)				///< The system cannot open the file
#define SFW_ERROR_ACCESS_DENIED						((SFW_RESULT)5)				///< Access is denied
#define SFW_ERROR_INVALID_HANDLE					((SFW_RESULT)6)				///< The handle is invalid
#define SFW_ERROR_NOT_ENOUGH_MEMORY					((SFW_RESULT)7)				///< Not enough storage is available to process this command
#define SFW_ERROR_BAD_FORMAT						((SFW_RESULT)8)				///< An attempt was made to load a program with an incorrect format
#define SFW_ERROR_INVALID_ACCESS					((SFW_RESULT)9)				///< The access code is invalid
#define SFW_ERROR_INVALID_DATA						((SFW_RESULT)10)			///< The data is invalid
#define SFW_ERROR_OUTOFMEMORY						((SFW_RESULT)11)			///< Not enough storage is available to complete this operation
#define SFW_ERROR_BAD_LENGTH						((SFW_RESULT)12)			///< The program issued a command but the command length is incorrect
#define SFW_ERROR_NOT_SUPPORTED						((SFW_RESULT)13)			///< The request is not supported
#define SFW_ERROR_NETWORK_BUSY						((SFW_RESULT)14)			///< The network is busy
#define SFW_ERROR_INVALID_PARAMETER					((SFW_RESULT)15)			///< The parameter is incorrect
#define SFW_ERROR_INVALID_POINTER					((SFW_RESULT)16)			///< The parameter is incorrect pointer
#define SFW_ERROR_SIZELIMIT							((SFW_RESULT)17)			///< The size limit for this request was exceeded
#define SFW_ERROR_ALREADY_EXISTS					((SFW_RESULT)18)			///< Already exists
#define SFW_ERROR_UPDATE							((SFW_RESULT)19)			///< Over write message
#define SFW_ERROR_INTERRUPT_QUIT					((SFW_RESULT)20)			///< interrupt quit

/*--------------------------------------------------------------------------------
	Common Error code
--------------------------------------------------------------------------------*/
// SFW_SEVERITY_SUCCESS
#define SFW_S_OK									SFW_MAKE_RESULT(SFW_SEVERITY_SUCCESS, 0, SFW_ERROR_SUCCESS)
#define SFW_S_SIZELIMIT								SFW_MAKE_RESULT(SFW_SEVERITY_SUCCESS, 0, SFW_ERROR_SIZELIMIT)
#define SFW_S_UPDATE								SFW_MAKE_RESULT(SFW_SEVERITY_SUCCESS, 0, SFW_ERROR_UPDATE)
#define SFW_S_INTERRUPT_QUIT						SFW_MAKE_RESULT(SFW_SEVERITY_SUCCESS, 0, SFW_ERROR_INTERRUPT_QUIT)

// SFW_SEVERITY_ERROR
#define SFW_E_FAIL									SFW_MAKE_RESULT(SFW_SEVERITY_ERROR,   0, SFW_ERROR_ACCESS_DENIED)
#define SFW_E_OUTOFMEMORY							SFW_MAKE_RESULT(SFW_SEVERITY_ERROR,   0, SFW_ERROR_OUTOFMEMORY)
#define SFW_E_INVALIDARG							SFW_MAKE_RESULT(SFW_SEVERITY_ERROR,   0, SFW_ERROR_INVALID_PARAMETER)
#define SFW_E_POINTER								SFW_MAKE_RESULT(SFW_SEVERITY_ERROR,   0, SFW_ERROR_INVALID_POINTER)
#define SFW_E_HANDLE								SFW_MAKE_RESULT(SFW_SEVERITY_ERROR,   0, SFW_ERROR_INVALID_HANDLE)
#define SFW_E_INIT_MEMORY_ALLOCATOR					SFW_MAKE_RESULT(SFW_SEVERITY_ERROR,   0, SFW_ERROR_NOT_ENOUGH_MEMORY)
#define SFW_E_INVALID_SIZELIMIT						SFW_MAKE_RESULT(SFW_SEVERITY_ERROR,   0, SFW_ERROR_SIZELIMIT)
#define SFW_E_INVALID_DATA							SFW_MAKE_RESULT(SFW_SEVERITY_ERROR,   0, SFW_ERROR_INVALID_DATA)
#define SFW_E_ACCESS_DENIED							SFW_MAKE_RESULT(SFW_SEVERITY_ERROR,   0, SFW_ERROR_ACCESS_DENIED)
#define SFW_E_BAD_LENGTH							SFW_MAKE_RESULT(SFW_SEVERITY_ERROR,   0, SFW_ERROR_BAD_LENGTH)
#define SFW_E_ALREADY_EXISTS						SFW_MAKE_RESULT(SFW_SEVERITY_ERROR,   0, SFW_ERROR_ALREADY_EXISTS)

#endif // !defined(_SERVICE_FRAMEWORK_ERROR_H__INCLUDED_)
