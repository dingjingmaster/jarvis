//
// Created by dingjing on 8/14/22.
//
#include "consul-data-types.h"

protocol::ConsulConfig::ConsulConfig()
{
    mPtr = new Config;
    mPtr->blockingQuery = false;
    mPtr->passing = false;
    mPtr->replaceChecks = false;
    mPtr->waitTTL = 300 * 1000;
    mPtr->checkCfg.interval = 5000;
    mPtr->checkCfg.timeout = 10000;
    mPtr->checkCfg.httpMethod = "GET";
    mPtr->checkCfg.initialStatus = "critical";
    mPtr->checkCfg.autoDeregisterTime = 10 * 60 * 1000;
    mPtr->checkCfg.successTimes = 0;
    mPtr->checkCfg.failureTimes = 0;
    mPtr->checkCfg.healthCheck = false;

    mRef = new std::atomic<int>(1);
}

protocol::ConsulConfig::~ConsulConfig()
{
    if (--mRef == 0) {
        delete mPtr;
        delete mRef;
    }

}

protocol::ConsulConfig::ConsulConfig(protocol::ConsulConfig &&move)
{
    mPtr = move.mPtr;
    mRef = move.mRef;

    move.mPtr = new Config;
    move.mRef = new std::atomic<int>(1);
}

protocol::ConsulConfig::ConsulConfig(const protocol::ConsulConfig &copy)
{
    mPtr = copy.mPtr;
    mRef = copy.mRef;
    ++mRef;
}

protocol::ConsulConfig &protocol::ConsulConfig::operator=(protocol::ConsulConfig &&move)
{
    if (&move != this) {
        this->~ConsulConfig();
        mPtr = move.mPtr;
        mRef = move.mRef;

        move.mPtr = new Config;
        move.mRef = new std::atomic<int>(1);
    }

    return *this;
}

protocol::ConsulConfig &protocol::ConsulConfig::operator=(protocol::ConsulConfig &copy)
{
    if (this != &copy) {
        this->~ConsulConfig();
        mPtr = copy.mPtr;
        mRef = copy.mRef;
        ++mRef;
    }

    return *this;
}

void protocol::ConsulConfig::setToken(const std::string &token)
{
    mPtr->token = token;
}

std::string protocol::ConsulConfig::getToken() const
{
    return mPtr->token;
}

void protocol::ConsulConfig::setDataCenter(const std::string& dataCenter)
{
    mPtr->dc = dataCenter;
}

std::string protocol::ConsulConfig::getDataCenter() const
{
    return mPtr->dc;
}

void protocol::ConsulConfig::setNearNode(const std::string& nearNode)
{
    mPtr->near = nearNode;
}

std::string protocol::ConsulConfig::getNearNode() const
{
    return mPtr->near;
}

void protocol::ConsulConfig::setFilterExpr(const std::string &filterExpr)
{
    mPtr->filter = filterExpr;
}

std::string protocol::ConsulConfig::getFilterExpr() const
{
    return mPtr->filter;
}

void protocol::ConsulConfig::setWaitTTL(int waitTTL)
{
    mPtr->waitTTL = waitTTL;
}

int protocol::ConsulConfig::getWaitTTL() const
{
    return mPtr->waitTTL;
}

bool protocol::ConsulConfig::blockingQuery() const
{
    return mPtr->blockingQuery;
}

void protocol::ConsulConfig::setBlockingQuery(bool enable)
{
    mPtr->blockingQuery = enable;
}

bool protocol::ConsulConfig::getPassing() const
{
    return mPtr->passing;
}

void protocol::ConsulConfig::setPassing(bool passing)
{
    mPtr->passing = passing;
}

void protocol::ConsulConfig::setReplaceChecks(bool replaceChecks)
{
    mPtr->replaceChecks = replaceChecks;
}

bool protocol::ConsulConfig::getReplaceChecks() const
{
    return mPtr->replaceChecks;
}

void protocol::ConsulConfig::setCheckName(const std::string &checkName)
{
    mPtr->checkCfg.checkName = checkName;
}

std::string protocol::ConsulConfig::getCheckName() const
{
    return mPtr->checkCfg.checkName;
}

void protocol::ConsulConfig::setCheckHttpURL(const std::string &url)
{
    mPtr->checkCfg.httpURL = url;
}

std::string protocol::ConsulConfig::getCheckHttpURL() const
{
    return mPtr->checkCfg.httpURL;
}

void protocol::ConsulConfig::setCheckHttpMethod(const std::string& method)
{
    mPtr->checkCfg.httpMethod = method;

}

std::string protocol::ConsulConfig::getCheckHttpMethod() const
{
    return mPtr->checkCfg.httpMethod;
}

void protocol::ConsulConfig::addHttpHeader(const std::string &key, const std::vector<std::string> &val)
{
    mPtr->checkCfg.headers.emplace(key, val);

}

const std::map<std::string, std::vector<std::string>> *protocol::ConsulConfig::getHttpHeaders() const
{
    return &mPtr->checkCfg.headers;
}

void protocol::ConsulConfig::setHttpBody(const std::string &body)
{
    mPtr->checkCfg.httpBody = body;
}

std::string protocol::ConsulConfig::getHttpBody() const
{
    return mPtr->checkCfg.httpBody;
}

int protocol::ConsulConfig::getCheckInterval() const
{
    return mPtr->checkCfg.interval;
}

void protocol::ConsulConfig::setCheckInterval(int in)
{
    mPtr->checkCfg.interval = in;
}

void protocol::ConsulConfig::setCheckTimeout(int timeout)
{
    mPtr->checkCfg.timeout = timeout;
}

int protocol::ConsulConfig::getCheckTimeout() const
{
    return mPtr->checkCfg.timeout;
}

void protocol::ConsulConfig::setCheckNotes(const std::string &notes)
{
    mPtr->checkCfg.notes = notes;
}

std::string protocol::ConsulConfig::getCheckNotes() const
{
    return mPtr->checkCfg.notes;
}

void protocol::ConsulConfig::setCheckTcp(const std::string &tcpAddr)
{
    mPtr->checkCfg.tcpAddress = tcpAddr;
}

std::string protocol::ConsulConfig::getCheckTcp() const
{
    return mPtr->checkCfg.notes;
}

void protocol::ConsulConfig::setInitialStatus(const std::string& initialStatus)
{
    mPtr->checkCfg.initialStatus = initialStatus;

}

std::string protocol::ConsulConfig::getInitialStatus() const
{
    return mPtr->checkCfg.initialStatus;
}

void protocol::ConsulConfig::setAutoDeregisterTime(int milliseconds)
{
    mPtr->checkCfg.autoDeregisterTime = milliseconds;
}

int protocol::ConsulConfig::getAutoDeregisterTime() const
{
    return mPtr->checkCfg.autoDeregisterTime;
}

void protocol::ConsulConfig::setSuccessTimes(int times)
{
    mPtr->checkCfg.successTimes = times;
}

int protocol::ConsulConfig::getSuccessTimes() const
{
    return mPtr->checkCfg.successTimes;
}

void protocol::ConsulConfig::setFailureTimes(int times)
{
    mPtr->checkCfg.failureTimes = times;
}

int protocol::ConsulConfig::getFailureTimes() const
{
    return mPtr->checkCfg.failureTimes;
}

void protocol::ConsulConfig::setHealthCheck(bool enable)
{
    mPtr->checkCfg.healthCheck = enable;
}

bool protocol::ConsulConfig::getHealthCheck() const
{
    return mPtr->checkCfg.healthCheck;
}
