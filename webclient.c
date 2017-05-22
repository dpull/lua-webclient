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

#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <curl/curl.h>
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#define IP_LENGTH               16
#define MAX(a, b)               (((a) > (b)) ? (a) : (b))
#define LUA_WEB_CLIENT_MT		("com.dpull.lib.WebClientMT")
#define ENABLE_FOLLOWLOCATION   1

struct webclient
{
    CURLM* curlm;
    CURL* encoding_curl;
};

struct webrequest
{
    CURL* curl;
    struct curl_slist* header;
    char error[CURL_ERROR_SIZE];
    char* content;
    size_t content_length;
    size_t content_maxlength;
    bool content_realloc_failed;
};

static int webclient_create(lua_State* l)
{
    curl_version_info_data* data = curl_version_info(CURLVERSION_NOW);
    if (data->version_num < 0x070F04)
        return luaL_error(l, "requires 7.15.4 or higher curl, current version is %s", data->version);
    
    curl_global_init(CURL_GLOBAL_ALL);
    CURLM* curlm = curl_multi_init();
    if (!curlm) {
        curl_global_cleanup();
        return luaL_error(l, "webclient create failed");
    }
    
    struct webclient* webclient = (struct webclient*)lua_newuserdata(l, sizeof(*webclient));
    webclient->curlm = curlm;
    webclient->encoding_curl = NULL;
 
    luaL_getmetatable(l, LUA_WEB_CLIENT_MT);
    lua_setmetatable(l, -2);

    return 1;
}

static int webclient_destory(lua_State* l)
{
    struct webclient* webclient = (struct webclient*)luaL_checkudata(l, 1, LUA_WEB_CLIENT_MT);
    if (!webclient)
        return luaL_argerror(l, 1, "parameter self invalid");
    
    if (webclient->encoding_curl) {
        curl_easy_cleanup(webclient->encoding_curl);
        webclient->encoding_curl = NULL;
    }
    curl_multi_cleanup(webclient->curlm);
    webclient->curlm = NULL;
    curl_global_cleanup();
    return 0;
}

static CURL* webclient_realquery(struct webclient* webclient)
{
    while (true)
    {
        int msgs_in_queue;
        CURLMsg* curlmsg = curl_multi_info_read(webclient->curlm, &msgs_in_queue);
        if (!curlmsg)
            return NULL;
        
        if (curlmsg->msg != CURLMSG_DONE)
            continue;
        
        return curlmsg->easy_handle;
    }
}

static int webclient_query(lua_State* l)
{
    struct webclient* webclient = (struct webclient*)luaL_checkudata(l, 1, LUA_WEB_CLIENT_MT);
    if (!webclient)
        return luaL_argerror(l, 1, "parameter self invalid");
    
    CURL* handle = webclient_realquery(webclient);
    if (handle) {
        lua_pushlightuserdata(l, handle);
        return 1;
    }
    
    int running_handles;
    CURLMcode euRetCode = curl_multi_perform(webclient->curlm, &running_handles);
    if (euRetCode != CURLM_OK && euRetCode != CURLM_CALL_MULTI_PERFORM) {
        return luaL_error(l, "webclient query failed");
    }
    
    handle = webclient_realquery(webclient);
    if (handle) {
        lua_pushlightuserdata(l, handle);
        return 1;
    }
    return 0;
}

static size_t write_callback(char* buffer, size_t block_size, size_t count, void* arg)
{
    struct webrequest* webrequest = (struct webrequest*)arg;
    assert(webrequest);
    
    size_t length = block_size * count;
    if (webrequest->content_realloc_failed)
        return length;
    
    if (webrequest->content_length + length > webrequest->content_maxlength) {
        webrequest->content_maxlength = MAX(webrequest->content_maxlength, webrequest->content_length + length);
        webrequest->content_maxlength = MAX(webrequest->content_maxlength, 512);
        webrequest->content_maxlength = 2 * webrequest->content_maxlength;

        void* new_content = (char*)realloc(webrequest->content, webrequest->content_maxlength);
        if (!new_content) {
            webrequest->content_realloc_failed = true;
            return length;
        }
        webrequest->content = new_content;
    }
    
    memcpy(webrequest->content + webrequest->content_length, buffer, length);
    webrequest->content_length += length;
    return length;
}

