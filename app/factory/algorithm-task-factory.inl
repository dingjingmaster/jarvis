//
// Created by dingjing on 8/25/22.
//

#ifndef JARVIS_ALGORITHM_TASK_FACTORY_INL
#define JARVIS_ALGORITHM_TASK_FACTORY_INL

#include <vector>
#include <utility>
#include <assert.h>
#include <algorithm>
#include <functional>

#include "workflow.h"
#include "../manager/global.h"

/********** Classes without CMP **********/

template<typename T>
class __SortTask : public SortTask<T>
{
protected:
    virtual void execute()
    {
        std::sort(this->mInput.first, this->mInput.last);
        this->mOutput.first = this->mInput.first;
        this->mOutput.last = this->mInput.last;
    }

public:
    __SortTask(ExecQueue *queue, Executor *executor, T *first, T *last, SortCallback<T>&& cb)
        : SortTask<T>(queue, executor, std::move(cb))
    {
        this->mInput.first = first;
        this->mInput.last = last;
        this->mOutput.first = NULL;
        this->mOutput.last = NULL;
    }
};

template<typename T>
class __MergeTask : public MergeTask<T>
{
protected:
    virtual void execute();

public:
    __MergeTask(ExecQueue *queue, Executor *executor, T *first1, T *last1, T *first2, T *last2, T *d_first, MergeCallback<T>&& cb)
        : MergeTask<T>(queue, executor, std::move(cb))
    {
        this->mInput.first1 = first1;
        this->mInput.last1 = last1;
        this->mInput.first2 = first2;
        this->mInput.last2 = last2;
        this->mInput.d_first = d_first;
        this->mOutput.first = NULL;
        this->mOutput.last = NULL;
    }
};

template<typename T>
void __MergeTask<T>::execute()
{
    auto *input = &this->mInput;
    auto *output = &this->mOutput;

    if (input->first1 == input->d_first && input->last1 == input->first2) {
        std::inplace_merge(input->first1, input->first2, input->last2);
        output->last = input->last2;
    } else if (input->first2 == input->d_first && input->last2 == input->first1) {
        std::inplace_merge(input->first2, input->first1, input->last1);
        output->last = input->last1;
    } else {
        output->last = std::merge(input->first1, input->last1, input->first2, input->first2, input->d_first);
    }

    output->first = input->d_first;
}

template<typename T>
class __ParSortTask : public __SortTask<T>
{
public:
    virtual void dispatch();

protected:
    virtual SubTask *done()
    {
        if (this->flag) {
            return seriesOf(this)->pop();
        }

        assert(this->mState == TASK_STATE_SUCCESS);
        return this->SortTask<T>::done();
    }

    virtual void execute();

protected:
    int                 depth;
    int                 flag;

public:
    __ParSortTask(ExecQueue *queue, Executor *executor, T *first, T *last, int depth, SortCallback<T>&& cb)
        : __SortTask<T>(queue, executor, first, last, std::move(cb))
    {
        this->depth = depth;
        this->flag = 0;
    }
};

template<typename T>
void __ParSortTask<T>::dispatch()
{
    size_t n = this->mInput.last - this->mInput.first;

    if (!this->flag && this->depth < 7 && n >= 32) {
        SeriesWork *series = seriesOf(this);
        T *middle = this->mInput.first + n / 2;
        auto *task1 = new __ParSortTask<T>(this->mQueue, this->mExecutor,
                                       this->mInput.first, middle,
                                       this->depth + 1,
                                       nullptr);
        auto *task2 =
                new __ParSortTask<T>(this->mQueue, this->mExecutor,
                                       middle, this->mInput.last,
                                       this->depth + 1,
                                       nullptr);
        SeriesWork *sub_series[2] = {
                Workflow::createSeriesWork(task1, nullptr),
                Workflow::createSeriesWork(task2, nullptr)
        };
        ParallelWork *parallel =
                Workflow::createParallelWork(sub_series, 2, nullptr);

        series->pushFront(this);
        series->pushFront(parallel);
        this->flag = 1;
        this->subTaskDone();
    }
    else
        this->__SortTask<T>::dispatch();
}

template<typename T>
void __ParSortTask<T>::execute()
{
    if (this->flag)
    {
        size_t n = this->mInput.last - this->mInput.first;
        T *middle = this->mInput.first + n / 2;

        std::inplace_merge(this->mInput.first, middle, this->mInput.last);
        this->mOutput.first = this->mInput.first;
        this->mOutput.last = this->mInput.last;
        this->flag = 0;
    }
    else
        this->__SortTask<T>::execute();
}

