//
// Created by dingjing on 8/23/22.
//
#include "task-factory.h"

#include <mutex>
#include <time.h>
#include <string>
#include <sys/types.h>

#include "../core/c-list.h"
#include "../core/rb-tree.h"
#include "../manager/global.h"

class __TimerTask : public TimerTask
{
protected:
    virtual int duration(struct timespec *value)
    {
        value->tv_sec = this->seconds;
        value->tv_nsec = this->nanoseconds;
        return 0;
    }

protected:
    time_t              seconds;
    long                nanoseconds;

public:
    __TimerTask(CommScheduler *scheduler, time_t seconds, long nanoseconds, TimerCallback && cb)
        : TimerTask(scheduler, std::move(cb))
    {
        this->seconds = seconds;
        this->nanoseconds = nanoseconds;
    }
};

TimerTask* TaskFactory::createTimerTask(unsigned int microseconds, TimerCallback callback)
{
    return new __TimerTask(Global::getScheduler(), (time_t)(microseconds / 1000000), (long)(microseconds % 1000000 * 1000), std::move(callback));
}

TimerTask *TaskFactory::createTimerTask(time_t seconds, long nanoseconds, TimerCallback callback)
{
    return new __TimerTask(Global::getScheduler(), seconds, nanoseconds, std::move(callback));
}

class __CounterTask;

struct __counter_node
{
    struct list_head list;
    unsigned int target_value;
    __CounterTask *task;
};

struct __CounterList
{
    __CounterList(const std::string& str):
            name(str)
    {
        INIT_LIST_HEAD(&this->head);
    }

    void pushBack(struct __counter_node *node)
    {
        list_add_tail(&node->list, &this->head);
    }

    bool empty() const
    {
        return list_empty(&this->head);
    }

    void del(struct __counter_node *node)
    {
        list_del(&node->list);
    }

    RBNode              rb;
    struct list_head    head;
    std::string         name;
};

static class __CounterMap
{
public:
    CounterTask *create(const std::string& name, unsigned int target_value,
                          std::function<void (CounterTask *)>&& cb);

    void count_n(const std::string& name, unsigned int n);
    void count(struct __CounterList *counters, struct __counter_node *node);
    void remove(struct __CounterList *counters, struct __counter_node *node);

private:
    void count_n_locked(struct __CounterList *counters, unsigned int n, struct list_head *task_list);
    RBRoot          counters_map_;
    std::mutex      mutex_;

public:
    __CounterMap()
    {
        counters_map_.rbNode = NULL;
    }
} __counter_map;

class __CounterTask : public CounterTask
{
public:
    __CounterTask(unsigned int target_value, struct __CounterList *counters,
                    std::function<void (CounterTask *)>&& cb) :
            CounterTask(1, std::move(cb)),
            counters_(counters)
    {
        node_.target_value = target_value;
        node_.task = this;
        counters_->pushBack(&node_);
    }

    virtual ~__CounterTask()
    {
        if (this->mValue != 0)
            __counter_map.remove(counters_, &node_);
    }

    virtual void count()
    {
        __counter_map.count(counters_, &node_);
    }

private:
    struct __counter_node node_;
    struct __CounterList *counters_;
    friend class __CounterMap;
};

CounterTask *__CounterMap::create(const std::string& name, unsigned int target_value, std::function<void (CounterTask *)>&& cb)
{
    if (target_value == 0)
        return new CounterTask(0, std::move(cb));

    RBNode**p = &counters_map_.rbNode;
    RBNode*parent = NULL;
    struct __CounterList *counters;
    std::lock_guard<std::mutex> lock(mutex_);

    while (*p) {
        parent = *p;
        counters = RB_ENTRY(*p, struct __CounterList, rb);

        if (name < counters->name)
            p = &(*p)->rbLeft;
        else if (name > counters->name)
            p = &(*p)->rbRight;
        else
            break;
    }

    if (*p == NULL) {
        counters = new struct __CounterList(name);
        rb_link_node(&counters->rb, parent, p);
        rb_insert_color(&counters->rb, &counters_map_);
    }

    return new __CounterTask(target_value, counters, std::move(cb));
}

void __CounterMap::count_n_locked(struct __CounterList *counters,
                                  unsigned int n, struct list_head *task_list)
{
    struct list_head *pos;
    struct list_head *tmp;
    struct __counter_node *node;

    list_for_each_safe(pos, tmp, &counters->head)
    {
        if (n == 0)
            return;

        node = list_entry(pos, struct __counter_node, list);
        if (n >= node->target_value)
        {
            n -= node->target_value;
            node->target_value = 0;
            list_move_tail(pos, task_list);
            if (counters->empty())
            {
                rb_erase(&counters->rb, &counters_map_);
                delete counters;
                return;
            }
        }
        else
        {
            node->target_value -= n;
            n = 0;
        }
    }
}

