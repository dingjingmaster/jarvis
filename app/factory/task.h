//
// Created by dingjing on 8/17/22.
//

#ifndef JARVIS_TASK_H
#define JARVIS_TASK_H

#include <atomic>
#include <utility>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <functional>

#include "workflow.h"
#include "connection.h"
#include "../core/c-log.h"
#include "../core/executor.h"
#include "../core/io-request.h"
#include "../core/exec-request.h"
#include "../core/communicator.h"
#include "../core/sleep-request.h"
#include "../core/common-request.h"
#include "../core/common-scheduler.h"

enum
{
    TASK_STATE_UNDEFINED        = -1,
    TASK_STATE_SUCCESS          = CS_STATE_SUCCESS,
    TASK_STATE_TOREPLY          = CS_STATE_TOREPLY,		/* for server task only */
    TASK_STATE_NOREPLY          = CS_STATE_TOREPLY + 1,	/* for server task only */
    TASK_STATE_SYS_ERROR        = CS_STATE_ERROR,
    TASK_STATE_SSL_ERROR        = 65,
    TASK_STATE_DNS_ERROR        = 66,					/* for client task only */
    TASK_STATE_TASK_ERROR       = 67,
    TASK_STATE_ABORTED          = CS_STATE_STOPPED
};


template <class INPUT, class OUTPUT>
class ThreadTask : public ExecRequest
{
public:
    void start()
    {
        assert(!seriesOf(this));
        Workflow::startSeriesWork(this, nullptr);
    }

    void dismiss()
    {
        assert(!seriesOf(this));
        delete this;
    }

public:
    INPUT* getInput() { return &mInput; }
    OUTPUT* getOutput() { return &mOutput; }


public:
    int getState() const { return mState; }
    int getError() const { return mError; }

public:
    void setCallback(std::function<void (ThreadTask<INPUT, OUTPUT> *)> cb)
    {
        mCallback = std::move(cb);
    }

protected:
    virtual SubTask *done()
    {
        SeriesWork *series = seriesOf(this);

        if (this->mCallback) {
            this->mCallback(this);
        }

        delete this;
        return series->pop();
    }

public:
    ThreadTask(ExecQueue *queue, Executor *executor, std::function<void (ThreadTask<INPUT, OUTPUT> *)>&& cb)
        : ExecRequest(queue, executor), mCallback(std::move(cb))
    {
        mError = 0;
        mUserData = NULL;
        mState = TASK_STATE_UNDEFINED;
    }

protected:
    virtual ~ThreadTask() { }

public:
    void*                                                   mUserData;

protected:
    INPUT                                                   mInput;
    OUTPUT                                                  mOutput;
    std::function<void (ThreadTask<INPUT, OUTPUT> *)>       mCallback;

};


template<class INPUT, class OUTPUT>
class MultiThreadTask : public ParallelTask
{
public:
    void start()
    {
        assert(!seriesOf(this));
        Workflow::startSeriesWork(this, nullptr);
    }

    void dismiss()
    {
        assert(!seriesOf(this));
        delete this;
    }

public:
    INPUT *getInput(size_t index)
    {
        return static_cast<Thread *>(this->subtasks[index])->getInput();
    }

    OUTPUT *getOutput(size_t index)
    {
        return static_cast<Thread *>(this->subtasks[index])->getOutput();
    }


public:
    int getState(size_t index) const
    {
        return static_cast<const Thread *>(this->subtasks[index])->getState();
    }