/********** Classes with CMP **********/

template<typename T, class CMP>
class __SortTaskCmp : public __SortTask<T>
{
protected:
    virtual void execute()
    {
        std::sort(this->mInput.first, this->mInput.last,
                  std::move(this->compare));
        this->mOutput.first = this->mInput.first;
        this->mOutput.last = this->mInput.last;
    }

protected:
    CMP compare;

public:
    __SortTaskCmp(ExecQueue *queue, Executor *executor, T *first, T *last, CMP&& cmp, SortCallback<T>&& cb)
        : __SortTask<T>(queue, executor, first, last, std::move(cb)), compare(std::move(cmp))
    {
    }
};

template<typename T, class CMP>
class __MergeTaskCmp : public __MergeTask<T>
{
protected:
    virtual void execute();

protected:
    CMP compare;

public:
    __MergeTaskCmp(ExecQueue *queue, Executor *executor, T *first1, T *last1, T *first2, T *last2, T *d_first, CMP&& cmp, MergeCallback<T>&& cb)
        : __MergeTask<T>(queue, executor, first1, last1, first2, last2, d_first, std::move(cb)), compare(std::move(cmp))
    {
    }
};

template<typename T, class CMP>
void __MergeTaskCmp<T, CMP>::execute()
{
    auto *input = &this->mInput;
    auto *output = &this->mOutput;

    if (input->first1 == input->d_first && input->last1 == input->first2) {
        std::inplace_merge(input->first1, input->first2, input->last2, std::move(this->compare));
        output->last = input->last2;
    } else if (input->first2 == input->d_first && input->last2 == input->first1) {
        std::inplace_merge(input->first2, input->first1, input->last1, std::move(this->compare));
        output->last = input->last1;
    } else {
        output->last = std::merge(input->first1, input->last1, input->first2, input->first2, input->d_first, std::move(this->compare));
    }

    output->first = input->d_first;
}

template<typename T, class CMP>
class __ParSortTaskCmp : public __SortTaskCmp<T, CMP>
{
public:
    virtual void dispatch();

protected:
    virtual SubTask *done()
    {
        if (this->flag)
            return seriesOf(this)->pop();

        assert(this->mState == TASK_STATE_SUCCESS);
        return this->SortTask<T>::done();
    }

    virtual void execute();

protected:
    int depth;
    int flag;

public:
    __ParSortTaskCmp(ExecQueue *queue, Executor *executor, T *first, T *last, CMP cmp, int depth, SortCallback<T>&& cb)
        : __SortTaskCmp<T, CMP>(queue, executor, first, last, std::move(cmp), std::move(cb))
    {
        this->depth = depth;
        this->flag = 0;
    }
};

template<typename T, class CMP>
void __ParSortTaskCmp<T, CMP>::dispatch()
{
    size_t n = this->mInput.last - this->mInput.first;

    if (!this->flag && this->depth < 7 && n >= 32)
    {
        SeriesWork *series = seriesOf(this);
        T *middle = this->mInput.first + n / 2;
        auto *task1 =
                new __ParSortTaskCmp<T, CMP>(this->mQueue, this->mExecutor,
                                               this->mInput.first, middle,
                                               this->compare, this->depth + 1,
                                               nullptr);
        auto *task2 =
                new __ParSortTaskCmp<T, CMP>(this->mQueue, this->mExecutor,
                                               middle, this->mInput.last,
                                               this->compare, this->depth + 1,
                                               nullptr);
        SeriesWork *sub_series[2] = {
                Workflow::createSeriesWork(task1, nullptr),
                Workflow::createSeriesWork(task2, nullptr)
        };
        ParallelWork *parallel = Workflow::createParallelWork(sub_series, 2, nullptr);

        series->pushFront(this);
        series->pushFront(parallel);
        this->flag = 1;
        this->subTaskDone();
    }
    else
        this->__SortTaskCmp<T, CMP>::dispatch();
}

template<typename T, class CMP>
void __ParSortTaskCmp<T, CMP>::execute()
{
    if (this->flag) {
        size_t n = this->mInput.last - this->mInput.first;
        T *middle = this->mInput.first + n / 2;

        std::inplace_merge(this->mInput.first, middle, this->mInput.last,
                           std::move(this->compare));
        this->mOutput.first = this->mInput.first;
        this->mOutput.last = this->mInput.last;
        this->flag = 0;
    } else {
        this->__SortTaskCmp<T, CMP>::execute();
    }
}

/********** Factory functions without CMP **********/

