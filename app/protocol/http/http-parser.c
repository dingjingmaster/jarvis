//
// Created by dingjing on 8/14/22.
//

#include "http-parser.h"

#include <stdlib.h>
#include <string.h>

#include "../../core/c-list.h"

#define MIN(x, y)	((x) <= (y) ? (x) : (y))
#define MAX(x, y)	((x) >= (y) ? (x) : (y))

#define HTTP_START_LINE_MAX		        8192
#define HTTP_HEADER_VALUE_MAX	        8192
#define HTTP_CHUNK_LINE_MAX		        1024
#define HTTP_TRAILER_LINE_MAX	        8192
#define HTTP_MSGBUF_INIT_SIZE	        2048

typedef struct _HeaderLine              HeaderLine;

enum
{
    HPS_START_LINE,
    HPS_HEADER_NAME,
    HPS_HEADER_VALUE,
    HPS_HEADER_COMPLETE
};

enum
{
    CPS_CHUNK_DATA,
    CPS_TRAILER_PART,
    CPS_CHUNK_COMPLETE
};

struct _HeaderLine
{
    struct list_head        list;
    int                     nameLen;
    int                     valueLen;
    char*                   buf;
};

static int _add_message_header(const void *name, size_t name_len, const void *value, size_t value_len, HttpParser *parser)
{
    logv("");
    size_t size = sizeof (HeaderLine) + name_len + value_len + 4;
    HeaderLine *line;

    line = (HeaderLine *)malloc(size);
    if (line) {
        line->buf = (char *)(line + 1);
        memcpy(line->buf, name, name_len);
        line->buf[name_len] = ':';
        line->buf[name_len + 1] = ' ';
        memcpy(line->buf + name_len + 2, value, value_len);
        line->buf[name_len + 2 + value_len] = '\r';
        line->buf[name_len + 2 + value_len + 1] = '\n';
        line->nameLen = name_len;
        line->valueLen = value_len;
        list_add_tail(&line->list, &parser->headerList);
        return 0;
    }

    return -1;
}

static int _set_message_header(const void *name, size_t name_len, const void *value, size_t value_len, HttpParser *parser)
{
    logv("");
    HeaderLine *line;
    struct list_head *pos;
    char *buf;

    list_for_each(pos, &parser->headerList) {
        line = list_entry(pos, HeaderLine, list);
        if (line->nameLen == name_len && strncasecmp(line->buf, name, name_len) == 0) {
            if (value_len > line->valueLen) {
                buf = (char *)malloc(name_len + value_len + 4);
                if (!buf)
                    return -1;

                if (line->buf != (char *)(line + 1))
                    free(line->buf);

                line->buf = buf;
                memcpy(buf, name, name_len);
                buf[name_len] = ':';
                buf[name_len + 1] = ' ';
            }

            memcpy(line->buf + name_len + 2, value, value_len);
            line->buf[name_len + 2 + value_len] = '\r';
            line->buf[name_len + 2 + value_len + 1] = '\n';
            line->valueLen = value_len;
            return 0;
        }
    }

    return _add_message_header(name, name_len, value, value_len, parser);
}

static int _match_request_line(const char *method, const char *uri, const char *version, HttpParser *parser)
{
    logv("");
    if (strcmp(version, "HTTP/1.0") == 0 || strncmp(version, "HTTP/0", 6) == 0)
        parser->keepAlive = 0;

    method = strdup(method);
    if (method) {
        uri = strdup(uri);
        if (uri) {
            version = strdup(version);
            if (version) {
                free(parser->method);
                free(parser->uri);
                free(parser->version);
                parser->method = (char *)method;
                parser->uri = (char *)uri;
                parser->version = (char *)version;
                return 0;
            }

            free((char *)uri);
        }

        free((char *)method);
    }

    return -1;
}

