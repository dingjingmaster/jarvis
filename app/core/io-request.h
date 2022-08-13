//
// Created by dingjing on 8/13/22.
//

#ifndef JARVIS_IO_REQUEST_H
#define JARVIS_IO_REQUEST_H

#include <errno.h>

#include "sub-task.h"
#include "communicator.h"

class IORequest : public SubTask, public IOSession
{
public:
    IORequest(IOService *service)
    {
        mService = service;
    }

public:
    virtual void dispatch()
    {
        if (mService->request(this) < 0) {
            handle(IOS_STATE_ERROR, errno);
        }

    }

protected:
    int                         mState;
    int                         mError;

protected:
    IOService*                  mService;

protected:
    virtual void handle(int state, int error)
    {
        mState = state;
        mError = error;
        subTaskDone();
    }
};

#endif //JARVIS_IO_REQUEST_H
