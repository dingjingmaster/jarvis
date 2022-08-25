//
// Created by dingjing on 8/25/22.
//

#include "graph-task.h"
SubTask *GraphNode::done()
{
    SeriesWork *series = seriesOf(this);

    if (!this->mUserData) {
        this->mValue = 1;
        this->mUserData = (void *)1;
    }
    else
        delete this;

    return series->pop();
}

GraphNode::~GraphNode()
{
    if (this->mUserData)
    {
        for (GraphNode *node : this->mSuccessors)
            node->CounterTask::count();
    }
}

GraphNode& GraphTask::createGraphNode(SubTask *task)
{
    GraphNode *node = new GraphNode;
    SeriesWork *series = Workflow::createSeriesWork(node, node, nullptr);

    series->pushBack(task);
    this->parallel->addSeries(series);
    return *node;
}

void GraphTask::dispatch()
{
    SeriesWork *series = seriesOf(this);

    if (this->parallel)
    {
        series->pushFront(this);
        series->pushFront(this->parallel);
        this->parallel = NULL;
    }
    else
        this->mState = TASK_STATE_SUCCESS;

    this->subTaskDone();
}

SubTask *GraphTask::done()
{
    SeriesWork *series = seriesOf(this);

    if (this->mState == TASK_STATE_SUCCESS)
    {
        if (this->callback)
            this->callback(this);

        delete this;
    }

    return series->pop();
}

GraphTask::~GraphTask()
{
    SeriesWork *series;
    size_t i;

    if (this->parallel) {
        for (i = 0; i < this->parallel->size(); i++) {
            series = this->parallel->seriesAt(i);
            series->unsetLastTask();
        }

        this->parallel->dismiss();
    }
}