static int _match_status_line(const char *version, const char *code, const char *phrase, HttpParser *parser)
{
    logv("");
    if (strcmp(version, "HTTP/1.0") == 0 || strncmp(version, "HTTP/0", 6) == 0)
        parser->keepAlive = 0;

    if (*code == '1' || strcmp(code, "204") == 0 || strcmp(code, "304") == 0)
        parser->transferLength = 0;

    version = strdup(version);
    if (version) {
        code = strdup(code);
        if (code) {
            phrase = strdup(phrase);
            if (phrase) {
                free(parser->version);
                free(parser->code);
                free(parser->phrase);
                parser->version = (char *)version;
                parser->code = (char *)code;
                parser->phrase = (char *)phrase;
                return 0;
            }

            free((char *)code);
        }

        free((char *)version);
    }

    return -1;
}

static void _check_message_header(const char *name, size_t name_len, const char *value, size_t value_len, HttpParser *parser)
{
    logv("");
    switch (name_len) {
        case 6:
            if (strncasecmp(name, "Expect", 6) == 0) {
                if (value_len == 12 && strncasecmp(value, "100-continue", 12) == 0)
                    parser->expectContinue = 1;
            }
            break;
        case 10:
            if (strncasecmp(name, "Connection", 10) == 0) {
                parser->hasConnection = 1;
                if (value_len == 10 && strncasecmp(value, "Keep-Alive", 10) == 0)
                    parser->keepAlive = 1;
                else if (value_len == 5 && strncasecmp(value, "close", 5) == 0)
                    parser->keepAlive = 0;
            } else if (strncasecmp(name, "Keep-Alive", 10) == 0)
                parser->hasKeepAlive = 1;

            break;

        case 14:
            if (strncasecmp(name, "Content-Length", 14) == 0) {
                parser->hasContentLength = 1;
                if (*value >= '0' && *value <= '9' && value_len <= 15) {
                    char buf[16];
                    memcpy(buf, value, value_len);
                    buf[value_len] = '\0';
                    parser->contentLength = atol(buf);
                }
            }

            break;

        case 17:
            if (strncasecmp(name, "Transfer-Encoding", 17) == 0) {
                if (value_len != 8 || strncasecmp(value, "identity", 8) != 0)
                    parser->chunked = 1;
                else
                    parser->chunked = 0;
            }

            break;
    }
}

static int _parse_start_line(const char *ptr, size_t len, HttpParser *parser)
{
    logv("");
    char start_line[HTTP_START_LINE_MAX];
    size_t min = MIN(HTTP_START_LINE_MAX, len);
    char *p1, *p2, *p3;
    size_t i;
    int ret;

    if (len >= 2 && ptr[0] == '\r' && ptr[1] == '\n') {
        parser->headerOffset += 2;
        return 1;
    }

    for (i = 0; i < min; i++) {
        start_line[i] = ptr[i];
        if (start_line[i] == '\r') {
            if (i == len - 1)
                return 0;

            if (ptr[i + 1] != '\n')
                return -2;

            start_line[i] = '\0';
            p1 = start_line;
            p2 = strchr(p1, ' ');
            if (p2)
                *p2++ = '\0';
            else
                return -2;

            p3 = strchr(p2, ' ');
            if (p3)
                *p3++ = '\0';
            else
                return -2;

            if (parser->isResp)
                ret = _match_status_line(p1, p2, p3, parser);
            else
                ret = _match_request_line(p1, p2, p3, parser);

            if (ret < 0)
                return -1;

            parser->headerOffset += i + 2;
            parser->headerState = HPS_HEADER_NAME;
            return 1;
        }

        if (start_line[i] == 0)
            return -2;
    }

    if (i == HTTP_START_LINE_MAX)
        return -2;

    return 0;
}

static int _parse_header_name(const char *ptr, size_t len, HttpParser *parser)
{
    logv("");
    size_t min = MIN(HTTP_HEADER_NAME_MAX, len);
    size_t i;

    if (len >= 2 && ptr[0] == '\r' && ptr[1] == '\n'){
        parser->headerOffset += 2;
        parser->headerState = HPS_HEADER_COMPLETE;
        return 1;
    }

    for (i = 0; i < min; i++) {
        if (ptr[i] == ':') {
            parser->nameBuf[i] = '\0';
            parser->headerOffset += i + 1;
            parser->headerState = HPS_HEADER_VALUE;
            return 1;
        }

        if ((signed char)ptr[i] <= 0)
            return -2;

        parser->nameBuf[i] = ptr[i];
    }

    if (i == HTTP_HEADER_NAME_MAX)
        return -2;

    return 0;
}

