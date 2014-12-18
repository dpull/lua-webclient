local Lib = require "lib"
local Luna = require "luna"
local Skynet = require "skynet"
local CJSON = require "cjson";
local MD5 = require "md5";
local WebClient;

function TestHttpGet()
	local status, body = Skynet.call(WebClient, "lua", "Request", "http://127.0.0.1/test", {a=1, b=2});
	assert(status)

	local ret = CJSON.decode(body);
	assert(ret.get.a == "1");
	assert(ret.get.b == "2");
end


function TestHttpPost()
	local status, body = Skynet.call(WebClient, "lua", "Request", "http://127.0.0.1/test", nil, "nihao");
	assert(status)

	local ret = CJSON.decode(body);
	assert(ret.post == "nihao");
end

function TestHttps()
	local status, body = Skynet.call(WebClient, "lua", "Request", "https://mail.qq.com/");
	assert(status)
end

function TestStress()
	while true do
		for i = 1, 8999 do
			Skynet.fork(function ()
				local status, body = Skynet.call(WebClient, "lua", "Request", "http://172.17.0.61/");
				if not status and string.find(body, "Connection time") == -1 then
					Lib.Tree(ret);
				end
			end);
		end		
		Skynet.sleep(3000);
	end
end

Skynet.start(function()
	WebClient = Skynet.queryservice("webclient");

	TestHttpGet();
	TestHttpPost();
	TestHttps();
	Skynet.fork(TestStress);
end)