    int getError(size_t index) const
    {
        return static_cast<const Thread *>(this->subtasks[index])->getError();
    }

public:
    void setCallback(std::function<void (MultiThreadTask<INPUT, OUTPUT> *)> cb)
    {
        this->callback = std::move(cb);
    }

protected:
    virtual SubTask *done()
    {
        SeriesWork *series = seriesOf(this);

        if (mCallback)
            mCallback(this);

        delete this;
        return series->pop();
    }


protected:
    using Thread = ThreadTask<INPUT, OUTPUT>;

public:
    MultiThreadTask(Thread *const tasks[], size_t n, std::function<void (MultiThreadTask<INPUT, OUTPUT> *)>&& cb)
        : ParallelTask(new SubTask *[n], n), mCallback(std::move(cb))
    {
        size_t i;

        for (i = 0; i < n; i++)
            this->subtasks[i] = tasks[i];

        mUserData = NULL;
    }

protected:
    virtual ~MultiThreadTask()
    {
        size_t n = mSubTasksNR;

        while (n > 0)
            delete mSubTasks[--n];

        delete []   mSubTasks;
    }


public:
    void*                                                   mUserData;

protected:
    std::function<void (MultiThreadTask<INPUT, OUTPUT> *)>  mCallback;
};


template<class REQ, class RESP>
class NetworkTask : public CommonRequest
{
public:
    /* start(), dismiss() are for client tasks only. */
    void start()
    {
        logv("");
        assert(!seriesOf(this));
        Workflow::startSeriesWork(this, nullptr);
    }

    void dismiss()
    {
        logv("");
        assert(!seriesOf(this));
        delete this;
    }

public:
    REQ *getReq()
    {
        logv("");
        return &mReq;
    }
    RESP *getResp()
    {
        logv("resp: %p", &mResp);
        return &mResp;
    }


public:
    int getState() const
    {
        logv("");
        return mState;
    }
    int getError() const
    {
        logv("");
        return mError;
    }

    /* Call when error is ETIMEDOUT, return values:
     * TOR_NOT_TIMEOUT, TOR_WAIT_TIMEOUT, TOR_CONNECT_TIMEOUT,
     * TOR_TRANSMIT_TIMEOUT (send or receive).
     * SSL connect timeout also returns TOR_CONNECT_TIMEOUT. */
    int getTimeoutReason() const { return mTimeoutReason; }

    /* Call only in callback or server's process. */
    long long getTaskSeq() const
    {
        logv("");
        if (!mTarget) {
            errno = ENOTCONN;
            return -1;
        }

        return getSeq();
    }

    int getPeerAddr(struct sockaddr *addr, socklen_t *addrLen) const;

    virtual Connection *getConnection() const = 0;

public:
    /* All in milliseconds. timeout == -1 for unlimited. */
    void setSendTimeout(int timeout)
    {
        logv("");
        mSendTime = timeout;
    }
    void setKeepAlive(int timeout)
    {
        logv("");
        mKeepAliveTime = timeout;
    }
    void setReceiveTimeout(int timeout)
    {
        logv("");
        mReceiveTime = timeout;
    }

public:
    /* noReply(), push() are for server tasks only. */
    void noReply()
    {
        logv("");
        if (this->state == TASK_STATE_TOREPLY)
            this->state = TASK_STATE_NOREPLY;
    }

    virtual int push(const void *buf, size_t size)
    {
        logv("");
        return this->mScheduler->push(buf, size, this);
    }

public:
    void setCallback(std::function<void (NetworkTask<REQ, RESP>*, void*)> cb)
    {
        logv("");
        mCallback = std::move(cb);
    }

protected:
    virtual int sendTimeout()
    {
        logv("");
        return mSendTime;
    }
    virtual int receiveTimeout()
    {
        logv("");
        return mReceiveTime;
    }
    virtual int keepAliveTimeout()
    {
        logv("");
        return mKeepAliveTime;
    }


protected:
    NetworkTask(CommSchedObject *object, CommScheduler *scheduler, std::function<void (NetworkTask<REQ, RESP>*, void*)>&& cb, void* udata)
        : CommonRequest(object, scheduler), mCallback(std::move(cb)), mUdata(udata)
    {
        mError = 0;
        mSendTime = -1;
        mReceiveTime = -1;
        mKeepAliveTime = 0;
        mTarget = NULL;
        mUserData = NULL;
        mState = TASK_STATE_UNDEFINED;
        mTimeoutReason = TOR_NOT_TIMEOUT;

        logv("");
    }

