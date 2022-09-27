//
// Created by dingjing on 9/27/22.
//

#ifndef JARVIS_GOLD_SPIDER_H
#define JARVIS_GOLD_SPIDER_H

#include "spider.h"
#include "../core/c-log.h"

static struct SpiderInfo gGoldSpider =
{
        .interval = 5,
        .spiderName = "gold spider",
        .requestURI = "https://data-asg.goldprice.org/dbXRates/USD,CNY",
        .httpMethod = HTTP_METHOD_GET,
        .rootParser = [] (SpiderContext* ctx) {
            loge("\n%s", ctx->c_str());
            },
};





#endif //JARVIS_GOLD_SPIDER_H
