#include <time.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <new>
#include <atomic>
#include <string>
#include <utility>
#include <functional>

#include "task.h"
#include "workflow.h"
#include "../manager/global.h"
#include "../factory/task-error.h"
#include "../manager/route-manager.h"
#include "../manager/endpoint-params.h"
#include "../nameservice/name-service.h"

#include "../utils/uri-parser.h"

class _GoTask : public GoTask
{
protected:
    virtual void execute()
    {
        this->go();
    }

protected:
    std::function<void ()> go;

public:
    _GoTask(ExecQueue *queue, Executor *executor, std::function<void ()>&& func)
        : GoTask(queue, executor), go(std::move(func))
    {
    }
};

class _TimedGoTask : public _GoTask
{
protected:
    virtual void dispatch();
    virtual SubTask *done();

protected:
    virtual void handle(int state, int error);

protected:
    static void timerCallback(TimerTask *timer);

protected:
    time_t                          mSeconds;
    long                            mNanoseconds;
    std::atomic<int>                mRef;

public:
    _TimedGoTask(time_t seconds, long nanoseconds, ExecQueue *queue, Executor *executor, std::function<void ()>&& func)
        : _GoTask(queue, executor, std::move(func)), mRef(4)
    {
        mSeconds = seconds;
        mNanoseconds = nanoseconds;
    }
};

template<class FUNC, class... ARGS>
inline GoTask* TaskFactory::createGoTask(const std::string& queueName, FUNC&& func, ARGS&&... args)
{
    auto&& tmp = std::bind(std::forward<FUNC>(func), std::forward<ARGS>(args)...);

    return new _GoTask(Global::getExecQueue(queueName), Global::getComputeExecutor(), std::move(tmp));
}

template<class FUNC, class... ARGS>
GoTask* TaskFactory::createTimedGoTask(time_t seconds, long nanoseconds, const std::string& queueName, FUNC&& func, ARGS&&... args)
{
    auto&& tmp = std::bind(std::forward<FUNC>(func), std::forward<ARGS>(args)...);

    return new _TimedGoTask(seconds, nanoseconds, Global::getExecQueue(queueName), Global::getComputeExecutor(), std::move(tmp));
}

template<class FUNC, class... ARGS>
inline GoTask* TaskFactory::createGoTask(ExecQueue *queue, Executor *executor, FUNC&& func, ARGS&&... args)
{
    auto&& tmp = std::bind(std::forward<FUNC>(func), std::forward<ARGS>(args)...);

    return new _GoTask(queue, executor, std::move(tmp));
}

template<class FUNC, class... ARGS>
GoTask* TaskFactory::createTimedGoTask(time_t seconds, long nanoseconds, ExecQueue *queue, Executor *executor, FUNC&& func, ARGS&&... args)
{
    auto&& tmp = std::bind(std::forward<FUNC>(func), std::forward<ARGS>(args)...);

    return new _TimedGoTask(seconds, nanoseconds, queue, executor, std::move(tmp));
}

class _DynamicTask : public DynamicTask
{
protected:
    virtual void dispatch()
    {
        seriesOf(this)->pushFront(mCreate(this));
        DynamicTask::dispatch();
    }

protected:
    std::function<SubTask* (DynamicTask*)>                  mCreate;

public:
    _DynamicTask(std::function<SubTask* (DynamicTask*)>&& create)
        : mCreate(std::move(create))
    {
    }
};

inline DynamicTask* TaskFactory::createDynamicTask(DynamicCreate create)
{
    return new _DynamicTask(std::move(create));
}

