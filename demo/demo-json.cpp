//
// Created by dingjing on 9/27/22.
//

#include <iostream>

#include <nlohmann/json.hpp>

#include <ctime>

using json = nlohmann::json;

int main ()
{
    const char* jsonStr = "{\"ts\":1664269593867,\"tsj\":1664269586147,\"date\":\"Sep 27th 2022, 05:06:26 am NY\",\"items\":"
                       "[{\"curr\":\"CNY\","
                       "    \"xauPrice\":11725.3723,"
                       "    \"xagPrice\":133.363,"
                       "    \"chgXau\":96.3891,"
                       "    \"chgXag\":1.3052,"
                       "    \"pcXau\":0.8289,"
                       "    \"pcXag\":0.9884,"
                       "    \"xauClose\":11628.98322,"
                       "    \"xagClose\":132.05782"
                       "},"
                       "{"
                       "    \"curr\":\"USD\","
                       "    \"xauPrice\":1636.205,"
                       "    \"xagPrice\":18.61,"
                       "    \"chgXau\":8.245,"
                       "    \"chgXag\":0.172,"
                       "    \"pcXau\":0.5065,"
                       "    \"pcXag\":0.9329,"
                       "    \"xauClose\":1627.96,"
                       "    \"xagClose\":18.438"
                       "}"
                       "]}";

    json js = json::parse (jsonStr);
    std::cout << std::setw(4) << js << std::endl;

    std::cout << std::setw(4) << js["items"] << std::endl;
    std::cout << std::setw(4) << js["ts"] << std::endl;

    time_t tim = js["ts"];
    tim /= 1000;

    std::cout << tim << std::endl;

    struct tm* ltm = localtime(&tim);

    std::cout << "年: "<< 1900 + ltm->tm_year << std::endl;
    std::cout << "月: "<< 1 + ltm->tm_mon<< std::endl;
    std::cout << "日: "<<  ltm->tm_mday << std::endl;
    std::cout << "时间: "<< ltm->tm_hour << ":" ;
    std::cout << ltm->tm_min << ":" ;
    std::cout << ltm->tm_sec << std::endl;

    char buf[32] = {0};

    strftime(buf, sizeof buf, "%Y%m%d", ltm);
    std::cout << buf << std::endl;
}