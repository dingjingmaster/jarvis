//
// Created by dingjing on 8/25/22.
//

#include <string>
#include <fcntl.h>
#include <unistd.h>

#include "task-factory.h"
#include "../manager/global.h"

class FilePReadTask : public FileIOTask
{
public:
    FilePReadTask(int fd, void *buf, size_t count, off_t offset, IOService *service, FileIOCallback&& cb)
        : FileIOTask(service, std::move(cb))
    {
        this->mArgs.fd = fd;
        this->mArgs.buf = buf;
        this->mArgs.count = count;
        this->mArgs.offset = offset;
    }

protected:
    virtual int prepare()
    {
        this->prepPRead(this->mArgs.fd, this->mArgs.buf, this->mArgs.count, this->mArgs.offset);
        return 0;
    }
};

class FilePWriteTask : public FileIOTask
{
public:
    FilePWriteTask(int fd, const void *buf, size_t count, off_t offset, IOService *service, FileIOCallback&& cb)
        : FileIOTask(service, std::move(cb))
    {
        this->mArgs.fd = fd;
        this->mArgs.buf = (void *)buf;
        this->mArgs.count = count;
        this->mArgs.offset = offset;
    }

protected:
    virtual int prepare()
    {
        this->prepPWrite(this->mArgs.fd, this->mArgs.buf, this->mArgs.count, this->mArgs.offset);
        return 0;
    }
};

class FilePReadVTask : public FileVIOTask
{
public:
    FilePReadVTask(int fd, const struct iovec *iov, int iovcnt, off_t offset, IOService *service, FileVIOCallback&& cb)
            : FileVIOTask(service, std::move(cb))
    {
        this->mArgs.fd = fd;
        this->mArgs.iov = iov;
        this->mArgs.iovCnt = iovcnt;
        this->mArgs.offset = offset;
    }

protected:
    virtual int prepare()
    {
        this->prepPReadv(this->mArgs.fd, this->mArgs.iov, this->mArgs.iovCnt, this->mArgs.offset);
        return 0;
    }
};

class FilePWriteVTask : public FileVIOTask
{
public:
    FilePWriteVTask(int fd, const struct iovec *iov, int iovcnt, off_t offset, IOService *service, FileVIOCallback&& cb)
            : FileVIOTask(service, std::move(cb))
    {
        this->mArgs.fd = fd;
        this->mArgs.iov = iov;
        this->mArgs.iovCnt = iovcnt;
        this->mArgs.offset = offset;
    }

protected:
    virtual int prepare()
    {
        this->prepPWritev(this->mArgs.fd, this->mArgs.iov, this->mArgs.iovCnt,
                           this->mArgs.offset);
        return 0;
    }
};

class FileFsyncTask : public FileSyncTask
{
public:
    FileFsyncTask(int fd, IOService *service, FileSyncCallback&& cb)
        : FileSyncTask(service, std::move(cb))
    {
        this->mArgs.fd = fd;
    }

protected:
    virtual int prepare()
    {
        this->prepFSync(this->mArgs.fd);
        return 0;
    }
};

class FilefdsyncTask : public FileSyncTask
{
public:
    FilefdsyncTask(int fd, IOService *service, FileSyncCallback&& cb)
        : FileSyncTask(service, std::move(cb))
    {
        this->mArgs.fd = fd;
    }

protected:
    virtual int prepare()
    {
        this->prepFdSync(this->mArgs.fd);
        return 0;
    }
};

/* File tasks created with path name. */

class __FilePReadTask : public FilePReadTask
{
public:
    __FilePReadTask(const std::string& path, void *buf, size_t count, off_t offset, IOService *service, FileIOCallback&& cb)
        : FilePReadTask(-1, buf, count, offset, service, std::move(cb)), pathname(path)
    {
    }

protected:
    virtual int prepare()
    {
        this->mArgs.fd = open(this->pathname.c_str(), O_RDONLY);
        if (this->mArgs.fd < 0)
            return -1;

        return FilePReadTask::prepare();
    }

    virtual SubTask *done()
    {
        if (this->mArgs.fd >= 0)
        {
            close(this->mArgs.fd);
            this->mArgs.fd = -1;
        }

        return FilePReadTask::done();
    }

protected:
    std::string pathname;
};

class __FilePWriteTask : public FilePWriteTask
{
public:
    __FilePWriteTask(const std::string& path, const void *buf, size_t count, off_t offset, IOService *service, FileIOCallback&& cb)
        : FilePWriteTask(-1, buf, count, offset, service, std::move(cb)), pathname(path)
    {
    }

protected:
    virtual int prepare()
    {
        this->mArgs.fd = open(this->pathname.c_str(), O_WRONLY | O_CREAT, 0644);
        if (this->mArgs.fd < 0)
            return -1;

        return FilePWriteTask::prepare();
    }

