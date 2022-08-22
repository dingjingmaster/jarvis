//
// Created by dingjing on 8/17/22.
//

#include <stdio.h>
#include "../app/factory/task.h"
#include "../app/utils/lru-cache.h"
#include "../app/utils/string-util.h"
#include "../app/factory/task-error.h"
#include "../app/factory/task-factory.h"
#include "../app/algorithm/dns-routine.h"

#include "../app/modules/http-server.h"
#include "../app/manager/route-manager.h"
#include "../app/nameservice/name-service.h"

int main (int argc, char* argv[])
{
    HttpServer server ([] (HttpTask* task) {
        task->getResp()->appendOutputBody("<html>Hello World!</html>");
    });

    if (0 == server.start(8888)) {
        getchar();
        server.stop();
    }

    return 0;
}
