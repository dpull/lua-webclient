WebClientLib = WebClientLib or require("luabase.webclient")
WebClient = WebClient or WebClientLib.Create();
Downloads = Downloads or {}

function DownloadData(url, callback)
    local index = WebClient:DownloadData(url);
    if not index then
        return;
    end

    Downloads[index] = {
        type = "data", 
        url = url, 
        callback = callback
    };
end

function DownloadFile(url, file, callback)
    local index = WebClient:DownloadFile(url, file);
    if not index then
        return;
    end

    Downloads[index] = {
        type = "file", 
        url = url, 
        file = file, 
        callback = callback
    };
end

function UrlEncoding(str)
    return WebClientLib.UrlEncoding(str);
end

function QueryProgressStatus(index)
    return WebClient:QueryProgressStatus(index);
end

function Query()
    local subTime = GetTickCount();
    local data = WebClient:Query();

    subTime = GetTickCount() - subTime;
    for k, v in pairs(data) do
        local para = Downloads[k];
        if para then
            if para.callback then
                Lib.CallBack({para.callback, v});
            end
            Downloads[k] = nil;         
        else
            print("unknown key", k);
        end
    end
end