    virtual ~NetworkTask()
    {
        logv("");
    }

public:
    void*                                                   mUserData;

protected:
    int                                                     mSendTime;
    int                                                     mReceiveTime;
    int                                                     mKeepAliveTime;
    REQ                                                     mReq;
    RESP                                                    mResp;
    std::function<void (NetworkTask<REQ, RESP>*, void*)>    mCallback;

private:
    void*                                                   mUdata;
};


class TimerTask : public SleepRequest
{
public:
    void start()
    {
        logv("");
        assert(!seriesOf(this));
        Workflow::startSeriesWork(this, nullptr);
    }

    void dismiss()
    {
        logv("");
        assert(!seriesOf(this));
        delete this;
    }


public:
    int getState() const
    {
        logv("");
        return mState;
    }
    int getError() const
    {
        logv("");
        return mError;
    }

public:
    void setCallback(std::function<void (TimerTask*)> cb)
    {
        logv("");
        mCallback = std::move(cb);
    }

protected:
    virtual SubTask *done()
    {
        logv("");
        SeriesWork *series = seriesOf(this);

        if (mCallback) {
            logv("");
            mCallback(this);
        }

        delete this;
        return series->pop();
    }


public:
    TimerTask(CommScheduler *scheduler, std::function<void (TimerTask*)> cb)
        : SleepRequest(scheduler), mCallback(std::move(cb))
    {
        logv("");
        mUserData = NULL;
        mState = TASK_STATE_UNDEFINED;
        mError = 0;
    }

protected:
    virtual ~TimerTask()
    {
        logv("");
    }


public:
    void*                                       mUserData;

protected:
    std::function<void (TimerTask*)>            mCallback;
};


template<class ARGS>
class FileTask : public IORequest
{
public:
    void start()
    {
        logv("");
        assert(!seriesOf(this));
        Workflow::startSeriesWork(this, nullptr);
    }

    void dismiss()
    {
        logv("");
        assert(!seriesOf(this));
        delete this;
    }

public:
    ARGS* getArgs()
    {
        logv("");
        return &mArgs;
    }

    long getRetVal() const
    {
        logv("");
        if (mState == TASK_STATE_SUCCESS)
            return this->getRes();
        else
            return -1;
    }


public:
    int getState() const
    {
        logv("");
        return mState;
    }
    int getError() const
    {
        logv("");
        return mError;
    }

public:
    void setCallback(std::function<void (FileTask<ARGS>*)> cb)
    {
        logv("");
        mCallback = std::move(cb);
    }

protected:
    virtual SubTask *done()
    {
        logv("");
        SeriesWork *series = seriesOf(this);

        if (mCallback)
            mCallback(this);

        delete this;
        return series->pop();
    }


public:
    FileTask(IOService *service, std::function<void (FileTask<ARGS>*)>&& cb)
        : IORequest(service), mCallback(std::move(cb))
    {
        logv("");
        mError = 0;
        mUserData = NULL;
        mState = TASK_STATE_UNDEFINED;
    }

protected:
    virtual ~FileTask()
    {
        logv("");
    }

public:
    void*                                       mUserData;

protected:
    ARGS                                        mArgs;
    std::function<void (FileTask<ARGS>*)>       mCallback;
};


class GenericTask : public SubTask
{
public:
    void start()
    {
        logv("");
        assert(!seriesOf(this));
        Workflow::startSeriesWork(this, nullptr);
    }

    void dismiss()
    {
        logv("");
        assert(!seriesOf(this));
        delete this;
    }


public:
    int getState() const
    {
        logv("");
        return mState;
    }
    int getError() const
    {
        logv("");
        return mError;
    }

protected:
    virtual void dispatch()
    {
        logv("");
        subTaskDone();
    }

