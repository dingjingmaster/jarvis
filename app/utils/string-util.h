//
// Created by dingjing on 8/17/22.
//

#ifndef JARVIS_STRING_UTIL_H
#define JARVIS_STRING_UTIL_H
#include <string>
#include <vector>

class StringUtil
{
public:
    static void urlDecode(std::string& str);
    static size_t urlDecode(char *str, size_t len);

    static std::string urlEncode(const std::string& str);
    static std::string urlEncodeComponent(const std::string& str);

    static std::string strip(const std::string& str);
    static std::vector<std::string> split(const std::string& str, char sep);
    static bool startWith(const std::string& str, const std::string& prefix);

    //this will filter any empty result, so the result vector has no empty string
    static std::vector<std::string> splitFilterEmpty(const std::string& str, char sep);
};


#endif //JARVIS_STRING_UTIL_H
