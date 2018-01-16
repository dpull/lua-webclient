local json = require "json"
local webclient_lib = require 'webclient'
local webclient = webclient_lib.create()
local requests = {};

local requests = {}

local req, key = webclient:request("http://httpbin.org/get?a=1&b=2")
requests[key] = req

while next(requests) do
	local finish_key, result = webclient:query()
	if finish_key then
		local req = requests[finish_key]
		assert(req)
		requests[finish_key] = nil
	
		assert(result == 0)
	
		local content, errmsg = webclient:get_respond(req)
		local data = json.decode(content)
		assert(data.args.a == "1")
		assert(data.args.b == "2")
		
		webclient:remove_request(req)
	end
end
