# LuaWebClient #

非堵塞的lua http库，是对curlm的简单封装。
可用于大量http请求在单线程内处理，如游戏服务器和渠道进行用户的账户密码验证。

## 文件说明 ##

    src/* 
        LuaWebClient的c++代码

    example/webclient.lua 
        对C接口的简单封装，可用于游戏逻辑线程直接调用

    example/skynet_webclient.lua 
        使用服务器框架skynet的简单封装，可作为skynet的一个服务来使用

## 依赖库 ##
    
    curl
    lua（5.1， 5.2均可）

