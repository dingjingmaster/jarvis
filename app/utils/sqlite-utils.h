//
// Created by dingjing on 10/19/22.
//

#ifndef JARVIS_SQLITEUTILS_H
#define JARVIS_SQLITEUTILS_H

#include "../spider/gold-spider.h"

class SqliteUtils
{
public:
    static GoldData getCurrentGoldPrice();
    static GoldData getCurrentSilverPrice();

};


#endif //JARVIS_SQLITEUTILS_H
