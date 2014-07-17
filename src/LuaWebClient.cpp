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
#define LUA_WEB_CLIENT			("luabase.webclient")
#define LUA_WEB_CLIENT_MT		("com.dpull.lib.WebClientMT")

using namespace std;

CWebMemStream::CWebMemStream()
{
	m_uDataLen = 0;
	m_uDataSize = 0;
	m_pszData = NULL;
}

CWebMemStream::~CWebMemStream()
{
	if (m_pszData)
	{
		free(m_pszData);
		m_pszData = NULL;
	}
}

BOOL CWebMemStream::Save(void* pvData, size_t uLen)
{
	BOOL bResult = false;
	size_t uNewDataLen = m_uDataLen + uLen;

	if (uNewDataLen >= m_uDataSize)
	{
		m_uDataSize *= 2;
		m_uDataSize = max(m_uDataSize, (size_t)MEM_STREAM_DEFAULT_SIZE);
		m_uDataSize = max(uNewDataLen, m_uDataSize);

		m_pszData = (char*)realloc(m_pszData, m_uDataSize);
	}

	CLOG_FAILED_JUMP(m_pszData);

	memcpy(m_pszData + m_uDataLen, pvData, uLen);
	m_uDataLen = uNewDataLen;

	bResult = true;
Exit0:
	return bResult;
}

BOOL CWebMemStream::PushResultTable(lua_State* L)
{
	lua_newtable(L);
	lua_pushlstring(L, m_pszData, m_uDataLen);
	lua_setfield(L, -2, "data");
	return true;
}

CWebFileStream::CWebFileStream(const char szFile[])
{
	m_pFile = fopen(szFile, "wb");
	if (!m_pFile)
	{
		Log(eLogDebug, "XWebDataFileStream create file [%s] failed!", szFile);
	}
}

CWebFileStream::~CWebFileStream()
{
	if (m_pFile)
	{
		fclose(m_pFile);
		m_pFile = NULL;
	}
}
BOOL CWebFileStream::Save(void* pvData, size_t uLen)
{
	BOOL bResult = false;
	size_t uResult = 0;

	C_FAILED_JUMP(m_pFile);

	uResult = fwrite(pvData, uLen, 1, m_pFile);
	CLOG_FAILED_JUMP(uResult == 1);

	bResult = true;
Exit0:
	return bResult;
}

BOOL CWebFileStream::PushResultTable(lua_State* L)
{
	lua_newtable(L);
	lua_pushboolean(L, m_pFile != NULL);
	lua_setfield(L, -2, "file");
	return true;
}

CWebRequest::CWebRequest()
{
	m_pIndex = NULL;
	m_szError[0] = '\0';
	m_szIP[0] = '\0';
	m_pStream = NULL;
}

CWebRequest::~CWebRequest()
{
	C_DELETE(m_pStream);
}

CWebClient::CWebClient()
{
	m_pCurlMHandle = NULL;
}

CWebClient::~CWebClient()
{
	Clear();
}

BOOL CWebClient::Setup()
{
	BOOL bResult = false;

	curl_global_init(CURL_GLOBAL_ALL);

	m_pCurlMHandle = curl_multi_init();
	CLOG_FAILED_JUMP(m_pCurlMHandle);

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
}

void CWebClient::Query(CWebDataList* pWebDataList)
{
	CURLMsg* pCurMsg = NULL;
	CURLMcode euRetCode = CURLM_LAST;
	int nRunningHandleCount = 0;
	int nLeftMessageCount = 0;

	euRetCode = curl_multi_perform(m_pCurlMHandle, &nRunningHandleCount);
	if (euRetCode != CURLM_OK && euRetCode != CURLM_CALL_MULTI_PERFORM)
	{
		Log(eLogDebug, "curl_multi_perform failed, error code:%d", euRetCode);
		goto Exit0;
	}

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

			if (euRetCode == CURLE_OK && pszIP)
			{
				strncpy(pData->m_szIP, pszIP, _countof(pData->m_szIP));
			}

			curl_easy_cleanup(pHandle);
			pWebDataList->push_back(pData);
			m_WebRequestTable.erase(it);
			continue;
		}

		curl_multi_remove_handle(m_pCurlMHandle, pCurMsg->easy_handle);
	}

Exit0:
	return;
}

size_t CWebClient::OnWebDataCallback(void* pvData, size_t uBlock, size_t uCount, void* pvArg)
{
	CWebRequest* pWebData = (CWebRequest*)pvArg;
	assert(pWebData);

	size_t uLen = uBlock * uCount;
	if (pWebData->m_szError[0] != '\0')
		return uLen;

	if (!pWebData->m_pStream->Save(pvData, uLen))
		strcpy(pWebData->m_szError, "XStream error!");

	return uLen;
}

