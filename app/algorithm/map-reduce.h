//
// Created by dingjing on 8/17/22.
//

#ifndef JARVIS_MAP_REDUCE_H
#define JARVIS_MAP_REDUCE_H

#include <vector>
#include <utility>
#include <functional>

#include "../core/rb-tree.h"

namespace algorithm
{

    template<typename VAL>
    class ReduceIterator
    {
    public:
        virtual const VAL *next() = 0;
        virtual size_t size() = 0;

    protected:
        virtual ~ReduceIterator() { }
    };

    template<typename KEY, typename VAL>
    using reduce_function_t =
    std::function<void (const KEY *, ReduceIterator<VAL> *, VAL *)>;

    template<typename KEY, typename VAL>
    class Reducer
    {
    public:
        void insert(KEY&& key, VAL&& val);

    public:
        void start(reduce_function_t<KEY, VAL> reduce,
                   std::vector<std::pair<KEY, VAL>> *output);

    private:
        struct rb_root key_tree;

    public:
        Reducer() { this->key_tree.rb_node = NULL; }
        virtual ~Reducer();
    };

}


#endif //JARVIS_MAP_REDUCE_H
