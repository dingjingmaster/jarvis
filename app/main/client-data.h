//
// Created by dingjing on 10/19/22.
//

#ifndef JARVIS_CLIENT_DATA_H
#define JARVIS_CLIENT_DATA_H

#include <string>

struct GoldDataClient
{
    int             dateTime;       // 20220102
    std::string     itemType;       // Au/Ag
    std::string     area;           // CNY
    double          price;          //
    double          priceAvg3;      //
    double          priceAvg7;      //
    double          priceAvg30;     //
};

#endif //JARVIS_CLIENT_DATA_H
