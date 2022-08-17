//
// Created by dingjing on 8/17/22.
//

#ifndef JARVIS_MD5_UTIL_H
#define JARVIS_MD5_UTIL_H
#include <string>
#include <utility>
#include <stdint.h>

class MD5Util
{
public:
    //128 bit binary data
    static std::string md5Bin(const std::string& str);
    //128 bit hex string style, lower case
    static std::string md5String32(const std::string& str);
    //64  bit hex string style, lower case
    static std::string md5String16(const std::string& str);

    //64  bit integer style
    static uint64_t md5Integer16(const std::string& str);

    //128 bit integer style
    static std::pair<uint64_t, uint64_t> md5Integer32(const std::string& str);
};


#endif //JARVIS_MD5_UTIL_H
