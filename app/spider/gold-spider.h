//
// Created by dingjing on 9/27/22.
//

#ifndef JARVIS_GOLD_SPIDER_H
#define JARVIS_GOLD_SPIDER_H

#include <ctime>
#include <nlohmann/json.hpp>
#include <sqlite_orm/sqlite_orm.h>

using json = nlohmann::json;

#include "spider.h"
#include "../core/c-log.h"


struct GoldData
{
    std::string     idx;            // 主键: dateTime+itemType+area
    int             dateTime;       // 时间: 20110101 主键
    std::string     itemType;       // 类型: Au/Ag
    std::string     area;           // 地区: USD/CNY
    double          price;          // 价格: 美国是美元、中国是人民币
};

static inline void save_data (GoldData&);

const float oz = 28.3495231;
static struct SpiderInfo gGoldSpider =
{
        .interval = 600,
        .spiderName = "gold spider",
        .requestURI = "https://data-asg.goldprice.org/dbXRates/CNY,USD",
        .httpMethod = HTTP_METHOD_GET,
        .rootParser = [] (Spider* sp) {
            if (!sp || sp->getContent().empty()) {
                loge("content is empty!");
                return;
            }
            logd("\n%s", sp->getContent().c_str());
            json js = json::parse (sp->getContent().c_str());

            time_t tim = js["ts"];
            tim /= 1000;

            struct tm* ltm = localtime(&tim);
            char buf[12] = {0};
            char idx[1024] = {0};
            strftime(buf, sizeof buf, "%Y%m%d", ltm);

            // 获取的质量单位是 oz，需要转成克
            double auPrice = js["items"][0]["xauPrice"];
            double agPrice = js["items"][0]["xagPrice"];
            std::string area = js["items"][0]["curr"];

            logi("au: %f, ag:%f", auPrice, agPrice);

            snprintf(idx, sizeof idx, "%s-%s-%s", buf, "Au", "CNY");
            GoldData gd {
                .idx = idx,
                .dateTime = atoi(buf),
                .itemType = "au",
                .area = area,
                .price = auPrice / oz
            };
            save_data(gd);

            snprintf(idx, sizeof idx, "%s-%s-%s", buf, "Ag", "CNY");
            gd.idx = idx;
            gd.itemType = "ag";
            gd.area = area;
            gd.price = agPrice / oz;
            save_data(gd);
            },
        .parsers = {},
        .requestHeaders = {}
};


static void save_data (GoldData& sp)
{
    using namespace sqlite_orm;

    auto gold = make_storage(SPIDER_DB,
            make_unique_index("idx_date_type_area", &GoldData::dateTime, &GoldData::itemType, &GoldData::area),
            make_table("gold",
                    make_column("idx", &GoldData::idx, primary_key()),
                    make_column("dateTime", &GoldData::dateTime),
                    make_column("itemType", &GoldData::itemType),
                    make_column("area", &GoldData::area),
                    make_column("price", &GoldData::price)));
    gold.sync_schema();

    sqlite_lock();
    try {
        gold.replace(sp);
    } catch (...) {
        gold.update(sp);
    }

    sqlite_unlock();
}



#endif //JARVIS_GOLD_SPIDER_H
