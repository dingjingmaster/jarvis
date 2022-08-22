//
// Created by dingjing on 8/17/22.
//

#ifndef JARVIS_ENCODE_STREAM_H
#define JARVIS_ENCODE_STREAM_H

#include <string>
#include <stdint.h>
#include <string.h>
#include <sys/uio.h>

#include "../core/c-list.h"

class EncodeStream
{
public:
    EncodeStream()
    {
        initVec(NULL, 0);
        INIT_LIST_HEAD(&mBufList);
    }

    EncodeStream(struct iovec *vectors, int max)
    {
        initVec(vectors, max);
        INIT_LIST_HEAD(&mBufList);
    }

    ~EncodeStream() { clearBufData(); }

    void reset(struct iovec *vectors, int max)
    {
        clearBufData();
        initVec(vectors, max);
    }

    int size() const { return mSize; }
    size_t bytes() const { return mBytes; }

    void appendNoCopy(const char *data, size_t len);

    void appendNoCopy(const char *data)
    {
        appendNoCopy(data, strlen(data));
    }

    void appendNoCopy(const std::string& data)
    {
        appendNoCopy(data.c_str(), data.size());
    }

    void appendCopy(const char *data, size_t len);

    void appendCopy(const char *data)
    {
        appendCopy(data, strlen(data));
    }

    void appendCopy(const std::string& data)
    {
        appendCopy(data.c_str(), data.size());
    }

private:
    void initVec(struct iovec *vectors, int max)
    {
        mVec = vectors;
        mMax = max;
        mBytes = 0;
        mSize = 0;
        mMergedBytes = 0;
        mMergedSize = 0;
    }

    void merge();
    void clearBufData();

private:
    struct iovec*                       mVec;
    int                                 mMax;
    int                                 mSize;
    size_t                              mBytes;
    int                                 mMergedSize;
    size_t                              mMergedBytes;
    struct list_head                    mBufList;
};

static inline EncodeStream& operator << (EncodeStream& stream, const char *data)
{
    stream.appendNoCopy(data, strlen(data));

    return stream;
}

static inline EncodeStream& operator << (EncodeStream& stream, const std::string& data)
{
    stream.appendNoCopy(data.c_str(), data.size());
    return stream;
}

static inline EncodeStream& operator << (EncodeStream& stream, const std::pair<const char *, size_t>& data)
{
    stream.appendNoCopy(data.first, data.second);
    return stream;
}

static inline EncodeStream& operator << (EncodeStream& stream, int64_t intV)
{
    stream.appendCopy(std::to_string(intV));
    return stream;
}

#endif //JARVIS_ENCODE_STREAM_H