static struct webrequest* webclient_realrequest(struct webclient* webclient, const char* url, const char* postdata, size_t postdatalen, long connect_timeout_ms)
{
    struct webrequest* webrequest = (struct webrequest*)malloc(sizeof(*webrequest));
    memset(webrequest, 0, sizeof(*webrequest));
    
    CURL* handle = curl_easy_init();
    if (!handle)
        goto failed;
    
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, false);
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, false);
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, ENABLE_FOLLOWLOCATION);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, webrequest);
    curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, webrequest->error);
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, connect_timeout_ms);
    curl_easy_setopt(handle, CURLOPT_URL, url);
    
    if (postdata) {
        curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, (long)postdatalen);
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, postdata);
    }
    
    if (curl_multi_add_handle(webclient->curlm, handle) == CURLM_OK) {
        webrequest->curl = handle;
        return webrequest;
    }
    
failed:
    if (handle) {
        curl_easy_cleanup(handle);
        handle = NULL;
    }
    free(webrequest);
    return NULL;
}

static int webclient_request(lua_State* l)
{
    struct webclient* webclient = (struct webclient*)luaL_checkudata(l, 1, LUA_WEB_CLIENT_MT);
    if (!webclient)
        return luaL_argerror(l, 1, "parameter self invalid");
    
    const char* url = lua_tostring(l, 2);
    if (!url)
        return luaL_argerror(l, 2, "parameter url invalid");
    
    const char* postdata = NULL;
    size_t postdatalen = 0;
    long connect_timeout_ms = 5000;
    
    int top = lua_gettop(l);
    if (top > 2 && lua_isstring(l, 3)) 
        postdata = lua_tolstring(l, 3, &postdatalen);
    
    if (top > 3 && lua_isnumber(l, 4)) {
        connect_timeout_ms = lua_tointeger(l, 4);
        if (connect_timeout_ms < 0)
            return luaL_argerror(l, 4, "parameter connect_timeout_ms invalid");
    }
    
    struct webrequest* webrequest = webclient_realrequest(webclient, url, postdata, postdatalen, connect_timeout_ms);
    if (!webrequest)
        return 0;
    
    lua_pushlightuserdata(l, webrequest);
    lua_pushlightuserdata(l, webrequest->curl);
    return 2;
}

static int webclient_removerequest(lua_State* l)
{
    struct webclient* webclient = (struct webclient*)luaL_checkudata(l, 1, LUA_WEB_CLIENT_MT);
    if (!webclient)
        return luaL_argerror(l, 1, "parameter self invalid");
    
    struct webrequest* webrequest = (struct webrequest*)lua_touserdata(l, 2);
    if (!webrequest)
        return luaL_argerror(l, 2, "parameter index invalid");

    curl_multi_remove_handle(webclient->curlm, webrequest->curl);
    curl_easy_cleanup(webrequest->curl);
    curl_slist_free_all(webrequest->header);
    if (webrequest->content)
        free(webrequest->content);
    free(webrequest);
    return 0;
}

static int webclient_getrespond(lua_State* l)
{
    struct webclient* webclient = (struct webclient*)luaL_checkudata(l, 1, LUA_WEB_CLIENT_MT);
    if (!webclient)
        return luaL_argerror(l, 1, "parameter self invalid");
    
    struct webrequest* webrequest = (struct webrequest*)lua_touserdata(l, 2);
    if (!webrequest)
        return luaL_argerror(l, 2, "parameter index invalid");
    
    if (webrequest->content_realloc_failed) {
        strncpy(webrequest->error, "not enough memory.", sizeof(webrequest->error));
    }
    
    if (webrequest->error[0] == '\0') {
        lua_pushlstring(l, webrequest->content, webrequest->content_length);
        return 1;
    }

    lua_pushlstring(l, webrequest->content, webrequest->content_length);
    lua_pushstring(l, webrequest->error);
    return 2;
}

