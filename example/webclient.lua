WebClientLib = WebClientLib or require("luna.webclient")
Downloads = Downloads or {}
WebClient = WebClient or WebClientLib.Create(function (index, data)
    local download = Downloads[index];
    assert(download);

    if download.type == "data" then
        download.data = download.data .. data;
    end
end);

function DownloadData(url, callback)
    local index = WebClient:DownloadData(url);
    if not index then
        return;
    end

    Downloads[index] = {
        type = "data", 
        url = url, 
        data = "",
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
                Lib.CallBack({para.callback, para.url, para.data});
            end
            Downloads[k] = nil;         
        else
            print("unknown key", k);
        end
    end
end
