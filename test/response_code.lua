local json = require "json"
local webclient_lib = require 'webclient'
local webclient = webclient_lib.create()
local requests = {};

local requests = {}
local response_codes = {401, 404, 418, 500, 502, 504}

for k, v in ipairs(response_codes) do
	local req, key = webclient:request("http://httpbin.org/status/" .. v) 
	requests[key] = {req, v}
end

while next(requests) do
	local finish_key, result = webclient:query()
	if finish_key then
		local req, response_code  = table.unpack(requests[finish_key])
		requests[finish_key] = nil
		
		assert(result == 0)
		
		local info = webclient:get_info(req)
		assert(info.response_code == response_code)
		
		webclient:remove_request(req)
	end
end
