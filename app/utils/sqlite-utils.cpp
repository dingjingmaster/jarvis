//
// Created by dingjing on 10/19/22.
//

#include "sqlite-utils.h"

#include "../utils/date-utils.h"

GoldData SqliteUtils::getCurrentGoldPrice()
{
    using namespace sqlite_orm;

    auto gold = getGoldStorage();

    int curDate = DateUtils::getCurrentDate1();

    loge("%d", curDate);

    auto rows = gold.select(columns(&GoldData::price, &GoldData::itemType, &GoldData::dateTime),
            where(is_equal(&GoldData::dateTime, curDate) && is_equal(&GoldData::itemType, "Au") && is_equal(&GoldData::area, "CNY")));
    if (!rows.empty()) {
        auto it = rows[0];

        GoldData d = {
                .idx = "",
                .dateTime = std::get<2>(it),
                .itemType = std::get<1>(it),
                .area = "CNY",
                .price = std::get<0>(it),
        };

        return std::move(d);
    }

    loge("empty");
    return GoldData();
}

GoldData SqliteUtils::getCurrentSilverPrice()
{
    using namespace sqlite_orm;

    auto gold = getGoldStorage();

    int curDate = DateUtils::getCurrentDate1();

    loge("%d", curDate);

    auto rows = gold.select(columns(&GoldData::price, &GoldData::itemType, &GoldData::dateTime),
            where(is_equal(&GoldData::dateTime, curDate) && is_equal(&GoldData::itemType, "Ag") && is_equal(&GoldData::area, "CNY")));
    if (!rows.empty()) {
        auto it = rows[0];

        GoldData d = {
                .idx = "",
                .dateTime = std::get<2>(it),
                .itemType = std::get<1>(it),
                .area = "CNY",
                .price = std::get<0>(it),
        };

        return std::move(d);
    }

    loge("empty");

    return GoldData();
}