void* CWebClient::DownloadData(const char szUrl[])
{
	void* pResult = 0;
	CURLMcode euRetCode = CURLM_LAST;
	CWebRequest* pWebData = new CWebRequest;
	CURL* pHandle = curl_easy_init();

	CLOG_FAILED_JUMP(pWebData);
	CLOG_FAILED_JUMP(pHandle);

	pWebData->m_pIndex = pHandle;

	pWebData->m_pStream = new CWebMemStream;
	CLOG_FAILED_JUMP(pWebData->m_pStream);

	curl_easy_setopt(pHandle, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(pHandle, CURLOPT_WRITEFUNCTION, CWebClient::OnWebDataCallback);
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

void* CWebClient::DownloadFile(const char szUrl[], const char szFile[])
{
	void* pResult = 0;
	CURLMcode euRetCode = CURLM_LAST;
	CWebRequest* pWebData = new CWebRequest;
	CURL* pHandle = curl_easy_init();

	CLOG_FAILED_JUMP(pWebData);
	CLOG_FAILED_JUMP(pHandle);

	pWebData->m_pIndex = pHandle;

	pWebData->m_pStream = new CWebFileStream(szFile);
	CLOG_FAILED_JUMP(pWebData->m_pStream);

	curl_easy_setopt(pHandle, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(pHandle, CURLOPT_WRITEFUNCTION, CWebClient::OnWebDataCallback);
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

CWebStream* CWebClient::GetWebStream(void* pIndex)
{
	CWebRequestTable::iterator it = m_WebRequestTable.find(pIndex);
	if (it != m_WebRequestTable.end())
		return it->second->m_pStream;
	return NULL;
}

int LuaCreate(lua_State* L)
{
	BOOL bResult = false;
	BOOL bRetCode = false;
	CWebClient* pClient = new CWebClient;
	CWebClient** ppUserValue = NULL;

	bRetCode = pClient->Setup();
	CLOG_FAILED_JUMP(bRetCode);

	ppUserValue = (CWebClient**)lua_newuserdata(L, sizeof(pClient));
	*ppUserValue = pClient;

	luaL_getmetatable(L, LUA_WEB_CLIENT_MT);
	lua_setmetatable(L, -2);

Exit0:
	return 1;
}

CWebClient* GetWebClient(lua_State* L, int nIndex)
{
	CWebClient** ppUserValue = (CWebClient**)luaL_checkudata(L, nIndex, LUA_WEB_CLIENT_MT);
	if (ppUserValue)
	{
		return *ppUserValue;
	}
	return NULL;
}

int LuaDestory(lua_State* L)
{
	CWebClient* pClient = GetWebClient(L, 1);
	C_DELETE(pClient);
	return 0;
}

int LuaQuery(lua_State* L)
{
	CWebClient* pClient = GetWebClient(L, 1);
	CWebDataList DataList;

	if (!pClient)
		return luaL_error(L, "parameter self invalid");

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
			pData->m_pStream->PushResultTable(L);
		}
		lua_settable(L, -3);
		C_DELETE(pData);
	}

	return 1;
}

int LuaDownloadData(lua_State* L)
{
	CWebClient* pClient = GetWebClient(L, 1);
	const char* pszUrl = lua_tostring(L, 2);

	if (!pClient)
		return luaL_error(L, "parameter self invalid");

	if (!pszUrl)
		return luaL_error(L, "parameter url invalid");

	void* pIndex = pClient->DownloadData(pszUrl);
	if (pIndex)
		lua_pushlightuserdata(L, pIndex);
	else
		lua_pushnil(L);

	return 1;
}

int LuaDownloadFile(lua_State* L)
{
	CWebClient* pClient = GetWebClient(L, 1);
	const char* pszUrl = lua_tostring(L, 2);
	const char* pszFile = lua_tostring(L, 3);

	if (!pClient)
		return luaL_error(L, "parameter self invalid");

	if (!pszUrl)
		return luaL_error(L, "parameter url invalid");

	if (!pszFile)
		return luaL_error(L, "parameter file invalid");

	void* pIndex = pClient->DownloadFile(pszUrl, pszFile);
	if (pIndex)
		lua_pushlightuserdata(L, pIndex);
	else
		lua_pushnil(L);

	return 1;
}

int LuaQueryProgressStatus(lua_State* L)
{
	CURLcode euRetCode = CURL_LAST;
	CWebClient* pClient = GetWebClient(L, 1);
	void* pIndex = lua_touserdata(L, 2);
	CWebStream* pStream = NULL;
	double dwContentLen = 0.0f;
	double dwDownloadLen = 0.0f;

	if (!pClient)
		return luaL_error(L, "parameter self invalid");

	if (!pIndex)
		return luaL_error(L, "parameter index invalid");

	pStream = pClient->GetWebStream(pIndex);
	if (!pStream)
		return luaL_error(L, "parameter index not exist");


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

int LuaUrlEncoding(lua_State* L)
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
	{ "DownloadData", LuaDownloadData },
	{ "DownloadFile", LuaDownloadFile },
	{ "QueryProgressStatus", LuaQueryProgressStatus },
	{ NULL, NULL }
};

#if (LUA_VERSION_NUM == 501)
extern "C" int luaopen_luabase_webclient(lua_State * L)
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
extern "C" int luaopen_luabase_webclient(lua_State * L)
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



