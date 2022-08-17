//
// Created by dingjing on 8/14/22.
//

#ifndef JARVIS_HTTP_PARSER_H
#define JARVIS_HTTP_PARSER_H
#include <stddef.h>
#include "../../core/c-list.h"

#define HTTP_HEADER_NAME_MAX            64

typedef struct _HttpParser              HttpParser;
typedef struct _HttpHeaderCursor        HttpHeaderCursor;

struct _HttpParser
{
    int                         headerState;
    int                         chunkState;
    size_t                      headerOffset;
    size_t                      chunkOffset;
    size_t                      contentLength;
    size_t                      transferLength;
    char*                       version;
    char*                       method;
    char*                       uri;
    char*                       code;
    char*                       phrase;
    struct list_head            headerList;
    char                        nameBuf[HTTP_HEADER_NAME_MAX];
    void*                       msgBuf;
    size_t                      msgSize;
    size_t                      bufSize;
    char                        hasConnection;
    char                        hasContentLength;
    char                        hasKeepAlive;
    char                        expectContinue;
    char                        keepAlive;
    char                        chunked;
    char                        complete;
    char                        isResp;
};

struct _HttpHeaderCursor
{
    const struct list_head*     head;
    const struct list_head*     next;
};

#ifdef __cplusplus
extern "C"
{
#endif

void http_parser_init(int is_resp, HttpParser *parser);

int http_parser_append_message(const void *buf, size_t *n, HttpParser *parser);

int http_parser_get_body(const void **body, size_t *size, const HttpParser *parser);

int http_parser_header_complete(const HttpParser *parser);

int http_parser_set_method(const char *method, HttpParser *parser);

int http_parser_set_uri(const char *uri, HttpParser *parser);

int http_parser_set_version(const char *version, HttpParser *parser);

int http_parser_set_code(const char *code, HttpParser *parser);

int http_parser_set_phrase(const char *phrase, HttpParser *parser);

int http_parser_add_header(const void *name, size_t name_len, const void *value, size_t value_len, HttpParser *parser);

int http_parser_set_header(const void *name, size_t name_len, const void *value, size_t value_len, HttpParser *parser);

void http_parser_deinit(HttpParser *parser);

int http_header_cursor_next(const void **name, size_t *name_len, const void **value, size_t *value_len, HttpHeaderCursor *cursor);

int http_header_cursor_find(const void *name, size_t name_len, const void **value, size_t *value_len,HttpHeaderCursor *cursor);

#ifdef __cplusplus
}
#endif

static inline const char *http_parser_get_method(const HttpParser *parser)
{
    return parser->method;
}

static inline const char *http_parser_get_uri(const HttpParser *parser)
{
    return parser->uri;
}

static inline const char *http_parser_get_version(const HttpParser *parser)
{
    return parser->version;
}

static inline const char *http_parser_get_code(const HttpParser *parser)
{
    return parser->code;
}

static inline const char *http_parser_get_phrase(const HttpParser *parser)
{
    return parser->phrase;
}

static inline int http_parser_chunked(const HttpParser *parser)
{
    return parser->chunked;
}

static inline int http_parser_keep_alive(const HttpParser *parser)
{
    return parser->keepAlive;
}

static inline int http_parser_has_connection(const HttpParser *parser)
{
    return parser->hasConnection;
}

static inline int http_parser_has_content_length(const HttpParser *parser)
{
    return parser->hasContentLength;
}

static inline int http_parser_has_keep_alive(const HttpParser *parser)
{
    return parser->hasKeepAlive;
}

static inline void http_parser_close_message(HttpParser *parser)
{
    parser->complete = 1;
}

static inline void http_header_cursor_init(HttpHeaderCursor *cursor, const HttpParser *parser)
{
    cursor->head = &parser->headerList;
    cursor->next = cursor->head;
}

static inline void http_header_cursor_rewind(HttpHeaderCursor *cursor)
{
    cursor->next = cursor->head;
}

static inline void http_header_cursor_deinit(HttpHeaderCursor *cursor)
{
}

#endif //JARVIS_HTTP_PARSER_H
