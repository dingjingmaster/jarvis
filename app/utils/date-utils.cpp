//
// Created by dingjing on 10/19/22.
//

#include "date-utils.h"

#include "../core/c-log.h"

#include <time.h>
#include <stdio.h>
#include <stdlib.h>

int DateUtils::getCurrentDate1()
{
    time_t now = time(NULL);

    tm* ltm = localtime(&now);

    char buf[12] = {0};

    snprintf(buf, sizeof(buf) - 1, "%04d%02d%02d", 1900 + ltm->tm_year, 1 + ltm->tm_mon, ltm->tm_mday);

    logd("%s", buf);

    return atoi(buf);
}
