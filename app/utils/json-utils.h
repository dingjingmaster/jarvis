//
// Created by dingjing on 10/19/22.
//

#ifndef JARVIS_JSONUTILS_H
#define JARVIS_JSONUTILS_H

#include "../spider/gold-spider.h"

class JsonUtils
{
public:
    enum PriceType
    {
        INDEX_PRICE_TYPE_AU,
        INDEX_PRICE_TYPE_AG,
    };
public:
    static std::string jsonBuildIndexPrice (PriceType type, GoldData& data);

};


#endif //JARVIS_JSONUTILS_H
