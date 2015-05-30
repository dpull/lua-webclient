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

bool Lua_XCall(lua_State* L, int nArgs, int nResults);



