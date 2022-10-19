//
// Created by dingjing on 10/19/22.
//

#ifndef JARVIS_DATEUTILS_H
#define JARVIS_DATEUTILS_H

#include <list>

class DateUtils
{
public:
    /**
     * @brief 获取当前时间
     * @return 返回值格式 20210202
     */
    static int getCurrentDate();

    /**
     * @brief 获取当前时间之前一段时间的时间戳
     * @return 返回数据格式 list<int>(20210101,20210102,...)
     */
    static std::list<int> getCurrentPeriodBeforeDate(int days);

};


#endif //JARVIS_DATEUTILS_H
