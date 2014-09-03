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

class CWebClient;
struct CWebRequest
{
	void* m_pIndex;
	char  m_szError[CURL_ERROR_SIZE];
	char  m_szIP[16];
	CWebClient* m_pWebClient;
};

typedef std::map<CURL*, CWebRequest*> CWebRequestTable;
typedef std::list<CWebRequest*> CWebDataList;
typedef size_t CWebClientWriteCallback(char* pszBuffer, size_t uBlockSize, size_t uCount, void* pvArg);

class CWebClient
{
public:
	CWebClient();
	virtual ~CWebClient();

public:
	bool Setup(CWebClientWriteCallback* pCallback, void* pvUserData);
	void Clear();
	
	CURLMcode Query(CWebDataList* pRetWebData);

	void* Download(const char szUrl[]);

	const void* GetUserData() { return m_pvUserData; }

private:
	static size_t OnWebDataCallback(void* pvData, size_t uBlock, size_t uCount, void* pvArg);

private:
	CWebRequestTable			m_WebRequestTable;
	CURLM*						m_pCurlMHandle;
	CWebClientWriteCallback*	m_pCallback;
	void*						m_pvUserData;
};
