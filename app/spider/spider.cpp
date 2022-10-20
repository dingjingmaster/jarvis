//
// Created by dingjing on 9/28/22.
//
#include "spider.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>

std::mutex lock;
static FILE* locker = nullptr;

void sqlite_lock()
{
    lock.lock();
    do {
        if (!locker) {
            if (!access (SPIDER_DB_LOCK, F_OK)) {
                locker = fopen(SPIDER_DB_LOCK, "w+");
                if (!locker) {
                    loge("spider lock file '%s' open error", SPIDER_DB_LOCK);
                    exit(-1);
                }
            } else {
                loge("lock file not exists!");
                exit(-2);
            }
        }
    } while (0);

    while (true) {
        if (flock (locker->_fileno, LOCK_EX | LOCK_NB) == 0) {
            break;
        }
        sleep(1);
    }

    lock.unlock();
}

void sqlite_unlock()
{
    lock.lock();
    flock (locker->_fileno, LOCK_UN);
    lock.unlock();
}


Spider::Spider(std::string &name, std::string &uri, RootParser& rootParser, const std::string &method)
        : mSpiderName(name), mBaseUrl(uri), mRootParser(rootParser), mWaitGroup(1)
{
    mHttpTask = TaskFactory::createHttpTask(mBaseUrl, 10, 10, http_request_callback, this);

    protocol::HttpRequest *req = mHttpTask->getReq();

    req->addHeaderPair("Accept", "*/*");
    req->addHeaderPair("Connection", "close");
    req->addHeaderPair("Cache-Control", "no-cache");
    req->addHeaderPair("User-Agent", "Mozilla/5.0 (X11; Linux x86_64; rv:105.0) Gecko/20100101 Firefox/105.0");
}

void Spider::addRule(std::string &name, Parser &parser)
{
    mParser[name] = parser;
}

bool Spider::executeRule(std::string &rule, void* udata)
{
    if (mParser.find(rule) != mParser.end()) {
        mParser[rule](this, udata);
        return true;
    }

    return false;
}


void Spider::run()
{
    mHttpTask->start();

    mWaitGroup.wait(-1);

    if (nullptr == mRootParser) {
        loge("http Not set root parser!");
        return;
    }

    if (mContext.empty()) {
        loge("http response is empty! %s", mContext.c_str());
        return;
    }

    mRootParser(this);
    mFinished = true;
}

void Spider::http_request_callback(HttpTask *task, void* spider)
{
    int state = task->getState();
    int error = task->getError();

    switch (state) {
        case TASK_STATE_SYS_ERROR:
            loge("system error: %s", strerror(error));
            break;
        case TASK_STATE_DNS_ERROR:
            loge("DNS error: %s", gai_strerror(error));
            break;
        case TASK_STATE_SSL_ERROR:
            loge("SSL error: %d", error);
            break;
        case TASK_STATE_TASK_ERROR:
            loge("Task error: %d", error);
            break;
        case TASK_STATE_SUCCESS:
            logi ("http request success");
            break;
    }

    const void* body = nullptr;
    size_t bodyLen = 0;

    for (int i = 10; i > 0; ++i) {
        if (task->getResp()->getParsedBody(&body, &bodyLen)) {
            logd("http request success!!!\n%s\n", (const char*)body);
            break;
        }
    }

    auto sp = static_cast<Spider*>(spider);
    if (sp && bodyLen > 0) {
        sp->mContext = move(std::string((const char*)body, bodyLen));
    }
    sp->mWaitGroup.done();
}

std::string Spider::getName()
{
    return mSpiderName;
}

void Spider::addRequestHeader(std::string key, std::string value)
{
    protocol::HttpRequest *req = mHttpTask->getReq();
    req->addHeaderPair(key, value);
}

void Spider::addRequestHeader(std::string &key, std::string &value)
{
    protocol::HttpRequest *req = mHttpTask->getReq();
    req->addHeaderPair(key, value);
}

void Spider::setParsers(std::map<std::string, Parser> &p)
{
    mParser = p;
}

std::string &Spider::getContent()
{
    return mContext;
}

bool Spider::finished()
{
    return mFinished;
}
