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

#include <string.h>
#include <curl/curl.h>
#include <algorithm> 
#include <map>
#include <list>
#include <assert.h>  
#include "ToolMacros.h"
#include "lua.hpp"
#include "LuaWebClient.h"

#ifdef _MSC_VER
#pragma comment(lib, "libcurl.lib")
#endif

#define LUA_WEB_CLIENT			("luna.webclient")
#define LUA_WEB_CLIENT_MT		("com.dpull.lib.WebClientMT")

using namespace std;

struct CWebRequest
{
    void* m_pIndex;
    char  m_szError[CURL_ERROR_SIZE];
    char  m_szIP[16];
    CWebClient* m_pWebClient;
};

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

bool CWebClient::RealQuery(void** pHandle, char** ppszError)
{
   	while (true)
    {
        int nLeftMessageCount;
        CURLMsg* pCurMsg = curl_multi_info_read(m_pCurlMHandle, &nLeftMessageCount);
        if (!pCurMsg)
            return false;
        
        if (pCurMsg->msg != CURLMSG_DONE)
            continue;
        
        CWebRequestTable::iterator it = m_WebRequestTable.find(pCurMsg->easy_handle);
        if (it == m_WebRequestTable.end())
        {
            Log(eLogError, "CWebClient::RealQuery unknown handle:%p", pCurMsg->easy_handle);
            curl_multi_remove_handle(m_pCurlMHandle, pCurMsg->easy_handle);
            continue;
        }
        
        CWebRequest* pRequest = (CWebRequest*)it->second;
        *pHandle = pRequest->m_pIndex;
        *ppszError = pRequest->m_szError;
        
        return true;
    }
}

bool CWebClient::Query(void** pHandle, char** ppszError)
{
    if (RealQuery(pHandle, ppszError))
        return true;
    
    int nRunningHandleCount;
    CURLMcode euRetCode = curl_multi_perform(m_pCurlMHandle, &nRunningHandleCount);
    if (euRetCode != CURLM_OK && euRetCode != CURLM_CALL_MULTI_PERFORM)
        return false;
    
    return RealQuery(pHandle, ppszError);
}

void CWebClient::RemoveRequest(void* pHandle)
{
    CWebRequestTable::iterator it = m_WebRequestTable.find(pHandle);
    if (it != m_WebRequestTable.end())
    {
        curl_multi_remove_handle(m_pCurlMHandle, pHandle);
        curl_easy_cleanup(pHandle);
        C_DELETE(it->second);
        m_WebRequestTable.erase(it);
    }
}

void* CWebClient::Request(const char szUrl[], const char* pPostData, size_t uPostDataLen)
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
    curl_easy_setopt(pHandle, CURLOPT_SSL_VERIFYPEER, false);
    curl_easy_setopt(pHandle, CURLOPT_SSL_VERIFYHOST, false);
    
    if (pPostData)
    {
        curl_easy_setopt(pHandle, CURLOPT_POSTFIELDSIZE, (long)uPostDataLen);
        curl_easy_setopt(pHandle, CURLOPT_POSTFIELDS, pPostData);
    }

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
    CURL*       m_pUrlEncoding;
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
    pLuaClient->m_pUrlEncoding = NULL;

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
        
        if (pLuaClient->m_pUrlEncoding)
        {
            curl_easy_cleanup(pLuaClient->m_pUrlEncoding);
            pLuaClient->m_pUrlEncoding = NULL;
        }
	}
	return 0;
}

static int LuaQuery(lua_State* L)
{
	CWebClient* pClient = GetWebClient(L, 1);
	if (!pClient)
		return luaL_argerror(L, 1, "parameter self invalid");

    void* pIndex = NULL;
    char* pszError = NULL;
	bool bRetCode = pClient->Query(&pIndex, &pszError);
    if (!bRetCode)
        return 0;
    
    lua_pushlightuserdata(L, pIndex);
    lua_pushstring(L, pszError);
    
	return 2;
}

static int LuaRequest(lua_State* L)
{
    int nTop = lua_gettop(L);
	CWebClient* pClient = GetWebClient(L, 1);
	const char* pszUrl = lua_tostring(L, 2);
    
	if (!pClient)
		return luaL_argerror(L, 1, "parameter self invalid");

	if (!pszUrl)
		return luaL_argerror(L, 2, "parameter url invalid");

    const char* pPost = NULL;
    size_t uLen = 0;
    
    if (nTop > 2)
        pPost = lua_tolstring(L, 3, &uLen);
    
	void* pIndex = pClient->Request(pszUrl, pPost, uLen);
	if (pIndex)
		lua_pushlightuserdata(L, pIndex);
	else
		lua_pushnil(L);

	return 1;
}

static int LuaRemoveRequest(lua_State* L)
{
    CWebClient* pClient = GetWebClient(L, 1);
    void* pIndex = lua_touserdata(L, 2);
    
    if (!pClient)
        return luaL_argerror(L, 1, "parameter self invalid");
    
    if (!pIndex)
        return luaL_argerror(L, 2, "parameter index invalid");
    
    pClient->RemoveRequest(pIndex);
    return 0;
}

static int LuaUrlEncoding(lua_State* L)
{
    CLuaWebClient* pLuaClient = (CLuaWebClient*)luaL_checkudata(L, 1, LUA_WEB_CLIENT_MT);
    if (!pLuaClient)
        return luaL_argerror(L, 1, "parameter self invalid");
    
    if (!pLuaClient->m_pUrlEncoding)
        pLuaClient->m_pUrlEncoding = curl_easy_init();

	size_t uLen = 0;
	const char* pszString = lua_tolstring(L, 2, &uLen);
	char* pszResult = NULL;

	pszResult = curl_easy_escape(pLuaClient->m_pUrlEncoding, pszString, (int)uLen);
	if (!pszResult)
    {
        lua_pushlstring(L, pszString, uLen);
        return 1;
    }
    
    lua_pushstring(L, pszResult);
    curl_free(pszResult);
    return 1;
}

luaL_Reg WebClientCreateFuns[] = {
	{ "Create", LuaCreate },
	{ NULL, NULL }
};

luaL_Reg WebClientFuns[] = {
	{ "__gc", LuaDestory },
	{ "Query", LuaQuery },
	{ "Request", LuaRequest },
    { "RemoveRequest", LuaRemoveRequest },
    { "UrlEncoding", LuaUrlEncoding },
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



