//
// Created by dingjing on 8/17/22.
//

#ifndef JARVIS_WORKFLOW_H
#define JARVIS_WORKFLOW_H

#include <mutex>
#include <utility>
#include <assert.h>
#include <stddef.h>
#include <functional>

#include "../core/sub-task.h"

class SeriesWork;
class ParallelWork;

using SeriesCallback    = std::function<void(const SeriesWork*)>;
using ParallelCallback  = std::function<void(const ParallelWork*)>;


class Workflow
{
public:
    static SeriesWork* createSeriesWork(SubTask* first, SeriesCallback callback);
    static void startSeriesWork(SubTask* first, SeriesCallback cb);

    static ParallelWork* createParallelWork (ParallelCallback cb);
    static ParallelWork* createParallelWork (SeriesWork* const allSeries[], size_t n, ParallelCallback cb);
    static void startParallelWork (SeriesWork* const allSeries[], size_t n, ParallelCallback cb);

    static SeriesWork* createSeriesWork(SubTask* first, SubTask* last, SeriesCallback cb);
    static void startSeriesWork(SubTask* first, SubTask* last, SeriesCallback cb);
};

class SeriesWork
{
    friend class Workflow;
    friend class ParallelWork;
public:
    void start()
    {
        assert(!mInParallel);
        mFirst->dispatch();
    }

    void dismiss ()
    {
        assert(!mInParallel);
        dismissRecursive ();
    }

    void pushBack (SubTask* task);
    void pushFront (SubTask* task);

    void* getContext() const
    {
        return mContext;
    }

    void setContext (void* context)
    {
        mContext = context;
    }

    virtual void cancel ()
    {
        mCanceled = true;
    }

    bool isCanceled () const
    {
        return mCanceled;
    }

    bool isFinished () const
    {
        return mFinished;
    }

    void setCallback (SeriesCallback cb)
    {
        mCallback = std::move(cb);
    }

    SubTask* pop ();

    void setLastTask (SubTask* last)
    {
        last->setPointer(this);
        mLast = last;
    }

    void unsetLastTask ()
    {
        mLast = nullptr;
    }

protected:
    virtual ~SeriesWork();
    SeriesWork(SubTask* first, SeriesCallback&& cb);

    SubTask* getLastTask () const
    {
        return mLast;
    }

    void setInParallel ()
    {
        mInParallel = true;
    }

    void dismissRecursive ();

private:
    SubTask* popTask ();
    void expandQueue ();

protected:
    void*                       mContext;
    SeriesCallback              mCallback;

private:
    SubTask*                    mBuf[4];
    SubTask*                    mFirst;
    SubTask*                    mLast;
    SubTask**                   mQueue;

    int                         mQueueSize;
    int                         mFront;
    int                         mBack;

    bool                        mInParallel;
    bool                        mCanceled;
    bool                        mFinished;

    std::mutex                  mMutex;
};

static inline SeriesWork *seriesOf(const SubTask *task)
{
    return (SeriesWork*)task->getPointer();
}

static inline SeriesWork& operator *(const SubTask& task)
{
    return *seriesOf(&task);
}

static inline SeriesWork& operator << (SeriesWork& series, SubTask *task)
{
    series.pushBack(task);
    return series;
}

class ParallelWork : public ParallelTask
{
    friend class Workflow;
public:
    void start ()
    {
        assert(!seriesOf (this));
        Workflow::startSeriesWork(this, nullptr);
    }

    void dismiss()
    {
        assert(!seriesOf(this));
        delete this;
    }

    void addSeries(SeriesWork* series);

    void* getContext() const
    {
        return mContext;
    }

    void setContext(void* context)
    {
        mContext = context;
    }

    SeriesWork* seriesAt (size_t index)
    {
        if (index < mSubTasksNR) {
            return mAllSeries[index];
        } else {
            return nullptr;
        }
    }

    const SeriesWork* seriesAt (size_t index) const
    {
        if (index < mSubTasksNR) {
            return mAllSeries[index];
        } else {
            return nullptr;
        }
    }

    SeriesWork& operator[] (size_t index)
    {
        return *seriesAt(index);
    }

    const SeriesWork& operator[] (size_t index) const
    {
        return *seriesAt(index);
    }

    size_t size () const
    {
        return mSubTasksNR;
    }

    void setCallback (ParallelCallback cb)
    {
        mCallback = std::move(cb);
    }

protected:
    explicit ParallelWork (ParallelCallback&& cb);
    ParallelWork (SeriesWork* const allSeries[], size_t n, ParallelCallback&& cb);
    ~ParallelWork() override;

    SubTask* done () override;

private:
    void expandBuf ();

protected:
    void*                   mContext;
    ParallelCallback        mCallback;

private:
    size_t                  mBufSize;
    SeriesWork**            mAllSeries;
};

#endif //JARVIS_WORKFLOW_H
