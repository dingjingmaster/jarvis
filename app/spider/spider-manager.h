//
// Created by dingjing on 9/27/22.
//

#ifndef JARVIS_SPIDER_MANAGER_H
#define JARVIS_SPIDER_MANAGER_H

#include <map>
#include <memory>
#include <functional>

#include "spider.h"
#include "../factory/task-factory.h"

class Spider;
class TimerTask;
class SpiderManager
{
public:
    static SpiderManager* instance ();

    void runAll ();

    void addSpider (SpiderInfo& spInfo);
    void addSpider(int intervalSecond, const std::string& spiderName, const std::string& uri, std::string method, RootParser& parser);

private:
    SpiderManager();
    ~SpiderManager();

private:
    bool                                                mRunning;
    static SpiderManager*                               gInstance;

    std::map<std::string, SpiderTask*>                  mTasks;
    std::map<std::string, std::shared_ptr<SpiderInfo>>  mSpiders;
};


#endif //JARVIS_SPIDER_MANAGER_H
