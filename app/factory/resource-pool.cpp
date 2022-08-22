//
// Created by dingjing on 8/22/22.
//

#include "resource-pool.h"

#include <string.h>

#include "task.h"
#include "../core/c-list.h"

class _RPConditional : public Conditional
{
public:
    struct list_head list;
    struct ResourcePool::Data *data;

public:
    virtual void dispatch();
    virtual void signal(void *res) { }

public:
    _RPConditional(SubTask *task, void **resbuf,
                    struct ResourcePool::Data *data) :
            Conditional(task, resbuf)
    {
        this->data = data;
    }

    _RPConditional(SubTask *task,
                    struct ResourcePool::Data *data) :
            Conditional(task)
    {
        this->data = data;
    }
};

void _RPConditional::dispatch()
{
    struct ResourcePool::Data *data = this->data;

    data->mutex.lock();
    if (--data->value >= 0)
        this->Conditional::signal(data->pop());
    else
        list_add_tail(&this->list, &data->wait_list);

    data->mutex.unlock();
    this->Conditional::dispatch();
}

Conditional *ResourcePool::get(SubTask *task, void **resbuf)
{
    return new _RPConditional(task, resbuf, &this->data);
}

Conditional *ResourcePool::get(SubTask *task)
{
    return new _RPConditional(task, &this->data);
}

void ResourcePool::create(size_t n)
{
    this->data.res = new void *[n];
    this->data.value = n;
    this->data.index = 0;
    INIT_LIST_HEAD(&this->data.wait_list);
    this->data.pool = this;
}

ResourcePool::ResourcePool(void *const *res, size_t n)
{
    this->create(n);
    memcpy(this->data.res, res, n * sizeof (void *));
}

ResourcePool::ResourcePool(size_t n)
{
    this->create(n);
    memset(this->data.res, 0, n * sizeof (void *));
}

void ResourcePool::post(void *res)
{
    struct ResourcePool::Data *data = &this->data;
    Conditional *cond;

    data->mutex.lock();
    if (++data->value <= 0)
    {
        cond = list_entry(data->wait_list.next, _RPConditional, list);
        list_del(data->wait_list.next);
    }
    else
    {
        cond = NULL;
        this->push(res);
    }

    data->mutex.unlock();
    if (cond)
        cond->Conditional::signal(res);
}