static int _parse_header_value(const char *ptr, size_t len, HttpParser *parser)
{
    logv("");
    char header_value[HTTP_HEADER_VALUE_MAX];
    const char *end = ptr + len;
    const char *begin = ptr;
    size_t i = 0;

    while (1) {
        while (1) {
            if (ptr == end)
                return 0;

            if (*ptr == ' ' || *ptr == '\t')
                ptr++;
            else
                break;
        }

        while (1) {
            if (i == HTTP_HEADER_VALUE_MAX)
                return -2;

            header_value[i] = *ptr++;
            if (ptr == end)
                return 0;

            if (header_value[i] == '\r')
                break;

            if ((signed char)header_value[i] <= 0)
                return -2;

            i++;
        }

        if (*ptr == '\n')
            ptr++;
        else
            return -2;

        if (ptr == end)
            return 0;

        while (i > 0) {
            if (header_value[i - 1] == ' ' || header_value[i - 1] == '\t')
                i--;
            else
                break;
        }

        if (*ptr != ' ' && *ptr != '\t')
            break;

        ptr++;
        header_value[i++] = ' ';
    }

    header_value[i] = '\0';
    if (http_parser_add_header(parser->nameBuf, strlen(parser->nameBuf), header_value, i, parser) < 0)
        return -1;

    parser->headerOffset += ptr - begin;
    parser->headerState = HPS_HEADER_NAME;

    return 1;
}

static int _parse_message_header(const void *message, size_t size, HttpParser *parser)
{
    logv("");
    const char *ptr;
    size_t len;
    int ret;

    do {
        ptr = (const char *)message + parser->headerOffset;
        len = size - parser->headerOffset;
        if (parser->headerState == HPS_START_LINE)
            ret = _parse_start_line(ptr, len, parser);
        else if (parser->headerState == HPS_HEADER_VALUE)
            ret = _parse_header_value(ptr, len, parser);
        else {
            ret = _parse_header_name(ptr, len, parser);
            if (parser->headerState == HPS_HEADER_COMPLETE)
                return 1;
        }
    } while (ret > 0);

    return ret;
}

#define CHUNK_SIZE_MAX		(2 * 1024 * 1024 * 1024U - HTTP_CHUNK_LINE_MAX - 4)

static int _parse_chunk_data(const char *ptr, size_t len, HttpParser *parser)
{
    logv("");
    char chunk_line[HTTP_CHUNK_LINE_MAX];
    size_t min = MIN(HTTP_CHUNK_LINE_MAX, len);
    long chunk_size;
    char *end;
    size_t i;

    for (i = 0; i < min; i++) {
        chunk_line[i] = ptr[i];
        if (chunk_line[i] == '\r') {
            if (i == len - 1)
                return 0;

            if (ptr[i + 1] != '\n')
                return -2;

            chunk_line[i] = '\0';
            chunk_size = strtol(chunk_line, &end, 16);
            if (end == chunk_line)
                return -2;

            if (chunk_size == 0) {
                chunk_size = i + 2;
                parser->chunkState = CPS_TRAILER_PART;
            } else if ((unsigned long)chunk_size < CHUNK_SIZE_MAX) {
                chunk_size += i + 4;
                if (len < (size_t)chunk_size)
                    return 0;
            } else
                return -2;

            parser->chunkOffset += chunk_size;
            return 1;
        }
    }

    if (i == HTTP_CHUNK_LINE_MAX)
        return -2;

    return 0;
}

