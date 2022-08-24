//
// Created by dingjing on 8/17/22.
//
#include "global.h"

#include <mutex>
#include <string>
#include <atomic>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <unordered_map>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/conf.h>
#include <openssl/engine.h>
#include <openssl/crypto.h>
#include <condition_variable>

#include "../core/executor.h"
#include "../core/common-scheduler.h"

#include "../factory/task-error.h"
#include "../factory/resource-pool.h"

#include "../client/dns-client.h"

class __Global
{
public:
    static __Global *get_instance()
    {
        static __Global kInstance;
        return &kInstance;
    }

    const char *get_default_port(const std::string& scheme)
    {
        const auto it = static_scheme_port_.find(scheme);

        if (it != static_scheme_port_.end())
            return it->second;

        const char *port = NULL;
        user_scheme_port_mutex_.lock();
        const auto it2 = user_scheme_port_.find(scheme);

        if (it2 != user_scheme_port_.end())
            port = it2->second.c_str();

        user_scheme_port_mutex_.unlock();
        return port;
    }

    void register_scheme_port(const std::string& scheme, unsigned short port)
    {
        user_scheme_port_mutex_.lock();
        user_scheme_port_[scheme] = std::to_string(port);
        user_scheme_port_mutex_.unlock();
    }

    void sync_operation_begin()
    {
        bool inc;

        sync_mutex_.lock();
        inc = ++sync_count_ > sync_max_;

        if (inc)
            sync_max_ = sync_count_;
        sync_mutex_.unlock();
        if (inc)
            Global::getScheduler()->increaseHandlerThread();
    }

    void sync_operation_end()
    {
        sync_mutex_.lock();
        sync_count_--;
        sync_mutex_.unlock();
    }

private:
    __Global();

private:
    std::unordered_map<std::string, const char *> static_scheme_port_;
    std::unordered_map<std::string, std::string> user_scheme_port_;
    std::mutex user_scheme_port_mutex_;
    std::mutex sync_mutex_;
    int sync_count_;
    int sync_max_;
};

__Global::__Global()
{
    static_scheme_port_["dns"] = "53";
    static_scheme_port_["Dns"] = "53";
    static_scheme_port_["DNS"] = "53";

    static_scheme_port_["dnss"] = "853";
    static_scheme_port_["Dnss"] = "853";
    static_scheme_port_["DNSs"] = "853";
    static_scheme_port_["DNSS"] = "853";

    static_scheme_port_["http"] = "80";
    static_scheme_port_["Http"] = "80";
    static_scheme_port_["HTTP"] = "80";

    static_scheme_port_["https"] = "443";
    static_scheme_port_["Https"] = "443";
    static_scheme_port_["HTTPs"] = "443";
    static_scheme_port_["HTTPS"] = "443";

    static_scheme_port_["redis"] = "6379";
    static_scheme_port_["Redis"] = "6379";
    static_scheme_port_["REDIS"] = "6379";

    static_scheme_port_["rediss"] = "6379";
    static_scheme_port_["Rediss"] = "6379";
    static_scheme_port_["REDISs"] = "6379";
    static_scheme_port_["REDISS"] = "6379";

    static_scheme_port_["mysql"] = "3306";
    static_scheme_port_["Mysql"] = "3306";
    static_scheme_port_["MySql"] = "3306";
    static_scheme_port_["MySQL"] = "3306";
    static_scheme_port_["MYSQL"] = "3306";

    static_scheme_port_["mysqls"] = "3306";
    static_scheme_port_["Mysqls"] = "3306";
    static_scheme_port_["MySqls"] = "3306";
    static_scheme_port_["MySQLs"] = "3306";
    static_scheme_port_["MYSQLs"] = "3306";
    static_scheme_port_["MYSQLS"] = "3306";

    static_scheme_port_["kafka"] = "9092";
    static_scheme_port_["Kafka"] = "9092";
    static_scheme_port_["KAFKA"] = "9092";

    sync_count_ = 0;
    sync_max_ = 0;
}

#if OPENSSL_VERSION_NUMBER < 0x10100000L
static std::mutex *__ssl_mutex;

static void ssl_locking_callback(int mode, int type, const char* file, int line)
{
	if (mode & CRYPTO_LOCK)
		__ssl_mutex[type].lock();
	else if (mode & CRYPTO_UNLOCK)
		__ssl_mutex[type].unlock();
}
#endif

