//
// Created by dingjing on 8/14/22.
//

#include "http-message.h"

namespace protocol
{
    typedef struct _HttpMessageBlock        HttpMessageBlock;

    struct _HttpMessageBlock
    {
        struct list_head        list;
        const void*             ptr;
        size_t                  size;
    };
}

bool protocol::HttpMessage::appendOutputBody(const void *buf, size_t size)
{
    size_t n = sizeof (HttpMessageBlock) + size;
    HttpMessageBlock *block = (HttpMessageBlock *)malloc(n);

    if (block) {
        memcpy(block + 1, buf, size);
        block->ptr = block + 1;
        block->size = size;
        list_add_tail(&block->list, &mOutputBody);
        mOutputBodySize += size;
        return true;
    }

    return false;
}

bool protocol::HttpMessage::appendOutputBodyNocopy(const void *buf, size_t size)
{
    size_t n = sizeof (HttpMessageBlock);
    HttpMessageBlock *block = (HttpMessageBlock *)malloc(n);

    if (block) {
        block->ptr = buf;
        block->size = size;
        list_add_tail(&block->list, &mOutputBody);
        mOutputBodySize += size;
        return true;
    }

    return false;
}

void protocol::HttpMessage::clearOutputBody()
{
    HttpMessageBlock *block;
    struct list_head *pos, *tmp;

    list_for_each_safe(pos, tmp, &mOutputBody) {
        block = list_entry(pos, HttpMessageBlock, list);
        list_del(pos);
        free(block);
    }

    mOutputBodySize = 0;
}

size_t protocol::HttpMessage::getOutputBodySize() const
{
    return mOutputBodySize;
}

int protocol::HttpMessage::append(const void *buf, size_t *size)
{
    int ret = http_parser_append_message(buf, size, mParser);

    if (ret >= 0) {
        mCurSize += *size;
        if (mCurSize > mSizeLimit) {
            errno = EMSGSIZE;
            ret = -1;
        }
    } else if (ret == -2) {
        errno = EBADMSG;
        ret = -1;
    }

    return ret;
}

int protocol::HttpMessage::encode(struct iovec *vectors, int max)
{
    int                         i;
    const char*                 startLine[3];
    HttpHeaderCursor            cursor;
    HttpMessageHeader           header;
    HttpMessageBlock*           block;
    struct list_head*           pos;
    size_t                      size;

    startLine[0] = http_parser_get_method(mParser);
    if (startLine[0]) {
        startLine[1] = http_parser_get_uri(mParser);
        startLine[2] = http_parser_get_version(mParser);
    } else {
        startLine[0] = http_parser_get_version(mParser);
        startLine[1] = http_parser_get_code(mParser);
        startLine[2] = http_parser_get_phrase(mParser);
    }

    if (!startLine[0] || !startLine[1] || !startLine[2]) {
        errno = EBADMSG;
        return -1;
    }

    vectors[0].iov_base = (void *)startLine[0];
    vectors[0].iov_len = strlen(startLine[0]);
    vectors[1].iov_base = (void *)" ";
    vectors[1].iov_len = 1;

    vectors[2].iov_base = (void *)startLine[1];
    vectors[2].iov_len = strlen(startLine[1]);
    vectors[3].iov_base = (void *)" ";
    vectors[3].iov_len = 1;

    vectors[4].iov_base = (void *)startLine[2];
    vectors[4].iov_len = strlen(startLine[2]);
    vectors[5].iov_base = (void *)"\r\n";
    vectors[5].iov_len = 2;

    i = 6;
    http_header_cursor_init(&cursor, mParser);
    while (http_header_cursor_next(&header.name, &header.nameLen, &header.value, &header.valueLen, &cursor) == 0) {
        if (i == max)
            break;

        vectors[i].iov_base = (void *)header.name;
        vectors[i].iov_len = header.nameLen + 2 + header.valueLen + 2;
        i++;
    }

    http_header_cursor_deinit(&cursor);
    if (i + 1 >= max) {
        errno = EOVERFLOW;
        return -1;
    }

    vectors[i].iov_base = (void *)"\r\n";
    vectors[i].iov_len = 2;
    i++;

    size = mOutputBodySize;
    list_for_each(pos, &mOutputBody) {
        if (i + 1 == max && pos != mOutputBody.prev) {
            pos = combineFrom(pos, size);
            if (!pos)
                return -1;
        }

        block = list_entry(pos, HttpMessageBlock, list);
        vectors[i].iov_base = (void *)block->ptr;
        vectors[i].iov_len = block->size;
        size -= block->size;
        i++;
    }

    return i;
}

struct list_head *protocol::HttpMessage::combineFrom(struct list_head *pos, size_t size)
{
    size_t n = sizeof (HttpMessageBlock) + size;
    HttpMessageBlock *block = (HttpMessageBlock *)malloc(n);
    HttpMessageBlock *entry;
    char *ptr;

