//
// Created by dingjing on 8/19/22.
//

#ifndef JARVIS_NAME_SERVICE_H
#define JARVIS_NAME_SERVICE_H

#include <utility>
#include <pthread.h>
#include <functional>

#include "../factory/task.h"
#include "../factory/workflow.h"

#include "../core/rb-tree.h"
#include "../core/communicator.h"

#include "../manager/route-manager.h"
#include "../manager/endpoint-params.h"

#include "../utils/uri-parser.h"

typedef struct _NSParams                NSParams;
typedef struct _NSPolicyEntry           NSPolicyEntry;

class RouterTask : public GenericTask
{
public:
    RouteManager::RouteResult *get_result() { return &mResult; }

public:
    void set_state(int state) { mState = state; }
    void set_error(int error) { mError = error; }

protected:
    virtual SubTask *done()
    {
        SeriesWork *series = seriesOf(this);

        if (mCallback)
            mCallback(this);

        delete this;
        return series->pop();
    }

public:
    RouterTask(std::function<void (RouterTask *)>&& cb)
        : mCallback(std::move(cb))
    {
    }

protected:
    RouteManager::RouteResult                   mResult;
    std::function<void (RouterTask*)>           mCallback;
};

class NSTracing
{
public:
    NSTracing()
    {
        mData = NULL;
        mDeleter = NULL;
    }

public:
    void*                       mData;
    void (*mDeleter)(void *);
};

struct _NSParams
{
    TransportType               mType;
    ParsedURI&                  mUri;
    const char*                 mInfo;
    bool                        mFixedAddr;
    int                         mRetryTimes;
    NSTracing*                  mTracing;
};

using RouterCallback = std::function<void (RouterTask*)>;

class NSPolicy
{
public:
    virtual RouterTask *createRouterTask(const NSParams *params, RouterCallback callback, void*) = 0;

    virtual void success(RouteManager::RouteResult *result, NSTracing *tracing, CommTarget *target)
    {
        RouteManager::notifyAvailable(result->mCookie, target);
    }

    virtual void failed(RouteManager::RouteResult *result, NSTracing *tracing, CommTarget *target)
    {
        if (target)
            RouteManager::notifyUnavailable(result->mCookie, target);
    }

public:
    virtual ~NSPolicy() { }
};

class NameService
{
public:
    int addPolicy(const char *name, NSPolicy *policy);
    NSPolicy *getPolicy(const char *name);
    NSPolicy *delPolicy(const char *name);

public:
    NSPolicy *getDefaultPolicy() const
    {
        return mDefaultPolicy;
    }

    void set_default_policy(NSPolicy *policy)
    {
        mDefaultPolicy = policy;
    }


private:
    NSPolicyEntry *getPolicyEntry(const char *name);

public:
    NameService(NSPolicy* defaultPolicy)
        : mRWLock(PTHREAD_RWLOCK_INITIALIZER)
    {
        mRoot.rbNode = NULL;
        mDefaultPolicy = defaultPolicy;
    }

    virtual ~NameService();

private:
    NSPolicy*                   mDefaultPolicy;
    RBRoot                      mRoot;
    pthread_rwlock_t            mRWLock;
};

#endif //JARVIS_NAME_SERVICE_H
