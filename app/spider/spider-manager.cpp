//
// Created by dingjing on 9/27/22.
//

#include "spider-manager.h"

#include <mutex>
#include <memory>
#include <utility>

#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>

#include "spider.h"
#include "gold-spider.h"

#define SPIDER_REGISTER(x)                          \
    do {                                            \
        addSpider(g##x);                            \
    } while (0);

SpiderManager* SpiderManager::gInstance = nullptr;


SpiderManager::SpiderManager()
{
    mode_t pre = umask(0);
    if (access (SPIDER_DB, F_OK)) {
        int fd = creat(SPIDER_DB, S_IRWXU | S_IRWXG | S_IRWXO);
        if (-1 == fd) {
            loge("spider db file '%s' create error", SPIDER_DB);
            exit(-1);
        }
    }
    chmod(SPIDER_DB, S_IRWXU | S_IRWXG | S_IRWXO);

    if (access(SPIDER_DB_LOCK, F_OK)) {
        int fd = creat(SPIDER_DB_LOCK, S_IRWXU | S_IRWXG | S_IRWXO);
        if (-1 == fd) {
            loge("spider db lock file '%s' create error", SPIDER_DB_LOCK);
            exit(-1);
        }
    }
    chmod(SPIDER_DB_LOCK, S_IRWXU | S_IRWXG | S_IRWXO);
    umask(pre);

    SPIDER_REGISTER(GoldSpider);
}

SpiderManager::~SpiderManager()
{
    for (auto t : mTasks) {
        t.second->setRepeat(false);
    }

    mSpiders.clear();
    mTasks.clear();
}

SpiderManager *SpiderManager::instance()
{
    if (!gInstance) {
        static std::mutex lock;
        lock.lock();
        if (!gInstance) {
            gInstance = new SpiderManager();
        }
        lock.unlock();
    }

    return gInstance;
}

void SpiderManager::runAll()
{
    for (auto& it : mSpiders) {
        if (mTasks.end() == mTasks.find(it.first)) {
            if (it.second->interval < 0) {
                logw("spider '%s' not running!", it.second->spiderName.c_str());
                continue;
            } else if (it.second->interval > 0) {
                SpiderTask* task = TaskFactory::createTimerTask(it.second->interval * 1000000, [=] (TimerTask*) {
                    std::shared_ptr<SpiderInfo> spInfo = mSpiders[it.first];
                    Spider* sp = TaskFactory::createSpider(spInfo->spiderName, spInfo->requestURI, spInfo->rootParser, spInfo->httpMethod);
                    sp->run();
                });
                task->setRepeat(true);
                task->start();
                mTasks[it.second->spiderName] = task;
                logi("spider '%s' start running!", it.second->spiderName.c_str());
            } else {
                auto task = TaskFactory::createTimerTask(it.second->interval * 1000000, [=] (TimerTask*) {
                });
                task->setRepeat(false);
                task->start();
            }
        }
    }
}

void SpiderManager::addSpider(int intervalSecond, const std::string& spiderName, const std::string& uri, std::string method, RootParser &parser)
{
    if (!spiderName.empty() && !uri.empty() && parser) {
        auto spInfo = new SpiderInfo;

        spInfo->interval = intervalSecond;
        spInfo->spiderName = spiderName;
        spInfo->requestURI = uri;
        spInfo->httpMethod = std::move(method);
        spInfo->rootParser = parser;

        mSpiders[spiderName] = std::make_shared<SpiderInfo>(*spInfo);
    }
}

void SpiderManager::addSpider(SpiderInfo &spInfo)
{
    mSpiders[spInfo.spiderName] = std::make_shared<SpiderInfo>(spInfo);
}



