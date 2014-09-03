// lua.hpp
// Lua header files for C++
// <<extern "C">> not supplied automatically because Lua also compiles as C++

extern "C"
{
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#if defined(LUA_BUILD_AS_DLL)
#define LUNA_API extern "C" __declspec(dllexport)
#else
#define LUNA_API extern "C"
#endif

#ifdef WIN32
#define LUA_STDCALL __stdcall
#else
#define LUA_STDCALL
#endif

inline bool Lua_XCall(lua_State* L, int nArgs, int nResults)
{
    bool bResult    = false;
    int  nRetCode   = 0;
    int  nFuncIdx   = lua_gettop(L) - nArgs;
    
    C_FAILED_JUMP(nFuncIdx > 0);
    
    lua_getglobal(L, "debug");
    C_FAILED_JUMP(lua_istable(L, -1));
    
    lua_getfield(L, -1, "traceback");
    C_FAILED_JUMP(lua_isfunction(L, -1));
    
    lua_remove(L, -2); // remove 'debug'
    lua_insert(L, nFuncIdx);
    nRetCode = lua_pcall(L, nArgs, nResults, nFuncIdx);
    if (nRetCode != 0)
    {
        const char* pszErrInfo = lua_tostring(L, -1);
        Log(eLogError, "[Lua] %s", pszErrInfo);
        goto Exit0;
    }
    
    bResult = true;
Exit0:
    return bResult;
}

inline bool HasUtf8BomHeader(const char* pszText)
{
    if (pszText[0] == (char)0xEF && pszText[1] == (char)0xBB && pszText[2] == (char)0xBF)
		return true;
    
	return false;
}