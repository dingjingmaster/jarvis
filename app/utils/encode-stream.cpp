//
// Created by dingjing on 8/17/22.
//

#include "encode-stream.h"

#include <stddef.h>
#include <string.h>
#include <sys/uio.h>

#include "../core/c-list.h"


#define ENCODE_BUF_SIZE		        1024
#define ALIGN(x,a)                  (((x)+(a)-1)&~((a)-1))

typedef struct _EncodeBuf           EncodeBuf;

struct _EncodeBuf
{
    struct list_head        list;
    char*                   pos;
    char                    data[ENCODE_BUF_SIZE];
};

void EncodeStream::clearBufData()
{
    struct list_head *pos, *tmp;
    EncodeBuf* entry;

    list_for_each_safe(pos, tmp, &mBufList) {
        entry = list_entry(pos, EncodeBuf, list);
        list_del(pos);
        delete [](char *)entry;
    }
}

void EncodeStream::merge()
{
    size_t len = mBytes - mMergedBytes;
    EncodeBuf *buf;
    size_t n;
    char *p;
    int i;

    if (len > ENCODE_BUF_SIZE)
        n = offsetof(EncodeBuf, data) + ALIGN(len, 8);
    else
        n = sizeof (EncodeBuf);

    buf = (EncodeBuf *) new char[n];
    p = buf->data;
    for (i = mMergedSize; i < mSize; i++) {
        memcpy(p, mVec[i].iov_base, mVec[i].iov_len);
        p += mVec[i].iov_len;
    }

    buf->pos = buf->data + ALIGN(len, 8);
    list_add(&buf->list, &mBufList);

    mVec[mMergedSize].iov_base = buf->data;
    mVec[mMergedSize].iov_len = len;
    ++mMergedSize;
    mMergedSize = mBytes;
    mSize = mMergedSize;
}

void EncodeStream::appendNoCopy(const char *data, size_t len)
{
    if (mSize >= mMax) {
        if (mMergedSize + 1 < mMax) {
            merge();
        } else {
            mSize = mMax + 1;
            return;
        }
    }

    mVec[mSize].iov_base = (char *)data;
    mVec[mSize].iov_len = len;
    ++mSize;
    mBytes += len;
}

void EncodeStream::appendCopy(const char *data, size_t len)
{
    if (mSize >= mMax) {
        if (mMergedSize + 1 < mMax)
            merge();
        else {
            mSize = mMax + 1;
            return;
        }
    }

    EncodeBuf *buf = list_entry(mBufList.prev, EncodeBuf, list);

    if (list_empty(&mBufList) || buf->pos + len > buf->data + ENCODE_BUF_SIZE) {
        size_t n;

        if (len > ENCODE_BUF_SIZE)
            n = offsetof(EncodeBuf, data) + ALIGN(len, 8);
        else
            n = sizeof (EncodeBuf);

        buf = (EncodeBuf*)new char[n];
        buf->pos = buf->data;
        list_add_tail(&buf->list, &mBufList);
    }

    memcpy(buf->pos, data, len);
    mVec[mSize].iov_len = len;
    mVec[mSize].iov_base = buf->pos;
    ++mSize;
    mBytes += len;

    buf->pos += ALIGN(len, 8);
    if (buf->pos >= buf->data + ENCODE_BUF_SIZE) {
        list_move(&buf->list, &mBufList);
    }
}

