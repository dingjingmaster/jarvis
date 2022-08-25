//
// Created by dingjing on 8/25/22.
//

#include <stdio.h>
#include <stdlib.h>

#include "../app/manager/facilities.h"
#include "../app/factory/task-factory.h"

using namespace algorithm;

static Facilities::WaitGroup waitGroup(1);

bool use_parallel_sort = false;

void callback(SortTask<int> *task)
{
    /* Sort task's input and output are identical. */
    SortInput<int> *input = task->getInput();
    int *first = input->first;
    int *last = input->last;

    /* You may remove this output to test speed. */
    int *p = first;

    while (p < last)
        printf("%d ", *p++);

    printf("\n");
    if (task->mUserData == nullptr) {
        auto cmp = [](int a1, int a2)->bool{return a2<a1;};
        SortTask<int> *reverse;

        if (use_parallel_sort)
            reverse = AlgorithmTaskFactory::createPSortTask("sort", first, last, cmp, callback);
        else
            reverse = AlgorithmTaskFactory::createSortTask("sort", first, last, cmp, callback);

        reverse->mUserData = (void *)1;	/* as a flag */
        seriesOf(task)->pushBack(reverse);
        printf("Sort reversely:\n");
    } else {
        waitGroup.done();
    }
}

int main(int argc, char *argv[])
{
    size_t count;
    int *array;
    int *end;
    size_t i;

    if (argc != 2 && argc != 3)
    {
        fprintf(stderr, "USAGE: %s <count> [p]\n", argv[0]);
        exit(1);
    }

    count = atoi(argv[1]);
    array = (int *)malloc(count * sizeof (int));
    if (!array)
    {
        perror("malloc");
        exit(1);
    }

    if (argc == 3 && (*argv[2] == 'p' || *argv[2] == 'P'))
        use_parallel_sort = true;

    for (i = 0; i < count; i++)
        array[i] = rand() % 65536;
    end = &array[count];

    SortTask<int> *task;
    if (use_parallel_sort)
        task = AlgorithmTaskFactory::createPSortTask("sort", array, end, callback);
    else
        task = AlgorithmTaskFactory::createSortTask("sort", array, end, callback);

    if (use_parallel_sort)
        printf("Start sorting parallelly...\n");
    else
        printf("Start sorting...\n");

    printf("Sort result:\n");
    task->start();

    waitGroup.wait();
    free(array);

    return 0;
}
