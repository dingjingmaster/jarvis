//
// Created by dingjing on 8/12/22.
//
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "msg-queue.h"
#include "thread-pool.h"

typedef struct _ThreadPoolTaskEntry ThreadPoolTaskEntry;

inline int thread_pool_in_pool(ThreadPool* pool);
inline void _thread_pool_schedule(const ThreadPoolTask* task, void* buf, ThreadPool* pool);

struct _ThreadPool
{
    MsgQueue*               msgQueue;
    size_t                  nThreads;
    size_t                  stackSize;
    pthread_t               tid;
    pthread_mutex_t         mutex;
    pthread_key_t           key;
    pthread_cond_t*         terminate;
};

struct _ThreadPoolTaskEntry
{
    void*                   link;
    ThreadPoolTask          task;
};

static pthread_t _zero_tid;

static void* _thread_pool_routine(void *arg)
{
    ThreadPool* pool = (ThreadPool*) arg;
    ThreadPoolTaskEntry* entry;
    void (*taskRoutine)(void*);
    void *taskContext;
    pthread_t tid;

    pthread_setspecific (pool->key, pool);
    while (!pool->terminate) {
        entry = (ThreadPoolTaskEntry*)msg_queue_get(pool->msgQueue);
        if (!entry) {
            break;
        }

        taskRoutine = entry->task.routine;
        taskContext = entry->task.context;
        free(entry);
        taskRoutine(taskContext);

        if (pool->nThreads == 0) {
            /* Thread pool was destroyed by the task. */
            free(pool);
            return NULL;
        }
    }

    /* One thread joins another. Don't need to keep all thread IDs. */
    pthread_mutex_lock(&pool->mutex);
    tid = pool->tid;
    pool->tid = pthread_self();
    if (--pool->nThreads == 0) {
        pthread_cond_signal(pool->terminate);
    }

    pthread_mutex_unlock(&pool->mutex);
    if (memcmp (&tid, &_zero_tid, sizeof (pthread_t)) != 0) {
        pthread_join(tid, NULL);
    }

    return NULL;
}

static void _thread_pool_terminate(int inPool, ThreadPool* pool)
{
    pthread_cond_t term = PTHREAD_COND_INITIALIZER;

    pthread_mutex_lock(&pool->mutex);
    msg_queue_set_nonblock(pool->msgQueue);
    pool->terminate = &term;

    if (inPool) {
        /* Thread pool destroyed in a pool thread is legal. */
        pthread_detach(pthread_self());
        --pool->nThreads;
    }

    while (pool->nThreads > 0) {
        pthread_cond_wait(&term, &pool->mutex);
    }

    pthread_mutex_unlock(&pool->mutex);
    if (memcmp(&pool->tid, &_zero_tid, sizeof (pthread_t)) != 0) {
        pthread_join(pool->tid, NULL);
    }
}

static int _thread_pool_create_threads(size_t nthreads, ThreadPool* pool)
{
    int                 ret;
    pthread_t           tid;
    pthread_attr_t      attr;

    ret = pthread_attr_init(&attr);
    if (ret == 0) {
        if (pool->stackSize) {
            pthread_attr_setstacksize(&attr, pool->stackSize);
        }

        while (pool->nThreads < nthreads) {
            ret = pthread_create(&tid, &attr, _thread_pool_routine, pool);
            if (ret == 0) {
                pool->nThreads++;
            } else {
                break;
            }
        }

        pthread_attr_destroy(&attr);
        if (pool->nThreads == nthreads) {
            return 0;
        }

        _thread_pool_terminate(0, pool);
    }

    errno = ret;
    return -1;
}

ThreadPool* thread_pool_create(size_t nthreads, size_t stackSize)
{
    int                     ret;
    ThreadPool*             pool;

    pool = (ThreadPool*)malloc(sizeof (ThreadPool));
    if (!pool) {
        return NULL;
    }

    pool->msgQueue = msg_queue_create((size_t) - 1, 0);
    if (pool->msgQueue) {
        ret = pthread_mutex_init(&pool->mutex, NULL);
        if (ret == 0) {
            ret = pthread_key_create(&pool->key, NULL);
            if (ret == 0) {
                pool->stackSize = stackSize;
                pool->nThreads = 0;
                memset(&pool->tid, 0, sizeof (pthread_t));
                pool->terminate = NULL;
                if (_thread_pool_create_threads(nthreads, pool) >= 0) {
                    return pool;
                }
                pthread_key_delete(pool->key);
            }
            pthread_mutex_destroy(&pool->mutex);
        }
        errno = ret;
        msg_queue_destroy(pool->msgQueue);
    }

    free(pool);

    return NULL;
}


void _thread_pool_schedule(const ThreadPoolTask* task, void *buf, ThreadPool* pool)
{
    ((ThreadPoolTaskEntry*)buf)->task = *task;
    msg_queue_put(buf, pool->msgQueue);
}

int thread_pool_schedule(const ThreadPoolTask* task, ThreadPool* pool)
{
    void *buf = malloc(sizeof (ThreadPoolTaskEntry));

    if (buf) {
        _thread_pool_schedule(task, buf, pool);
        return 0;
    }

    return -1;
}

int thread_pool_increase (ThreadPool* pool)
{
    pthread_attr_t attr;
    pthread_t tid;
    int ret;

    ret = pthread_attr_init(&attr);
    if (ret == 0) {
        if (pool->stackSize) {
            pthread_attr_setstacksize(&attr, pool->stackSize);
        }

        pthread_mutex_lock(&pool->mutex);
        ret = pthread_create(&tid, &attr, _thread_pool_routine, pool);
        if (ret == 0) {
            pool->nThreads++;
        }

        pthread_mutex_unlock(&pool->mutex);
        pthread_attr_destroy(&attr);
        if (ret == 0) {
            return 0;
        }
    }
    errno = ret;

    return -1;
}


int thread_pool_in_pool (ThreadPool* pool)
{
    return pthread_getspecific (pool->key) == pool;
}

void thread_pool_destroy(void (*pending)(const ThreadPoolTask*), ThreadPool* pool)
{
    int in_pool = thread_pool_in_pool(pool);
    ThreadPoolTaskEntry* entry;

    _thread_pool_terminate(in_pool, pool);
    while (1) {
        entry = (ThreadPoolTaskEntry* )msg_queue_get(pool->msgQueue);
        if (!entry) {
            break;
        }

        if (pending) {
            pending(&entry->task);
        }

        free(entry);
    }

    pthread_key_delete(pool->key);
    pthread_mutex_destroy(&pool->mutex);
    msg_queue_destroy(pool->msgQueue);
    if (!in_pool) {
        free(pool);
    }
}
