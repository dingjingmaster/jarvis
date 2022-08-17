//
// Created by dingjing on 8/17/22.
//

#include "workflow.h"

#include <mutex>
#include <utility>
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <functional>


inline SeriesWork* Workflow::createSeriesWork(SubTask* first, SeriesCallback callback)
{
    return new SeriesWork(first, std::move(callback));
}

inline void Workflow::startSeriesWork(SubTask *first, SeriesCallback callback)
{
    new SeriesWork(first, std::move(callback));
    first->dispatch();
}

inline SeriesWork* Workflow::createSeriesWork(SubTask *first, SubTask *last, SeriesCallback callback)
{
    SeriesWork *series = new SeriesWork(first, std::move(callback));
    series->setLastTask(last);

    return series;
}

inline void Workflow::startSeriesWork(SubTask *first, SubTask *last, SeriesCallback callback)
{
    SeriesWork *series = new SeriesWork(first, std::move(callback));
    series->setLastTask(last);
    first->dispatch();
}

ParallelWork::ParallelWork(ParallelCallback&& cb)
    : ParallelTask(new SubTask * [2 * 4], 0), mCallback(std::move(cb))
{
    mBufSize = 4;
    mAllSeries = (SeriesWork **)&mSubTasks[mBufSize];
    mContext = NULL;
}

ParallelWork::ParallelWork(SeriesWork *const *allSeries, size_t n, ParallelCallback &&cb)
    : ParallelTask(new SubTask *[2 * (n > 4 ? n : 4)], n), mCallback(std::move(cb))
{
    size_t      i = 0;

    mBufSize = (n > 4 ? n : 4);
    mAllSeries = (SeriesWork **)&mSubTasks[mBufSize];
    for (i = 0; i < n; i++) {
        assert(!mAllSeries[i]->mInParallel);
        mAllSeries[i]->mInParallel = true;
        mAllSeries[i] = allSeries[i];
        mSubTasks[i] = allSeries[i]->mFirst;
    }

    mContext = NULL;
}

ParallelWork::~ParallelWork()
{
    for (size_t i = 0; i < mSubTasksNR; ++i) {
        mAllSeries[i]->mInParallel = false;
        mAllSeries[i]->dismissRecursive();
    }

    delete []this->mSubTasks;
}


void ParallelWork::expandBuf()
{
    SubTask**               buf;
    size_t                  size;

    mBufSize *= 2;
    buf = new SubTask *[2 * mBufSize];
    size = mSubTasksNR * sizeof (void *);
    memcpy(buf, mSubTasks, size);
    memcpy(buf + mBufSize, mAllSeries, size);

    delete []mSubTasks;
    mSubTasks = buf;
    mAllSeries = (SeriesWork **)&buf[mBufSize];
}

SubTask *ParallelWork::done()
{
    SeriesWork *series = seriesOf(this);
    size_t i;

    if (mCallback)
        mCallback(this);

    for (i = 0; i < mSubTasksNR; ++i)
        delete mAllSeries[i];

    mSubTasksNR = 0;
    delete this;

    return series->pop();
}

void ParallelWork::addSeries(SeriesWork *series)
{
    if (mSubTasksNR == mBufSize)
        expandBuf();

    assert(!series->mInParallel);
    series->mInParallel = true;
    mAllSeries[mSubTasksNR] = series;
    mSubTasks[mSubTasksNR] = series->mFirst;
    ++mSubTasksNR;
}

void SeriesWork::pushBack(SubTask *task)
{
    mMutex.lock();
    task->setPointer(this);
    mQueue[mBack] = task;
    if (++mBack == mQueueSize)
        mBack = 0;

    if (mFront == mBack)
        expandQueue();

    mMutex.unlock();
}

void SeriesWork::pushFront(SubTask *task)
{
    mMutex.lock();
    if (--mFront == -1)
        mFront = mQueueSize - 1;

    task->setPointer(this);
    mQueue[mFront] = task;
    if (mFront == mBack)
        expandQueue();

    mMutex.unlock();
}

SubTask *SeriesWork::pop()
{
    bool canceled = mCanceled;
    SubTask *task = popTask();

    if (!canceled)
        return task;

    while (task) {
        delete task;
        task = popTask();
    }

    return nullptr;
}

SeriesWork::~SeriesWork()
{
    if (mQueue != mBuf)
        delete [] mQueue;
}

SeriesWork::SeriesWork(SubTask *first, SeriesCallback &&cb)
    : mCallback(std::move(cb))
{
    mQueue = mBuf;
    mQueueSize = sizeof mBuf / sizeof *mBuf;
    mFront = 0;
    mBack = 0;
    mInParallel = false;
    mCanceled = false;
    mFinished = false;
    assert(!seriesOf(first));
    first->setPointer(this);
    mFirst = first;
    mLast = NULL;
    mContext = NULL;
}

void SeriesWork::dismissRecursive()
{
    SubTask *task = mFirst;

    mCallback = nullptr;
    do {
        delete task;
        task = popTask();
    } while (task);
}

SubTask *SeriesWork::popTask()
{
    SubTask *task;

    mMutex.lock();
    if (mFront != mBack) {
        task = mQueue[mFront];
        if (++mFront == mQueueSize)
            mFront = 0;
    } else {
        task = mLast;
        mLast = NULL;
    }

    mMutex.unlock();
    if (!task) {
        mFinished = true;

        if (mCallback)
            mCallback(this);

        if (!mInParallel)
            delete this;
    }

    return task;
}

void SeriesWork::expandQueue()
{
    int size = 2 * mQueueSize;
    SubTask **queue = new SubTask *[size];
    int i, j;

    i = 0;
    j = mFront;
    do {
        queue[i++] = mQueue[j++];
        if (j == mQueueSize)
            j = 0;
    } while (j != mBack);

    if (mQueue != mBuf)
        delete []   mQueue;

    mQueue = queue;
    mQueueSize = size;
    mFront = 0;
    mBack = i;
}

inline ParallelWork* Workflow::createParallelWork(ParallelCallback callback)
{
    return new ParallelWork(std::move(callback));
}

inline ParallelWork* Workflow::createParallelWork(SeriesWork *const all_series[], size_t n, ParallelCallback callback)
{
    return new ParallelWork(all_series, n, std::move(callback));
}

inline void Workflow::startParallelWork(SeriesWork *const all_series[], size_t n, ParallelCallback callback)
{
    ParallelWork *p = new ParallelWork(all_series, n, std::move(callback));
    Workflow::startSeriesWork(p, nullptr);
}
