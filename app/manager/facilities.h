//
// Created by dingjing on 8/24/22.
//

#ifndef JARVIS_FACILITIES_H
#define JARVIS_FACILITIES_H

#include "future.h"
#include "../factory/task-factory.h"

class Facilities
{
public:
    static void usleep(unsigned int microseconds);
    static Future<void> asyncUsleep(unsigned int microseconds);

public:
    template<class FUNC, class... ARGS>
    static void go(const std::string& queue_name, FUNC&& func, ARGS&&... args);

public:
    template<class RESP>
    struct NetworkResult
    {
        RESP                resp;
        long long           seqId;
        int                 taskState;
        int                 taskError;
    };

    template<class REQ, class RESP>
    static NetworkResult<RESP> request(TransportType type, const std::string& url, REQ&& req, int retryMax);

    template<class REQ, class RESP>
    static Future<NetworkResult<RESP>> asyncRequest(TransportType type, const std::string& url, REQ&& req, int retryMax);

public:// async fileIO
    static Future<ssize_t> asyncPRead(int fd, void *buf, size_t count, off_t offset);
    static Future<ssize_t> asyncPWrite(int fd, const void *buf, size_t count, off_t offset);
    static Future<ssize_t> asyncPReadV(int fd, const struct iovec *iov, int iovcnt, off_t offset);
    static Future<ssize_t> asyncPWriteV(int fd, const struct iovec *iov, int iovcnt, off_t offset);
    static Future<int> asyncFsync(int fd);
    static Future<int> asyncFdatasync(int fd);

public:
    class WaitGroup
    {
    public:
        WaitGroup(int n);
        ~WaitGroup();

        void done();
        void wait() const;
        std::future_status wait(int timeout) const;

    private:
        static void __wait_group_callback(CounterTask *task);

        std::atomic<int>                mNLeft;
        CounterTask*                    mTask;
        Future<void>                    mFuture;
    };

private:
    static void __timer_future_callback(TimerTask *task);
    static void __fio_future_callback(FileIOTask *task);
    static void __fvio_future_callback(FileVIOTask *task);
    static void __fsync_future_callback(FileSyncTask *task);
};

#include "facilities.inl"

#endif //JARVIS_FACILITIES_H
