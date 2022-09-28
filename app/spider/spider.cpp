//
// Created by dingjing on 9/28/22.
//
#include "spider.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>

#define SPIDER_DB_LOCK          ("/data/db/spider.db.lock")

std::mutex lock;
static FILE* locker = nullptr;

void sqlite_lock()
{
    lock.lock();
    do {
        if (!locker) {
            if (access (SPIDER_DB_LOCK, F_OK)) {
                if (creat(SPIDER_DB_LOCK, S_IRWXU | S_IRWXG | S_IRWXO)) {
                    loge("spider lock file create error");
                    exit(-1);
                }
            }

            locker = fopen(SPIDER_DB_LOCK, "a+");
            if (!locker) {
                loge("spider lock file open error");
                exit(-1);
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

