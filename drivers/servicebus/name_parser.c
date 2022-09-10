/********************************************************************************/
/** @file		name_parser.c
	@brief		サービス名パーサ
	@note		Copyright (C) 2013 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2013/02/26
	@version	1.0
*********************************************************************************/
/*--------------------------------------------------------------------------------
	Include
--------------------------------------------------------------------------------*/
#include "servicebus_common.h"													// service bus common include
#include "name_parser.h"														// name parser include

/*--------------------------------------------------------------------------------
	Local define
--------------------------------------------------------------------------------*/
#define SERVICE_BUS_DEFAULT_USER_NAME		("anonymous")

/*----------------------------------------------------------------------------
	external funciton
----------------------------------------------------------------------------*/
extern char* get_hostname(void);

/*--------------------------------------------------------------------------------
	parse
--------------------------------------------------------------------------------*/
/**
 *	@fn			bool name_parser_parse(NameParserObject* obj, const char* name)
 *	@brief		パースの処理の実施
 *	@param[in]	const char* name
 *	@return		bool true...OK, falseK...NG
 */
static bool name_parser_parse(NameParserObject* obj, const char* name)
{
	char* pos_atmark = NULL;
	char* pos_slash = NULL;
	char* pos_serivce_start = (char*)name;
	bool parse_result = false;

	// check param
	if ((name == NULL) || (strlen(name) == 0)) return false;

	// init value
	obj->host_name_assign_ = false;
	obj->user_name_assign_ = false;

	obj->service_name_[0]  = 0;
	obj->host_name_[0] 	   = 0;
	obj->user_name_[0]	   = 0;

	// memcpy(obj->host_name_, SERVICE_BUS_DEFAULT_HOST_NAME, strlen(SERVICE_BUS_DEFAULT_HOST_NAME) + 1);
	// memcpy(obj->user_name_, SERVICE_BUS_DEFAULT_USER_NAME, strlen(SERVICE_BUS_DEFAULT_USER_NAME) + 1);

	/*
	// ユーザ名を示す@を探す
	//          v
	// user_name@host_name/service_name
	*/
	pos_atmark = strchr(name, '@');

	// 見つからない場合、発見場所が先頭の場合などはデフォルト値のまま
	if (pos_atmark != NULL)
	{
		int len = pos_atmark - name;

		if (len > 0)
		{
			if (len > USER_NAME_LEN_MAX) len = USER_NAME_LEN_MAX;

			memcpy(obj->user_name_, name, len);

			obj->user_name_[len] = 0;
			obj->user_name_assign_ = true;
		}
	}

	/*
	// ホスト名を示す/を探す
	//                    v
	// user_name@host_name/service_name
	//           host_name/service_name
	*/
	pos_slash = strchr(name, '/');

	// 見つからない場合、発見場所が先頭の場合などはデフォルト値のまま
	if (pos_slash != NULL)
	{
		char* slash_start = (pos_atmark == NULL) ? (char*)name : pos_atmark + 1;

		int len = pos_slash - slash_start;

		if (len > 0)
		{
			if (len > HOST_NAME_LEN_MAX) len = HOST_NAME_LEN_MAX;

			memcpy(obj->host_name_, slash_start, len);

			obj->host_name_[len] = 0;
			obj->host_name_assign_ = true;
		}

	}

	/*
	// 以下の全パターンからサービス名を取得する
	//
	// user_name@host_name/service_name
	//           host_name/service_name
	// user_name@          service_name
	//                     service_name
	*/

	if (pos_slash != NULL)
	{
		pos_serivce_start = pos_slash + 1;
	}
	else if (pos_atmark != NULL)
	{
		pos_serivce_start = pos_atmark + 1;
	}

	// copy service name
	{
		int len = strlen(name) - (pos_serivce_start - name);

		// サービス名を指定していなかった場合はlenが0となるので、コピーしない
		if (len > 0)
		{
			if (len > SERVICE_NAME_LEN_MAX) len = SERVICE_NAME_LEN_MAX;

			memcpy(obj->service_name_, pos_serivce_start, len);

			obj->service_name_[len] = 0;
			parse_result = true;
		}
	}

	return parse_result;
}