template<class REQ, class RESP, typename CTX = bool>
class ComplexClientTask : public ClientTask<REQ, RESP>
{
protected:
    using TaskCallback = std::function<void (NetworkTask<REQ, RESP>*)>;

public:
    ComplexClientTask(int retry_max, TaskCallback && cb)
        : ClientTask<REQ, RESP>(NULL, Global::getScheduler(), std::move(cb))
    {
        mType = TT_TCP;
        mFixedAddr = false;
        mRetryMax = retry_max;
        mRetryTimes = 0;
        mRedirect = false;
        mNsPolicy = NULL;
        mRouterTask = NULL;
    }

protected:
    // new api for children
    virtual bool initSuccess() { return true; }
    virtual void initFailed() {}
    virtual bool checkRequest() { return true; }
    virtual RouterTask *route();
    virtual bool finishOnce() { return true; }

public:
    void init(const ParsedURI& uri)
    {
        mUri = uri;
        initWithUri();
    }

    void init(ParsedURI&& uri)
    {
        mUri = std::move(uri);
        initWithUri();
    }

    void init(TransportType type,
              const struct sockaddr *addr,
              socklen_t addrlen,
              const std::string& info);

    void setTransportType(TransportType type)
    {
        mType = type;
    }

    TransportType getTransportType() const { return mType; }

    virtual const ParsedURI *getCurrentUri() const { return &mUri; }

    void setRedirect(const ParsedURI& uri)
    {
        mRedirect = true;
        init(uri);
    }

    void setRedirect(TransportType type, const struct sockaddr *addr,
                      socklen_t addrlen, const std::string& info)
    {
        mRedirect = true;
        init(type, addr, addrlen, info);
    }

    bool isFixedAddr() const { return this->fixed_addr_; }

protected:
    void setFixedAddr(int fixed) { this->fixed_addr_ = fixed; }

    void setInfo(const std::string& info)
    {
        mInfo.assign(info);
    }

    void setInfo(const char *info)
    {
        mInfo.assign(info);
    }

protected:
    virtual void dispatch();
    virtual SubTask *done();

    void clearResp()
    {
        size_t size = this->resp.get_size_limit();

        this->resp.~RESP();
        new(&this->resp) RESP();
        this->resp.setSizeLimit(size);
    }

    void disableRetry()
    {
        mRetryTimes = mRetryMax;
    }

protected:
    TransportType                           mType;
    ParsedURI                               mUri;
    std::string                             mInfo;
    bool                                    mFixedAddr;
    bool                                    mRedirect;
    CTX                                     mCtx;
    int                                     mRetryMax;
    int                                     mRetryTimes;
    NSPolicy*                               mNsPolicy;
    RouterTask*                             mRouterTask;
    RouteManager::RouteResult               mRouteResult;
    NSTracing                               mTracing;

public:
    CTX* getMutableCtx() { return &mCtx; }

private:
    void clearPrevState();
    void initWithUri();
    bool setPort();
    void routerCallback(void *t);
    void switchCallback(void *t);
};

template<class REQ, class RESP, typename CTX>
void ComplexClientTask<REQ, RESP, CTX>::clearPrevState()
{
    mNsPolicy = NULL;
    mRouteResult.clear();
    if (mTracing.deleter) {
        mTracing.deleter(mTracing.data);
        mTracing.deleter = NULL;
    }
    mTracing.data = NULL;
    mRetryTimes = 0;
    this->state = TASK_STATE_UNDEFINED;
    this->error = 0;
    this->timeout_reason = TOR_NOT_TIMEOUT;
}

template<class REQ, class RESP, typename CTX>
void ComplexClientTask<REQ, RESP, CTX>::init(TransportType type, const struct sockaddr *addr, socklen_t addrLen, const std::string& info)
{
    if (mRedirect)
        clearPrevState();

    auto params = Global::getGlobalSettings()->endpointParams;
    struct addrinfo addrInfo = { };
    addrInfo.ai_family = addr->sa_family;
    addrInfo.ai_socktype = SOCK_STREAM;
    addrInfo.ai_addr = (struct sockaddr *)addr;
    addrInfo.ai_addrlen = addrLen;

    mType = type;
    mInfo.assign(info);
    params.use_tls_sni = false;
    if (Global::getRouteManager()->get(type, &addrInfo, mInfo, &params, "", mRouteResult) < 0) {
        this->state = TASK_STATE_SYS_ERROR;
        this->error = errno;
    } else if (this->init_success()) {
        return;
    }

    this->init_failed();
}

