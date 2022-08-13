//
// Created by dingjing on 8/12/22.
//

#ifndef JARVIS_THREAD_POOL_H
#define JARVIS_THREAD_POOL_H
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif
typedef struct _ThreadPool              ThreadPool;
typedef struct _ThreadPoolTask          ThreadPoolTask;

struct _ThreadPoolTask
{
    void (*routine) (void*);
    void* context;
};

ThreadPool* thread_pool_create (size_t nThreads, size_t stackSize);
int thread_pool_schedule (const ThreadPoolTask * task, ThreadPool *pool);
int thread_pool_increase(ThreadPool *pool);
int thread_pool_in_pool(ThreadPool *pool);
void thread_pool_destroy(void (*pending)(const ThreadPoolTask *), ThreadPool *pool);

#ifdef __cplusplus
};
#endif
#endif //JARVIS_THREAD_POOL_H
