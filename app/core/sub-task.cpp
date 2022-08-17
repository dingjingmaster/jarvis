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
        parent = cur->mParent;
        entry = cur->mEntry;
        cur = cur->done();
        if (cur) {
            cur->mParent = parent;
            cur->mEntry = entry;
            if (parent) {
                *entry = cur;
            }
            cur->dispatch();
        } else if (parent) {
            if (__sync_sub_and_fetch(&parent->mLeft, 1) == 0) {
                cur = parent;
                continue;
            }
        }
        break;
    }
}

void ParallelTask::dispatch()
{
    SubTask** end = mSubTasks + mSubTasksNR;
    SubTask** p = mSubTasks;

    mLeft = mSubTasksNR;
    if (mLeft != 0) {
        do {
            (*p)->mParent = this;
            (*p)->mEntry = p;
            (*p)->dispatch();
        } while (++p != end);
    } else {
        this->subTaskDone();
    }
}