template<class REQ, class RESP, typename CTX>
bool ComplexClientTask<REQ, RESP, CTX>::setPort()
{
    if (mUri.port) {
        int port = atoi(mUri.port);

        if (port <= 0 || port > 65535) {
            this->state = TASK_STATE_TASK_ERROR;
            this->error = TASK_ERROR_URI_PORT_INVALID;
            return false;
        }

        return true;
    }

    if (mUri.scheme) {
        const char *port_str = Global::getDefaultPort(mUri.scheme);

        if (port_str) {
            mUri.port = strdup(port_str);
            if (mUri.port)
                return true;

            this->state = TASK_STATE_SYS_ERROR;
            this->error = errno;
            return false;
        }
    }

    this->state = TASK_STATE_TASK_ERROR;
    this->error = TASK_ERROR_URI_SCHEME_INVALID;
    return false;
}

template<class REQ, class RESP, typename CTX>
void ComplexClientTask<REQ, RESP, CTX>::initWithUri()
{
    if (mRedirect) {
        clearPrevState();
        mNsPolicy = Global::getDnsResolver();
    }

    if (mUri.state == URI_STATE_SUCCESS) {
        if (this->set_port()) {
            if (this->init_success()) {
                return;
            }
        }
    } else if (mUri.state == URI_STATE_ERROR) {
        this->state = TASK_STATE_SYS_ERROR;
        this->error = mUri.error;
    } else {
        this->state = TASK_STATE_TASK_ERROR;
        this->error = TASK_ERROR_URI_PARSE_FAILED;
    }

    this->init_failed();
}

template<class REQ, class RESP, typename CTX>
RouterTask *ComplexClientTask<REQ, RESP, CTX>::route()
{
    auto&& cb = std::bind(&ComplexClientTask::router_callback, this, std::placeholders::_1);
    NSParams params = {
            .type			=	mType,
            .uri			=	mUri,
            .info			=	mInfo.c_str(),
            .fixed_addr		=	mFixedAddr,
            .retry_times	=	mRetryTimes,
            .tracing		=	&mTracing,
    };

    if (!mNsPolicy) {
        NameService *ns = Global::getNameService();
        mNsPolicy = ns->getPolicy(mUri.host ? mUri.host : "");
    }

    return mNsPolicy->create_router_task(&params, std::move(cb));
}

template<class REQ, class RESP, typename CTX>
void ComplexClientTask<REQ, RESP, CTX>::routerCallback(void *t)
{
    RouterTask *task = (RouterTask *)t;

    this->state = task->getState();
    if (this->state == TASK_STATE_SUCCESS)
        mRouteResult = std::move(*task->get_result());
    else if (this->state == TASK_STATE_UNDEFINED) {
        /* should not happend */
        this->state = TASK_STATE_SYS_ERROR;
        this->error = ENOSYS;
    } else {
        this->error = task->getError();
    }
}

template<class REQ, class RESP, typename CTX>
void ComplexClientTask<REQ, RESP, CTX>::dispatch()
{
    switch (this->state) {
        case TASK_STATE_UNDEFINED: {
            if (this->check_request()) {
                if (this->route_result_.request_object) {
                    case TASK_STATE_SUCCESS:
                        this->set_request_object(mRouteResult.request_object);
                    this->ClientTask<REQ, RESP>::dispatch();
                    return;
                }

                mRouterTask = this->route();
                seriesOf(this)->pushFront(this);
                seriesOf(this)->pushFront(mRouterTask);
            }
        }
        default:
            break;
    }

    this->subtask_done();
}

