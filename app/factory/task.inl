



template<class REQ, class RESP>
int NetworkTask<REQ, RESP>::getPeerAddr(struct sockaddr *addr, socklen_t *addrLen) const
{
    const struct sockaddr *p;
    socklen_t len;

    if (mTarget) {
        mTarget->getAddr(&p, &len);
        if (*addrLen >= len) {
            memcpy(addr, p, len);
            *addrLen = len;
            return 0;
        }
        errno = ENOBUFS;
    } else {
        errno = ENOTCONN;
    }

    return -1;
}

template<class REQ, class RESP>
class ClientTask : public NetworkTask<REQ, RESP>
{
protected:
    virtual CommMessageOut *messageOut()
    {
        /* By using prepare function, users can modify request after
         * the connection is established. */
        if (mPrepare) {
            mPrepare(this);
        }

        return &mReq;
    }

    virtual CommMessageIn *messageIn() { return &mResp; }

protected:
    virtual Connection *get_connection() const
    {
        CommConnection *conn;

        if (this->target) {
            conn = this->CommSession::get_connection();
            if (conn) {
                return (Connection*)conn;
            }
        }

        errno = ENOTCONN;
        return NULL;
    }

protected:
    virtual SubTask *done()
    {
        SeriesWork *series = seriesOf(this);

        if (mState == TASK_STATE_SYS_ERROR && mError < 0) {
            mState = TASK_STATE_SSL_ERROR;
            mError = -mError;
        }

        if (mCallback) {
            mCallback(this);
        }

        delete this;
        return series->pop();
    }

public:
    void setPrepare(std::function<void (NetworkTask<REQ, RESP>*)> prep)
    {
        mPrepare = std::move(prep);
    }

public:
    ClientTask(CommSchedObject *object, CommScheduler *scheduler, std::function<void (NetworkTask<REQ, RESP> *)>&& cb)
            : NetworkTask<REQ, RESP>(object, scheduler, std::move(cb))
    {
    }

protected:
    virtual ~ClientTask() { }

protected:
    std::function<void (NetworkTask<REQ, RESP>*)>       mPrepare;
};

template<class REQ, class RESP>
class ServerTask : public NetworkTask<REQ, RESP>
{
protected:
    virtual CommMessageOut *messageOut() { return &mResp; }
    virtual CommMessageIn *messageIn() { return &mReq; }
    virtual void handle(int state, int error);

protected:
    /* CommSession::get_connection() is supposed to be called only in the
     * implementations of it's virtual functions. As a server task, to call
     * this function after process() and before callback() is very dangerous
     * and should be blocked. */
    virtual Connection *getConnection() const
    {
        if (this->processor.task)
            return (Connection *)this->CommSession::getConnection();

        errno = EPERM;
        return NULL;
    }

protected:
    virtual void dispatch()
    {
        if (mState == TASK_STATE_TOREPLY) {
            /* Enable get_connection() again if the reply() call is success. */
            mProcessor.task = this;
            if (mScheduler->reply(this) >= 0)
                return;

            mError = errno;
            mProcessor.task = NULL;
            mState = TASK_STATE_SYS_ERROR;
        }

        subTaskDone();
    }

    virtual SubTask *done()
    {
        SeriesWork *series = series_of(this);

        if (mState == TASK_STATE_SYS_ERROR && mError < 0) {
            mState = TASK_STATE_SSL_ERROR;
            mError = -mError;
        }

        if (mCallback)
            mCallback(this);

        /* Defer deleting the task. */
        return series->pop();
    }

protected:
    class Processor : public SubTask
    {
    public:
        Processor(ServerTask<REQ, RESP>*task, std::function<void (NetworkTask<REQ, RESP> *)>& proc)
                : mProcess(proc)
        {
            mTask = task;
        }

        virtual void dispatch()
        {
            mProcess(this->task);
            mTask = NULL;	/* As a flag. get_conneciton() disabled. */
            subTaskDone();
        }

        virtual SubTask *done()
        {
            return seriesOf(this)->pop();
        }

        std::function<void (NetworkTask<REQ, RESP>*)>&          mProcess;
        ServerTask<REQ, RESP>*                                  mTask;
    } mProcessor;

    class Series : public SeriesWork
    {
    public:
        virtual void cancel()
        {
            SeriesWork::cancel();
            if (getLastTask() == mTask)
                unsetLastTask();
        }

        Series(ServerTask<REQ, RESP> *task)
                : SeriesWork(&task->processor, nullptr)
        {
            setLastTask(task);
            mTask = task;
        }

        virtual ~Series()
        {
            delete mTask;
        }

        ServerTask<REQ, RESP>*                  mTask;
    };

public:
    ServerTask(CommService *service, CommScheduler *scheduler, std::function<void (NetworkTask<REQ, RESP> *)>& proc)
            : NetworkTask<REQ, RESP>(NULL, scheduler, nullptr), mProcessor(this, proc)
    {
    }

protected:
    virtual ~ServerTask() { }
};

template<class REQ, class RESP>
void ServerTask<REQ, RESP>::handle(int state, int error)
{
    if (state == TASK_STATE_TOREPLY) {
        mState = TASK_STATE_TOREPLY;
        mTarget = getTarget();
        new Series(this);
        mProcessor.dispatch();
    } else if (mState == TASK_STATE_TOREPLY) {
        mState = state;
        mError = error;
        if (error == ETIMEDOUT) {
            mTimeoutReason = TOR_TRANSMIT_TIMEOUT;
        }
        subTaskDone();
    } else {
        delete this;
    }
}
