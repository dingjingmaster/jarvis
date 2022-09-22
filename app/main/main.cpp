//
// Created by dingjing on 8/26/22.
//

#include "../core/c-log.h"
#include "../modules/http-server.h"

int main (int argc, char* argv[])
{
#if DEBUG
    log_init(LOG_TYPE_FILE, LOG_VERB, LOG_ROTATE_FALSE, -1, "/tmp/", LOG_TAG, "log");
#else
    log_init(LOG_TYPE_FILE, LOG_INFO, LOG_ROTATE_FALSE, -1, "/tmp/", LOG_TAG, "log");
#endif

    logi("start progress " LOG_TAG "... ");
    HttpServer server ([] (HttpTask* task) {
        logv("");
        task->getResp()->appendOutputBody("<html>Hello World!</html>");
    });

    if (0 == server.start(8888)) {
        getchar();
        server.stop();
    }
    logi("stop progress " LOG_TAG "!");

    return 0;
}
