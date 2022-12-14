//
// Created by dingjing on 10/16/22.
//

#include <string.h>
#include "../app/spider/gold-spider.h"

/**
    std::string     idx;            // 主键: dateTime+itemType+area
    int             dateTime;       // 时间: 20110101 主键
    std::string     itemType;       // 类型: Au/Ag
    std::string     area;           // 地区: US/CN/UK
    double          price;          // 价格: 美国是美元、中国是人民币
 */

int main (int argc, char* argv[])
{
    if (argc != 6) {
        printf("Usage: gold-tool option(u/i) date(20220909) type(Au/Ag) area(US/CN/UK) price(1000.00)\n");
        printf("\toption: u(update price); i(insert price)\n");
        exit(1);
    }

    if (!strcmp(argv[3], "u") && !strcmp(argv[3], "i")) {
        printf("option error\n");
        exit(1);
    }

    if (8 != strlen(argv[2]) && !atoi(argv[2])) {
        printf("date error\n");
        exit(1);
    }

    if (!strcmp(argv[3], "Au") && !strcmp(argv[3], "Ag")) {
        printf("type error\n");
        exit(1);
    }

    if (!strcmp(argv[4], "US") && !strcmp(argv[4], "CN") && !strcmp(argv[4], "UK")) {
        printf("area error\n");
        exit(1);
    }

    if (strlen(argv[5]) <= 0 || !atof(argv[5])) {
        printf("price error\n");
        exit(1);
    }

    char idx[1024] = {0};
    snprintf(idx, sizeof idx, "%s-%s-%s", argv[2], argv[3], argv[4]);
    GoldData data = {
            .idx = idx,
            .dateTime = atoi(argv[2]),
            .itemType = argv[3],
            .area = argv[4],
            .price = atof(argv[5])
    };

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
    if (!strcasecmp(argv[1], "u")) {
        gold.replace(data);
    } else if (!strcasecmp(argv[1], "i")) {
        auto rows = gold.select(columns(&GoldData::idx), where(is_equal(&GoldData::idx, data.idx)));
        if (rows.empty()) {
            gold.replace(data);
        }
    }
    sqlite_unlock();

    return 0;
}
