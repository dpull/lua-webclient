local json = require "json"
local webclient_lib = require 'webclient'
local webclient = webclient_lib.create()
local requests = {};

local requests = {}

local req, key = webclient:request("http://httpbin.org/relative-redirect/6") --302
requests[key] = req

local req, key = webclient:request("http://httpbin.org/absolute-redirect/6") --302
requests[key] = req

local req, key = webclient:request("http://httpbin.org/redirect-to?status_code=307&url=" .. webclient:url_encoding("http://httpbin.org/get")) --307
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

		assert(data.url == "http://httpbin.org/get")
		
		local info = webclient:get_info(req)
		assert(info.response_code == 200)
		webclient:remove_request(req)
	end
end
