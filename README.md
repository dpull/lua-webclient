# webclient

非堵塞的lua http库，是对curlm的简单封装。
可用于大量http请求在单线程内处理，如游戏服务器和渠道进行用户的账户密码验证。

webclient 是 [herocode](https://bitbucket.org/beings/herocode) 的子集

## 文件说明

	webclient.c 
		webclient 对curlm的封装
		
	webclient.lua
		webclient 在 skynet 中的简单应用
		
	test.lua
		webclient 使用纯lua的示例
		
			
	