template<class REQ, class RESP, typename CTX>
void ComplexClientTask<REQ, RESP, CTX>::switchCallback(void *t)
{
    if (!mRedirect) {
        if (this->state == TASK_STATE_SYS_ERROR && this->error < 0) {
            this->state = TASK_STATE_SSL_ERROR;
            this->error = -this->error;
        }

        if (mTracing.deleter) {
            mTracing.deleter(mTracing.data);
            mTracing.deleter = NULL;
        }

        if (this->callback) {
            this->callback(this);
        }
    }

    if (mRedirect) {
        mRedirect = false;
        clearResp();
        this->target = NULL;
        seriesOf(this)->pushFront(this);
    } else {
        delete this;
    }
}

template<class REQ, class RESP, typename CTX>
SubTask* ComplexClientTask<REQ, RESP, CTX>::done()
{
    SeriesWork *series = seriesOf(this);

    if (mRouterTask) {
        mRouterTask = NULL;
        return series->pop();
    }

    bool is_user_request = this->finish_once();

    if (mNsPolicy && mRouteResult.request_object) {
        if (this->state == TASK_STATE_SYS_ERROR)
            mNsPolicy->failed(&mRouteResult, &mTracing, this->target);
        else
            mNsPolicy->success(&mRouteResult, &mTracing, this->target);
    }

    if (this->state == TASK_STATE_SUCCESS) {
        if (!is_user_request)
            return this;
    } else if (this->state == TASK_STATE_SYS_ERROR) {
        if (mRetryTimes < mRetryMax) {
            mRedirect = true;
            if (mNsPolicy)
                mRouteResult.clear();

            this->state = TASK_STATE_UNDEFINED;
            this->error = 0;
            this->timeout_reason = 0;
            mRetryTimes++;
        }
    }

    /*
     * When target is NULL, it's very likely that we are in the caller's
     * thread or DNS thread (dns failed). Running a timer will switch callback
     * function to a handler thread, and this can prevent stack overflow.
     */
    if (!this->target) {
        auto&& cb = std::bind(&ComplexClientTask::switch_callback, this, std::placeholders::_1);
        TimerTask *timer;

        timer = TaskFactory::createTimerTask(0, 0, std::move(cb));
        series->pushFront(timer);
    } else {
        this->switch_callback(NULL);
    }

    return series->pop();
}

/**********Template Network Factory**********/

template<class REQ, class RESP>
NetworkTask<REQ, RESP> *
NetworkTaskFactory<REQ, RESP>::createClientTask(TransportType type, const std::string& host, unsigned short port, int retry_max, std::function<void (NetworkTask<REQ, RESP> *)> callback)
{
    auto *task = new ComplexClientTask<REQ, RESP>(retry_max, std::move(callback));
    char buf[8];
    std::string url = "scheme://";
    ParsedURI uri;

    sprintf(buf, "%u", port);
    url += host;
    url += ":";
    url += buf;
    URIParser::parse(url, uri);
    task->init(std::move(uri));
    task->set_transport_type(type);
    return task;
}

template<class REQ, class RESP>
NetworkTask<REQ, RESP> *
NetworkTaskFactory<REQ, RESP>::createClientTask(TransportType type, const std::string& url, int retry_max, std::function<void (NetworkTask<REQ, RESP> *)> callback)
{
    auto *task = new ComplexClientTask<REQ, RESP>(retry_max, std::move(callback));
    ParsedURI uri;

    URIParser::parse(url, uri);
    task->init(std::move(uri));
    task->set_transport_type(type);
    return task;
}

template<class REQ, class RESP>
NetworkTask<REQ, RESP> *
NetworkTaskFactory<REQ, RESP>::createClientTask(TransportType type, const ParsedURI& uri, int retry_max, std::function<void (NetworkTask<REQ, RESP> *)> callback)
{
    auto *task = new ComplexClientTask<REQ, RESP>(retry_max, std::move(callback));

    task->init(uri);
    task->set_transport_type(type);
    return task;
}

