//
// Created by dingjing on 8/17/22.
//

#ifndef JARVIS_CONNECTION_H
#define JARVIS_CONNECTION_H

#include <atomic>
#include <utility>
#include <functional>

#include "../core/communicator.h"


class Connection : public CommConnection
{
public:
    Connection() : mContext(nullptr)
    {

    }

    void* getContext() const
    {
        return mContext;
    }

    void setContext (void* context, std::function<void (void*)> deleter)
    {
        mContext = context;
        mDeleter = std::move(deleter);
    }

    void* testSetContext (void* testCtx, void* newCtx, std::function<void(void*)> deleter)
    {
        if (mContext.compare_exchange_strong(testCtx, newCtx)) {
            mDeleter = std::move(deleter);
            return newCtx;
        }

        return testCtx;
    }

protected:
    virtual ~Connection()
    {
        if (mDeleter)       mDeleter(mContext);
    }



private:
    std::atomic<void*>              mContext;
    std::function<void(void*)>      mDeleter;
};


#endif //JARVIS_CONNECTION_H