static int webclient_getinfo(lua_State* l)
{
    struct webclient* webclient = (struct webclient*)luaL_checkudata(l, 1, LUA_WEB_CLIENT_MT);
    if (!webclient)
        return luaL_argerror(l, 1, "parameter self invalid");
    
    struct webrequest* webrequest = (struct webrequest*)lua_touserdata(l, 2);
    if (!webrequest)
        return luaL_argerror(l, 2, "parameter index invalid");
    
    lua_newtable(l);
    
    char* ip = NULL;
    if (curl_easy_getinfo(webrequest->curl, CURLINFO_PRIMARY_IP, &ip) == CURLE_OK) {
        lua_pushstring(l, "ip");
        lua_pushstring(l, ip);
        lua_settable(l, -3);
    }
    
    long port = 0;
    if (curl_easy_getinfo(webrequest->curl, CURLINFO_LOCAL_PORT, &port) == CURLE_OK) {
        lua_pushstring(l, "port");
        lua_pushinteger(l, port);
        lua_settable(l, -3);
    }
    
    double content_length = 0;
    if (curl_easy_getinfo(webrequest->curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &content_length) == CURLE_OK) {
        lua_pushstring(l, "content_length");
        lua_pushnumber(l, content_length);
        lua_settable(l, -3);
    }
    
    long response_code = 0;
    if (curl_easy_getinfo(webrequest->curl, CURLINFO_RESPONSE_CODE, &response_code) == CURLE_OK) {
        lua_pushstring(l, "response_code");
        lua_pushinteger(l, response_code);
        lua_settable(l, -3);
    }
    
    if (webrequest->content_realloc_failed) {
        lua_pushstring(l, "content_save_failed");
        lua_pushboolean(l, webrequest->content_realloc_failed);
        lua_settable(l, -3);
    }
    return 1;
}

static int webclient_sethttpheader(lua_State* l)
{
    struct webclient* webclient = (struct webclient*)luaL_checkudata(l, 1, LUA_WEB_CLIENT_MT);
    if (!webclient)
        return luaL_argerror(l, 1, "parameter self invalid");
    
    struct webrequest* webrequest = (struct webrequest*)lua_touserdata(l, 2);
    if (!webrequest)
        return luaL_argerror(l, 2, "parameter index invalid");
    
    int top = lua_gettop(l);
    for (int i = 3; i <= top; ++i) {
        const char* str = lua_tostring(l, i);
        webrequest->header = curl_slist_append(webrequest->header, str);
    }
    
    if (webrequest->header) {
        curl_easy_setopt(webrequest->curl, CURLOPT_HTTPHEADER, webrequest->header);
    }
    return 0;
}

static int webclient_debug(lua_State* l)
{
    struct webclient* webclient = (struct webclient*)luaL_checkudata(l, 1, LUA_WEB_CLIENT_MT);
    if (!webclient)
        return luaL_argerror(l, 1, "parameter self invalid");
    
    struct webrequest* webrequest = (struct webrequest*)lua_touserdata(l, 2);
    if (!webrequest)
        return luaL_argerror(l, 2, "parameter index invalid");
    
    int enable = lua_toboolean(l, 3);
    curl_easy_setopt(webrequest->curl, CURLOPT_VERBOSE, enable ? 1L : 0L);
    return 0;
}

static int url_encoding(lua_State* l)
{
    struct webclient* webclient = (struct webclient*)luaL_checkudata(l, 1, LUA_WEB_CLIENT_MT);
    if (!webclient)
        return luaL_argerror(l, 1, "parameter self invalid");
    
    if (!webclient->encoding_curl)
        webclient->encoding_curl = curl_easy_init();
    
    size_t length = 0;
    const char* str = lua_tolstring(l, 2, &length);
    char* ret = curl_easy_escape(webclient->encoding_curl, str, (int)length);
    if (!ret) {
        lua_pushlstring(l, str, length);
        return 1;
    }
    
    lua_pushstring(l, ret);
    curl_free(ret);
    return 1;
}

luaL_Reg webclient_createfuns[] = {
    { "create", webclient_create },
    { NULL, NULL }
};

luaL_Reg webclient_funs[] = {
    { "__gc", webclient_destory },
    { "query", webclient_query },
    { "request", webclient_request },
    { "remove_request", webclient_removerequest },
    { "get_respond", webclient_getrespond },
    { "get_info", webclient_getinfo },
    { "set_httpheader", webclient_sethttpheader },
    { "debug", webclient_debug },
    { "url_encoding", url_encoding },
    
    { NULL, NULL }
};

int luaopen_webclient(lua_State * L)
{
    luaL_checkversion(L);
    
    if (luaL_newmetatable(L, LUA_WEB_CLIENT_MT))
    {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        luaL_setfuncs(L, webclient_funs, 0);
        lua_pop(L, 1);
    }
    
    luaL_newlib(L, webclient_createfuns);
    return 1;
}
