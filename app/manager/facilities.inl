//
// Created by dingjing on 8/24/22.
//

#ifndef JARVIS_FACILITIES_INL
#define JARVIS_FACILITIES_INL

inline void Facilities::usleep(unsigned int microseconds)
{
    asyncUsleep(microseconds).get();
}

inline Future<void> Facilities::asyncUsleep(unsigned int microseconds)
{
    auto *pr = new Promise<void>();
    auto fr = pr->getFuture();
    auto *task = TaskFactory::createTimerTask(microseconds, __timer_future_callback);

    task->mUserData = pr;
    task->start();
    return fr;
}

template<class FUNC, class... ARGS>
void Facilities::go(const std::string& queue_name, FUNC&& func, ARGS&&... args)
{
    TaskFactory::createGoTask(queue_name, std::forward<FUNC>(func), std::forward<ARGS>(args)...)->start();
}

template<class REQ, class RESP>
Facilities::NetworkResult<RESP> Facilities::request(TransportType type, const std::string& url, REQ&& req, int retry_max)
{
    return asyncRequest<REQ, RESP>(type, url, std::forward<REQ>(req), retry_max).get();
}

template<class REQ, class RESP>
Future<Facilities::NetworkResult<RESP>> Facilities::asyncRequest(TransportType type, const std::string& url, REQ&& req, int retry_max)
{
    ParsedURI uri;
    auto *pr = new Promise<NetworkResult<RESP>>();
    auto fr = pr->getFuture();
    auto *task = new ComplexClientTask<REQ, RESP>(retry_max, [pr](NetworkTask<REQ, RESP> *task) {
        NetworkResult<RESP> res;

        res.seqid = task->get_task_seq();
        res.task_state = task->get_state();
        res.task_error = task->get_error();
        if (res.task_state == TASK_STATE_SUCCESS) {
            res.resp = std::move(*task->get_resp());
        }

        pr->setValue(std::move(res));
        delete pr;
    });

    URIParser::parse(url, uri);
    task->init(std::move(uri));
    task->set_transport_type(type);
    *task->get_req() = std::forward<REQ>(req);
    task->start();
    return fr;
}

inline Future<ssize_t> Facilities::asyncPRead(int fd, void *buf, size_t count, off_t offset)
{
    auto *pr = new Promise<ssize_t>();
    auto fr = pr->getFuture();
    auto *task = TaskFactory::createPReadTask(fd, buf, count, offset, __fio_future_callback);

    task->mUserData = pr;
    task->start();
    return fr;
}

inline Future<ssize_t> Facilities::asyncPWrite(int fd, const void *buf, size_t count, off_t offset)
{
    auto *pr = new Promise<ssize_t>();
    auto fr = pr->getFuture();
    auto *task = TaskFactory::createPWriteTask(fd, buf, count, offset, __fio_future_callback);

    task->mUserData = pr;
    task->start();
    return fr;
}

inline Future<ssize_t> Facilities::asyncPReadV(int fd, const struct iovec *iov, int iovcnt, off_t offset)
{
    auto *pr = new Promise<ssize_t>();
    auto fr = pr->getFuture();
    auto *task = TaskFactory::createPReadVTask(fd, iov, iovcnt, offset, __fvio_future_callback);

    task->mUserData = pr;
    task->start();
    return fr;
}

inline Future<ssize_t> Facilities::asyncPWriteV(int fd, const struct iovec *iov, int iovcnt, off_t offset)
{
    auto *pr = new Promise<ssize_t>();
    auto fr = pr->getFuture();
    auto *task = TaskFactory::createPWriteVTask(fd, iov, iovcnt, offset, __fvio_future_callback);

    task->mUserData = pr;
    task->start();
    return fr;
}

inline Future<int> Facilities::asyncFsync(int fd)
{
    auto *pr = new Promise<int>();
    auto fr = pr->getFuture();
    auto *task = TaskFactory::createFsyncTask(fd, __fsync_future_callback);

    task->mUserData = pr;
    task->start();
    return fr;
}

inline Future<int> Facilities::asyncFdatasync(int fd)
{
    auto *pr = new Promise<int>();
    auto fr = pr->getFuture();
    auto *task = TaskFactory::createFdSyncTask(fd, __fsync_future_callback);

    task->mUserData = pr;
    task->start();
    return fr;
}

inline void Facilities::__timer_future_callback(TimerTask *task)
{
    auto *pr = static_cast<Promise<void> *>(task->mUserData);

    pr->setValue();
    delete pr;
}

inline void Facilities::__fio_future_callback(FileIOTask *task)
{
    auto *pr = static_cast<Promise<ssize_t> *>(task->mUserData);

    pr->setValue(task->getRetVal());
    delete pr;
}

inline void Facilities::__fvio_future_callback(FileVIOTask *task)
{
    auto *pr = static_cast<Promise<ssize_t> *>(task->mUserData);

    pr->setValue(task->getRetVal());
    delete pr;
}

inline void Facilities::__fsync_future_callback(FileSyncTask *task)
{
    auto *pr = static_cast<Promise<int> *>(task->mUserData);

    pr->setValue(task->getRetVal());
    delete pr;
}

inline Facilities::WaitGroup::WaitGroup(int n) : mNLeft(n)
{
    if (n <= 0) {
        this->mNLeft = -1;
        return;
    }

    auto *pr = new Promise<void>();

    this->mTask = TaskFactory::createCounterTask(1, __wait_group_callback);
    this->mFuture = pr->getFuture();
    this->mTask->mUserData = pr;
    this->mTask->start();
}

inline Facilities::WaitGroup::~WaitGroup()
{
    if (this->mNLeft > 0)
        this->mTask->count();
}

inline void Facilities::WaitGroup::done()
{
    if (--this->mNLeft == 0) {
        this->mTask->count();
    }
}

inline void Facilities::WaitGroup::wait() const
{
    if (this->mNLeft < 0)
        return;

    this->mFuture.wait();
}

inline std::future_status Facilities::WaitGroup::wait(int timeout) const
{
    if (this->mNLeft < 0)
        return std::future_status::ready;

    if (timeout < 0)
    {
        this->mFuture.wait();
        return std::future_status::ready;
    }

    return this->mFuture.waitFor(std::chrono::milliseconds(timeout));
}

inline void Facilities::WaitGroup::__wait_group_callback(CounterTask *task)
{
    auto *pr = static_cast<Promise<void> *>(task->mUserData);

    pr->setValue();
    delete pr;
}



#endif //JARVIS_FACILITIES_INL