    virtual SubTask *done()
    {
        logv("");
        SeriesWork *series = seriesOf(this);
        delete this;
        return series->pop();
    }

public:
    GenericTask()
    {
        logv("");
        mUserData = NULL;
        mState = TASK_STATE_UNDEFINED;
        mError = 0;
    }

protected:
    virtual ~GenericTask()
    {
        logv("");
    }


public:
    void*                               mUserData;

protected:
    int                                 mState;
    int                                 mError;
};


class CounterTask : public GenericTask
{
public:
    virtual void count()
    {
        logv("");
        if (--this->mValue == 0) {
            mState = TASK_STATE_SUCCESS;
            subTaskDone();
        }
    }

public:
    void setCallback(std::function<void (CounterTask*)> cb)
    {
        logv("");
        mCallback = std::move(cb);
    }

protected:
    virtual void dispatch()
    {
        logv("");
        CounterTask::count();
    }

    virtual SubTask *done()
    {
        logv("");
        SeriesWork *series = seriesOf(this);

        if (mCallback)
            mCallback(this);

        delete this;
        return series->pop();
    }


public:
    CounterTask(unsigned int targetValue, std::function<void (CounterTask*)>&& cb)
        : mValue(targetValue + 1), mCallback(std::move(cb))
    {
        logv("");
    }

protected:
    virtual ~CounterTask()
    {
        logv("");
    }

protected:
    std::atomic<unsigned int>                           mValue;
    std::function<void (CounterTask *)>                 mCallback;
};


class MailboxTask : public GenericTask
{
public:
    void send(void *msg)
    {
        logv("");
        *mNext++ = msg;
        count();
    }

    void **getMailbox(size_t *n)
    {
        logv("");
        *n = mNext - mMailbox;
        return mMailbox;
    }

public:
    void setCallback(std::function<void (MailboxTask *)> cb)
    {
        logv("");
        mCallback = std::move(cb);
    }

public:
    virtual void count()
    {
        logv("");
        if (--mValue == 0) {
            mState = TASK_STATE_SUCCESS;
            subTaskDone();
        }
    }

protected:
    virtual void dispatch()
    {
        logv("");
        MailboxTask::count();
    }

    virtual SubTask *done()
    {
        logv("");
        SeriesWork *series = seriesOf(this);

        if (mCallback)
            mCallback(this);

        delete this;
        return series->pop();
    }


public:
    MailboxTask(void** mailbox, size_t size, std::function<void (MailboxTask*)>&& cb)
        : mNext(mailbox), mValue(size + 1), mCallback(std::move(cb))
    {
        logv("");
        mMailbox = mailbox;
    }

    MailboxTask(std::function<void (MailboxTask*)>&& cb)
        : mNext(&mUserData), mValue(2), mCallback(std::move(cb))
    {
        logv("");
        mMailbox = &mUserData;
    }

protected:
    virtual ~MailboxTask()
    {
        logv("");
    }

protected:
    void**                                  mMailbox;
    std::atomic<void**>                     mNext;
    std::atomic<size_t>                     mValue;
    std::function<void (MailboxTask*)>      mCallback;
};


class Conditional : public GenericTask
{
public:
    virtual void signal(void *msg)
    {
        logv("");
        *mMsgBuf = msg;
        if (mFlag.exchange(true)) {
            subTaskDone();
        }
    }

protected:
    virtual void dispatch()
    {
        logv("");
        seriesOf(this)->pushFront(mTask);
        mTask = NULL;
        if (mFlag.exchange(true)) {
            subTaskDone();
        }
    }

public:
    Conditional(SubTask *task, void **msgBuf) : mFlag(false)
    {
        logv("");
        mTask = task;
        mMsgBuf = msgBuf;
    }

    Conditional(SubTask *task) : mFlag(false)
    {
        logv("");
        mTask = task;
        mMsgBuf = &mUserData;
    }

protected:
    virtual ~Conditional()
    {
        logv("");
        delete mTask;
    }

protected:
    std::atomic<bool>                       mFlag;
    SubTask*                                mTask;
    void**                                  mMsgBuf;
};


