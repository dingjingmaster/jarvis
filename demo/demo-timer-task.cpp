//
// Created by dingjing on 9/26/22.
//
#include <iostream>

#include "../app/manager/facilities.h"
#include "../app/factory/task-factory.h"

static Facilities::WaitGroup waitGroup(1);

int main (void)
{
    TimerTask* task = TaskFactory::createTimerTask(10000, [=] (TimerTask*) {
        std::cout << "sss" << std::endl;
    });

    task->start();

    waitGroup.wait();
}