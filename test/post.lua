local json = require "json"
local webclient_lib = require 'webclient'
local webclient = webclient_lib.create()
local requests = {};

local requests = {}

local req, key = webclient:request("http://httpbin.org/post", "a=1&b=2")
requests[key] = req

local req, key = webclient:request("http://httpbin.org/post", {{name="a", contents = 1}, {name="b", file="main.lua", content_type="text/plain", filename="example.txt"}})
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

		assert(data.form.a == "1")

		if data.form.b then
			assert(data.form.b == "2")
		else
			assert(data.files.b)
		end
		
		webclient:remove_request(req)
	end
end
