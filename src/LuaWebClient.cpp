/****************************************************************************
Copyright (c) 2014      dpull.com

http://www.dpull.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/

#include "ToolMacros.h"
#include "lua.hpp"
#include <curl/curl.h>
#include <algorithm> 
#include <map>
#include <list>
#include <assert.h>  
#include "LuaWebClient.h"

#ifdef _MSC_VER
#pragma comment(lib, "libcurl_imp.lib")
#endif

#define MEM_STREAM_DEFAULT_SIZE	(8 * 1024)
#define LUA_WEB_CLIENT			("luna.webclient")
#define LUA_WEB_CLIENT_MT		("com.dpull.lib.WebClientMT")

using namespace std;

CWebClient::CWebClient()
{
	m_pCurlMHandle = NULL;
	m_pCallback = NULL;
	m_pvUserData = NULL;
}

CWebClient::~CWebClient()
{
	Clear();
}

bool CWebClient::Setup(CWebClientWriteCallback* pCallback, void* pvUserData)
{
	bool bResult = false;

	assert(pCallback);
	m_pCallback = pCallback;
	m_pvUserData = pvUserData;

	curl_global_init(CURL_GLOBAL_ALL);

	m_pCurlMHandle = curl_multi_init();
	C_FAILED_JUMP(m_pCurlMHandle);

	bResult = true;
Exit0:
	return bResult;
}

void CWebClient::Clear()
{
	for (CWebRequestTable::iterator it = m_WebRequestTable.begin(); it != m_WebRequestTable.end(); ++it)
	{
		CURL* pHandle = it->first;
		CWebRequest* pData = it->second;

		C_DELETE(pData);
		curl_easy_cleanup(pHandle);
		curl_multi_remove_handle(m_pCurlMHandle, pHandle);
	}
	m_WebRequestTable.clear();

	if (m_pCurlMHandle)
	{
		curl_multi_cleanup(m_pCurlMHandle);
		m_pCurlMHandle = NULL;
	}

	curl_global_cleanup();

	m_pCallback = NULL;
	m_pvUserData = NULL;
}

CURLMcode CWebClient::Query(CWebDataList* pWebDataList)
{
	int nResult = CURLM_LAST;
	CURLMsg* pCurMsg = NULL;
	CURLMcode euRetCode = CURLM_LAST;
	int nRunningHandleCount = 0;
	int nLeftMessageCount = 0;

	euRetCode = curl_multi_perform(m_pCurlMHandle, &nRunningHandleCount);
	C_FAILED_RET_CODE(((euRetCode == CURLM_OK) || (euRetCode == CURLM_CALL_MULTI_PERFORM)), euRetCode);

	while (true)
	{
		pCurMsg = curl_multi_info_read(m_pCurlMHandle, &nLeftMessageCount);
		if (!pCurMsg)
			break;

		if (pCurMsg->msg != CURLMSG_DONE)
			continue;

		CWebRequestTable::iterator it = m_WebRequestTable.find(pCurMsg->easy_handle);
		if (it != m_WebRequestTable.end())
		{
			CURL* pHandle = it->first;
			CWebRequest* pData = it->second;
			const char* pszIP = NULL;
			CURLcode euCurlRetCode = curl_easy_getinfo(pHandle, CURLINFO_PRIMARY_IP, &pszIP);

			if (euCurlRetCode == CURLE_OK && pszIP)
			{
				strncpy(pData->m_szIP, pszIP, sizeof(pData->m_szIP) / sizeof(pData->m_szIP[0]));
			}

			curl_easy_cleanup(pHandle);
			pWebDataList->push_back(pData);
			m_WebRequestTable.erase(it);
			continue;
		}

		curl_multi_remove_handle(m_pCurlMHandle, pCurMsg->easy_handle);
	}

	nResult = CURLM_OK;
Exit0:
	return (CURLMcode)nResult;
}

void* CWebClient::Download(const char szUrl[])
{
	void* pResult = 0;
	CURLMcode euRetCode = CURLM_LAST;
	CWebRequest* pWebData = new CWebRequest;
	CURL* pHandle = curl_easy_init();

	CLOG_FAILED_JUMP(pWebData);
	CLOG_FAILED_JUMP(pHandle);

	pWebData->m_pIndex = pHandle;
	pWebData->m_szError[0] = '\0';
	pWebData->m_szIP[0] = '\0';
	pWebData->m_pWebClient = this;

	curl_easy_setopt(pHandle, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(pHandle, CURLOPT_WRITEFUNCTION, m_pCallback);
	curl_easy_setopt(pHandle, CURLOPT_WRITEDATA, pWebData);
	curl_easy_setopt(pHandle, CURLOPT_ERRORBUFFER, pWebData->m_szError);
	curl_easy_setopt(pHandle, CURLOPT_CONNECTTIMEOUT_MS, 5000);
	curl_easy_setopt(pHandle, CURLOPT_URL, szUrl);

	euRetCode = curl_multi_add_handle(m_pCurlMHandle, pHandle);
	CLOG_FAILED_JUMP(euRetCode == CURLM_OK);

	m_WebRequestTable[pHandle] = pWebData;

	pResult = pWebData->m_pIndex;
Exit0:
	if (!pResult)
	{
		C_DELETE(pWebData);
		if (pHandle)
		{
			curl_easy_cleanup(pHandle);
			pHandle = NULL;
		}
	}
	return pResult;
}

struct CLuaWebClient
{
	CWebClient* m_pWebClient;
	lua_State*	m_pLuaState;
	int			m_nLuaFuncRef;
};

static size_t LuaWriteCallback(char* pszBuffer, size_t uBlockSize, size_t uCount, void* pvArg)
{
	CWebRequest* pWebData = (CWebRequest*)pvArg;
	assert(pWebData);

	size_t uLen = uBlockSize * uCount;
	if (pWebData->m_szError[0] != '\0')
		return uLen;

    bool bRetCode = false;
	CLuaWebClient* pLuaWebClient = (CLuaWebClient*)pWebData->m_pWebClient->GetUserData();

    int nTop = lua_gettop(pLuaWebClient->m_pLuaState);
    
	lua_rawgeti(pLuaWebClient->m_pLuaState, LUA_REGISTRYINDEX, pLuaWebClient->m_nLuaFuncRef);
	if (lua_isfunction(pLuaWebClient->m_pLuaState, -1))
	{
        
        
        lua_pushlightuserdata(pLuaWebClient->m_pLuaState, pWebData->m_pIndex);
		lua_pushlstring(pLuaWebClient->m_pLuaState, pszBuffer, uLen);
        
        bRetCode = Lua_XCall(pLuaWebClient->m_pLuaState, 2, 0);
		if (!bRetCode)
		{
			strncpy(pWebData->m_szError, "XStream error!", sizeof(pWebData->m_szError) / sizeof(pWebData->m_szError[0]));
		}
	}
    
    lua_settop(pLuaWebClient->m_pLuaState, nTop);
	return uLen;
}

static CWebClient* GetWebClient(lua_State* L, int nIndex)
{
	CLuaWebClient* pLuaClient = (CLuaWebClient*)luaL_checkudata(L, nIndex, LUA_WEB_CLIENT_MT);
	if (pLuaClient)
	{
		return pLuaClient->m_pWebClient;
	}
	return NULL;
}

static int LuaCreate(lua_State* L)
{
	if (!lua_isfunction(L, 1))
		return luaL_argerror(L, 1, "parameter callback invalid");

	bool bRetCode = false;
	CWebClient* pClient = new CWebClient;
	CLuaWebClient* pLuaClient = (CLuaWebClient*)lua_newuserdata(L, sizeof(CLuaWebClient));
    int nTop = lua_gettop(L);

	pLuaClient->m_pLuaState = L;
    lua_pushvalue(L, 1);
	pLuaClient->m_nLuaFuncRef = luaL_ref(L, LUA_REGISTRYINDEX);
	pLuaClient->m_pWebClient = pClient;

	bRetCode = pClient->Setup(LuaWriteCallback, pLuaClient);
	if (!bRetCode)
	{
		luaL_unref(L, LUA_REGISTRYINDEX, pLuaClient->m_nLuaFuncRef);
		pLuaClient->m_nLuaFuncRef = LUA_NOREF;
		pLuaClient->m_pLuaState = NULL;
		pLuaClient->m_pWebClient = NULL;
		C_DELETE(pClient);
		return luaL_error(L, "webclient create failed");
	}

    lua_settop(L, nTop);
	luaL_getmetatable(L, LUA_WEB_CLIENT_MT);
	lua_setmetatable(L, -2);
	return 1;
}

static int LuaDestory(lua_State* L)
{
	CLuaWebClient* pLuaClient = (CLuaWebClient*)luaL_checkudata(L, 1, LUA_WEB_CLIENT_MT);
	if (pLuaClient)
	{
		luaL_unref(L, LUA_REGISTRYINDEX, pLuaClient->m_nLuaFuncRef);
		pLuaClient->m_nLuaFuncRef = LUA_NOREF;
		pLuaClient->m_pLuaState = NULL;
		C_DELETE(pLuaClient->m_pWebClient);
	}
	return 0;
}

static int LuaQuery(lua_State* L)
{
	CWebClient* pClient = GetWebClient(L, 1);
	CWebDataList DataList;

	if (!pClient)
		return luaL_argerror(L, 1, "parameter self invalid");

	pClient->Query(&DataList);

	lua_newtable(L);
	for (CWebDataList::iterator it = DataList.begin(); it != DataList.end(); ++it)
	{
		CWebRequest* pData = *it;
		lua_pushlightuserdata(L, pData->m_pIndex);

		if (pData->m_szError[0] != '\0')
		{
			lua_newtable(L);
			lua_pushstring(L, pData->m_szError);
			lua_setfield(L, -2, "error");

			lua_pushstring(L, pData->m_szIP);
			lua_setfield(L, -2, "ip");
		}
		else
		{
			lua_newtable(L);
		}
		lua_settable(L, -3);
		C_DELETE(pData);
	}

	return 1;
}

static int LuaDownload(lua_State* L)
{
	CWebClient* pClient = GetWebClient(L, 1);
	const char* pszUrl = lua_tostring(L, 2);

	if (!pClient)
		return luaL_argerror(L, 1, "parameter self invalid");

	if (!pszUrl)
		return luaL_argerror(L, 2, "parameter url invalid");

	void* pIndex = pClient->Download(pszUrl);
	if (pIndex)
		lua_pushlightuserdata(L, pIndex);
	else
		lua_pushnil(L);

	return 1;
}

static int LuaQueryProgressStatus(lua_State* L)
{
	CURLcode euRetCode = CURL_LAST;
	CWebClient* pClient = GetWebClient(L, 1);
	void* pIndex = lua_touserdata(L, 2);
	double dwContentLen = 0.0f;
	double dwDownloadLen = 0.0f;

	if (!pClient)
		return luaL_argerror(L, 1, "parameter self invalid");

	if (!pIndex)
		return luaL_argerror(L, 2, "parameter index invalid");

	euRetCode = curl_easy_getinfo(pIndex, CURLINFO_SIZE_DOWNLOAD, &dwDownloadLen);
	if (euRetCode != CURLE_OK)
		return 0;

	euRetCode = curl_easy_getinfo(pIndex, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &dwContentLen);
	if (euRetCode != CURLE_OK)
		return 0;

	lua_pushnumber(L, dwDownloadLen);
	lua_pushnumber(L, dwContentLen);
	return 2;
}

static int LuaUrlEncoding(lua_State* L)
{
	CURL* pHandle = curl_easy_init();
	size_t uLen = 0;
	const char* pszUrl = lua_tolstring(L, 1, &uLen);
	char* pszResult = NULL;

	CLOG_FAILED_JUMP(pHandle);
	CLOG_FAILED_JUMP(pszUrl);

	pszResult = curl_easy_escape(pHandle, pszUrl, uLen);
	if (pszResult)
	{
		lua_pushstring(L, pszResult);
		curl_free(pszResult);
	}

Exit0:
	if (pHandle)
	{
		curl_easy_cleanup(pHandle);
		pHandle = NULL;
	}
	return 1;
}

luaL_Reg WebClientCreateFuns[] = {
	{ "Create", LuaCreate },
	{ "UrlEncoding", LuaUrlEncoding },
	{ NULL, NULL }
};

luaL_Reg WebClientFuns[] = {
	{ "__gc", LuaDestory },
	{ "Query", LuaQuery },
	{ "Download", LuaDownload },
	{ "QueryProgressStatus", LuaQueryProgressStatus },
	{ NULL, NULL }
};

#if (LUA_VERSION_NUM == 501)
LUNA_API int luaopen_luna_webclient(lua_State * L)
{
	if (luaL_newmetatable(L, LUA_WEB_CLIENT_MT))
	{
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
		luaL_register(L, NULL, WebClientFuns);
		lua_pop(L, 1);
	}

	luaL_register(L, LUA_WEB_CLIENT, WebClientCreateFuns);
	return 1;
}
#elif (LUA_VERSION_NUM == 502)
LUNA_API int luaopen_luna_webclient(lua_State * L)
{
	luaL_checkversion(L);

	if (luaL_newmetatable(L, LUA_WEB_CLIENT_MT))
	{
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
		luaL_setfuncs(L, WebClientFuns, 0);
		lua_pop(L, 1);
	}

	luaL_newlib(L, WebClientCreateFuns);
	return 1;
}
#endif



