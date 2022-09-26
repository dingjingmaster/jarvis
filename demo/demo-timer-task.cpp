//
// Created by dingjing on 9/26/22.
//
#include <iostream>

#include "../app/manager/facilities.h"
#include "../app/factory/task-factory.h"

static Facilities::WaitGroup waitGroup(1);

int main (void)
{
    TimerTask* task = TaskFactory::createTimerTask(1000000, [=] (TimerTask*) {
        static int a = 0;
        std::cout << "sss: " << a++ << std::endl;
    });

    task->setRepeat(true);

    task->start();

    waitGroup.wait();
}