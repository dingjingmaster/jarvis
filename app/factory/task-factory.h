//
// Created by dingjing on 8/17/22.
//

#ifndef JARVIS_TASK_FACTORY_H
#define JARVIS_TASK_FACTORY_H
#include <time.h>
#include <utility>
#include <sys/uio.h>
#include <functional>
#include <sys/types.h>

#include "task.h"
#include "workflow.h"
#include "../utils/uri-parser.h"

#include "../protocol/dns/dns-message.h"
#include "../protocol/http/http-message.h"

#include "../manager/endpoint-params.h"
#include "algorithm-task-factory.h"

//#include "RedisMessage.h"
//#include "MySQLMessage.h"
//#include "GraphTask.h"


using HttpTask = NetworkTask<protocol::HttpRequest, protocol::HttpResponse>;
using HttpCallback = std::function<void (HttpTask*)>;

typedef struct _FileIOArgs              FileIOArgs;
typedef struct _FileVIOArgs             FileVIOArgs;
typedef struct _FileSyncArgs            FileSyncArgs;

// File IO tasks

struct _FileIOArgs
{
    int                     fd;
    void*                   buf;
    size_t                  count;
    off_t                   offset;
};

struct _FileVIOArgs
{
    int                     fd;
    const struct iovec*     iov;
    int                     iovCnt;
    off_t                   offset;
};

struct _FileSyncArgs
{
    int                     fd;
};

using FileIOTask = FileTask<FileIOArgs>;
using FileIOCallback = std::function<void (FileIOTask*)>;

using FileVIOTask = FileTask<FileVIOArgs>;
using FileVIOCallback = std::function<void (FileVIOTask*)>;

using FileSyncTask = FileTask<FileSyncArgs>;
using FileSyncCallback = std::function<void (FileSyncTask*)>;

// Timer and counter
using TimerCallback = std::function<void (TimerTask*)>;
using CounterCallback = std::function<void (CounterTask*)>;

// Mailbox is like counter with data passing
using MailboxCallback = std::function<void (MailboxTask*)>;

// Graph (DAG) task.
//using graph_callback_t = std::function<void (WFGraphTask *)>;

using EmptyTask = GenericTask;

using DynamicTask = GenericTask;
using DynamicCreate = std::function<SubTask *(DynamicTask*)>;

using RepeatedCreate = std::function<SubTask *(RepeaterTask*)>;
using RepeaterCallback = std::function<void (RepeaterTask*)>;

using ModuleCallback = std::function<void (const ModuleTask*)>;

// DNS
using DnsTask = NetworkTask<protocol::DnsRequest, protocol::DnsResponse>;
using DnsCallback = std::function<void (DnsTask*)>;


class TaskFactory
{
public:
    static HttpTask* createHttpTask(const ParsedURI& uri, int redirectMax, int retryMax, HttpCallback callback);
    static HttpTask* createHttpTask(const std::string& url, int redirectMax, int retryMax, HttpCallback callback);
    static HttpTask* createHttpTask(const ParsedURI& uri, const ParsedURI& proxyUri, int redirectMax, int retryMax, HttpCallback callback);
    static HttpTask* createHttpTask(const std::string& url, const std::string& proxyUrl, int redirectMax, int retryMax, HttpCallback callback);

    static FileIOTask* createPreadTask(int fd, void *buf, size_t count, off_t offset, FileIOCallback callback);
    static FileIOTask* createPwriteTask(int fd, const void *buf, size_t count, off_t offset, FileIOCallback callback);

    /* preadv and pwritev tasks are supported by Linux aio only.
     * On macOS or others, you will get an ENOSYS error in callback. */

    static FileVIOTask* createPreadvTask(int fd, const struct iovec *iov, int iovCnt, off_t offset, FileVIOCallback callback);
    static FileVIOTask* createPwritevTask(int fd, const struct iovec *iov, int iovCnt, off_t offset, FileVIOCallback callback);

    static FileSyncTask* createFsyncTask(int fd, FileSyncCallback callback);

    /* On systems that do not support fdatasync(), like macOS,
     * fdsync task is equal to fsync task. */
    static FileSyncTask* createFdSyncTask(int fd, FileSyncCallback callback);

    /* File tasks with path name. */
    static FileIOTask* createPreadTask(const std::string& pathname, void *buf, size_t count, off_t offset, FileIOCallback callback);
    static FileIOTask* createPwriteTask(const std::string& pathname, const void *buf, size_t count, off_t offset, FileIOCallback callback);

    static FileVIOTask* createPreadvTask(const std::string& pathname, const struct iovec *iov, int iovCnt, off_t offset, FileVIOCallback callback);
    static FileVIOTask* createPwritevTask(const std::string& pathname, const struct iovec *iov, int iovCnt, off_t offset, FileVIOCallback callback);

    static TimerTask* createTimerTask(unsigned int microseconds, TimerCallback callback);

    static TimerTask* createTimerTask(time_t seconds, long nanoseconds, TimerCallback callback);

