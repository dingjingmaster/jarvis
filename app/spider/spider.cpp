//
// Created by dingjing on 9/22/22.
//
#include "spider.h"
#include "../manager/facilities.h"

static Facilities::WaitGroup waitGroup(1);

spider::Spider::Spider(std::string &name, std::string &uri, RootParser& rootParser, const std::string &method)
    : mSpiderName(name), mBaseUrl(uri), mRootParser(rootParser)
{
    mHttpTask = TaskFactory::createHttpTask(mBaseUrl, 10, 10, http_request_callback, this);

    protocol::HttpRequest *req = mHttpTask->getReq();

    req->addHeaderPair("Accept", "*/*");
    req->addHeaderPair("User-Agent", "Wget/1.14 (linux-gnu)");
    req->addHeaderPair("Connection", "close");
}

void spider::Spider::addRule(std::string &name, spider::Parser &parser)
{
    mParser[name] = parser;
}

bool spider::Spider::executeRule(std::string &rule, void* udata)
{
    if (mParser.find(rule) != mParser.end()) {
        mParser[rule](&mContext, udata);
        return true;
    }

    return false;
}


void spider::Spider::run()
{
    mHttpTask->start();

    waitGroup.wait();

    const void* body;
    size_t bodyLen;

    if (mHttpTask->getResp()->getParsedBody(&body, &bodyLen)) {
        logi("http request success!!!");
        mContext = move(std::string((char*)body, bodyLen));
    }

    if (nullptr == mRootParser) {
        loge("http Not set root parser!");
        return;
    }

    if (mContext.empty()) {
        loge("http response is empty!");
        return;
    }

    mRootParser(&mContext);
}

void spider::Spider::http_request_callback(HttpTask *task, void* spider)
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
        break;
    }

    if (state != TASK_STATE_SUCCESS) {
        loge ("http request failed!!!");
    }

    const void* body;
    size_t bodyLen;
    if (task->getResp()->getParsedBody(&body, &bodyLen)) {
        logd("http request success!!!\n%s\n", (const char*)body);
    }

    auto sp = static_cast<Spider*>(spider);
    if (spider && sp) {
        sp->mContext = move(std::string((const char*)body, bodyLen));
    }
    waitGroup.done();
}
