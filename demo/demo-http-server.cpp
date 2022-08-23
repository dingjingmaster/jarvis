//
// Created by dingjing on 8/17/22.
//

#include "../app/modules/http-server.h"

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
