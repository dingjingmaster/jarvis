//
// Created by dingjing on 8/13/22.
//

#include "common-scheduler.h"

#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>

#define PTHREAD_COND_TIMEDWAIT(cond, mutex, abstime) \
	((abstime) ? pthread_cond_timedwait(cond, mutex, abstime) : \
				 pthread_cond_wait(cond, mutex))

static struct timespec *__get_abstime(int timeout, struct timespec *ts)
{
    if (timeout < 0)
        return NULL;

    clock_gettime(CLOCK_REALTIME, ts);
    ts->tv_sec += timeout / 1000;
    ts->tv_nsec += timeout % 1000 * 1000000;
    if (ts->tv_nsec >= 1000000000) {
        ts->tv_nsec -= 1000000000;
        ts->tv_sec++;
    }

    return ts;
}

int CommSchedTarget::init(const struct sockaddr *addr, socklen_t addrlen, int connect_timeout, int response_timeout, size_t max_connections)
{
    int ret;

    if (max_connections == 0) {
        errno = EINVAL;
        return -1;
    }

    if (this->CommTarget::init(addr, addrlen, connect_timeout, response_timeout) >= 0) {
        ret = pthread_mutex_init(&mMutex, NULL);
        if (ret == 0) {
            ret = pthread_cond_init(&mCond, NULL);
            if (ret == 0) {
                mMaxLoad = max_connections;
                mCurLoad = 0;
                mWaitCnt = 0;
                mGroup = NULL;
                return 0;
            }

            pthread_mutex_destroy(&mMutex);
        }

        errno = ret;
        this->CommTarget::deInit();
    }

    return -1;
}

void CommSchedTarget::deInit()
{
    pthread_cond_destroy(&mCond);
    pthread_mutex_destroy(&mMutex);
    this->CommTarget::deInit();
}

CommTarget *CommSchedTarget::acquire(int wait_timeout)
{
    pthread_mutex_t *mutex = &mMutex;
    int ret;

    pthread_mutex_lock(mutex);
    if (mGroup)
    {
        mutex = &mGroup->mMutex;
        pthread_mutex_lock(mutex);
        pthread_mutex_unlock(&mMutex);
    }

    if (mCurLoad >= mMaxLoad)
    {
        if (wait_timeout != 0)
        {
            struct timespec ts;
            struct timespec *abstime = __get_abstime(wait_timeout, &ts);

            do
            {
                mWaitCnt++;
                ret = PTHREAD_COND_TIMEDWAIT(&mCond, mutex, abstime);
                mWaitCnt--;
            } while (mCurLoad >= mMaxLoad && ret == 0);
        }
        else
            ret = EAGAIN;
    }

    if (mCurLoad < mMaxLoad)
    {
        mCurLoad++;
        if (mGroup)
        {
            mGroup->mCurLoad++;
            mGroup->heapify(mIndex);
        }

        ret = 0;
    }

    pthread_mutex_unlock(mutex);
    if (ret)
    {
        errno = ret;
        return NULL;
    }

    return this;
}

void CommSchedTarget::release(int keep_alive)
{
    pthread_mutex_t *mutex = &mMutex;

    pthread_mutex_lock(mutex);
    if (mGroup)
    {
        mutex = &mGroup->mMutex;
        pthread_mutex_lock(mutex);
        pthread_mutex_unlock(&mMutex);
    }

    mCurLoad--;
    if (mWaitCnt > 0)
        pthread_cond_signal(&mCond);

    if (mGroup)
    {
        mGroup->mCurLoad--;
        if (mWaitCnt == 0 && mGroup->mWaitCnt > 0)
            pthread_cond_signal(&mGroup->mCond);

        mGroup->heap_adjust(mIndex, keep_alive);
    }

    pthread_mutex_unlock(mutex);
}

int CommSchedGroup::target_cmp(CommSchedTarget *target1,
                               CommSchedTarget *target2)
{
    size_t load1 = target1->mCurLoad * target2->mMaxLoad;
    size_t load2 = target2->mCurLoad * target1->mMaxLoad;

    if (load1 < load2)
        return -1;
    else if (load1 > load2)
        return 1;
    else
        return 0;
}

void CommSchedGroup::heap_adjust(int index, int swap_on_equal)
{
    CommSchedTarget *target = mTgHeap[index];
    CommSchedTarget *parent;

    while (index > 0)
    {
        parent = mTgHeap[(index - 1) / 2];
        if (CommSchedGroup::target_cmp(target, parent) < !!swap_on_equal)
        {
            mTgHeap[index] = parent;
            parent->mIndex = index;
            index = (index - 1) / 2;
        }
        else
            break;
    }

    mTgHeap[index] = target;
    target->mIndex = index;
}

