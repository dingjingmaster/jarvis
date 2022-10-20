//
// Created by dingjing on 10/19/22.
//

#include "sqlite-utils.h"

#include "../common/area.h"
#include "../utils/date-utils.h"

#include <glib.h>

static inline std::tuple<int, int> getMinAndMax ()
{
    auto days = DateUtils::getCurrentPeriodBeforeDate(60);
    int minTime = 0, maxTime = 0;
    for (auto day : days) {
        minTime = minTime == 0 ? day : minTime;
        minTime = MIN(day, minTime);
        maxTime = MAX(day, maxTime);
    }

    return std::move(std::make_tuple(minTime, maxTime));
}

static inline void getD3D7D30AveragePrice (auto rows, GoldDataClient* data)
{
    if (rows.empty() || nullptr == data) {
        return;
    }

    int curMaxTime = 0;
    double d3 = 0, d7 = 0, d30 = 0;
    int id = 0, d3d = 0, d7d = 0, d30d = 0;
    for (auto row : rows) {
        if (curMaxTime <= std::get<2>(row)) {
            data->dateTime = curMaxTime = std::get<2>(row);
            data->price = std::get<0>(row);
        }
        if (id < 3) {
            ++d3d;
            d3 += std::get<0>(row);
        }

        if (id < 7) {
            ++d7d;
            d7 += std::get<0>(row);
        }

        if (id < 30) {
            ++d30d;
            d30 += std::get<0>(row);
        } else {
            break;
        }

        ++id;
    }

    if (d3d > 0)        data->priceAvg3 = d3 / d3d;
    if (d7d > 0)        data->priceAvg7 = d7 / d7d;
    if (d30d > 0)       data->priceAvg30 = d30 / d30d;
}

GoldDataClient SqliteUtils::getCurrentGoldPrice(std::string& area)
{
    GoldDataClient d = {
            .dateTime = 0,
            .itemType = "Au",
            .area = "CN",
            .price = 0,
            .priceAvg3 = 0,
            .priceAvg7 = 0,
            .priceAvg30 = 0,
    };

    if (!g_strv_contains(gAreaString, area.c_str())) {
        return d;
    }

    d.area = area;

    using namespace sqlite_orm;

    auto gold = getGoldStorage();

    int minTime = 0, maxTime = 0;
    std::tie(minTime, maxTime) = getMinAndMax();

    logd("%d -- %d", minTime, maxTime);

    auto rows = gold.select(columns(&GoldData::price, &GoldData::itemType, &GoldData::dateTime),
            where(between(&GoldData::dateTime, minTime, maxTime) && is_equal(&GoldData::itemType, "Au") && is_equal(&GoldData::area, area)), order_by(&GoldData::dateTime).desc());

    getD3D7D30AveragePrice(rows, &d);

    return std::move(d);
}

GoldDataClient SqliteUtils::getCurrentSilverPrice(std::string& area)
{
    GoldDataClient d = {
            .dateTime = 0,
            .itemType = "Ag",
            .area = "CN",
            .price = 0,
            .priceAvg3 = 0,
            .priceAvg7 = 0,
            .priceAvg30 = 0,
    };

    if (!g_strv_contains(gAreaString, area.c_str())) {
        return d;
    }

    d.area = area;

    using namespace sqlite_orm;

    auto gold = getGoldStorage();

    int minTime = 0, maxTime = 0;
    std::tie(minTime, maxTime) = getMinAndMax();

    logd("%d -- %d", minTime, maxTime);

    auto rows = gold.select(columns(&GoldData::price, &GoldData::itemType, &GoldData::dateTime),
            where(between(&GoldData::dateTime, minTime, maxTime) && is_equal(&GoldData::itemType, "Ag") && is_equal(&GoldData::area, area)), order_by(&GoldData::dateTime).desc());

    getD3D7D30AveragePrice(rows, &d);

    return std::move(d);
}
