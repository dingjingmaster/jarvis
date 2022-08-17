//
// Created by dingjing on 8/13/22.
//

#ifndef JARVIS_SUB_TASK_H
#define JARVIS_SUB_TASK_H

#include <stddef.h>

class ParallelTask;

class SubTask
{
    friend class ParallelTask;
public:
    virtual void dispatch() = 0;

private:
    virtual SubTask* done() = 0;

protected:
    void subTaskDone();

public:
    ParallelTask* getParentTask() const { return mParent; }
    void* getPointer() const { return mPointer; }
    void setPointer(void *pointer) { mPointer = pointer; }

private:
    SubTask**               mEntry;
    ParallelTask*           mParent;
    void*                   mPointer;

public:
    SubTask()
    {
        mParent = NULL;
        mEntry = NULL;
        mPointer = NULL;
    }

    virtual ~SubTask() { }
};


class ParallelTask : public SubTask
{
    friend class SubTask;
public:
    ParallelTask (SubTask** subTasks, size_t n)
    {
        this->mSubTasks = subTasks;
        this->mSubTasksNR = n;
    }

    SubTask** getSubTasks(size_t *n) const
    {
        *n = this->mSubTasksNR;
        return this->mSubTasks;
    }

    void setSubTasks(SubTask** subTasks, size_t n)
    {
        this->mSubTasks = subTasks;
        this->mSubTasksNR = n;
    }

public:
    virtual void dispatch();

protected:
    SubTask**           mSubTasks;
    size_t              mSubTasksNR;

private:
    size_t              mLeft;
};

#endif //JARVIS_SUB_TASK_H
