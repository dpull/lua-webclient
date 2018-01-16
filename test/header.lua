local json = require "json"
local webclient_lib = require 'webclient'
local webclient = webclient_lib.create()
local requests = {};

local requests = {}

local req, key = webclient:request("http://httpbin.org/headers")
webclient:set_httpheader(req, "User-Agent: dpull", [[If-None-Match:"573dff7cd86a737f0fd9ecc862aed14f"]])

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
		assert(data.headers["User-Agent"] == "dpull")
		assert(data.headers["If-None-Match"] == [["573dff7cd86a737f0fd9ecc862aed14f"]])

		webclient:remove_request(req)
	end
end
