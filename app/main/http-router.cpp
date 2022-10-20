//
// Created by dingjing on 10/18/22.
//

#include "http-router.h"

#include "common/area.h"
#include "client-data.h"
#include "../utils/sqlite-utils.h"
#include "../utils/file-read-write.h"

#include <mutex>
#include <unistd.h> // windows 在 windows.h
#include <utils/json-utils.h>

static std::mutex locker;
HttpRouter* HttpRouter::gInstance = nullptr;

HttpRouter *HttpRouter::getInstance()
{
    if (!gInstance) {
        std::lock_guard l(locker);
        if (!gInstance) {
            gInstance = new HttpRouter();
        }
    }

    return gInstance;
}

HttpRouter::HttpRouter()
{

}

bool HttpRouter::responseStaticResource(HttpTask *task)
{
    if (!task || !task->getReq() || !task->getResp()) {
        loge("bad http request");
        return true;
    }

    std::string f = requestStaticResource(*task->getReq());
    if (!f.empty()) {
        task->getResp()->appendOutputBody(FileReadWrite::getFileContent(f));
        return true;
    }

    logd("not static resource");
    return false;
}

std::string HttpRouter::requestStaticResource(protocol::HttpRequest &request) const
{
    std::string uri = request.getRequestUri();
    logd(WEB_HOME);
    logd("request uri: %s", uri.c_str());

    if ("/" == uri || "/index.html" == uri) {
        std::string path = WEB_HOME "/index.html";
        if (!access(path.c_str(), R_OK)) {
            return std::move(path);
        } else {
            loge("%d - %s", errno, strerror(errno));
        }
    }
    else if (uri.ends_with(".js")) {
        std::string path = WEB_HOME + uri;
        if (!access(path.c_str(), R_OK)) {
            return std::move(path);
        } else {
            loge("%d - %s", errno, strerror(errno));
        }
    }

    return std::string();
}

bool HttpRouter::responseDynamicResource(HttpTask *task)
{
    std::string uri = task->getReq()->getRequestUri();
    logd(uri.c_str());

    if (uri.starts_with("/index/au-price/")) {
        std::string param = uri.substr(16);
        logd(param.c_str());
        auto info = SqliteUtils::getCurrentGoldPrice(param);
        task->getResp()->setHeaderPair("content-type", "application/json;charset=utf-8");
        task->getResp()->appendOutputBody(JsonUtils::jsonBuildIndexPrice(info));
        return true;
    } else if (uri.starts_with("/index/ag-price/")) {
        std::string param = uri.substr(16);
        logd(param.c_str());
        auto info = SqliteUtils::getCurrentSilverPrice(param);
        task->getResp()->setHeaderPair("content-type", "application/json;charset=utf-8");
        task->getResp()->appendOutputBody(JsonUtils::jsonBuildIndexPrice(info));
        return true;
    }

    return false;
}