class __SSLManager
{
public:
    static __SSLManager *get_instance()
    {
        static __SSLManager kInstance;
        return &kInstance;
    }

    SSL_CTX *get_ssl_client_ctx() { return ssl_client_ctx_; }
    SSL_CTX *new_ssl_server_ctx() { return SSL_CTX_new(SSLv23_server_method()); }

private:
    __SSLManager()
    {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
        __ssl_mutex = new std::mutex[CRYPTO_num_locks()];
		CRYPTO_set_locking_callback(ssl_locking_callback);
		SSL_library_init();
		SSL_load_error_strings();
		//ERR_load_crypto_strings();
		//OpenSSL_add_all_algorithms();
#endif

        ssl_client_ctx_ = SSL_CTX_new(SSLv23_client_method());
        if (ssl_client_ctx_ == NULL)
            abort();
    }

    ~__SSLManager()
    {
        SSL_CTX_free(ssl_client_ctx_);

#if OPENSSL_VERSION_NUMBER < 0x10100000L
        //free ssl to avoid memory leak
		FIPS_mode_set(0);
		CRYPTO_set_locking_callback(NULL);
# ifdef CRYPTO_LOCK_ECDH
		CRYPTO_THREADID_set_callback(NULL);
# else
		CRYPTO_set_id_callback(NULL);
# endif
		ENGINE_cleanup();
		CONF_modules_unload(1);
		ERR_free_strings();
		EVP_cleanup();
# ifdef CRYPTO_LOCK_ECDH
		ERR_remove_thread_state(NULL);
# else
		ERR_remove_state(0);
# endif
		CRYPTO_cleanup_all_ex_data();
		sk_SSL_COMP_free(SSL_COMP_get_compression_methods());
		delete []__ssl_mutex;
#endif
    }

private:
    SSL_CTX *ssl_client_ctx_;
};

class __FileIOService : public IOService
{
public:
    __FileIOService(CommScheduler *scheduler):
            scheduler_(scheduler),
            flag_(true)
    {}

    int bind()
    {
        mutex_.lock();
        flag_ = false;

        int ret = scheduler_->ioBind(this);

        if (ret < 0)
            flag_ = true;

        mutex_.unlock();
        return ret;
    }

    void deInit()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (!flag_)
            cond_.wait(lock);

        lock.unlock();
        IOService::deInit();
    }

private:
    virtual void handleUnbound()
    {
        mutex_.lock();
        flag_ = true;
        cond_.notify_one();
        mutex_.unlock();
    }

    virtual void handleStop(int error)
    {
        scheduler_->ioUnbind(this);
    }

    CommScheduler *scheduler_;
    std::mutex mutex_;
    std::condition_variable cond_;
    bool flag_;
};

class __ThreadDnsManager
{
public:
    static __ThreadDnsManager *get_instance()
    {
        static __ThreadDnsManager kInstance;
        return &kInstance;
    }

    ExecQueue *get_dns_queue() { return &dns_queue_; }
    Executor *get_dns_executor() { return &dns_executor_; }

    __ThreadDnsManager()
    {
        int ret;

        ret = dns_queue_.init();
        if (ret < 0)
            abort();

        ret = dns_executor_.init(Global::getGlobalSettings()->dns_threads);
        if (ret < 0)
            abort();
    }

    ~__ThreadDnsManager()
    {
        dns_executor_.deInit();
        dns_queue_.deInit();
    }

private:
    ExecQueue dns_queue_;
    Executor dns_executor_;
};

class __CommManager
{
public:
    static __CommManager *get_instance()
    {
        static __CommManager kInstance;
        return &kInstance;
    }

    CommScheduler *get_scheduler() { return &scheduler_; }
    IOService *get_io_service()
    {
        if (!fio_flag_)
        {
            fio_mutex_.lock();
            if (!fio_flag_)
            {
                fio_service_ = new __FileIOService(&scheduler_);
                //todo EAGAIN 65536->2
                if (fio_service_->init(8192) < 0)
                    abort();

                if (fio_service_->bind() < 0)
                    abort();

                fio_flag_ = true;
            }

            fio_mutex_.unlock();
        }

        return fio_service_;
    }

private:
    __CommManager():
            fio_service_(NULL),
            fio_flag_(false)
    {
        const auto *settings = Global::getGlobalSettings();
        if (scheduler_.init(settings->poller_threads,
                            settings->handler_threads) < 0)
            abort();

        signal(SIGPIPE, SIG_IGN);
    }

