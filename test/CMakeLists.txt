set(PROJ_NAME "Test")

set (WEBCLIENT_SRC ${CMAKE_SOURCE_DIR}/webclient.c main.c)

set(LUA_SRC_PATH "${CMAKE_SOURCE_DIR}/3rd/lua")
set(LUA_SRC_IGNORE "${LUA_SRC_PATH}/lua.c;${LUA_SRC_PATH}/luac.c")

aux_source_directory(${LUA_SRC_PATH} LUA_SRC)
list(REMOVE_ITEM LUA_SRC ${LUA_SRC_IGNORE})

include_directories(${CMAKE_SOURCE_DIR}/3rd/lua)
add_executable(${PROJ_NAME} ${WEBCLIENT_SRC} ${LUA_SRC})

source_group(lua FILES ${LUA_SRC})
source_group(test FILES ${WEBCLIENT_SRC})

if (CURL_FOUND)
    include_directories(${CURL_INCLUDE_DIR})
    target_link_libraries(${PROJ_NAME} ${CURL_LIBRARY})
else(CURL_FOUND)
    add_definitions(-DCURL_STATICLIB)
    include_directories(${CMAKE_SOURCE_DIR}/3rd/curl/include)
    include_directories(${CMAKE_BINARY_DIR}/3rd/curl/include/curl)
    target_link_libraries(${PROJ_NAME} libcurl)
endif(CURL_FOUND)

if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif(MSVC)