template<typename T, class CB>
SortTask<T> *AlgorithmTaskFactory::createSortTask(const std::string& name, T *first, T *last, CB callback)
{
    return new __SortTask<T>(Global::getExecQueue(name), Global::getComputeExecutor(), first, last, std::move(callback));
}

template<typename T, class CB>
MergeTask<T> *AlgorithmTaskFactory::createMergeTask(const std::string& name, T *first1, T *last1, T *first2, T *last2, T *d_first, CB callback)
{
    return new __MergeTask<T>(Global::getExecQueue(name), Global::getComputeExecutor(), first1, last1, first2, last2, d_first, std::move(callback));
}

template<typename T, class CB>
SortTask<T> *AlgorithmTaskFactory::createPSortTask(const std::string& name, T *first, T *last,CB callback)
{
    return new __ParSortTask<T>(Global::getExecQueue(name), Global::getComputeExecutor(), first, last, 0, std::move(callback));
}

/********** Factory functions with CMP **********/

template<typename T, class CMP, class CB>
SortTask<T> *AlgorithmTaskFactory::createSortTask(const std::string& name, T *first, T *last, CMP compare, CB callback)
{
    return new __SortTaskCmp<T, CMP>(Global::getExecQueue(name), Global::getComputeExecutor(), first, last, std::move(compare), std::move(callback));
}

template<typename T, class CMP, class CB>
MergeTask<T> *AlgorithmTaskFactory::createMergeTask(const std::string& name, T *first1, T *last1, T *first2, T *last2, T *d_first, CMP compare, CB callback)
{
    return new __MergeTaskCmp<T, CMP>(Global::getExecQueue(name), Global::getComputeExecutor(), first1, last1, first2, last2, d_first, std::move(compare), std::move(callback));
}

template<typename T, class CMP, class CB>
SortTask<T> *AlgorithmTaskFactory::createPSortTask(const std::string& name, T *first, T *last, CMP compare, CB callback)
{
    return new __ParSortTaskCmp<T, CMP>(Global::getExecQueue(name), Global::getComputeExecutor(), first, last, std::move(compare), 0, std::move(callback));
}

/****************** MapReduce ******************/

template<typename KEY, typename VAL>
class __ReduceTask : public ReduceTask<KEY, VAL>
{
protected:
    virtual void execute();

protected:
    algorithm::ReduceFunction<KEY, VAL>     reduce;

public:
    __ReduceTask(ExecQueue *queue, Executor *executor, algorithm::ReduceFunction<KEY, VAL>&& red, ReduceCallback<KEY, VAL>&& cb)
        : ReduceTask<KEY, VAL>(queue, executor, std::move(cb)), reduce(std::move(red))
    {
    }

    __ReduceTask(ExecQueue *queue, Executor *executor, algorithm::ReduceInput<KEY, VAL>&& input, algorithm::ReduceFunction<KEY, VAL>&& red, ReduceCallback<KEY, VAL>&& cb)
        : ReduceTask<KEY, VAL>(queue, executor, std::move(cb)), reduce(std::move(red))
    {
        this->mInput = std::move(input);
    }
};

template<class KEY, class VAL>
void __ReduceTask<KEY, VAL>::execute()
{
    algorithm::Reducer<KEY, VAL> reducer;
    auto iter = this->mInput.begin();

    while (iter != this->mInput.end())
    {
        reducer.insert(std::move(iter->first), std::move(iter->second));
        iter++;
    }

    this->mInput.clear();
    reducer.start(this->reduce, &this->mOutput);
}

template<typename KEY, typename VAL, class RED, class CB>
ReduceTask<KEY, VAL> *
AlgorithmTaskFactory::createReduceTask(const std::string& name,
                                      RED reduce,
                                      CB callback)
{
    return new __ReduceTask<KEY, VAL>(Global::getExecQueue(name),
                                        Global::getComputeExecutor(),
                                        std::move(reduce),
                                        std::move(callback));
}

template<typename KEY, typename VAL, class RED, class CB>
ReduceTask<KEY, VAL> *
AlgorithmTaskFactory::createReduceTask(const std::string& name,
                                      algorithm::ReduceInput<KEY, VAL> input,
                                      RED reduce,
                                      CB callback)
{
    return new __ReduceTask<KEY, VAL>(Global::getExecQueue(name),
                                        Global::getComputeExecutor(),
                                        std::move(input),
                                        std::move(reduce),
                                        std::move(callback));
}

#endif //JARVIS_ALGORITHM_TASK_FACTORY_INL
