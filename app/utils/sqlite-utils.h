//
// Created by dingjing on 10/19/22.
//

#ifndef JARVIS_SQLITEUTILS_H
#define JARVIS_SQLITEUTILS_H

#include "../main/client-data.h"
#include "../spider/gold-spider.h"


class SqliteUtils
{
public:
    static GoldDataClient getCurrentGoldPrice(std::string& area);
    static GoldDataClient getCurrentSilverPrice(std::string& area);
};


#endif //JARVIS_SQLITEUTILS_H
