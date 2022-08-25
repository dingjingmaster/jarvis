//
// Created by dingjing on 8/25/22.
//

#include <stdio.h>

#include "../app/manager/facilities.h"
#include "../app/factory/graph-task.h"
#include "../app/factory/task-factory.h"
#include "../app/protocol/http/http-message.h"

using namespace protocol;

static Facilities::WaitGroup waitGroup(1);

void go_func(const size_t *size1, const size_t *size2)
{
    printf("page1 size = %zu, page2 size = %zu\n", *size1, *size2);
}

void http_callback(HttpTask *task)
{
    size_t *size = (size_t *)task->mUserData;
    const void *body;

    if (task->getState() == TASK_STATE_SUCCESS)
        task->getResp()->getParsedBody(&body, size);
    else
        *size = (size_t)-1;
}

#define REDIRECT_MAX	3
#define RETRY_MAX		1

int main()
{
    TimerTask *timer;
    HttpTask *http_task1;
    HttpTask *http_task2;
    GoTask *go_task;
    size_t size1;
    size_t size2;

    timer = TaskFactory::createTimerTask(1000000, [](TimerTask *) {
        printf("timer task complete(1s).\n");
    });

    /* Http task1 */
    http_task1 = TaskFactory::createHttpTask("https://www.sogou.com/",
                                                 REDIRECT_MAX, RETRY_MAX,
                                                 http_callback);
    http_task1->mUserData = &size1;

    /* Http task2 */
    http_task2 = TaskFactory::createHttpTask("https://www.baidu.com/",
                                                 REDIRECT_MAX, RETRY_MAX,
                                                 http_callback);
    http_task2->mUserData = &size2;

    /* go task will print the http pages size */
    go_task = TaskFactory::createGoTask("go", go_func, &size1, &size2);

    /* Create a graph. Graph is also a kind of task */
    GraphTask *graph = TaskFactory::createGraphTask([](GraphTask *) {
        printf("Graph task complete. Wakeup main process\n");
        waitGroup.done();
    });

    /* Create graph nodes */
    GraphNode& a = graph->createGraphNode(timer);
    GraphNode& b = graph->createGraphNode(http_task1);
    GraphNode& c = graph->createGraphNode(http_task2);
    GraphNode& d = graph->createGraphNode(go_task);

    /* Build the graph */
    a-->b;
    a-->c;
    b-->d;
    c-->d;

    graph->start();
    waitGroup.wait();

    return 0;
}

