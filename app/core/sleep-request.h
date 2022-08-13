//
// Created by dingjing on 8/13/22.
//

#ifndef JARVIS_SLEEP_REQUEST_H
#define JARVIS_SLEEP_REQUEST_H
#include <errno.h>
#include "sub-task.h"
#include "communicator.h"
#include "common-scheduler.h"

class SleepRequest : public SubTask, public SleepSession
{
public:
    SleepRequest(CommScheduler *scheduler)
    {
        mScheduler = scheduler;
    }

public:
    virtual void dispatch()
    {
        if (mScheduler->sleep(this) < 0) {
            handle(SS_STATE_ERROR, errno);
        }
    }

protected:
    int                         mState;
    int                         mError;
    CommScheduler*              mScheduler;

protected:
    virtual void handle(int state, int error)
    {
        mState = state;
        mError = error;
        subTaskDone();
    }
};

#endif //JARVIS_SLEEP_REQUEST_H
