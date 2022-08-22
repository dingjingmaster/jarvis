//
// Created by dingjing on 8/22/22.
//

#ifndef JARVIS_RESOURCE_POOL_H
#define JARVIS_RESOURCE_POOL_H

#include <mutex>

#include "task.h"
#include "../core/c-list.h"

class ResourcePool
{
public:
    Conditional* get(SubTask *task, void **resbuf);
    Conditional* get(SubTask *task);
    void post(void *res);

public:
    struct Data
    {
        void *pop() { return this->pool->pop(); }
        void push(void *res) { this->pool->push(res); }

        void **res;
        long value;
        size_t index;
        struct list_head wait_list;
        std::mutex mutex;
        ResourcePool *pool;
    };

protected:
    virtual void *pop()
    {
        return this->data.res[this->data.index++];
    }

    virtual void push(void *res)
    {
        this->data.res[--this->data.index] = res;
    }

protected:
    struct Data data;

private:
    void create(size_t n);

public:
    ResourcePool(void *const *res, size_t n);
    ResourcePool(size_t n);
    virtual ~ResourcePool() { delete []this->data.res; }
};


#endif //JARVIS_RESOURCE_POOL_H