    ~__CommManager()
    {
        // scheduler_.deinit() will triger fio_service to stop
        scheduler_.deInit();
        if (fio_service_)
        {
            fio_service_->deInit();
            delete fio_service_;
        }
    }

private:
    CommScheduler scheduler_;
    __FileIOService *fio_service_;
    volatile bool fio_flag_;
    std::mutex fio_mutex_;
};

class __ExecManager
{
protected:
    using ExecQueueMap = std::unordered_map<std::string, ExecQueue *>;

public:
    static __ExecManager *get_instance()
    {
        static __ExecManager kInstance;
        return &kInstance;
    }

    ExecQueue *get_exec_queue(const std::string& queue_name)
    {
        ExecQueue *queue = NULL;
        ExecQueueMap::const_iterator iter;

        pthread_rwlock_rdlock(&rwlock_);
        iter = queue_map_.find(queue_name);
        if (iter != queue_map_.cend())
            queue = iter->second;

        pthread_rwlock_unlock(&rwlock_);
        if (queue)
            return queue;

        pthread_rwlock_wrlock(&rwlock_);
        iter = queue_map_.find(queue_name);
        if (iter == queue_map_.cend())
        {
            queue = new ExecQueue();
            if (queue->init() >= 0)
                queue_map_.emplace(queue_name, queue);
            else
            {
                delete queue;
                queue = NULL;
            }
        }
        else
            queue = iter->second;

        pthread_rwlock_unlock(&rwlock_);
        return queue;
    }

    Executor *get_compute_executor() { return &compute_executor_; }

private:
    __ExecManager():
            rwlock_(PTHREAD_RWLOCK_INITIALIZER)
    {
        int compute_threads = Global::getGlobalSettings()->compute_threads;

        if (compute_threads <= 0)
            compute_threads = sysconf(_SC_NPROCESSORS_ONLN);

        if (compute_executor_.init(compute_threads) < 0)
            abort();
    }

    ~__ExecManager()
    {
        compute_executor_.deInit();

        for (auto& kv : queue_map_)
        {
            kv.second->deInit();
            delete kv.second;
        }
    }

private:
    pthread_rwlock_t rwlock_;
    ExecQueueMap queue_map_;
    Executor compute_executor_;
};

#define MAX(x, y)	((x) >= (y) ? (x) : (y))
#define HOSTS_LINEBUF_INIT_SIZE	128

static void __split_merge_str(const char *p, bool is_nameserver,
                              std::string& result)
{
    const char *start;

    if (!isspace(*p))
        return;

    while (1)
    {
        while (isspace(*p))
            p++;

        start = p;
        while (*p && *p != '#' && *p != ';' && !isspace(*p))
            p++;

        if (start == p)
            break;

        if (!result.empty())
            result.push_back(',');

        std::string str(start, p);
        if (is_nameserver)
        {
            struct in6_addr buf;
            if (inet_pton(AF_INET6, str.c_str(), &buf) > 0)
                str = "[" + str + "]";
        }

        result.append(str);
    }
}

static inline const char *__try_options(const char *p, const char *q,
                                        const char *r)
{
    size_t len = strlen(r);
    if ((size_t)(q - p) >= len && strncmp(p, r, len) == 0)
        return p + len;
    return NULL;
}

static void __set_options(const char *p,
                          int *ndots, int *attempts, bool *rotate)
{
    const char *start;
    const char *opt;

    if (!isspace(*p))
        return;

    while (1)
    {
        while (isspace(*p))
            p++;

        start = p;
        while (*p && *p != '#' && *p != ';' && !isspace(*p))
            p++;

        if (start == p)
            break;

        if ((opt = __try_options(start, p, "ndots:")) != NULL)
            *ndots = atoi(opt);
        else if ((opt = __try_options(start, p, "attempts:")) != NULL)
            *attempts = atoi(opt);
        else if ((opt = __try_options(start, p, "rotate")) != NULL)
            *rotate = true;
    }
}

static int __parse_resolv_conf(const char *path,
                               std::string& url, std::string& search_list,
                               int *ndots, int *attempts, bool *rotate)
{
    size_t bufsize = 0;
    char *line = NULL;
    FILE *fp;
    int ret;

    fp = fopen(path, "r");
    if (!fp)
        return -1;

    while ((ret = getline(&line, &bufsize, fp)) > 0)
    {
        if (strncmp(line, "nameserver", 10) == 0)
            __split_merge_str(line + 10, true, url);
        else if (strncmp(line, "search", 6) == 0)
            __split_merge_str(line + 6, false, search_list);
        else if (strncmp(line, "options", 7) == 0)
            __set_options(line + 7, ndots, attempts, rotate);
    }

    ret = ferror(fp) ? -1 : 0;
    free(line);
    fclose(fp);
    return ret;
}

