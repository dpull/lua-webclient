# lua-webclient

非堵塞的lua http库，是对libcurl multi interface（curlm）的简单封装。
可用于大量http、https请求在单线程内非阻塞处理，如游戏服务器和渠道进行用户的账户密码验证。

## 文件说明

	webclient.c 
		webclient 对curlm的封装
		
	test/
		webclient 使用纯lua的示例

	webclient.lua
		webclient 在 skynet 中的简单应用	

## Build

* linux or mac `cmake ../` 
* windows `cmake -G "Visual Studio 15 2017" ../` 

## Usage

[example.lua](https://github.com/dpull/lua-webclient/blob/master/test/example.lua)
		
## 如何接入skynet

[参考makefile](https://github.com/dpull/lua-webclient/issues/13) 感谢 [@peigongdh](https://github.com/peigongdh) 