/*--------------------------------------------------------------------------------
	assign_user_name
--------------------------------------------------------------------------------*/
/**
 *	@fn			bool name_parser_assign_user_name(NameParserObject* obj)
 *	@brief		ユーザ名が指定されたかどうか
 *	@param 		なし
 *	@return		bool true...OK, falseK...NG
 */
static bool name_parser_assign_user_name(NameParserObject* obj)
{
	return obj->user_name_assign_;
}

/*--------------------------------------------------------------------------------
	assign_host_name
--------------------------------------------------------------------------------*/
/**
 *	@fn			bool name_parser_assign_host_name(NameParserObject* obj)
 *	@brief		ホスト名が指定されたかどうか
 *	@param 		なし
 *	@return		bool true...OK, falseK...NG
 */
static bool name_parser_assign_host_name(NameParserObject* obj)
{
	return obj->host_name_assign_;
}

/*--------------------------------------------------------------------------------
	get_service_name
--------------------------------------------------------------------------------*/
/**
 *	@fn			const char* name_parser_get_service_name(NameParserObject* obj)
 *	@brief		サービス名の取得
 *	@param 		なし
 *	@return		const char*
 */
static const char* name_parser_get_service_name(NameParserObject* obj)
{
	return obj->service_name_;
}

/*--------------------------------------------------------------------------------
	get_user_name
--------------------------------------------------------------------------------*/
/**
 *	@fn			const char* name_parser_get_user_name(NameParserObject* obj)
 *	@brief		ユーザ名の取得
 *	@param 		なし
 *	@return		const char*
 */
static const char* name_parser_get_user_name(NameParserObject* obj)
{
	return obj->user_name_assign_ ? obj->user_name_ : SERVICE_BUS_DEFAULT_USER_NAME;
}

/*--------------------------------------------------------------------------------
	get_host_name
--------------------------------------------------------------------------------*/
/**
 *	@fn			const char* name_parser_get_host_name(NameParserObject* obj)
 *	@brief		ホスト名の取得
 *	@param 		なし
 *	@return		const char*
 */
static const char* name_parser_get_host_name(NameParserObject* obj)
{
	return obj->host_name_assign_ ? obj->host_name_ : get_hostname();
}

/*--------------------------------------------------------------------------------
	get_full_name
--------------------------------------------------------------------------------*/
/**
 *	@fn			const char* name_parser_get_full_name(NameParserObject* obj)
 *	@brief		フルネームの取得
 *	@param 		なし
 *	@return		const char*
 */
static const char* name_parser_get_full_name(NameParserObject* obj)
{
	obj->full_name_[0] = 0;

	strcat(obj->full_name_, obj->get_user_name(obj));
	strcat(obj->full_name_, "@");
	strcat(obj->full_name_, obj->get_host_name(obj));
	strcat(obj->full_name_, "/");
	strcat(obj->full_name_, obj->get_service_name(obj));

	return obj->full_name_;
}

/*--------------------------------------------------------------------------------
	InitializeNameParserObject
--------------------------------------------------------------------------------*/
/**
 *	@fn			Result InitializeNameParserObject(NameParserObject* obj)
 *	@brief		コンストラクタ
 *	@param		なし
 *	@return		Result SFW_S_OK...OK, !SFW_S_OK...NG
 */
Result InitializeNameParserObject(NameParserObject* obj)
{
	if (obj == NULL) return SFW_E_INVALIDARG;

	obj->host_name_assign_		= false;
	obj->user_name_assign_		= false;

	obj->service_name_[0] 		= 0;
	obj->host_name_[0] 			= 0;
	obj->user_name_[0] 			= 0;
	obj->full_name_[0] 			= 0;

	obj->parse					= name_parser_parse;
	obj->assign_user_name		= name_parser_assign_user_name;
	obj->assign_host_name 		= name_parser_assign_host_name;
	obj->get_service_name 		= name_parser_get_service_name;
	obj->get_user_name 			= name_parser_get_user_name;
	obj->get_host_name 			= name_parser_get_host_name;
	obj->get_full_name			= name_parser_get_full_name;

	return SFW_S_OK;
}
