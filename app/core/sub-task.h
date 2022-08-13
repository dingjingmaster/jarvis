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
    ParallelTask* getParentTask() const { return this->parent; }
    void* getPointer() const { return this->pointer; }
    void setPointer(void *pointer) { this->pointer = pointer; }

private:
    SubTask**               entry;
    ParallelTask*           parent;
    void*                   pointer;

public:
    SubTask()
    {
        this->parent = NULL;
        this->entry = NULL;
        this->pointer = NULL;
    }

    virtual ~SubTask() { }
};


class ParallelTask : public SubTask
{
    friend class SubTask;
public:
    ParallelTask (SubTask** subTasks, size_t n)
    {
        this->subTasks = subTasks;
        this->subTasksNR = n;
    }

    SubTask** getSubTasks(size_t *n) const
    {
        *n = this->subTasksNR;
        return this->subTasks;
    }

    void setSubTasks(SubTask** subTasks, size_t n)
    {
        this->subTasks = subTasks;
        this->subTasksNR = n;
    }

public:
    virtual void dispatch();

protected:
    SubTask**           subTasks;
    size_t              subTasksNR;

private:
    size_t              nLeft;
};

#endif //JARVIS_SUB_TASK_H