void __CounterMap::count_n(const std::string& name, unsigned int n)
{
    RBNode**p = &counters_map_.rbNode;
    struct __CounterList *counters;
    struct __counter_node *node;
    LIST_HEAD(task_list);

    mutex_.lock();
    while (*p) {
        counters = RB_ENTRY(*p, struct __CounterList, rb);

        if (name < counters->name)
            p = &(*p)->rbLeft;
        else if (name > counters->name)
            p = &(*p)->rbRight;
        else {
            count_n_locked(counters, n, &task_list);
            break;
        }
    }

    mutex_.unlock();
    while (!list_empty(&task_list)) {
        node = list_entry(task_list.next, struct __counter_node, list);
        list_del(&node->list);
        node->task->CounterTask::count();
    }
}

void __CounterMap::count(struct __CounterList *counters, struct __counter_node *node)
{
    __CounterTask *task = NULL;

    mutex_.lock();
    if (--node->target_value == 0)
    {
        task = node->task;
        counters->del(node);
        if (counters->empty())
        {
            rb_erase(&counters->rb, &counters_map_);
            delete counters;
        }
    }

    mutex_.unlock();
    if (task)
        task->CounterTask::count();
}

void __CounterMap::remove(struct __CounterList *counters, struct __counter_node *node)
{
    mutex_.lock();
    counters->del(node);
    if (counters->empty())
    {
        rb_erase(&counters->rb, &counters_map_);
        delete counters;
    }

    mutex_.unlock();
}

CounterTask *TaskFactory::createCounterTask(const std::string& counter_name, unsigned int target_value, CounterCallback callback)
{
    return __counter_map.create(counter_name, target_value, std::move(callback));
}

void TaskFactory::countByName(const std::string& counter_name, unsigned int n)
{
    __counter_map.count_n(counter_name, n);
}

/********MailboxTask*************/

class __MailboxTask : public MailboxTask
{
public:
    __MailboxTask(size_t size, MailboxCallback && cb)
        : MailboxTask(new void *[size], size, std::move(cb))
    {
    }

    virtual ~__MailboxTask()
    {
        delete []this->mMailbox;
    }
};

MailboxTask *TaskFactory::createMailboxTask(size_t size, MailboxCallback callback)
{
    return new __MailboxTask(size, std::move(callback));
}

MailboxTask *TaskFactory::createMailboxTask(MailboxCallback callback)
{
    return new MailboxTask(std::move(callback));
}

/****************** Named Conditional ******************/

class __Conditional;

struct __conditional_node
{
    struct list_head list;
    __Conditional *cond;
};

struct __ConditionalList
{
    __ConditionalList(const std::string& str):
            name(str)
    {
        INIT_LIST_HEAD(&this->head);
    }

    void push_back(struct __conditional_node *node)
    {
        list_add_tail(&node->list, &this->head);
    }

    bool empty() const
    {
        return list_empty(&this->head);
    }

    void del(struct __conditional_node *node)
    {
        list_del(&node->list);
    }

    RBNode                      rb;
    struct list_head            head;
    std::string                 name;
    friend class __ConditionalMap;
};

static class __ConditionalMap
{
public:
    Conditional *create(const std::string& name,
                          SubTask *task, void **msgbuf);

    Conditional *create(const std::string& name, SubTask *task);

    void signal(const std::string& name, void *msg);
    void signal(struct __ConditionalList *conds,
                struct __conditional_node *node,
                void *msg);
    void remove(struct __ConditionalList *conds,
                struct __conditional_node *node);

private:
    struct __ConditionalList *get_list(const std::string& name);
    RBRoot                  conds_map_;
    std::mutex              mutex_;

public:
    __ConditionalMap()
    {
        conds_map_.rbNode = NULL;
    }
} __conditional_map;

class __Conditional : public Conditional
{
public:
    __Conditional(SubTask *task, void **msgbuf,
                    struct __ConditionalList *conds) :
            Conditional(task, msgbuf),
            conds_(conds)
    {
        node_.cond = this;
        conds_->push_back(&node_);
    }

    __Conditional(SubTask *task, struct __ConditionalList *conds) :
            Conditional(task),
            conds_(conds)
    {
        node_.cond = this;
        conds_->push_back(&node_);
    }

    virtual ~__Conditional()
    {
        if (!this->mFlag)
            __conditional_map.remove(conds_, &node_);
    }