template<class REQ, class RESP>
NetworkTask<REQ, RESP> *
NetworkTaskFactory<REQ, RESP>::createServerTask(CommService *service, std::function<void (NetworkTask<REQ, RESP> *)>& process)
{
    return new ServerTask<REQ, RESP>(service, Global::getScheduler(), process);
}

/**********Server Factory**********/

class ServerTaskFactory
{
public:
    static HttpTask *create_http_task(CommService *service, std::function<void (HttpTask *)>& process);
    //static MySQLTask *create_mysql_task(CommService *service, std::function<void (MySQLTask *)>& process);
};

/**********Template Thread Task Factory**********/

template<class INPUT, class OUTPUT>
class _ThreadTask : public ThreadTask<INPUT, OUTPUT>
{
protected:
    virtual void execute()
    {
        this->routine(&this->input, &this->output);
    }

protected:
    std::function<void (INPUT *, OUTPUT *)> routine;

public:
    _ThreadTask(ExecQueue *queue, Executor *executor,
                   std::function<void (INPUT *, OUTPUT *)>&& rt,
                   std::function<void (ThreadTask<INPUT, OUTPUT>*)>&& cb) :
            ThreadTask<INPUT, OUTPUT>(queue, executor, std::move(cb)),
            routine(std::move(rt))
    {
    }
};

template<class INPUT, class OUTPUT>
ThreadTask<INPUT, OUTPUT> *
ThreadTaskFactory<INPUT, OUTPUT>::createThreadTask(const std::string& queue_name,
                                                       std::function<void (INPUT *, OUTPUT *)> routine,
                                                       std::function<void (ThreadTask<INPUT, OUTPUT>*)> callback)
{
    return new _ThreadTask<INPUT, OUTPUT>(Global::getExecQueue(queue_name),
                                             Global::getComputeExecutor(),
                                             std::move(routine),
                                             std::move(callback));
}

template<class INPUT, class OUTPUT>
ThreadTask<INPUT, OUTPUT> *
ThreadTaskFactory<INPUT, OUTPUT>::createThreadTask(ExecQueue *queue, Executor *executor,
                                                       std::function<void (INPUT *, OUTPUT *)> routine,
                                                       std::function<void (ThreadTask<INPUT, OUTPUT> *)> callback)
{
    return new _ThreadTask<INPUT, OUTPUT>(queue, executor,
                                             std::move(routine),
                                             std::move(callback));
}

template<class INPUT, class OUTPUT>
class _ThreadTask__ : public _ThreadTask<INPUT, OUTPUT>
{
private:
    virtual SubTask *done() { return NULL; }

public:
    using _ThreadTask<INPUT, OUTPUT>::_ThreadTask;
};

template<class INPUT, class OUTPUT>
MultiThreadTask<INPUT, OUTPUT>*
ThreadTaskFactory<INPUT, OUTPUT>::createMultiThreadTask(const std::string& queue_name,
                                                             std::function<void (INPUT *, OUTPUT *)> routine, size_t nthreads,
                                                             std::function<void (MultiThreadTask<INPUT, OUTPUT> *)> callback)
{
    ThreadTask<INPUT, OUTPUT> **tasks = new ThreadTask<INPUT, OUTPUT> *[nthreads];
    char buf[32];
    size_t i;

    for (i = 0; i < nthreads; i++) {
        sprintf(buf, "-%zu@MTT", i);
        tasks[i] = new _ThreadTask__<INPUT, OUTPUT>
                (Global::getExecQueue(queue_name + buf),
                 Global::getComputeExecutor(),
                 std::function<void (INPUT *, OUTPUT *)>(routine),
                 nullptr);
    }

    auto *mt = new MultiThreadTask<INPUT, OUTPUT>(tasks, nthreads, std::move(callback));
    delete []tasks;
    return mt;
}