    if (block) {
        block->ptr = block + 1;
        block->size = size;
        ptr = (char *)block->ptr;

        do {
            entry = list_entry(pos, HttpMessageBlock, list);
            pos = pos->next;
            list_del(&entry->list);
            memcpy(ptr, entry->ptr, entry->size);
            ptr += entry->size;
            free(entry);
        } while (pos != &mOutputBody);

        list_add_tail(&block->list, &mOutputBody);
        return &block->list;
    }

    return NULL;
}

protocol::HttpMessage::HttpMessage(protocol::HttpMessage &&msg)
    : ProtocolMessage(std::move(msg))
{
    mParser = msg.mParser;
    msg.mParser = NULL;

    INIT_LIST_HEAD(&mOutputBody);
    list_splice_init(&msg.mOutputBody, &mOutputBody);
    mOutputBodySize = msg.mOutputBodySize;
    msg.mOutputBodySize = 0;

    this->mCurSize = msg.mCurSize;
    msg.mCurSize = 0;
}

protocol::HttpMessage &protocol::HttpMessage::operator=(protocol::HttpMessage &&msg)
{
    if (&msg != this) {
        *(ProtocolMessage *)this = std::move(msg);

        if (mParser) {
            http_parser_deinit(mParser);
            delete mParser;
        }

        mParser = msg.mParser;
        msg.mParser = nullptr;

        clearOutputBody();
        list_splice_init(&msg.mOutputBody, &mOutputBody);
        mOutputBodySize = msg.mOutputBodySize;
        msg.mOutputBodySize = 0;

        mCurSize = msg.mCurSize;
        msg.mCurSize = 0;
    }

    return *this;
}



#define HTTP_100_STATUS_LINE        "HTTP/1.1 100 Continue"
#define HTTP_400_STATUS_LINE        "HTTP/1.1 400 Bad Request"
#define HTTP_413_STATUS_LINE        "HTTP/1.1 413 Request Entity Too Large"
#define HTTP_417_STATUS_LINE        "HTTP/1.1 417 Expectation Failed"
#define CONTENT_LENGTH_ZERO         "Content-Length: 0"
#define CONNECTION_CLOSE            "Connection: close"
#define CRLF                        "\r\n"

#define HTTP_100_RESP               HTTP_100_STATUS_LINE CRLF \
                                    CRLF
#define HTTP_400_RESP               HTTP_400_STATUS_LINE CRLF \
                                    CONTENT_LENGTH_ZERO CRLF \
                                    CONNECTION_CLOSE CRLF \
                                    CRLF

#define HTTP_413_RESP               HTTP_413_STATUS_LINE CRLF \
                                    CONTENT_LENGTH_ZERO CRLF \
                                    CONNECTION_CLOSE CRLF \
                                    CRLF

#define HTTP_417_RESP               HTTP_417_STATUS_LINE CRLF \
                                    CONTENT_LENGTH_ZERO CRLF \
                                    CONNECTION_CLOSE CRLF \
                                    CRLF

int protocol::HttpRequest::append(const void* buf, size_t* size)
{
    int ret = HttpMessage::append(buf, size);

    if (ret == 0) {
        if (mParser->expectContinue && http_parser_header_complete(mParser)) {
            mParser->expectContinue = 0;
            ret = handleExpectContinue();
        }
    } else if (ret < 0) {
        if (errno == EBADMSG)
            this->feedback(HTTP_400_RESP, strlen(HTTP_400_RESP));
        else if (errno == EMSGSIZE)
            this->feedback(HTTP_413_RESP, strlen(HTTP_413_RESP));
    }

    return ret;
}

int protocol::HttpRequest::handleExpectContinue()
{
    size_t trans_len = mParser->transferLength;
    int ret;

    if (trans_len != (size_t)-1) {
        if (mParser->headerOffset + trans_len > mSizeLimit) {
            this->feedback(HTTP_417_RESP, strlen(HTTP_417_RESP));
            errno = EMSGSIZE;
            return -1;
        }
    }

    ret = this->feedback(HTTP_100_RESP, strlen(HTTP_100_RESP));
    if (ret != strlen(HTTP_100_RESP)) {
        if (ret >= 0)
            errno = EAGAIN;
        return -1;
    }

    return 0;
}

int protocol::HttpResponse::append(const void *buf, size_t *size)
{
    int ret = HttpMessage::append(buf, size);

    if (ret > 0) {
        if (0 == strcmp(http_parser_get_code(mParser), "100")) {
            http_parser_deinit(mParser);
            http_parser_init(1, mParser);
            ret = 0;
        }
    }

    return ret;
}
