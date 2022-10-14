//
// Created by dingjing on 9/28/22.
//
#include "spider.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>

std::mutex lock;
static FILE* locker = nullptr;

void sqlite_lock()
{
    lock.lock();
    do {
        if (!locker) {
            if (access (SPIDER_DB_LOCK, F_OK)) {
                mode_t pre = umask(0);
                locker = fopen(SPIDER_DB_LOCK, "w+");
                if (!locker) {
                    loge("spider lock file '%s' open error", SPIDER_DB_LOCK);
                    exit(-1);
                }
                umask(pre);
            }
        }
    } while (0);

    while (true) {
        if (flock (locker->_fileno, LOCK_EX | LOCK_NB) == 0) {
            break;
        }
        usleep(100);
    }

    lock.unlock();
}

void sqlite_unlock()
{
    lock.lock();
    flock (locker->_fileno, LOCK_UN);
    lock.unlock();
}

