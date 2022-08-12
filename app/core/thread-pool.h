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

thrdpool_t *thrdpool_create(size_t nthreads, size_t stacksize);
int thrdpool_schedule(const struct thrdpool_task *task, thrdpool_t *pool);
int thrdpool_increase(thrdpool_t *pool);
int thrdpool_in_pool(thrdpool_t *pool);
void thrdpool_destroy(void (*pending)(const struct thrdpool_task *),
                      thrdpool_t *pool);

#ifdef __cplusplus
};
#endif
#endif //JARVIS_THREAD_POOL_H
