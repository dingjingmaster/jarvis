//
// Created by dingjing on 9/27/22.
//

#ifndef JARVIS_SPIDER_H
#define JARVIS_SPIDER_H

#include "../manager/facilities.h"
#include "../factory/task-factory.h"
#include "../protocol/http/http-util.h"

#define SPIDER_DB               "/var/lib/jarvis/db/spider.db"
#define SPIDER_DB_LOCK          "/var/lib/jarvis/db/spider.db.lock"

class Spider;
using SpiderTask = TimerTask;
using SpiderInit = std::function<Spider*()>;

using Request = protocol::HttpRequest;
using Response = protocol::HttpResponse;

using SpiderContext = std::string;
using RootParser = std::function<void (Spider*)>;
using Parser = std::function<std::string (Spider*, void* data)>;

struct SpiderInfo
{
    int                                 interval;               // 0 表示只执行一次，大于 0 表示间隔秒数
    std::string                         spiderName;             // 名字
    std::string                         requestURI;             // 请求 uri
    std::string                         httpMethod;             // 请求方法：
    RootParser                          rootParser;             // 解析方法
    std::map<std::string, Parser>       parsers;                // 解析
    std::map<std::string, std::string>  requestHeaders;         // 请求头
};

void sqlite_lock();
void sqlite_unlock();

class Spider
{
public:
    explicit Spider (std::string& name, std::string& uri, RootParser& rootParser, const std::string& method=HTTP_METHOD_GET);

    void run ();
    bool finished();
    std::string getName ();

    std::string& getContent ();
    void addRule (std::string& name, Parser& parser);
    bool executeRule(std::string& rule, void* udata);
    void addRequestHeader(std::string key, std::string value);
    void addRequestHeader(std::string& key, std::string& value);

    void setParsers (std::map<std::string, Parser>& p);

private:
    static void http_request_callback(HttpTask *task, void*);


public:
    std::map<std::string, std::string>      mField;         // use for save result

private:
    bool                                    mFinished;
    std::string                             mSpiderName;
    std::string                             mBaseUrl;

    std::string                             mContext;

    RootParser                              mRootParser;
    std::map<std::string, Parser>           mParser;

    HttpTask*                               mHttpTask;
    Facilities::WaitGroup                   mWaitGroup;
};


#endif //JARVIS_SPIDER_H
