//
// Created by dingjing on 8/17/22.
//

#include <algorithm>
#include "http-util.h"

protocol::HttpHeaderCursor::HttpHeaderCursor(const protocol::HttpMessage *msg)
{
    http_header_cursor_init(&mCursor, msg->getParser());
}

bool protocol::HttpHeaderCursor::find(const std::string &name, std::string &value)
{
    HttpMessageHeader header = {
            .name       =   name.c_str(),
            .nameLen    =   name.size()
    };

    if (this->find(&header)) {
        value.assign((const char *)header.value, header.valueLen);
        return true;
    }

    return false;
}

bool protocol::HttpHeaderCursor::find(protocol::HttpMessageHeader *header)
{
    return http_header_cursor_find(header->name, header->nameLen, &header->value, &header->valueLen, &mCursor) == 0;
}

bool protocol::HttpHeaderCursor::next(protocol::HttpMessageHeader *header)
{
    return http_header_cursor_next(&header->name, &header->nameLen, &header->value, &header->valueLen, &mCursor) == 0;
}

void protocol::HttpHeaderCursor::rewind()
{
    http_header_cursor_rewind(&mCursor);
}

protocol::HttpHeaderCursor::~HttpHeaderCursor()
{
    http_header_cursor_deinit(&mCursor);
}

bool protocol::HttpHeaderCursor::next(std::string &name, std::string &value)
{
    HttpMessageHeader header;

    if (next(&header)) {
        name.assign((const char *)header.name, header.nameLen);
        value.assign((const char *)header.value, header.valueLen);
        return true;
    }

    return false;
}

void protocol::HttpUtil::setResponseStatus(protocol::HttpResponse *resp, int statusCode)
{
    char buf[32] = {0};
    sprintf(buf, "%d", statusCode);
    resp->setStatusCode(buf);

    switch (statusCode) {
        case 100:
            resp->setReasonPhrase("Continue");
            break;

        case 101:
            resp->setReasonPhrase("Switching Protocols");
            break;

        case 102:
            resp->setReasonPhrase("Processing");
            break;

        case 200:
            resp->setReasonPhrase("OK");
            break;

        case 201:
            resp->setReasonPhrase("Created");
            break;

        case 202:
            resp->setReasonPhrase("Accepted");
            break;

        case 203:
            resp->setReasonPhrase("Non-Authoritative Information");
            break;

        case 204:
            resp->setReasonPhrase("No Content");
            break;

        case 205:
            resp->setReasonPhrase("Reset Content");
            break;

        case 206:
            resp->setReasonPhrase("Partial Content");
            break;

        case 207:
            resp->setReasonPhrase("Multi-Status");
            break;

        case 208:
            resp->setReasonPhrase("Already Reported");
            break;

        case 226:
            resp->setReasonPhrase("IM Used");
            break;

        case 300:
            resp->setReasonPhrase("Multiple Choices");
            break;

        case 301:
            resp->setReasonPhrase("Moved Permanently");
            break;

        case 302:
            resp->setReasonPhrase("Found");
            break;

        case 303:
            resp->setReasonPhrase("See Other");
            break;

        case 304:
            resp->setReasonPhrase("Not Modified");
            break;

        case 305:
            resp->setReasonPhrase("Use Proxy");
            break;

        case 306:
            resp->setReasonPhrase("Switch Proxy");
            break;

        case 307:
            resp->setReasonPhrase("Temporary Redirect");
            break;

        case 308:
            resp->setReasonPhrase("Permanent Redirect");
            break;

        case 400:
            resp->setReasonPhrase("Bad Request");
            break;

        case 401:
            resp->setReasonPhrase("Unauthorized");
            break;

        case 402:
            resp->setReasonPhrase("Payment Required");
            break;

        case 403:
            resp->setReasonPhrase("Forbidden");
            break;

        case 404:
            resp->setReasonPhrase("Not Found");
            break;

        case 405:
            resp->setReasonPhrase("Method Not Allowed");
            break;

        case 406:
            resp->setReasonPhrase("Not Acceptable");
            break;

        case 407:
            resp->setReasonPhrase("Proxy Authentication Required");
            break;

        case 408:
            resp->setReasonPhrase("Request Timeout");
            break;

        case 409:
            resp->setReasonPhrase("Conflict");
            break;

        case 410:
            resp->setReasonPhrase("Gone");
            break;

        case 411:
            resp->setReasonPhrase("Length Required");
            break;

        case 412:
            resp->setReasonPhrase("Precondition Failed");
            break;

        case 413:
            resp->setReasonPhrase("Request Entity Too Large");
            break;

        case 414:
            resp->setReasonPhrase("Request-URI Too Long");
            break;

        case 415:
            resp->setReasonPhrase("Unsupported Media Type");
            break;

        case 416:
            resp->setReasonPhrase("Requested Range Not Satisfiable");
            break;

        case 417:
            resp->setReasonPhrase("Expectation Failed");
            break;

        case 418:
            resp->setReasonPhrase("I'm a teapot");
            break;

        case 420:
            resp->setReasonPhrase("Enhance Your Caim");
            break;

        case 421:
            resp->setReasonPhrase("Misdirected Request");
            break;

        case 422:
            resp->setReasonPhrase("Unprocessable Entity");
            break;

        case 423:
            resp->setReasonPhrase("Locked");
            break;

        case 424:
            resp->setReasonPhrase("Failed Dependency");
            break;

        case 425:
            resp->setReasonPhrase("Too Early");
            break;

        case 426:
            resp->setReasonPhrase("Upgrade Required");
            break;

        case 428:
            resp->setReasonPhrase("Precondition Required");
            break;

        case 429:
            resp->setReasonPhrase("Too Many Requests");
            break;

        case 431:
            resp->setReasonPhrase("Request Header Fields Too Large");
            break;

        case 444:
            resp->setReasonPhrase("No Response");
            break;

        case 450:
            resp->setReasonPhrase("Blocked by Windows Parental Controls");
            break;

        case 451:
            resp->setReasonPhrase("Unavailable For Legal Reasons");
            break;

        case 494:
            resp->setReasonPhrase("Request Header Too Large");
            break;

        case 500:
            resp->setReasonPhrase("Internal Server Error");
            break;

        case 501:
            resp->setReasonPhrase("Not Implemented");
            break;

        case 502:
            resp->setReasonPhrase("Bad Gateway");
            break;

        case 503:
            resp->setReasonPhrase("Service Unavailable");
            break;

        case 504:
            resp->setReasonPhrase("Gateway Timeout");
            break;

        case 505:
            resp->setReasonPhrase("HTTP Version Not Supported");
            break;

        case 506:
            resp->setReasonPhrase("Variant Also Negotiates");
            break;

        case 507:
            resp->setReasonPhrase("Insufficient Storage");
            break;

        case 508:
            resp->setReasonPhrase("Loop Detected");
            break;

        case 510:
            resp->setReasonPhrase("Not Extended");
            break;

        case 511:
            resp->setReasonPhrase("Network Authentication Required");
            break;

        default:
            resp->setReasonPhrase("Unknown");
            break;
    }
}

