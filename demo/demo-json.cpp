//
// Created by dingjing on 9/27/22.
//

#include <nlohmann/json.hpp>
#include <iostream>

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
}