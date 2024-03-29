//
// Created by dingjing on 8/12/22.
//

#include "msg-queue.h"
#include "../common/c-log.h"

#include <errno.h>
#include <stdlib.h>
#include <pthread.h>

struct _MsgQueue
{
    size_t              msgMax;
    size_t              msgCNT;
    int                 linkOff;
    int                 nonBlock;                       // 非阻塞插入信号队列
    void*               head1;
    void*               head2;
    void**              getHead;
    void**              putHead;
    void**              putTail;
    pthread_mutex_t     getMutex;
    pthread_mutex_t     putMutex;
    pthread_cond_t      getCond;
    pthread_cond_t      putCond;
};

void msg_queue_set_nonblock (MsgQueue* queue)
{
    queue->nonBlock = 1;
    pthread_mutex_lock(&queue->putMutex);
    pthread_cond_signal(&queue->getCond);
    pthread_cond_broadcast(&queue->putCond);
    pthread_mutex_unlock(&queue->putMutex);
}

void msg_queue_set_block (MsgQueue* queue)
{
    queue->nonBlock = 0;
}

static size_t _msg_queue_swap (MsgQueue* queue)
{
    void **getHead = queue->getHead;
    size_t cnt;

    queue->getHead = queue->putHead;
    pthread_mutex_lock(&queue->putMutex);
    while (queue->msgCNT == 0 && !queue->nonBlock) {
        pthread_cond_wait(&queue->getCond, &queue->putMutex);
    }

    cnt = queue->msgCNT;
    if (cnt > queue->msgMax - 1) {
        pthread_cond_broadcast(&queue->putCond);
    }

    queue->putHead = getHead;
    queue->putTail = getHead;
    queue->msgCNT = 0;
    pthread_mutex_unlock(&queue->putMutex);

    return cnt;
}

void msg_queue_put(void *msg, MsgQueue * queue)
{
    void** link = (void**)((char *)msg + queue->linkOff);

    *link = NULL;
    pthread_mutex_lock(&queue->putMutex);
    while (queue->msgCNT > queue->msgMax - 1 && !queue->nonBlock) {
        pthread_cond_wait(&queue->putCond, &queue->putMutex);
    }

    *queue->putTail = link;
    queue->putTail = link;
    ++queue->msgCNT;
    pthread_mutex_unlock(&queue->putMutex);
    pthread_cond_signal(&queue->getCond);
}

void* msg_queue_get (MsgQueue* queue)
{
    void *msg;

    pthread_mutex_lock(&queue->getMutex);
    if (*queue->getHead || _msg_queue_swap(queue) > 0) {
        msg = (char*)*queue->getHead - queue->linkOff;
        *queue->getHead = *(void **)*queue->getHead;
    } else {
        msg = NULL;
        errno = ENOENT;
    }

    pthread_mutex_unlock(&queue->getMutex);

    return msg;
}

MsgQueue* msg_queue_create(size_t maxLen, int linkOff)
{
    logv("in");
    MsgQueue* queue = (MsgQueue*)malloc(sizeof (MsgQueue));
    int ret;

    if (!queue) {
        return NULL;
    }

    ret = pthread_mutex_init(&queue->getMutex, NULL);
    if (ret == 0) {
        ret = pthread_mutex_init(&queue->putMutex, NULL);
        if (ret == 0) {
            ret = pthread_cond_init(&queue->getCond, NULL);
            if (ret == 0) {
                ret = pthread_cond_init(&queue->putCond, NULL);
                if (ret == 0) {
                    queue->msgMax = maxLen;
                    queue->linkOff = linkOff;
                    queue->head1 = NULL;
                    queue->head2 = NULL;
                    queue->getHead = &queue->head1;
                    queue->putHead = &queue->head2;
                    queue->putTail = &queue->head2;
                    queue->msgCNT = 0;
                    queue->nonBlock = 0;
                    return queue;
                }
                pthread_cond_destroy(&queue->getCond);
            }
            pthread_mutex_destroy(&queue->putMutex);
        }
        pthread_mutex_destroy(&queue->getMutex);
    }

    errno = ret;
    free (queue);

    return NULL;
}

void msg_queue_destroy (MsgQueue* queue)
{
    pthread_cond_destroy(&queue->putCond);
    pthread_cond_destroy(&queue->getCond);
    pthread_mutex_destroy(&queue->putMutex);
    pthread_mutex_destroy(&queue->getMutex);

    free(queue);
}