static int _parse_trailer_part(const char *ptr, size_t len, HttpParser *parser)
{
    logv("");
    size_t min = MIN(HTTP_TRAILER_LINE_MAX, len);
    size_t i;

    for (i = 0; i < min; i++) {
        if (ptr[i] == '\r') {
            if (i == len - 1)
                return 0;

            if (ptr[i + 1] != '\n')
                return -2;

            parser->chunkOffset += i + 2;
            if (i == 0)
                parser->chunkState = CPS_CHUNK_COMPLETE;

            return 1;
        }
    }

    if (i == HTTP_TRAILER_LINE_MAX)
        return -2;

    return 0;
}

static int _parse_chunk(const void *message, size_t size, HttpParser *parser)
{
    logv("");
    const char *ptr;
    size_t len;
    int ret;

    do {
        ptr = (const char *)message + parser->chunkOffset;
        len = size - parser->chunkOffset;
        if (parser->chunkState == CPS_CHUNK_DATA)
            ret = _parse_chunk_data(ptr, len, parser);
        else {
            ret = _parse_trailer_part(ptr, len, parser);
            if (parser->chunkState == CPS_CHUNK_COMPLETE)
                return 1;
        }
    } while (ret > 0);

    return ret;
}

void http_parser_init(int is_resp, HttpParser *parser)
{
    logv("");
    parser->headerState = HPS_START_LINE;
    parser->headerOffset = 0;
    parser->transferLength = (size_t)-1;
    parser->contentLength = is_resp ? (size_t)-1 : 0;
    parser->version = NULL;
    parser->method = NULL;
    parser->uri = NULL;
    parser->code = NULL;
    parser->phrase = NULL;
    INIT_LIST_HEAD(&parser->headerList);
    parser->msgBuf = NULL;
    parser->msgSize = 0;
    parser->bufSize = 0;
    parser->hasConnection = 0;
    parser->hasContentLength = 0;
    parser->hasKeepAlive = 0;
    parser->expectContinue = 0;
    parser->keepAlive = 1;
    parser->chunked = 0;
    parser->complete = 0;
    parser->isResp = is_resp;
}

int http_parser_append_message(const void *buf, size_t *n, HttpParser *parser)
{
    logv("");
    int ret;

    if (parser->complete) {
        *n = 0;
        return 1;
    }

    if (parser->msgSize + *n + 1 > parser->bufSize) {
        size_t new_size = MAX(HTTP_MSGBUF_INIT_SIZE, 2 * parser->bufSize);
        void *new_base;

        while (new_size < parser->msgSize + *n + 1)
            new_size *= 2;

        new_base = realloc(parser->msgBuf, new_size);
        if (!new_base)
            return -1;

        parser->msgBuf = new_base;
        parser->bufSize = new_size;
    }

    memcpy((char *)parser->msgBuf + parser->msgSize, buf, *n);
    parser->msgSize += *n;
    if (parser->headerState != HPS_HEADER_COMPLETE) {
        ret = _parse_message_header(parser->msgBuf, parser->msgSize, parser);
        if (ret <= 0)
            return ret;

        if (parser->chunked) {
            parser->chunkOffset = parser->headerOffset;
            parser->chunkState = CPS_CHUNK_DATA;
        }
        else if (parser->transferLength == (size_t)-1)
            parser->transferLength = parser->contentLength;
    }

    if (parser->transferLength != (size_t)-1) {
        size_t total = parser->headerOffset + parser->transferLength;

        if (parser->msgSize >= total) {
            *n -= parser->msgSize - total;
            parser->msgSize = total;
            parser->complete = 1;
            return 1;
        }

        return 0;
    }

    if (!parser->chunked)
        return 0;

    if (parser->chunkState != CPS_CHUNK_COMPLETE) {
        ret = _parse_chunk(parser->msgBuf, parser->msgSize, parser);
        if (ret <= 0)
            return ret;
    }

    *n -= parser->msgSize - parser->chunkOffset;
    parser->msgSize = parser->chunkOffset;
    parser->complete = 1;
    return 1;
}

