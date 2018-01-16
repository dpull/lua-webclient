#include <stdio.h>
#include "lauxlib.h"
#include "lualib.h"

extern int luaopen_webclient(lua_State * L);

int main(int argc, const char * argv[]) {
    lua_State* l = luaL_newstate();
    luaL_requiref(l, "webclient", luaopen_webclient, 0);
    luaL_openlibs(l);
    if (luaL_dofile(l, "main.lua"))
        printf("[Lua] %s\n", lua_tostring(l, -1));
    lua_close(l);
    return 0;
}
