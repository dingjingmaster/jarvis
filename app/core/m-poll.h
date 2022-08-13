//
// Created by dingjing on 8/13/22.
//

#ifndef JARVIS_M_POLL_H
#define JARVIS_M_POLL_H
#include "c-poll.h"

#ifdef __cplusplus
extern "C"
{
#endif
typedef struct _MPoll           MPoll;

struct _MPoll
{
    void**                      nodesBuf;
    unsigned int                nThreads;
    CPoll*                      poll[1];
};

int m_poll_start(MPoll* mPoll);
void m_poll_stop(MPoll* mPoll);
void m_poll_destroy(MPoll* mPoll);
MPoll* m_poll_create (const CPollParams* params, size_t nthreads);


static inline int m_poll_add(const CPollData* data, int timeout, MPoll* mPoll)
{
    unsigned int index = (unsigned int)data->fd % mPoll->nThreads;

    return poll_add(data, timeout, mPoll->poll[index]);
}

static inline int m_poll_del(int fd, MPoll* mPoll)
{
    unsigned int index = (unsigned int)fd % mPoll->nThreads;

    return poll_del(fd, mPoll->poll[index]);
}

static inline int m_poll_mod(const CPollData* data, int timeout, MPoll* mPoll)
{
    unsigned int index = (unsigned int)data->fd % mPoll->nThreads;

    return poll_mod(data, timeout, mPoll->poll[index]);
}

static inline int m_poll_set_timeout(int fd, int timeout, MPoll* mPoll)
{
    unsigned int index = (unsigned int)fd % mPoll->nThreads;

    return poll_set_timeout(fd, timeout, mPoll->poll[index]);
}

static inline int m_poll_add_timer(const struct timespec *value, void *context, MPoll* mPoll)
{
    static unsigned int n = 0;
    unsigned int index = n++ % mPoll->nThreads;

    return poll_add_timer(value, context, mPoll->poll[index]);
}

#ifdef __cplusplus
}
#endif
#endif //JARVIS_M_POLL_H
