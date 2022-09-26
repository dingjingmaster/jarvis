//
// Created by dingjing on 9/22/22.
//

#include "../app/manager/facilities.h"
#include "../app/factory/task-factory.h"

int main (int argc, char* argv[])
{
    log_init(LOG_TYPE_CONSOLE, LOG_DEBUG, LOG_ROTATE_FALSE, -1, "/tmp/", LOG_TAG, "log");

    Spider* sp = TaskFactory::createSpider("demo", "http://www.baidu.com", [] (SpiderContext* ctx) {
        loge("http:\n %s\n", ctx->c_str());
    });

    sp->run();

    return 0;
}