    /* DNS - start */
    static DnsTask *createDnsTask(const std::string& url, int retryMax, DnsCallback callback);
    static DnsTask *createDnsTask(const ParsedURI& uri, int retryMax, DnsCallback callback);
    /* DNS - end */


    /* Counter is like semaphore. The callback of counter is called when
     * 'count' operations reach target_value & after the task is started.
     * It's perfectly legal to call 'count' before the task is started. */

    /* Create an unnamed counter. Call counter->count() directly.
     * NOTE: never call count() exceeding target_value. */
    static CounterTask* createCounterTask(unsigned int targetValue, CounterCallback callback)
    {
        return new CounterTask(targetValue, std::move(callback));
    }

    /* Create a named counter. */
    static CounterTask* createCounterTask(const std::string& counterName, unsigned int targetValue, CounterCallback callback);

    /* Count by a counter's name. When count_by_name(), it's safe to count
     * exceeding target_value. When multiple counters share a same name,
     * this operation will be performed on the first created. If no counter
     * matches the name, nothing is performed. */
    static void countByName(const std::string& counterName)
    {
        TaskFactory::countByName(counterName, 1);
    }

    /* Count by name with a value n. When multiple counters share this name,
     * the operation is performed on the counters in the sequence of its
     * creation, and more than one counter may reach target value. */
    static void countByName(const std::string& counterName, unsigned int n);

    static MailboxTask* createMailboxTask(size_t size, MailboxCallback callback);

    /* Use 'user_data' as mailbox. Store only one message. */
    static MailboxTask* createMailboxTask(MailboxCallback callback);

    static Conditional* createConditional(SubTask *task, void **msgBuf)
    {
        return new Conditional(task, msgBuf);
    }

    static Conditional* createConditional(SubTask *task)
    {
        return new Conditional(task);
    }

    static Conditional* createConditional(const std::string& condName, SubTask *task);
    static Conditional* createConditional(const std::string& condName, SubTask *task, void **msgBuf);

    static void signalByName(const std::string& condName, void *msg);

    template<class FUNC, class... ARGS>
    static GoTask *createGoTask(const std::string& queueName, FUNC&& func, ARGS&&... args);

    /* Create 'Go' task with running time limit in seconds plus nanoseconds.
     * If time exceeded, state WFT_STATE_ABORTED will be got in callback. */
    template<class FUNC, class... ARGS>
    static GoTask *createTimedGoTask(time_t seconds, long nanoseconds, const std::string& queueName, FUNC&& func, ARGS&&... args);

    /* Create 'Go' task on user's executor and execution queue. */
    template<class FUNC, class... ARGS>
    static GoTask *createGoTask(ExecQueue *queue, Executor *executor, FUNC&& func, ARGS&&... args);

    template<class FUNC, class... ARGS>
    static GoTask *createTimedGoTask(time_t seconds, long nanoseconds, ExecQueue *queue, Executor *executor, FUNC&& func, ARGS&&... args);

#if 0
    static GraphTask* createGraphTask(graph_callback_t callback)
    {
        return new WFGraphTask(std::move(callback));
    }
#endif

    static EmptyTask* createEmptyTask()
    {
        return new EmptyTask;
    }

    static DynamicTask* createDynamicTask(DynamicCreate create);

    static RepeaterTask* createRepeaterTask(RepeatedCreate create, RepeaterCallback callback)
    {
        return new RepeaterTask(std::move(create), std::move(callback));
    }

public:
    static ModuleTask* createModuleTask(SubTask *first, ModuleCallback callback)
    {
        return new ModuleTask(first, std::move(callback));
    }

    static ModuleTask* createModuleTask(SubTask *first, SubTask *last, ModuleCallback callback)
    {
        ModuleTask *task = new ModuleTask(first, std::move(callback));
        task->subSeries()->setLastTask(last);
        return task;
    }
};

template<class REQ, class RESP>
class NetworkTaskFactory
{
private:
    using T = NetworkTask<REQ, RESP>;

public:
    static T *createClientTask(TransportType type, const ParsedURI& uri, int retry_max, std::function<void (T *)> callback);
    static T *createClientTask(TransportType type, const std::string& url, int retry_max, std::function<void (T *)> callback);
    static T *createClientTask(TransportType type, const std::string& host, unsigned short port, int retryMax, std::function<void (T *)> callback);

    static T *createServerTask(CommService *service, std::function<void (T *)>& process);
};

template<class INPUT, class OUTPUT>
class ThreadTaskFactory
{
private:
    using T = ThreadTask<INPUT, OUTPUT>;
    using MT = MultiThreadTask<INPUT, OUTPUT>;

public:
    static T *createThreadTask(const std::string& queueName, std::function<void (INPUT *, OUTPUT *)> routine, std::function<void (T *)> callback);
    static MT*createMultiThreadTask(const std::string& queueName, std::function<void (INPUT *, OUTPUT *)> routine, size_t nthreads, std::function<void (MT *)> callback);
    static T *createThreadTask(ExecQueue *queue, Executor *executor, std::function<void (INPUT *, OUTPUT *)> routine, std::function<void (T *)> callback);
};

#include "task-factory.inl"

#endif //JARVIS_TASK_FACTORY_H
