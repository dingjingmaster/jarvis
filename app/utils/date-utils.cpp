//
// Created by dingjing on 10/19/22.
//

#include "date-utils.h"

#include "../common/c-log.h"

#include <glib.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib/gprintf.h>

int DateUtils::getCurrentDate()
{
    time_t now = time(NULL);

    tm* ltm = localtime(&now);

    char buf[12] = {0};
    snprintf(buf, sizeof(buf) - 1, "%04d%02d%02d", 1900 + ltm->tm_year, 1 + ltm->tm_mon, ltm->tm_mday);

    logd("%s", buf);

    return atoi(buf);
}

std::list<int> DateUtils::getCurrentPeriodBeforeDate(int days)
{
    if (days <= 0) {
        return std::list<int>();
    }

    g_autoptr(GDateTime) nowTmp = g_date_time_new_now_local();
    g_autoptr(GDateTime) now = g_date_time_add_days(nowTmp, -1);

    std::list<int> ret{};

    for (int i = 0; i < days; ++i) {
        g_autoptr(GDateTime) t = g_date_time_add_days(now, -i);
        char buf[12] = {0};
        snprintf(buf, sizeof(buf) - 1, "%04d%02d%02d",
                g_date_time_get_year(t),
                g_date_time_get_month(t),
                g_date_time_get_day_of_month(t));
        ret.push_back(atoi(buf));
    }

    return ret;
}
