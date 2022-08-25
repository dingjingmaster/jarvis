//
// Created by dingjing on 8/25/22.
//

#ifndef JARVIS_GRAPH_TASK_H
#define JARVIS_GRAPH_TASK_H

#include <vector>
#include <utility>
#include <functional>

#include "task.h"
#include "workflow.h"

class GraphNode : protected CounterTask
{
    friend class GraphTask;
public:
    void precede(GraphNode& node)
    {
        node.mValue++;
        this->mSuccessors.push_back(&node);
    }

    void succeed(GraphNode& node)
    {
        node.precede(*this);
    }

protected:
    virtual SubTask *done();

protected:
    std::vector<GraphNode *>        mSuccessors;

protected:
    GraphNode() : CounterTask(0, nullptr) { }
    virtual ~GraphNode();
};

static inline GraphNode& operator --(GraphNode& node, int)
{
    return node;
}

static inline GraphNode& operator > (GraphNode& prec, GraphNode& succ)
{
    prec.precede(succ);
    return succ;
}

static inline GraphNode& operator < (GraphNode& succ, GraphNode& prec)
{
    succ.succeed(prec);
    return prec;
}

static inline GraphNode& operator --(GraphNode& node)
{
    return node;
}

class GraphTask : public GenericTask
{
public:
    GraphNode& createGraphNode(SubTask *task);

public:
    void setCallback(std::function<void (GraphTask*)> cb)
    {
        this->callback = std::move(cb);
    }

protected:
    virtual void dispatch();
    virtual SubTask *done();

protected:
    ParallelWork*                           parallel;
    std::function<void (GraphTask*)>        callback;

public:
    GraphTask(std::function<void (GraphTask*)>&& cb) :
            callback(std::move(cb))
    {
        this->parallel = Workflow::createParallelWork(nullptr);
    }

protected:
    virtual ~GraphTask();
};


#endif //JARVIS_GRAPH_TASK_H
