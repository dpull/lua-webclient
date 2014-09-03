local Skynet = require "skynet";
WebClientLib = WebClientLib or require("luna.webclient")
Downloads = Downloads or {}
WebClient = WebClient or WebClientLib.Create(function (index, data)
    local download = Downloads[index];
    assert(download);

    if download.type == "data" then
        download.data = download.data .. data;
    end
end);
local Lib = {};

function FormatUrl(urlFormat, ...)
    local tb = {...};
    for i, v in ipairs(tb) do
        tb[i] = WebClientLib.UrlEncoding(v);
    end

    return string.format(urlFormat, unpack(tb));    
end

function Lib.DownloadData(session, source, urlFormat, ...)
    local url = FormatUrl(urlFormat, ...);
    local index = WebClient:DownloadData(url);
    if not index then
        return Skynet.ret();
    end

    Downloads[index] = {
        type = "data", 
        url = url, 
        data = "",
        session = session,
        address = source,
    };
end

function Lib.QueryProgressStatus(session, source, index)
    Skynet.ret(Skynet.pack(WebClientLib.QueryProgressStatus(index)));
end

function Lib.Query()
    local data = WebClient:Query();

    for k, v in pairs(data) do
        local para = Downloads[k];
        if para then
            local param, size;
            if v.error then
                param, size = Skynet.pack(v);
            else
                param, size = Skynet.pack({data = para.data});
            end
            Downloads[k] = nil;
        end
    end
end

function Query()
    while true do
        Lib.Query();
        Skynet.sleep(1);
    end
end

Skynet.start(function()
    Skynet.dispatch("lua", function(session, source, command, ...)
        local f = Lib[command];
        if f then
            f(session, source, ...);
        else
            print("webclient unknown cmd ", command);
        end
    end)

    Skynet.fork(Query);
end)