local json = require "json"
local webclient_lib = require 'webclient'
local webclient = webclient_lib.create()
local requests = {};

local requests = {}

local req, key = webclient:request("http://httpbin.org/anything?a=1&b=2", "c=3&d=4&e=" .. webclient:url_encoding("http://httpbin.org/"), 5000) -- connect timeout 5000 ms
webclient:set_httpheader(req, "User-Agent: dpull", [[If-None-Match:"573dff7cd86a737f0fd9ecc862aed14f"]])
webclient:debug(req, true)

requests[key] = req

while next(requests) do
	local finish_key, result = webclient:query()
	if finish_key then
		local req = requests[finish_key]
		assert(req)
		requests[finish_key] = nil
	
		local content, errmsg = webclient:get_respond(req)
		print("respond", result, content, errmsg)
		
		local info = webclient:get_info(req)
		print("info", info.ip, info.port, info.content_length, info.response_code)
		
		webclient:remove_request(req)
	else
		local is_get_upload_progress = false
		local down, total = webclient:get_progress(req, is_get_upload_progress)
		-- print("progress", down, total)
	end
end
