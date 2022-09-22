

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

        return &this->mReq;
    }

    virtual CommMessageIn *messageIn() { return &this->mResp; }

protected:
    virtual Connection *getConnection() const
    {
        CommConnection *conn;

        if (this->mTarget) {
            conn = this->CommSession::getConnection();
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

        if (this->mState == TASK_STATE_SYS_ERROR && this->mError < 0) {
            this->mState = TASK_STATE_SSL_ERROR;
            this->mError = -this->mError;
        }

        if (this->mCallback) {
            // FIXME:// DJ-
            this->mCallback(this, nullptr);
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
    ClientTask(CommSchedObject *object, CommScheduler *scheduler, std::function<void (NetworkTask<REQ, RESP>*, void*)>&& cb, void* udata)
            : NetworkTask<REQ, RESP>(object, scheduler, std::move(cb), udata)
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
    virtual CommMessageOut *messageOut() { return &this->mResp; }
    virtual CommMessageIn *messageIn() { return &this->mReq; }
    virtual void handle(int state, int error);

protected:
    /* CommSession::getConnection() is supposed to be called only in the
     * implementations of it's virtual functions. As a server task, to call
     * this function after process() and before callback() is very dangerous
     * and should be blocked. */
    virtual Connection *getConnection() const
    {
        if (this->mProcessor.mTask)
            return (Connection *)this->CommSession::getConnection();

        errno = EPERM;
        return NULL;
    }

protected:
    virtual void dispatch()
    {
        if (this->mState == TASK_STATE_TOREPLY) {
            /* Enable getConnection() again if the reply() call is success. */
            mProcessor.mTask = this;
            if (this->mScheduler->reply(this) >= 0)
                return;

            this->mError = errno;
            mProcessor.mTask = NULL;
            this->mState = TASK_STATE_SYS_ERROR;
        }

        this->subTaskDone();
    }

    virtual SubTask *done()
    {
        SeriesWork *series = seriesOf(this);

        if (this->mState == TASK_STATE_SYS_ERROR && this->mError < 0) {
            this->mState = TASK_STATE_SSL_ERROR;
            this->mError = -this->mError;
        }

        if (this->mCallback) {
            // FIXME:// DJ-
            this->mCallback(this, nullptr);
        }

        /* Defer deleting the task. */
        return series->pop();
    }

protected:
    class Processor : public SubTask
    {
    public:
        Processor(ServerTask<REQ, RESP>*task, std::function<void (NetworkTask<REQ, RESP> *)>& proc, void* udata)
                : mProcess(proc)
        {
            mTask = task;
        }

        virtual void dispatch()
        {
            mProcess(this->mTask);
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
                : SeriesWork(&task->mProcessor, nullptr)
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
    ServerTask(CommService *service, CommScheduler *scheduler, std::function<void (NetworkTask<REQ, RESP> *)>& proc, void* udata)
            : NetworkTask<REQ, RESP>(NULL, scheduler, nullptr, udata), mProcessor(this, proc, udata)
    {
    }

protected:
    virtual ~ServerTask() { }
};

template<class REQ, class RESP>
void ServerTask<REQ, RESP>::handle(int state, int error)
{
    if (state == TASK_STATE_TOREPLY) {
        this->mState = TASK_STATE_TOREPLY;
        this->mTarget = this->getTarget();
        new Series(this);
        mProcessor.dispatch();
    } else if (this->mState == TASK_STATE_TOREPLY) {
        this->mState = state;
        this->mError = error;
        if (error == ETIMEDOUT) {
            this->mTimeoutReason = TOR_TRANSMIT_TIMEOUT;
        }
        this->subTaskDone();
    } else {
        delete this;
    }
}
