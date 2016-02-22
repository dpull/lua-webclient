--- webclient. (skynet服务).
--
-- @module webclient
-- @usage local webclient = skynet.newservice("webclient")

local skynet = require "skynet";
local webclientlib = require "luna.webclient";
local webclient = webclientlib.create();
local requests = nil;

local function resopnd(request)
    if not request.response then
        return;
    end

    local content, errmsg = webclient:get_respond(request.req)
    if not errmsg then
        request.response(true, true, content);
    else
        request.response(true, false, errmsg);
    end
end

local function query()
    while next(requests) do
        local finish_key = webclient:query()
        if finish_key then
            local request = requests[finish_key];
            assert(request)

            xpcall(resopnd, function() print(debug.traceback()) end, request)

            webclient:remove_request(request.req)
            requests[finish_key] = nil;
        else
            skynet.sleep(1);
        end
    end 
    requests = nil;
end

--- 请求某个url
-- @function request
-- @string url url
-- @tab[opt] get get的参数
-- @param[opt] post post参数，table or string类型 
-- @bool[opt] no_reply 使用skynet.call则要设置为nil或false，使用skynet.send则要设置为true
-- @usage skynet.call(webclient, "lua", "request", "http://www.dpull.com") or skynet.send(webclient, "lua", "request", "http://www.dpull.com", nil, nil, true)
local function request(url, get, post, no_reply)
    if get then
        local i = 0;
        for k, v in pairs(get) do
            k = webclient:url_encoding(k);
            v = webclient:url_encoding(v);

            url = string.format("%s%s%s=%s", url, i == 0 and "?" or "&", k, v);
            i = i + 1;
        end
    end

    if post and type(post) == "table" then
        local data = {}
        for k,v in pairs(post) do
            k = webclient:url_encoding(k);
            v = webclient:url_encoding(v);

            table.insert(data, string.format("%s=%s", k, v));
        end   
        post = table.concat(data , "&");
    end   

    local req, key = webclient:request(url, post);
    if not req then
        return skynet.ret();
    end
    assert(key);

    local response = nil;
    if not no_reply then
        response = skynet.response();
    end

    if requests == nil then
        requests = {}
        skynet.fork(query);
    end

    requests[key] = {
        url = url, 
        req = req,
        response = response,
    };
end

skynet.start(function()
    skynet.dispatch("lua", function(session, source, command, ...)
        assert(command == "request");
        request(...);
    end)
end)
