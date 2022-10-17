//
// Created by dingjing on 9/22/22.
//

#include "../app/spider/spider.h"
#include "../app/manager/facilities.h"
#include "../app/factory/task-factory.h"

int main (int argc, char* argv[])
{
    log_init(LOG_TYPE_CONSOLE, LOG_DEBUG, LOG_ROTATE_FALSE, -1, "/tmp/", LOG_TAG, "log");

#if 0
    Spider* sp = TaskFactory::createSpider("demo", "http://www.baidu.com", [] (SpiderContext* ctx) {
        loge("http:\n %s\n", ctx->c_str());
    });
#endif
    Spider* sp = TaskFactory::createSpider("international gold price", "https://data-asg.goldprice.org/dbXRates/USD,CNY", [=] (Spider* ctx) {
        logi("http:\n %s\n", ctx->getContent().c_str());
    });

    sp->run();


    return 0;
}
