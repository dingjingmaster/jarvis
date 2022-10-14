//
// Created by dingjing on 9/27/22.
//

#ifndef JARVIS_SPIDER_H
#define JARVIS_SPIDER_H

#include "../factory/task-factory.h"

#define SPIDER_DB               "/var/lib/jarvis/db/spider.db"

class Spider;
using SpiderTask = TimerTask;
using SpiderInit = std::function<Spider*()>;

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

#endif //JARVIS_SPIDER_H
