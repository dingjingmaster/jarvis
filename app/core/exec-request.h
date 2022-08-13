//
// Created by dingjing on 8/13/22.
//

#ifndef JARVIS_EXEC_REQUEST_H
#define JARVIS_EXEC_REQUEST_H
#include "sub-task.h"
#include "executor.h"

#include <errno.h>

class ExecRequest : public SubTask, public ExecSession
{
public:
    ExecRequest(ExecQueue *queue, Executor *executor)
    {
        mExecutor = executor;
        mQueue = queue;
    }

    ExecQueue *get_request_queue() const { return mQueue; }
    void set_request_queue(ExecQueue *queue) { mQueue = queue; }

public:
    virtual void dispatch()
    {
        if (mExecutor->request(this, mQueue) < 0) {
            handle(ES_STATE_ERROR, errno);
        }
    }

protected:
    int                     mState;
    int                     mError;

    ExecQueue*              mQueue;
    Executor*               mExecutor;

protected:
    virtual void handle(int state, int error)
    {
        mState = state;
        mError = error;
        subTaskDone();
    }
};

#endif //JARVIS_EXEC_REQUEST_H
