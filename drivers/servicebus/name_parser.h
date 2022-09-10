/********************************************************************************/
/** @file		name_parser.h
	@brief		サービス名パーサ
	@note		Copyright (C) 2013 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2013/02/26
	@version	1.0
*********************************************************************************/
#if !defined(_NAME_PARSER__INCLUDED_)
#define _NAME_PARSER__INCLUDED_

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
typedef struct _NameParserObject							NameParserObjectType;

/*--------------------------------------------------------------------------------
							  typedef function
--------------------------------------------------------------------------------*/
typedef bool (*NameParser_Parse)(NameParserObjectType*, const char* name);
typedef bool (*NameParser_AssignUserName)(NameParserObjectType*);
typedef bool (*NameParser_AssignHostName)(NameParserObjectType*);
typedef const char* (*NameParser_GetServiceName)(NameParserObjectType*);
typedef const char* (*NameParser_GetUserName)(NameParserObjectType*);
typedef const char* (*NameParser_GetHostName)(NameParserObjectType*);
typedef const char* (*NameParser_GetFullName)(NameParserObjectType*);

/*--------------------------------------------------------------------------------
							  local typedef struct
--------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------
						Object (NameParserObject)
--------------------------------------------------------------------------------*/
typedef struct _NameParserObject
{
	/* +++ member value +++ */
	bool											host_name_assign_;
	bool											user_name_assign_;
	bool											padding[2];

	char											service_name_[SERVICE_NAME_LEN_MAX + STR_TERMINATE_SIZE];
	char											host_name_[HOST_NAME_LEN_MAX + STR_TERMINATE_SIZE];
	char											user_name_[USER_NAME_LEN_MAX + STR_TERMINATE_SIZE];
	char											full_name_[FULL_NAME_LEN_MAX + STR_TERMINATE_SIZE];

	/* +++ public member function +++ */
	NameParser_Parse								parse;						// parse method
	NameParser_AssignUserName						assign_user_name;			// assign_user_name method
	NameParser_AssignHostName						assign_host_name;			// assign_host_name method
	NameParser_GetServiceName						get_service_name;			// get_service_name method
	NameParser_GetUserName							get_user_name;				// get_user_name method
	NameParser_GetHostName							get_host_name;				// get_host_name method
	NameParser_GetFullName							get_full_name;				// get_full_name method

} NameParserObject;

/*--------------------------------------------------------------------------------
					interface of InitializeNameParserObject
--------------------------------------------------------------------------------*/
extern Result InitializeNameParserObject(NameParserObject* obj);

#endif // _NAME_PARSER__INCLUDED_
