//
// Created by dingjing on 8/13/22.
//

#include "m-poll.h"
#include <stddef.h>
#include <stdlib.h>
#include "c-poll.h"

extern CPoll* _poll_create(void **, const CPollParams*);
extern void _poll_destroy(CPoll*);

static int _m_poll_create(const CPollParams* params, MPoll* mPoll)
{
    void **nodesBuf = (void **)calloc(params->maxOpenFiles, sizeof (void *));
    unsigned int i;

    if (nodesBuf) {
        for (i = 0; i < mPoll->nThreads; i++) {
            mPoll->poll[i] = _poll_create(nodesBuf, params);
            if (!mPoll->poll[i]) {
                break;
            }
        }

        if (i == mPoll->nThreads) {
            mPoll->nodesBuf = nodesBuf;
            return 0;
        }

        while (i > 0) {
            _poll_destroy(mPoll->poll[--i]);
        }

        free(nodesBuf);
    }

    return -1;
}

MPoll* m_poll_create(const CPollParams* params, size_t nthreads)
{
    MPoll* poll;
    size_t size;

    if (nthreads == 0)
        nthreads = 1;

    size = offsetof(MPoll, poller) + nthreads * sizeof (void *);
    poll = (MPoll*)malloc(size);
    if (poll) {
        poll->nThreads = (unsigned int)nthreads;
        if (_m_poll_create(params, poll) >= 0) {
            return poll;
        }
        free(poll);
    }

    return NULL;
}

int m_poll_start(MPoll* poll)
{
    size_t i;

    for (i = 0; i < poll->nThreads; i++) {
        if (poll_start(poll->poll[i]) < 0) {
            break;
        }
    }

    if (i == poll->nThreads) {
        return 0;
    }

    while (i > 0) {
        poll_stop(poll->poll[--i]);
    }

    return -1;
}

void m_poll_stop(MPoll* poll)
{
    size_t i;

    for (i = 0; i < poll->nThreads; i++) {
        poll_stop(poll->poll[i]);
    }
}

void m_poll_destroy(MPoll* poll)
{
    size_t i;

    for (i = 0; i < poll->nThreads; i++) {
        _poll_destroy(poll->poll[i]);
    }

    free(poll->nodesBuf);
    free(poll);
}