class __DnsClientManager
{
public:
    static __DnsClientManager *get_instance()
    {
        static __DnsClientManager kInstance;
        return &kInstance;
    }

public:
    DnsClient *get_dns_client() { return client_; }
    ResourcePool *get_dns_respool() { return &respool_; };

private:
    __DnsClientManager() : respool_(Global::getGlobalSettings()->
            dnsServerParams.maxConnections)
    {
        const char *path = Global::getGlobalSettings()->resolv_conf_path;

        client_ = NULL;
        if (path && path[0])
        {
            int ndots = 1;
            int attempts = 2;
            bool rotate = false;
            std::string url;
            std::string search;

            __parse_resolv_conf(path, url, search, &ndots, &attempts, &rotate);
            if (url.size() == 0)
                url = "8.8.8.8";

            client_ = new DnsClient;
            if (client_->init(url, search, ndots, attempts, rotate) >= 0)
                return;

            delete client_;
            client_ = NULL;
        }
    }

    ~__DnsClientManager()
    {
        if (client_)
        {
            client_->deInit();
            delete client_;
        }
    }

    DnsClient *client_;
    ResourcePool respool_;
};

GlobalSettings Global::gSettings = GLOBAL_SETTINGS_DEFAULT;
RouteManager Global::gRouteManager;
DnsCache Global::gDnsCache;
DnsResolver Global::gDnsResolver;
NameService Global::gNameService(&Global::gDnsResolver);

CommScheduler* Global::getScheduler()
{
    logv("");
    return __CommManager::get_instance()->get_scheduler();
}

SSL_CTX *Global::getSslClientCtx()
{
    return __SSLManager::get_instance()->get_ssl_client_ctx();
}

SSL_CTX *Global::newSslServerCtx()
{
    return __SSLManager::get_instance()->new_ssl_server_ctx();
}

ExecQueue *Global::getExecQueue(const std::string& queue_name)
{
    return __ExecManager::get_instance()->get_exec_queue(queue_name);
}

Executor *Global::getComputeExecutor()
{
    return __ExecManager::get_instance()->get_compute_executor();
}

IOService *Global::getIoService()
{
    return __CommManager::get_instance()->get_io_service();
}

ExecQueue *Global::getDnsQueue()
{
    return __ThreadDnsManager::get_instance()->get_dns_queue();
}

Executor *Global::getDnsExecutor()
{
    return __ThreadDnsManager::get_instance()->get_dns_executor();
}

DnsClient *Global::getDnsClient()
{
    return __DnsClientManager::get_instance()->get_dns_client();
}

ResourcePool *Global::getDnsRespool()
{
    return __DnsClientManager::get_instance()->get_dns_respool();
}

const char *Global::getDefaultPort(const std::string& scheme)
{
    return __Global::get_instance()->get_default_port(scheme);
}

void Global::registerSchemePort(const std::string& scheme, unsigned short port)
{
    __Global::get_instance()->register_scheme_port(scheme, port);
}

void Global::syncOperationBegin()
{
    __Global::get_instance()->sync_operation_begin();
}

void Global::syncOperationEnd()
{
    __Global::get_instance()->sync_operation_end();
}

static inline const char *__get_ssl_error_string(int error)
{
    switch (error)
    {
        case SSL_ERROR_NONE:
            return "SSL Error None";

        case SSL_ERROR_ZERO_RETURN:
            return "SSL Error Zero Return";

        case SSL_ERROR_WANT_READ:
            return "SSL Error Want Read";

        case SSL_ERROR_WANT_WRITE:
            return "SSL Error Want Write";

        case SSL_ERROR_WANT_CONNECT:
            return "SSL Error Want Connect";

        case SSL_ERROR_WANT_ACCEPT:
            return "SSL Error Want Accept";

        case SSL_ERROR_WANT_X509_LOOKUP:
            return "SSL Error Want X509 Lookup";

#ifdef SSL_ERROR_WANT_ASYNC
        case SSL_ERROR_WANT_ASYNC:
            return "SSL Error Want Async";
#endif

#ifdef SSL_ERROR_WANT_ASYNC_JOB
        case SSL_ERROR_WANT_ASYNC_JOB:
            return "SSL Error Want Async Job";
#endif

#ifdef SSL_ERROR_WANT_CLIENT_HELLO_CB
        case SSL_ERROR_WANT_CLIENT_HELLO_CB:
            return "SSL Error Want Client Hello CB";
#endif

        case SSL_ERROR_SYSCALL:
            return "SSL System Error";

        case SSL_ERROR_SSL:
            return "SSL Error SSL";

        default:
            break;
    }

    return "Unknown";
}