    virtual SubTask *done()
    {
        if (this->mArgs.fd >= 0)
        {
            close(this->mArgs.fd);
            this->mArgs.fd = -1;
        }

        return FilePWriteTask::done();
    }

protected:
    std::string pathname;
};

class __FilePReadVTask : public FilePReadVTask
{
public:
    __FilePReadVTask(const std::string& path, const struct iovec *iov, int iovcnt, off_t offset, IOService *service, FileVIOCallback&& cb)
            : FilePReadVTask(-1, iov, iovcnt, offset, service, std::move(cb)), pathname(path)
    {
    }

protected:
    virtual int prepare()
    {
        this->mArgs.fd = open(this->pathname.c_str(), O_RDONLY);
        if (this->mArgs.fd < 0)
            return -1;

        return FilePReadVTask::prepare();
    }

    virtual SubTask *done()
    {
        if (this->mArgs.fd >= 0)
        {
            close(this->mArgs.fd);
            this->mArgs.fd = -1;
        }

        return FilePReadVTask::done();
    }

protected:
    std::string pathname;
};

class __FilePWriteVTask : public FilePWriteVTask
{
public:
    __FilePWriteVTask(const std::string& path, const struct iovec *iov, int iovcnt, off_t offset, IOService *service, FileVIOCallback&& cb)
        : FilePWriteVTask(-1, iov, iovcnt, offset, service, std::move(cb)), pathname(path)
    {
    }

protected:
    virtual int prepare()
    {
        this->mArgs.fd = open(this->pathname.c_str(), O_WRONLY | O_CREAT, 0644);
        if (this->mArgs.fd < 0)
            return -1;

        return FilePWriteVTask::prepare();
    }

protected:
    virtual SubTask *done()
    {
        if (this->mArgs.fd >= 0)
        {
            close(this->mArgs.fd);
            this->mArgs.fd = -1;
        }

        return FilePWriteVTask::done();
    }

protected:
    std::string pathname;
};

/* Factory functions with fd. */

FileIOTask *TaskFactory::createPReadTask(int fd, void *buf, size_t count, off_t offset, FileIOCallback callback)
{
    return new FilePReadTask(fd, buf, count, offset, Global::getIoService(), std::move(callback));
}

FileIOTask *TaskFactory::createPWriteTask(int fd, const void *buf, size_t count, off_t offset, FileIOCallback callback)
{
    return new FilePWriteTask(fd, buf, count, offset, Global::getIoService(), std::move(callback));
}

FileVIOTask *TaskFactory::createPReadVTask(int fd, const struct iovec *iovec, int iovcnt, off_t offset, FileVIOCallback callback)
{
    return new FilePReadVTask(fd, iovec, iovcnt, offset, Global::getIoService(), std::move(callback));
}

FileVIOTask *TaskFactory::createPWriteVTask(int fd, const struct iovec *iovec, int iovcnt, off_t offset, FileVIOCallback callback)
{
    return new FilePWriteVTask(fd, iovec, iovcnt, offset, Global::getIoService(), std::move(callback));
}

FileSyncTask *TaskFactory::createFsyncTask(int fd, FileSyncCallback callback)
{
    return new FileFsyncTask(fd, Global::getIoService(), std::move(callback));
}

FileSyncTask *TaskFactory::createFdSyncTask(int fd, FileSyncCallback callback)
{
    return new FilefdsyncTask(fd, Global::getIoService(), std::move(callback));
}

/* Factory functions with path name. */

FileIOTask *TaskFactory::createPReadTask(const std::string& pathname, void *buf, size_t count, off_t offset, FileIOCallback callback)
{
    return new __FilePReadTask(pathname, buf, count, offset, Global::getIoService(), std::move(callback));
}

FileIOTask *TaskFactory::createPWriteTask(const std::string& pathname, const void *buf, size_t count, off_t offset, FileIOCallback callback)
{
    return new __FilePWriteTask(pathname, buf, count, offset, Global::getIoService(), std::move(callback));
}

FileVIOTask *TaskFactory::createPReadVTask(const std::string& pathname, const struct iovec *iovec, int iovcnt, off_t offset, FileVIOCallback callback)
{
    return new __FilePReadVTask(pathname, iovec, iovcnt, offset, Global::getIoService(), std::move(callback));
}

FileVIOTask *TaskFactory::createPWriteVTask(const std::string& pathname, const struct iovec *iovec, int iovcnt, off_t offset, FileVIOCallback callback)
{
    return new __FilePWriteVTask(pathname, iovec, iovcnt, offset, Global::getIoService(), std::move(callback));
}