std::string protocol::HttpUtil::decodeChunkedBody(const protocol::HttpMessage *msg)
{
    const void*         body;
    size_t              bodyLen;
    const void*         chunk;
    size_t              chunkSize;
    std::string         decodeResult;
    HttpChunkCursor     cursor(msg);

    if (msg->getParsedBody(&body, &bodyLen)) {
        decodeResult.reserve(bodyLen);
        while (cursor.next(&chunk, &chunkSize)) {
            decodeResult.append((const char *)chunk, chunkSize);
        }
    }

    return decodeResult;
}

bool protocol::HttpHeaderMap::get(std::string key, std::string &value)
{
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    const auto it = mHeaderMap.find(key);

    if (it == mHeaderMap.end() || it->second.empty())
        return false;

    value = it->second[0];

    return true;
}

protocol::HttpHeaderMap::HttpHeaderMap(const protocol::HttpMessage* message)
{
    ::HttpHeaderCursor      cursor;
    HttpMessageHeader       header;

    http_header_cursor_init(&cursor, message->getParser());
    while (http_header_cursor_next(&header.name, &header.nameLen, &header.value, &header.valueLen, &cursor) == 0) {
        std::string key((const char *)header.name, header.nameLen);
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        mHeaderMap[key].emplace_back((const char *)header.value, header.valueLen);
    }

    http_header_cursor_deinit(&cursor);
}

bool protocol::HttpHeaderMap::keyExists(std::string key)
{
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);

    return mHeaderMap.count(key) > 0;
}

std::string protocol::HttpHeaderMap::get(std::string key)
{
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    const auto it = mHeaderMap.find(key);

    if (it == mHeaderMap.end() || it->second.empty())
        return std::string();

    return it->second[0];
}

std::vector<std::string> protocol::HttpHeaderMap::getStrict(std::string key)
{
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);

    return mHeaderMap[key];
}

bool protocol::HttpHeaderMap::getStrict(std::string key, std::vector<std::string> &values)
{
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    const auto it = mHeaderMap.find(key);

    if (it == mHeaderMap.end() || it->second.empty())
        return false;

    values = it->second;

    return true;
}

protocol::HttpChunkCursor::HttpChunkCursor(const protocol::HttpMessage *msg)
{
    if (msg->getParsedBody(&mBody, &mBodyLen)) {
        mPos = mBody;
        mChunked = msg->isChunked();
        mEnd = false;
    } else {
        mBody = NULL;
        mEnd = true;
    }
}

bool protocol::HttpChunkCursor::next(const void **chunk, size_t *size)
{
    if (mEnd)
        return false;

    if (!mChunked) {
        *chunk = mBody;
        *size = mBodyLen;
        mEnd = true;
        return true;
    }

    const char *cur = (const char*)mPos;
    char *end;

    *size = strtol(cur, &end, 16);
    if (*size == 0) {
        mEnd = true;
        return false;
    }

    cur = strchr(end, '\r');
    *chunk = cur + 2;
    cur += *size + 4;
    mPos = cur;

    return true;
}

void protocol::HttpChunkCursor::rewind()
{
    if (mBody) {
        mPos = mBody;
        mEnd = false;
    }
}
