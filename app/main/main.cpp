//
// Created by dingjing on 8/26/22.
//

#include "../core/c-log.h"
#include "manager/facilities.h"
#include "../modules/http-server.h"
#include "../spider/spider-manager.h"

static Facilities::WaitGroup waitGroup(1);

int main (int argc, char* argv[])
{
#if DEBUG
    log_init(LOG_TYPE_CONSOLE, LOG_DEBUG, LOG_ROTATE_FALSE, -1, "/tmp/", LOG_TAG, "log");
#else
    log_init(LOG_TYPE_FILE, LOG_INFO, LOG_ROTATE_FALSE, -1, "/tmp/", LOG_TAG, "log");
#endif
    logi("start progress " LOG_TAG "... ");

    SpiderManager::instance()->runAll();

    int port = 8888;
    HttpServer server ([] (HttpTask* task) {
        logv("");
        task->getResp()->appendOutputBody("<html>Hello World!</html>");
    });

    if (0 == server.start(port)) {
        logi("http server 'http://127.0.0.1:%d' is running!", port);
    }

    waitGroup.wait();

    logi("stop progress " LOG_TAG "!");

    return 0;
}