/* Fastest heapify ever. */
void CommSchedGroup::heapify(int top)
{
    CommSchedTarget *target = mTgHeap[top];
    int last = mHeapSize - 1;
    CommSchedTarget **child;
    int i;

    while (i = 2 * top + 1, i < last)
    {
        child = &mTgHeap[i];
        if (CommSchedGroup::target_cmp(child[0], target) < 0)
        {
            if (CommSchedGroup::target_cmp(child[1], child[0]) < 0)
            {
                mTgHeap[top] = child[1];
                child[1]->mIndex = top;
                top = i + 1;
            }
            else
            {
                mTgHeap[top] = child[0];
                child[0]->mIndex = top;
                top = i;
            }
        }
        else
        {
            if (CommSchedGroup::target_cmp(child[1], target) < 0)
            {
                mTgHeap[top] = child[1];
                child[1]->mIndex = top;
                top = i + 1;
            }
            else
            {
                mTgHeap[top] = target;
                target->mIndex = top;
                return;
            }
        }
    }

    if (i == last)
    {
        child = &mTgHeap[i];
        if (CommSchedGroup::target_cmp(child[0], target) < 0)
        {
            mTgHeap[top] = child[0];
            child[0]->mIndex = top;
            top = i;
        }
    }

    mTgHeap[top] = target;
    target->mIndex = top;
}

int CommSchedGroup::heap_insert(CommSchedTarget *target)
{
    if (mHeapSize == mHeapBufSize)
    {
        int new_size = 2 * mHeapBufSize;
        void *new_base = realloc(mTgHeap, new_size * sizeof (void *));

        if (new_base)
        {
            mTgHeap = (CommSchedTarget **)new_base;
            mHeapBufSize = new_size;
        }
        else
            return -1;
    }

    mTgHeap[mHeapSize] = target;
    target->mIndex = mHeapSize;
    this->heap_adjust(mHeapSize, 0);
    mHeapSize++;
    return 0;
}

void CommSchedGroup::heap_remove(int index)
{
    CommSchedTarget *target;

    mHeapSize--;
    if (index != mHeapSize)
    {
        target = mTgHeap[mHeapSize];
        mTgHeap[index] = target;
        target->mIndex = index;
        this->heap_adjust(index, 0);
        this->heapify(target->mIndex);
    }
}

#define COMMGROUP_INIT_SIZE		4

int CommSchedGroup::init()
{
    size_t size = COMMGROUP_INIT_SIZE * sizeof (void *);
    int ret;

    mTgHeap = (CommSchedTarget **)malloc(size);
    if (mTgHeap)
    {
        ret = pthread_mutex_init(&mMutex, NULL);
        if (ret == 0)
        {
            ret = pthread_cond_init(&mCond, NULL);
            if (ret == 0)
            {
                mHeapBufSize = COMMGROUP_INIT_SIZE;
                mHeapSize = 0;
                mMaxLoad = 0;
                mCurLoad = 0;
                mWaitCnt = 0;
                return 0;
            }

            pthread_mutex_destroy(&mMutex);
        }

        errno = ret;
        free(mTgHeap);
    }

    return -1;
}

void CommSchedGroup::deInit()
{
    pthread_cond_destroy(&mCond);
    pthread_mutex_destroy(&mMutex);
    free(mTgHeap);
}

int CommSchedGroup::add(CommSchedTarget *target)
{
    int ret = -1;

    pthread_mutex_lock(&target->mMutex);
    pthread_mutex_lock(&mMutex);
    if (target->mGroup == NULL && target->mWaitCnt == 0)
    {
        if (this->heap_insert(target) >= 0)
        {
            target->mGroup = this;
            mMaxLoad += target->mMaxLoad;
            mCurLoad += target->mCurLoad;
            if (mWaitCnt > 0 && mCurLoad < mMaxLoad)
                pthread_cond_signal(&mCond);

            ret = 0;
        }
    }
    else if (target->mGroup == this)
        errno = EEXIST;
    else if (target->mGroup)
        errno = EINVAL;
    else
        errno = EBUSY;

    pthread_mutex_unlock(&mMutex);
    pthread_mutex_unlock(&target->mMutex);
    return ret;
}

int CommSchedGroup::remove(CommSchedTarget *target)
{
    int ret = -1;

    pthread_mutex_lock(&target->mMutex);
    pthread_mutex_lock(&mMutex);
    if (target->mGroup == this && target->mWaitCnt == 0)
    {
        this->heap_remove(target->mIndex);
        mMaxLoad -= target->mMaxLoad;
        mCurLoad -= target->mCurLoad;
        target->mGroup = NULL;
        ret = 0;
    }
    else if (target->mGroup != this)
        errno = ENOENT;
    else
        errno = EBUSY;

    pthread_mutex_unlock(&mMutex);
    pthread_mutex_unlock(&target->mMutex);
    return ret;
}

CommTarget *CommSchedGroup::acquire(int wait_timeout)
{
    pthread_mutex_t *mutex = &mMutex;
    CommSchedTarget *target;
    int ret;

    pthread_mutex_lock(mutex);
    if (mCurLoad >= mMaxLoad)
    {
        if (wait_timeout != 0)
        {
            struct timespec ts;
            struct timespec *abstime = __get_abstime(wait_timeout, &ts);

            do
            {
                mWaitCnt++;
                ret = PTHREAD_COND_TIMEDWAIT(&mCond, mutex, abstime);
                mWaitCnt--;
            } while (mCurLoad >= mMaxLoad && ret == 0);
        }
        else
            ret = EAGAIN;
    }

    if (mCurLoad < mMaxLoad)
    {
        target = mTgHeap[0];
        target->mCurLoad++;
        mCurLoad++;
        this->heapify(0);
        ret = 0;
    }

    pthread_mutex_unlock(mutex);
    if (ret)
    {
        errno = ret;
        return NULL;
    }

    return target;
}
