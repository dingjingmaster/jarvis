//
// Created by dingjing on 8/13/22.
//

#include "sub-task.h"


void SubTask::subTaskDone()
{
    SubTask* cur = this;
    ParallelTask* parent;
    SubTask** entry;

    while (true) {
        // mParent 表示 所有子任务的父任务
        parent = cur->mParent;
        // mEntry 表示子任务的入口，也就是第一个子任务
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