int http_parser_header_complete(const HttpParser *parser)
{
    logv("");
    return parser->headerState == HPS_HEADER_COMPLETE;
}

int http_parser_get_body(const void **body, size_t *size, const HttpParser *parser)
{
    logv("");
    if (parser->complete && parser->headerState == HPS_HEADER_COMPLETE) {
        *body = (char *)parser->msgBuf + parser->headerOffset;
        *size = parser->msgSize - parser->headerOffset;
        ((char *)parser->msgBuf)[parser->msgSize] = '\0';
        return 0;
    }

    return 1;
}

int http_parser_set_method(const char *method, HttpParser *parser)
{
    logv("");
    method = strdup(method);
    if (method) {
        free(parser->method);
        parser->method = (char *)method;
        return 0;
    }

    return -1;
}

int http_parser_set_uri(const char *uri, HttpParser *parser)
{
    logv("");
    uri = strdup(uri);
    if (uri) {
        free(parser->uri);
        parser->uri = (char *)uri;
        return 0;
    }

    return -1;
}

int http_parser_set_version(const char *version, HttpParser *parser)
{
    logv("");
    version = strdup(version);
    if (version) {
        free(parser->version);
        parser->version = (char *)version;
        return 0;
    }

    return -1;
}

int http_parser_set_code(const char *code, HttpParser *parser)
{
    logv("");
    code = strdup(code);
    if (code) {
        free(parser->code);
        parser->code = (char *)code;
        return 0;
    }

    return -1;
}

int http_parser_set_phrase(const char *phrase, HttpParser *parser)
{
    logv("");
    phrase = strdup(phrase);
    if (phrase) {
        free(parser->phrase);
        parser->phrase = (char *)phrase;
        return 0;
    }

    return -1;
}

int http_parser_add_header(const void *name, size_t name_len, const void *value, size_t value_len, HttpParser *parser)
{
    logv("");
    if (_add_message_header(name, name_len, value, value_len, parser) >= 0) {
        _check_message_header((const char *)name, name_len, (const char *)value, value_len, parser);
        return 0;
    }

    return -1;
}

int http_parser_set_header(const void *name, size_t name_len, const void *value, size_t value_len, HttpParser *parser)
{
    logv("");
    if (_set_message_header(name, name_len, value, value_len, parser) >= 0) {
        _check_message_header((const char *)name, name_len, (const char *)value, value_len, parser);
        return 0;
    }

    return -1;
}

void http_parser_deinit(HttpParser *parser)
{
    logv("");
    HeaderLine *line;
    struct list_head *pos, *tmp;

    list_for_each_safe(pos, tmp, &parser->headerList) {
        line = list_entry(pos, HeaderLine, list);
        list_del(pos);
        if (line->buf != (char *)(line + 1))
            free(line->buf);

        free(line);
    }

    free(parser->version);
    free(parser->method);
    free(parser->uri);
    free(parser->code);
    free(parser->phrase);
    free(parser->msgBuf);
}

int http_header_cursor_next(const void **name, size_t *name_len, const void **value, size_t *value_len, HttpHeaderCursor *cursor)
{
    logv("");
    HeaderLine *line;

    if (cursor->next->next != cursor->head) {
        cursor->next = cursor->next->next;
        line = list_entry(cursor->next, HeaderLine, list);
        *name = line->buf;
        *name_len = line->nameLen;
        *value = line->buf + line->nameLen + 2;
        *value_len = line->valueLen;
        return 0;
    }

    return 1;
}

int http_header_cursor_find(const void *name, size_t name_len, const void **value, size_t *value_len, HttpHeaderCursor *cursor)
{
    logv("");
    HeaderLine *line;

    while (cursor->next->next != cursor->head) {
        cursor->next = cursor->next->next;
        line = list_entry(cursor->next, HeaderLine, list);
        if (line->nameLen == name_len) {
            if (strncasecmp(line->buf, name, name_len) == 0) {
                *value = line->buf + name_len + 2;
                *value_len = line->valueLen;
                return 0;
            }
        }
    }

    return 1;
}
