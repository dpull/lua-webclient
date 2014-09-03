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

enum LogType
{
	eLogError = 0,
	eLogWarning,
	eLogInfo,
	eLogDebug
};

#ifndef _USE_CUSTOM_LOG
#define Log(eLevel, cszFormat, ...)	fprintf(stderr, cszFormat, ##__VA_ARGS__)	  
#endif

#ifdef _MSC_VER
    #define __THIS_FUNCTION__   __FUNCTION__
#endif

#if defined(__linux) || defined(__APPLE__)
    #define __THIS_FUNCTION__   __PRETTY_FUNCTION__
#endif

#define C_FAILED_JUMP(Condition) \
    do  \
    {   \
        if (!(Condition))   \
            goto Exit0;     \
    } while (false)

#define C_FAILED_RET_CODE(Condition, nCode) \
    do  \
    {   \
        if (!(Condition))   \
        {   \
            nResult = nCode;    \
            goto Exit0;         \
        }   \
    } while (false)

#define C_SUCCESS_JUMP(Condition) \
    do  \
    {   \
        if (Condition)      \
            goto Exit1;     \
    } while (false)
	
#define CCOM_FAILED_JUMP(Condition) \
    do  \
    {   \
        if (FAILED(Condition))   \
            goto Exit0;     \
    } while (false)
	
#define CLOG_FAILED_JUMP(Condition) \
    do  \
    {   \
        if (!(Condition))   \
        {                   \
            Log(            \
                eLogDebug,  \
                    "CLOG_FAILED_JUMP(%s) at %s:%d in %s", #Condition, __FILE__, __LINE__, __THIS_FUNCTION__   \
            );              \
            goto Exit0;     \
        }                   \
    } while (false)

#define C_DELETE_ARRAY(pArray)     \
    do  \
    {   \
        if (pArray)                 \
        {                           \
            delete[] (pArray);      \
            (pArray) = NULL;        \
        }                           \
    } while (false)


#define C_DELETE(p)    \
    do  \
    {   \
        if (p)              \
        {                   \
            delete (p);     \
            (p) = NULL;     \
        }                   \
    } while (false)

#define C_FREE(pvBuffer) \
    do  \
    {   \
        if (pvBuffer)               \
        {                           \
            free(pvBuffer);         \
            (pvBuffer) = NULL;      \
        }                           \
    } while (false)


#define C_COM_RELEASE(pInterface) \
    do  \
    {   \
        if (pInterface)                 \
        {                               \
            (pInterface)->Release();    \
            (pInterface) = NULL;        \
        }                               \
    } while (false)

#define C_CLOSE_FILE(p)    \
    do  \
    {   \
        if  (p) \
        {       \
            fclose(p);  \
            (p) = NULL; \
        }   \
    } while (false)

#define C_CLOSE_FD(p)    \
    do  \
    {   \
        if  ((p) >= 0) \
        {       \
            close(p);  \
            (p) = -1; \
        }   \
    } while (false)


#define C_FREE_STRING(pvString) \
    do \
    { \
    if (pvString) \
        { \
        PlatformFreeString(pvString); \
        pvString = NULL; \
        } \
    } while (false)


// 用来取代MS的MAKELONG,MAKEWORD
#define MAKE_DWORD(a, b)      ((DWORD)(((WORD)(((DWORD)(a)) & 0xffff)) | ((DWORD)((WORD)(((DWORD)(b)) & 0xffff))) << 16))
#define MAKE_WORD(a, b)      ((WORD)(((BYTE)(((DWORD)(a)) & 0xff)) | ((WORD)((BYTE)(((DWORD)(b)) & 0xff))) << 8))
#define _countof(_Array) (sizeof(_Array) / sizeof(_Array[0]))

#if defined(__linux) || defined(__APPLE__)
    #define LOWORD(l)           ((WORD)(((DWORD)(l)) & 0xffff))
    #define HIWORD(l)           ((WORD)((((DWORD)(l)) >> 16) & 0xffff))
    #define LOBYTE(w)           ((BYTE)(((DWORD)(w)) & 0xff))
    #define HIBYTE(w)           ((BYTE)((((DWORD)(w)) >> 8) & 0xff))
#endif

typedef unsigned char       BYTE;