    virtual void signal(void *msg)
    {
        __conditional_map.signal(conds_, &node_, msg);
    }

private:
    struct __conditional_node node_;
    struct __ConditionalList *conds_;
    friend class __ConditionalMap;
};

Conditional *__ConditionalMap::create(const std::string& name,
                                        SubTask *task, void **msgbuf)
{
    std::lock_guard<std::mutex> lock(mutex_);
    struct __ConditionalList *conds = get_list(name);
    return new __Conditional(task, msgbuf, conds);
}

Conditional *__ConditionalMap::create(const std::string& name,
                                        SubTask *task)
{
    std::lock_guard<std::mutex> lock(mutex_);
    struct __ConditionalList *conds = get_list(name);
    return new __Conditional(task, conds);
}

struct __ConditionalList *__ConditionalMap::get_list(const std::string& name)
{
    RBNode**p = &conds_map_.rbNode;
    RBNode*parent = NULL;
    struct __ConditionalList *conds;

    while (*p) {
        parent = *p;
        conds = RB_ENTRY(*p, struct __ConditionalList, rb);
        if (name < conds->name)
            p = &(*p)->rbLeft;
        else if (name > conds->name)
            p = &(*p)->rbRight;
        else
            break;
    }

    if (*p == NULL) {
        conds = new struct __ConditionalList(name);
        rb_link_node(&conds->rb, parent, p);
        rb_insert_color(&conds->rb, &conds_map_);
    }

    return conds;
}

void __ConditionalMap::signal(const std::string& name, void *msg)
{
    RBNode**p = &conds_map_.rbNode;
    struct __ConditionalList *conds = NULL;

    mutex_.lock();
    while (*p) {
        conds = RB_ENTRY(*p, struct __ConditionalList, rb);

        if (name < conds->name)
            p = &(*p)->rbLeft;
        else if (name > conds->name)
            p = &(*p)->rbRight;
        else {
            rb_erase(&conds->rb, &conds_map_);
            break;
        }
    }

    mutex_.unlock();
    if (!conds)
        return;

    struct list_head *pos;
    struct list_head *tmp;
    struct __conditional_node *node;

    list_for_each_safe(pos, tmp, &conds->head)
    {
        node = list_entry(pos, struct __conditional_node, list);
        node->cond->Conditional::signal(msg);
    }

    delete conds;
}

void __ConditionalMap::signal(struct __ConditionalList *conds,
                              struct __conditional_node *node,
                              void *msg)
{
    mutex_.lock();
    conds->del(node);
    if (conds->empty())
    {
        rb_erase(&conds->rb, &conds_map_);
        delete conds;
    }

    mutex_.unlock();
    node->cond->Conditional::signal(msg);
}

void __ConditionalMap::remove(struct __ConditionalList *conds, struct __conditional_node *node)
{
    mutex_.lock();
    conds->del(node);
    if (conds->empty()) {
        rb_erase(&conds->rb, &conds_map_);
        delete conds;
    }

    mutex_.unlock();
}

Conditional *TaskFactory::createConditional(const std::string& cond_name, SubTask *task, void **msgbuf)
{
    return __conditional_map.create(cond_name, task, msgbuf);
}

Conditional *TaskFactory::createConditional(const std::string& cond_name, SubTask *task)
{
    return __conditional_map.create(cond_name, task);
}

void TaskFactory::signalByName(const std::string& cond_name, void *msg)
{
    __conditional_map.signal(cond_name, msg);
}

/**************** Timed Go Task *****************/

void _TimedGoTask::dispatch()
{
    TimerTask *timer;

    timer = TaskFactory::createTimerTask(this->mSeconds, this->mNanoseconds, _TimedGoTask::timerCallback);
    timer->mUserData = this;

    this->_GoTask::dispatch();
    timer->start();
}

SubTask *_TimedGoTask::done()
{
    if (this->mCallback)
        this->mCallback(this);

    return seriesOf(this)->pop();
}

void _TimedGoTask::handle(int state, int error)
{
    if (--this->mRef == 3) {
        this->mState = state;
        this->mError = error;
        this->subTaskDone();
    }

    if (--this->mRef == 0)
        delete this;
}

void _TimedGoTask::timerCallback(TimerTask *timer)
{
    _TimedGoTask *task = (_TimedGoTask *)timer->mUserData;

    if (--task->mRef == 3)
    {
        task->mState = TASK_STATE_ABORTED;
        task->mError = 0;
        task->subTaskDone();
    }

    if (--task->mRef == 0)
        delete task;
}

