//
// Created by dingjing on 8/25/22.
//

#include <stdio.h>
#include <stdlib.h>
#include <algorithm>

#include "../app/manager/facilities.h"
#include "../app/factory/algorithm-task-factory.h"

using namespace algorithm;

struct array
{
    int *a;
    int n;
    size_t size() { return n; }
};

void reduce(const int *key, ReduceIterator<array> *iter, array *res)
{
    const array *v1 = iter->next();
    const array *v2 = iter->next();

    res->a = new int[v1->n + v2->n];
    res->n = v1->n + v2->n;
    std::merge(v1->a, v1->a + v1->n, v2->a, v2->a + v2->n, res->a);
    delete []v1->a;
    delete []v2->a;
}

Facilities::WaitGroup waitGroup(1);

void callback(ReduceTask<int, array> *task)
{
    ReduceOutput<int, array>& output = *task->getOutput();
    array& res = output[0].second;

    for (int i = 0; i < res.n; i++)
        printf("%d ", res.a[i]);

    printf("\n");
    delete []res.a;
    waitGroup.done();
}

int main(int argc, char *argv[])
{
    ReduceInput<int, array> input;
    array arr;
    int i;

    if (argc != 2)
    {
        fprintf(stderr, "USAGE: %s <num>\n", argv[0]);
        exit(1);
    }

    int n = atoi(argv[1]);

    for (i = 0; i < n; i++)
    {
        arr.n = 1;
        arr.a = new int[1];
        arr.a[0] = rand() % 65536;
        input.emplace_back(0, arr);
    }

    auto *task = AlgorithmTaskFactory::createReduceTask("sort", std::move(input), reduce, callback);
    task->start();

    waitGroup.wait();
    return 0;
}