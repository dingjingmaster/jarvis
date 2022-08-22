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
#include "NameService.h"

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
        type_ = TT_TCP;
        fixed_addr_ = false;
        retry_max_ = retry_max;
        retry_times_ = 0;
        redirect_ = false;
        ns_policy_ = NULL;
        router_task_ = NULL;
    }

protected:
    // new api for children
    virtual bool init_success() { return true; }
    virtual void init_failed() {}
    virtual bool check_request() { return true; }
    virtual RouterTask *route();
    virtual bool finish_once() { return true; }

public:
    void init(const ParsedURI& uri)
    {
        uri_ = uri;
        init_with_uri();
    }

    void init(ParsedURI&& uri)
    {
        uri_ = std::move(uri);
        init_with_uri();
    }

    void init(TransportType type,
              const struct sockaddr *addr,
              socklen_t addrlen,
              const std::string& info);

    void set_transport_type(TransportType type)
    {
        type_ = type;
    }

    TransportType get_transport_type() const { return type_; }

    virtual const ParsedURI *get_current_uri() const { return &uri_; }

    void set_redirect(const ParsedURI& uri)
    {
        redirect_ = true;
        init(uri);
    }

    void set_redirect(TransportType type, const struct sockaddr *addr,
                      socklen_t addrlen, const std::string& info)
    {
        redirect_ = true;
        init(type, addr, addrlen, info);
    }

    bool is_fixed_addr() const { return this->fixed_addr_; }

protected:
    void set_fixed_addr(int fixed) { this->fixed_addr_ = fixed; }

    void set_info(const std::string& info)
    {
        info_.assign(info);
    }

    void set_info(const char *info)
    {
        info_.assign(info);
    }

protected:
    virtual void dispatch();
    virtual SubTask *done();

    void clear_resp()
    {
        size_t size = this->resp.get_size_limit();

        this->resp.~RESP();
        new(&this->resp) RESP();
        this->resp.set_size_limit(size);
    }

    void disable_retry()
    {
        retry_times_ = retry_max_;
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
    void clear_prev_state();
    void init_with_uri();
    bool set_port();
    void router_callback(void *t);
    void switch_callback(void *t);
};

template<class REQ, class RESP, typename CTX>
void ComplexClientTask<REQ, RESP, CTX>::clear_prev_state()
{
    ns_policy_ = NULL;
    route_result_.clear();
    if (tracing_.deleter)
    {
        tracing_.deleter(tracing_.data);
        tracing_.deleter = NULL;
    }
    tracing_.data = NULL;
    retry_times_ = 0;
    this->state = TASK_STATE_UNDEFINED;
    this->error = 0;
    this->timeout_reason = TOR_NOT_TIMEOUT;
}

template<class REQ, class RESP, typename CTX>
void ComplexClientTask<REQ, RESP, CTX>::init(TransportType type, const struct sockaddr *addr, socklen_t addrLen, const std::string& info)
{
    if (redirect_)
        clear_prev_state();

    auto params = Global::getGlobalSettings()->endpointParams;
    struct addrinfo addrInfo = { };
    addrInfo.ai_family = addr->sa_family;
    addrInfo.ai_socktype = SOCK_STREAM;
    addrInfo.ai_addr = (struct sockaddr *)addr;
    addrInfo.ai_addrlen = addrLen;

    type_ = type;
    info_.assign(info);
    params.use_tls_sni = false;
    if (Global::getRouteManager()->get(type, &addrinfo, info_, &params, "", route_result_) < 0) {
        this->state = TASK_STATE_SYS_ERROR;
        this->error = errno;
    } else if (this->init_success()) {
        return;
    }

    this->init_failed();
}

template<class REQ, class RESP, typename CTX>
bool ComplexClientTask<REQ, RESP, CTX>::set_port()
{
    if (uri_.port) {
        int port = atoi(uri_.port);

        if (port <= 0 || port > 65535) {
            this->state = TASK_STATE_TASK_ERROR;
            this->error = TASK_ERR_URI_PORT_INVALID;
            return false;
        }

        return true;
    }

    if (uri_.scheme) {
        const char *port_str = Global::get_default_port(uri_.scheme);

        if (port_str) {
            uri_.port = strdup(port_str);
            if (uri_.port)
                return true;

            this->state = TASK_STATE_SYS_ERROR;
            this->error = errno;
            return false;
        }
    }

    this->state = TASK_STATE_TASK_ERROR;
    this->error = TASK_ERR_URI_SCHEME_INVALID;
    return false;
}

template<class REQ, class RESP, typename CTX>
void ComplexClientTask<REQ, RESP, CTX>::init_with_uri()
{
    if (redirect_) {
        clear_prev_state();
        ns_policy_ = Global::get_dns_resolver();
    }

    if (uri_.state == URI_STATE_SUCCESS) {
        if (this->set_port()) {
            if (this->init_success()) {
                return;
            }
        }
    } else if (uri_.state == URI_STATE_ERROR) {
        this->state = TASK_STATE_SYS_ERROR;
        this->error = uri_.error;
    } else {
        this->state = TASK_STATE_TASK_ERROR;
        this->error = TASK_ERR_URI_PARSE_FAILED;
    }

    this->init_failed();
}

template<class REQ, class RESP, typename CTX>
RouterTask *ComplexClientTask<REQ, RESP, CTX>::route()
{
    auto&& cb = std::bind(&ComplexClientTask::router_callback, this, std::placeholders::_1);
    struct WFNSParams params = {
            .type			=	type_,
            .uri			=	uri_,
            .info			=	info_.c_str(),
            .fixed_addr		=	fixed_addr_,
            .retry_times	=	retry_times_,
            .tracing		=	&tracing_,
    };

    if (!ns_policy_) {
        NameService *ns = Global::getNameService();
        ns_policy_ = ns->get_policy(uri_.host ? uri_.host : "");
    }

    return ns_policy_->create_router_task(&params, std::move(cb));
}

