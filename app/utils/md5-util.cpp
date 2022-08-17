//
// Created by dingjing on 8/17/22.
//

#include "md5-util.h"

#include <string>
#include <string.h>
#include <openssl/md5.h>

static inline void _md5(const std::string& str, unsigned char *md)
{
    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, str.c_str(), str.size());
    MD5_Final(md, &ctx);
}

std::string MD5Util::md5Bin(const std::string& str)
{
    unsigned char md[16];

    _md5(str, md);
    return std::string((const char *)md, 16);
}

static inline char _hex_char(int v)
{
    return v < 10 ? '0' + v : 'a' + v - 10;
}

static inline void _plain_hex(char *s, int ch)
{
    *s = _hex_char(ch / 16);
    *(s + 1) = _hex_char(ch % 16);
}

std::string MD5Util::md5String32(const std::string& str)
{
    unsigned char md[16];
    char out[32];

    _md5(str, md);
    for (int i = 0; i < 16; i++)
        _plain_hex(out + (i * 2), md[i]);

    return std::string((const char *)out, 32);
}

std::string MD5Util::md5String16(const std::string& str)
{
    unsigned char md[16];
    char out[16];

    _md5(str, md);
    for (int i = 0; i < 8; i++)
        _plain_hex(out + (i * 2), md[i + 4]);

    return std::string((const char *)out, 16);
}

std::pair<uint64_t, uint64_t> MD5Util::md5Integer32(const std::string& str)
{
    unsigned char md[16];
    std::pair<uint64_t, uint64_t> res;

    _md5(str, md);
    memcpy(&res.first, md, sizeof (uint64_t));
    memcpy(&res.second, md + 8, sizeof (uint64_t));
    return res;
}

uint64_t MD5Util::md5Integer16(const std::string& str)
{
    unsigned char md[16];
    uint64_t res;

    _md5(str, md);
    memcpy(&res, md + 4, sizeof (uint64_t));
    return res;
}

