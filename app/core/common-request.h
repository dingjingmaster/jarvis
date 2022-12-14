//
// Created by dingjing on 8/13/22.
//

#ifndef JARVIS_COMMON_REQUEST_H
#define JARVIS_COMMON_REQUEST_H

#include <errno.h>
#include <stddef.h>

#include "sub-task.h"
#include "communicator.h"
#include "common-scheduler.h"

class CommonRequest : public SubTask, public CommSession
{
public:
    CommonRequest(CommSchedObject *object, CommScheduler *scheduler)
    {
        logv("");
        mScheduler = scheduler;
        mObject = object;
        mWaitTimeout = 0;
    }

    CommSchedObject *getRequestObject() const { return mObject; }

    void setRequestObject(CommSchedObject *object) { mObject = object; }

    int getWaitTimeout() const { return mWaitTimeout; }

    void setWaitTimeout(int timeout) { mWaitTimeout = timeout; }

public:
    virtual void dispatch()
    {
        logv("");
        if (mScheduler->request(this, mObject, mWaitTimeout, &mTarget) < 0) {
            handle(CS_STATE_ERROR, errno);
        }
    }

protected:
    int                             mState;
    int                             mError;
    CommTarget*                     mTarget;

#define TOR_NOT_TIMEOUT             0
#define TOR_WAIT_TIMEOUT            1
#define TOR_CONNECT_TIMEOUT         2
#define TOR_TRANSMIT_TIMEOUT        3

protected:
    int                             mTimeoutReason;
    int                             mWaitTimeout;
    CommSchedObject*                mObject;
    CommScheduler*                  mScheduler;

protected:
    virtual void handle(int state, int error);
};


#endif //JARVIS_COMMON_REQUEST_H