static inline const char *__get_task_error_string(int error)
{
    switch (error) {
        case TASK_ERROR_URI_PARSE_FAILED:
            return "URI Parse Failed";

        case TASK_ERROR_URI_SCHEME_INVALID:
            return "URI Scheme Invalid";

        case TASK_ERROR_URI_PORT_INVALID:
            return "URI Port Invalid";

        case TASK_ERROR_UPSTREAM_UNAVAILABLE:
            return "Upstream Unavailable";

        case TASK_ERROR_HTTP_BAD_REDIRECT_HEADER:
            return "Http Bad Redirect Header";

        case TASK_ERROR_HTTP_PROXY_CONNECT_FAILED:
            return "Http Proxy Connect Failed";

        case TASK_ERROR_REDIS_ACCESS_DENIED:
            return "Redis Access Denied";

        case TASK_ERROR_REDIS_COMMAND_DISALLOWED:
            return "Redis Command Disallowed";

        case TASK_ERROR_MYSQL_HOST_NOT_ALLOWED:
            return "MySQL Host Not Allowed";

        case TASK_ERROR_MYSQL_ACCESS_DENIED:
            return "MySQL Access Denied";

        case TASK_ERROR_MYSQL_INVALID_CHARACTER_SET:
            return "MySQL Invalid Character Set";

        case TASK_ERROR_MYSQL_COMMAND_DISALLOWED:
            return "MySQL Command Disallowed";

        case TASK_ERROR_MYSQL_QUERY_NOT_SET:
            return "MySQL Query Not Set";

        case TASK_ERROR_MYSQL_SSL_NOT_SUPPORTED:
            return "MySQL SSL Not Supported";

        case TASK_ERROR_KAFKA_PARSE_RESPONSE_FAILED:
            return "Kafka parse response failed";

        case TASK_ERROR_KAFKA_PRODUCE_FAILED:
            return "Kafka produce api failed";

        case TASK_ERROR_KAFKA_FETCH_FAILED:
            return "Kafka fetch api failed";

        case TASK_ERROR_KAFKA_CGROUP_FAILED:
            return "Kafka cgroup failed";

        case TASK_ERROR_KAFKA_COMMIT_FAILED:
            return "Kafka commit api failed";

        case TASK_ERROR_KAFKA_META_FAILED:
            return "Kafka meta api failed";

        case TASK_ERROR_KAFKA_LEAVEGROUP_FAILED:
            return "Kafka leavegroup failed";

        case TASK_ERROR_KAFKA_API_UNKNOWN:
            return "Kafka api type unknown";

        case TASK_ERROR_KAFKA_VERSION_DISALLOWED:
            return "Kafka broker version not supported";

        case TASK_ERROR_CONSUL_API_UNKNOWN:
            return "Consul api type unknown";

        case TASK_ERROR_CONSUL_CHECK_RESPONSE_FAILED:
            return "Consul check response failed";

        default:
            break;
    }

    return "Unknown";
}

const char *Global::getErrorString(int state, int error)
{
    switch (state) {
        case TASK_STATE_SUCCESS:
            return "Success";

        case TASK_STATE_TOREPLY:
            return "To Reply";

        case TASK_STATE_NOREPLY:
            return "No Reply";

        case TASK_STATE_SYS_ERROR:
            return strerror(error);

        case TASK_STATE_SSL_ERROR:
            return __get_ssl_error_string(error);

        case TASK_STATE_DNS_ERROR:
            return gai_strerror(error);

        case TASK_STATE_TASK_ERROR:
            return __get_task_error_string(error);

        case TASK_STATE_UNDEFINED:
            return "Undefined";

        default:
            break;
    }

    return "Unknown";
}

void jarvis_library_init(const GlobalSettings *settings)
{
    Global::setGlobalSettings(settings);
}