template<class REQ, class RESP, typename CTX>
void ComplexClientTask<REQ, RESP, CTX>::router_callback(void *t)
{
    RouterTask *task = (RouterTask *)t;

    this->state = task->get_state();
    if (this->state == TASK_STATE_SUCCESS)
        route_result_ = std::move(*task->get_result());
    else if (this->state == TASK_STATE_UNDEFINED) {
        /* should not happend */
        this->state = TASK_STATE_SYS_ERROR;
        this->error = ENOSYS;
    } else {
        this->error = task->get_error();
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
                        this->set_request_object(route_result_.request_object);
                    this->ClientTask<REQ, RESP>::dispatch();
                    return;
                }

                router_task_ = this->route();
                seriesOf(this)->pushFront(this);
                seriesOf(this)->pushFront(router_task_);
            }
        }
        default:
            break;
    }

    this->subtask_done();
}

template<class REQ, class RESP, typename CTX>
void ComplexClientTask<REQ, RESP, CTX>::switch_callback(void *t)
{
    if (!redirect_) {
        if (this->state == WFT_STATE_SYS_ERROR && this->error < 0) {
            this->state = WFT_STATE_SSL_ERROR;
            this->error = -this->error;
        }

        if (tracing_.deleter) {
            tracing_.deleter(tracing_.data);
            tracing_.deleter = NULL;
        }

        if (this->callback) {
            this->callback(this);
        }
    }

    if (redirect_) {
        redirect_ = false;
        clear_resp();
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

    if (router_task_) {
        router_task_ = NULL;
        return series->pop();
    }

    bool is_user_request = this->finish_once();

    if (ns_policy_ && route_result_.request_object) {
        if (this->state == WFT_STATE_SYS_ERROR)
            ns_policy_->failed(&route_result_, &tracing_, this->target);
        else
            ns_policy_->success(&route_result_, &tracing_, this->target);
    }

    if (this->state == TASK_STATE_SUCCESS) {
        if (!is_user_request)
            return this;
    } else if (this->state == TASK_STATE_SYS_ERROR) {
        if (retry_times_ < retry_max_)
        {
            redirect_ = true;
            if (ns_policy_)
                route_result_.clear();

            this->state = TASK_STATE_UNDEFINED;
            this->error = 0;
            this->timeout_reason = 0;
            retry_times_++;
        }
    }

    /*
     * When target is NULL, it's very likely that we are in the caller's
     * thread or DNS thread (dns failed). Running a timer will switch callback
     * function to a handler thread, and this can prevent stack overflow.
     */
    if (!this->target) {
        auto&& cb = std::bind(&ComplexClientTask::switch_callback, this, std::placeholders::_1);
        WFTimerTask *timer;

        timer = TaskFactory::create_timer_task(0, 0, std::move(cb));
        series->push_front(timer);
    } else {
        this->switch_callback(NULL);
    }

    return series->pop();
}

/**********Template Network Factory**********/

template<class REQ, class RESP>
NetworkTask<REQ, RESP> *
NetworkTaskFactory<REQ, RESP>::create_client_task(TransportType type, const std::string& host, unsigned short port, int retry_max, std::function<void (NetworkTask<REQ, RESP> *)> callback)
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
NetworkTaskFactory<REQ, RESP>::create_client_task(TransportType type, const std::string& url, int retry_max, std::function<void (NetworkTask<REQ, RESP> *)> callback)
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
NetworkTaskFactory<REQ, RESP>::create_client_task(TransportType type, const ParsedURI& uri, int retry_max, std::function<void (NetworkTask<REQ, RESP> *)> callback)
{
    auto *task = new ComplexClientTask<REQ, RESP>(retry_max, std::move(callback));

    task->init(uri);
    task->set_transport_type(type);
    return task;
}

template<class REQ, class RESP>
NetworkTask<REQ, RESP> *
NetworkTaskFactory<REQ, RESP>::create_server_task(CommService *service, std::function<void (NetworkTask<REQ, RESP> *)>& process)
{
    return new ServerTask<REQ, RESP>(service, WFGlobal::get_scheduler(), process);
}

/**********Server Factory**********/

class ServerTaskFactory
{
public:
    static HttpTask *create_http_task(CommService *service, std::function<void (WFHttpTask *)>& process);
    static MySQLTask *create_mysql_task(CommService *service, std::function<void (WFMySQLTask *)>& process);
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
ThreadTaskFactory<INPUT, OUTPUT>::create_thread_task(const std::string& queue_name,
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
ThreadTaskFactory<INPUT, OUTPUT>::create_thread_task(ExecQueue *queue, Executor *executor,
                                                       std::function<void (INPUT *, OUTPUT *)> routine,
                                                       std::function<void (ThreadTask<INPUT, OUTPUT> *)> callback)
{
    return new _ThreadTask<INPUT, OUTPUT>(queue, executor,
                                             std::move(routine),
                                             std::move(callback));
}

template<class INPUT, class OUTPUT>
class _ThreadTask__ : public __WFThreadTask<INPUT, OUTPUT>
{
private:
    virtual SubTask *done() { return NULL; }

public:
    using _ThreadTask<INPUT, OUTPUT>::_ThreadTask;
};

template<class INPUT, class OUTPUT>
MultiThreadTask<INPUT, OUTPUT> *
ThreadTaskFactory<INPUT, OUTPUT>::create_multi_thread_task(const std::string& queue_name,
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
