//
// Created by dingjing on 8/13/22.
//
#include "executor.h"

#include <errno.h>
#include <stdlib.h>
#include <pthread.h>

#include "c-list.h"
#include "thread-pool.h"

typedef struct _ExecTaskEntry       ExecTaskEntry;

struct _ExecTaskEntry
{
    struct list_head        list;
    ExecSession*            session;
    ThreadPool*             threadPool;
};

extern "C" void _thread_pool_schedule(const ThreadPoolTask*, void*, ThreadPool*);

int ExecQueue::init ()
{
    int ret;

    ret = pthread_mutex_init(&mMutex, NULL);
    if (ret == 0) {
        INIT_LIST_HEAD(&mTaskList);
        return 0;
    }

    errno = ret;

    return -1;
}

void ExecQueue::deInit ()
{
    pthread_mutex_destroy(&mMutex);
}

int Executor::init(size_t nthreads)
{
    if (nthreads == 0) {
        errno = EINVAL;
        return -1;
    }

    mThreadPool = thread_pool_create(nthreads, 0);
    if (mThreadPool) {
        return 0;
    }

    return -1;
}

void Executor::deInit ()
{
    thread_pool_destroy(Executor::executor_cancel_tasks, mThreadPool);
}

void Executor::executor_thread_routine(void *context)
{
    ExecQueue *queue = (ExecQueue *)context;
    ExecTaskEntry *entry;
    ExecSession *session;

    pthread_mutex_lock(&queue->mMutex);
    entry = list_entry(queue->mTaskList.next, ExecTaskEntry, list);
    list_del(&entry->list);
    session = entry->session;
    if (!list_empty(&queue->mTaskList)) {
        ThreadPoolTask task = {
                .routine	=	Executor::executor_thread_routine,
                .context	=	queue
        };
        _thread_pool_schedule(&task, entry, entry->threadPool);
    } else {
        free(entry);
    }

    pthread_mutex_unlock(&queue->mMutex);
    session->execute();
    session->handle(ES_STATE_FINISHED, 0);
}

void Executor::executor_cancel_tasks(const ThreadPoolTask* task)
{
    ExecQueue *queue = (ExecQueue *)task->context;
    ExecTaskEntry *entry;
    struct list_head *pos, *tmp;
    ExecSession *session;

    list_for_each_safe(pos, tmp, &queue->mTaskList) {
        entry = list_entry(pos, ExecTaskEntry, list);
        list_del(pos);
        session = entry->session;
        free(entry);

        session->handle(ES_STATE_CANCELED, 0);
    }
}

int Executor::request(ExecSession *session, ExecQueue *queue)
{
    ExecTaskEntry *entry;

    session->mQueue = queue;
    entry = (ExecTaskEntry *)malloc(sizeof (ExecTaskEntry));
    if (entry) {
        entry->session = session;
        entry->threadPool = mThreadPool;
        pthread_mutex_lock(&queue->mMutex);
        list_add_tail(&entry->list, &queue->mTaskList);
        if (queue->mTaskList.next == &entry->list) {
            ThreadPoolTask task = {
                    .routine	=	Executor::executor_thread_routine,
                    .context	=	queue
            };
            if (thread_pool_schedule(&task, mThreadPool) < 0) {
                list_del(&entry->list);
                free(entry);
                entry = NULL;
            }
        }
        pthread_mutex_unlock(&queue->mMutex);
    }

    return -!entry;
}

