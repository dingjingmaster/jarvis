//
// Created by dingjing on 8/17/22.
//

#include "workflow.h"
#include "../core/c-log.h"

#include <mutex>
#include <utility>
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <functional>


SeriesWork* Workflow::createSeriesWork(SubTask* first, SeriesCallback callback)
{
    logv("");
    return new SeriesWork(first, std::move(callback));
}

void Workflow::startSeriesWork(SubTask *first, SeriesCallback callback)
{
    logv("");
    new SeriesWork(first, std::move(callback));
    first->dispatch();
}

SeriesWork* Workflow::createSeriesWork(SubTask *first, SubTask *last, SeriesCallback callback)
{
    logv("");
    SeriesWork *series = new SeriesWork(first, std::move(callback));
    series->setLastTask(last);

    return series;
}

void Workflow::startSeriesWork(SubTask *first, SubTask *last, SeriesCallback callback)
{
    logv("");
    SeriesWork *series = new SeriesWork(first, std::move(callback));
    series->setLastTask(last);
    first->dispatch();
}

ParallelWork::ParallelWork(ParallelCallback&& cb)
    : ParallelTask(new SubTask * [2 * 4], 0), mCallback(std::move(cb))
{
    logv("");
    mBufSize = 4;
    mAllSeries = (SeriesWork **)&mSubTasks[mBufSize];
    mContext = NULL;
}

ParallelWork::ParallelWork(SeriesWork *const *allSeries, size_t n, ParallelCallback &&cb)
    : ParallelTask(new SubTask *[2 * (n > 4 ? n : 4)], n), mCallback(std::move(cb))
{
    logv("");
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
    logv("");
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

    logv("");
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
    logv("");
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
    logv("");
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
    logv("");
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
    logv("");
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
    logv("");
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
    logv("");
    if (mQueue != mBuf)
        delete [] mQueue;
}

SeriesWork::SeriesWork(SubTask *first, SeriesCallback &&cb)
    : mCallback(std::move(cb))
{
    logv("");
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
    logv("");
    SubTask *task = mFirst;

    mCallback = nullptr;
    do {
        delete task;
        task = popTask();
    } while (task);
}

SubTask *SeriesWork::popTask()
{
    logv("");
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
    logv("");
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

ParallelWork* Workflow::createParallelWork(ParallelCallback callback)
{
    logv("");
    return new ParallelWork(std::move(callback));
}

ParallelWork* Workflow::createParallelWork(SeriesWork *const all_series[], size_t n, ParallelCallback callback)
{
    logv("");
    return new ParallelWork(all_series, n, std::move(callback));
}

void Workflow::startParallelWork(SeriesWork *const all_series[], size_t n, ParallelCallback callback)
{
    logv("");
    ParallelWork *p = new ParallelWork(all_series, n, std::move(callback));
    Workflow::startSeriesWork(p, nullptr);
}
