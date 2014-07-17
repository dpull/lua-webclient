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

#pragma once

class CWebStream
{
public:
	virtual ~CWebStream() {}
	virtual BOOL Save(void* pvData, size_t uLen) = 0;
	virtual BOOL PushResultTable(lua_State* L) = 0;
};

class CWebMemStream :public CWebStream
{
public:
	CWebMemStream();
	virtual ~CWebMemStream();

	virtual BOOL Save(void* pvData, size_t uLen);
	virtual BOOL PushResultTable(lua_State* L);

private:
	char*  m_pszData;
	size_t m_uDataSize;
	size_t m_uDataLen;
};

class CWebFileStream :public CWebStream
{
public:
	CWebFileStream(const char szFile[]);
	virtual ~CWebFileStream();

	virtual BOOL Save(void* pvData, size_t uLen);
	virtual BOOL PushResultTable(lua_State* L);

private:
	FILE* m_pFile;
};

class CWebRequest
{
public:
	CWebRequest();
	virtual ~CWebRequest();

public:
	void*			m_pIndex;
	CWebStream*		m_pStream;
	char			m_szError[CURL_ERROR_SIZE];
	char			m_szIP[16];
};

typedef std::map<CURL*, CWebRequest*> CWebRequestTable;
typedef std::list<CWebRequest*> CWebDataList;

class CWebClient
{
public:
	CWebClient();
	virtual ~CWebClient();

public:
	BOOL Setup();
	void Clear();
	
	// XY_DELETE 每个XWebData*
	void Query(CWebDataList* pRetWebData);

	void* DownloadData(const char szUrl[]);
	void* DownloadFile(const char szUrl[], const char szFile[]);

	CWebStream* GetWebStream(void* pIndex);

private:
	static size_t OnWebDataCallback(void* pvData, size_t uBlock, size_t uCount, void* pvArg);

private:
	CWebRequestTable	m_WebRequestTable;
	CURLM*				m_pCurlMHandle;
};