class GoTask : public ExecRequest
{
public:
    void start()
    {
        logv("");
        assert(!seriesOf(this));
        Workflow::startSeriesWork(this, nullptr);
    }

    void dismiss()
    {
        logv("");
        assert(!seriesOf(this));
        delete this;
    }

public:
    int getState() const
    {
        logv("");
        return mState;
    }
    int getError() const
    {
        logv("");
        return mError;
    }

public:
    void setCallback(std::function<void (GoTask*)> cb)
    {
        logv("");
        mCallback = std::move(cb);
    }

protected:
    virtual SubTask *done()
    {
        logv("");
        SeriesWork *series = seriesOf(this);

        if (mCallback) {
            mCallback(this);
        }

        delete this;
        return series->pop();
    }


public:
    GoTask(ExecQueue *queue, Executor *executor)
        : ExecRequest(queue, executor)
    {
        logv("");
        mError = 0;
        mUserData = NULL;
        mState = TASK_STATE_UNDEFINED;
    }

protected:
    virtual ~GoTask() { }

public:
    void*                           mUserData;

protected:
    std::function<void (GoTask*)>   mCallback;
};


class RepeaterTask : public GenericTask
{
public:
    void setCreate(std::function<SubTask *(RepeaterTask*)> create)
    {
        logv("");
        mCreate = std::move(create);
    }

    void setCallback(std::function<void (RepeaterTask*)> callback)
    {
        logv("");
        mCallback = std::move(callback);
    }

protected:
    virtual void dispatch()
    {
        logv("");
        SubTask *task = mCreate(this);

        if (task) {
            seriesOf(this)->pushFront(this);
            seriesOf(this)->pushFront(task);
        } else {
            mState = TASK_STATE_SUCCESS;
        }

        subTaskDone();
    }

    virtual SubTask *done()
    {
        logv("");
        SeriesWork *series = seriesOf(this);

        if (mState != TASK_STATE_UNDEFINED) {
            if (mCallback) {
                mCallback(this);
            }

            delete this;
        }

        return series->pop();
    }


public:
    RepeaterTask(std::function<SubTask* (RepeaterTask*)>&& create, std::function<void (RepeaterTask*)>&& cb)
        : mCreate(std::move(create)), mCallback(std::move(cb))
    {
        logv("");
    }

protected:
    virtual ~RepeaterTask()
    {
        logv("");
    }

protected:
    std::function<SubTask *(RepeaterTask*)>         mCreate;
    std::function<void (RepeaterTask*)>             mCallback;
};


class ModuleTask : public ParallelTask, protected SeriesWork
{
public:
    void start()
    {
        logv("");
        assert(!seriesOf(this));
        Workflow::startSeriesWork(this, nullptr);
    }

    void dismiss()
    {
        logv("");
        assert(!seriesOf(this));
        delete this;
    }

public:
    SeriesWork* subSeries()
    {
        logv("");
        return this;
    }

    const SeriesWork* subSeries() const
    {
        logv("");
        return this;
    }

public:
    void setCallback(std::function<void (const ModuleTask *)> cb)
    {
        logv("");
        mCallback = std::move(cb);
    }

protected:
    virtual SubTask *done()
    {
        logv("");
        SeriesWork *series = seriesOf(this);

        if (mCallback) {
            mCallback(this);
        }
        delete this;
        return series->pop();
    }

public:
    ModuleTask(SubTask *first, std::function<void (const ModuleTask *)>&& cb)
        : ParallelTask(&mFirst, 1), SeriesWork(first, nullptr), mCallback(std::move(cb))
    {
        logv("");
        mFirst = first;
        setInParallel();
        mUserData = NULL;
    }

protected:
    virtual ~ModuleTask()
    {
        logv("");
        if (!isFinished()) {
            dismissRecursive();
        }
    }

public:
    void*                                           mUserData;

protected:
    SubTask*                                        mFirst;
    std::function<void (const ModuleTask*)>         mCallback;
};


#include "task.inl"







#endif //JARVIS_TASK_H
