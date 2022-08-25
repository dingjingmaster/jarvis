//
// Created by dingjing on 8/25/22.
//

#ifndef JARVIS_ALGORITHM_TASK_FACTORY_H
#define JARVIS_ALGORITHM_TASK_FACTORY_H
#include <string>
#include <vector>
#include <utility>

#include "task.h"
#include "../algorithm/map-reduce.h"

namespace algorithm
{
    template<typename T>
    struct SortInput
    {
        T*          first;
        T*          last;
    };

    template<typename T>
    struct SortOutput
    {
        T*          first;
        T*          last;
    };

    template<typename T>
    struct MergeInput
    {
        T*          first1;
        T*          last1;
        T*          first2;
        T*          last2;
        T*          d_first;
    };

    template<typename T>
    struct MergeOutput
    {
        T*          first;
        T*          last;
    };

    template<typename KEY = std::string, typename VAL = std::string>
    using ReduceInput = std::vector<std::pair<KEY, VAL>>;

    template<typename KEY = std::string, typename VAL = std::string>
    using ReduceOutput = std::vector<std::pair<KEY, VAL>>;

} /* namespace algorithm */

template<typename T>
using SortTask = ThreadTask<algorithm::SortInput<T>, algorithm::SortOutput<T>>;
template<typename T>
using SortCallback = std::function<void (SortTask<T> *)>;

template<typename T>
using MergeTask = ThreadTask<algorithm::MergeInput<T>, algorithm::MergeOutput<T>>;
template<typename T>
using MergeCallback = std::function<void (MergeTask<T> *)>;

template<typename KEY = std::string, typename VAL = std::string>
using ReduceTask = ThreadTask<algorithm::ReduceInput<KEY, VAL>, algorithm::ReduceOutput<KEY, VAL>>;
template<typename KEY = std::string, typename VAL = std::string>
using ReduceCallback = std::function<void (ReduceTask<KEY, VAL>*)>;

class AlgorithmTaskFactory
{
public:
    template<typename T, class CB = SortCallback<T>>
    static SortTask<T> *createSortTask(const std::string& queueName, T *first, T *last, CB callback);

    template<typename T, class CMP, class CB = SortCallback<T>>
    static SortTask<T> *createSortTask(const std::string& queueName, T *first, T *last, CMP compare, CB callback);

    template<typename T, class CB = MergeCallback<T>>
    static MergeTask<T> *createMergeTask(const std::string& queueName, T *first1, T *last1, T *first2, T *last2, T *d_first, CB callback);

    template<typename T, class CMP, class CB = MergeCallback<T>>
    static MergeTask<T> *createMergeTask(const std::string& queueName, T *first1, T *last1, T *first2, T *last2, T *d_first, CMP compare, CB callback);

    template<typename T, class CB = SortCallback<T>>
    static SortTask<T> *createPSortTask(const std::string& queueName, T *first, T *last, CB callback);

    template<typename T, class CMP, class CB = SortCallback<T>>
    static SortTask<T> *createPSortTask(const std::string& queueName, T *first, T *last, CMP compare, CB callback);

    template<typename KEY = std::string, typename VAL = std::string, class RED = algorithm::ReduceFunction<KEY, VAL>, class CB = ReduceCallback<KEY, VAL>>
    static ReduceTask<KEY, VAL>* createReduceTask(const std::string& queueName, RED reduce, CB callback);

    template<typename KEY = std::string, typename VAL = std::string, class RED = algorithm::ReduceFunction<KEY, VAL>, class CB = ReduceCallback<KEY, VAL>>
    static ReduceTask<KEY, VAL>* createReduceTask(const std::string& queueName, algorithm::ReduceInput<KEY, VAL> input, RED reduce, CB callback);
};

#include "algorithm-task-factory.inl"
#endif //JARVIS_ALGORITHM_TASK_FACTORY_H
