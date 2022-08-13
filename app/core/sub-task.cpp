//
// Created by dingjing on 8/13/22.
//

#include "sub-task.h"


void SubTask::subTaskDone()
{
    SubTask *cur = this;
    ParallelTask *parent;
    SubTask **entry;

    while (1) {
        parent = cur->parent;
        entry = cur->entry;
        cur = cur->done();
        if (cur) {
            cur->parent = parent;
            cur->entry = entry;
            if (parent) {
                *entry = cur;
            }
            cur->dispatch();
        } else if (parent) {
            if (__sync_sub_and_fetch(&parent->nLeft, 1) == 0) {
                cur = parent;
                continue;
            }
        }
        break;
    }
}

void ParallelTask::dispatch()
{
    SubTask** end = this->subTasks + this->subTasksNR;
    SubTask** p = this->subTasks;

    this->nLeft = this->subTasksNR;
    if (this->nLeft != 0) {
        do {
            (*p)->parent = this;
            (*p)->entry = p;
            (*p)->dispatch();
        } while (++p != end);
    } else {
        this->subTaskDone();
    }
}
