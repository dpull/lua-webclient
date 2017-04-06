local webclientlib = require 'webclient'
local webclient = webclientlib.create()

local urls = {
	"http://dldir1.qq.com/invc/cyclone/QQDownload_Setup_48_773_400.exe",
	"http://dldir1.qq.com/qqyy/pc/QQPlayer_Setup_39_934.exe",
	"http://dldir1.qq.com/qqtv/qqpc/TencentVideo9.12.1291.0.exe",
	"http://www.foxmail.com/win/download",
	"http://dldir1.qq.com/music/clntupate/QQMusicForYQQ.exe",
}

local requests = {};
for i, v in ipairs(urls) do
	local req, key = webclient:request(v)
	assert(req)
	assert(key)
	requests[key] = {req, v}

    webclient:set_httpheader(req, "User-Agent: dpull", [[If-None-Match:"573dff7cd86a737f0fd9ecc862aed14f"]])

    if i == 1 then
        webclient:debug(req, true)
    end
end

while next(requests) do
	local finish_key = webclient:query()
	if finish_key then
		assert(requests[finish_key])

		local req, url = table.unpack(requests[finish_key])
		local content, errmsg = webclient:get_respond(req)
		local info = webclient:get_info(req)
		print(url, #content, errmsg, info.ip, info.port, info.content_length, info.response_code)
		webclient:remove_request(req)
		requests[finish_key] = nil
	end
end 
print("test webclient finish!")
