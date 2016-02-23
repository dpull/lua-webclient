# lua-webclient

非堵塞的lua http库，是对libcurl multi interface（curlm）的简单封装。
可用于大量http、https请求在单线程内非阻塞处理，如游戏服务器和渠道进行用户的账户密码验证。

## 文件说明

	webclient.c 
		webclient 对curlm的封装
		
	webclient.lua
		webclient 在 skynet 中的简单应用
		
	test.lua
		webclient 使用纯lua的示例
		
			
	
