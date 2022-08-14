//
// Created by dingjing on 8/14/22.
//

#include "package-wrapper.h"

int protocol::PackageWrapper::append(const void *buf, size_t *size)
{
    int ret = ProtocolWrapper::append(buf, size);

    if (ret > 0) {
        mMsg = next(mMsg);
        if (mMsg) {
            reNew();
            ret = 0;
        }
    }

    return ret;
}

int protocol::PackageWrapper::encode(struct iovec *vectors, int max)
{
    int cnt = 0;
    int ret;

    while (max >= 8) {
        ret = this->ProtocolWrapper::encode(vectors, max);
        if ((unsigned int)ret > (unsigned int)max) {
            if (ret < 0) {
                return ret;
            }
            break;
        }

        cnt += ret;
        mMsg = this->next(mMsg);
        if (!mMsg) {
            return cnt;
        }

        vectors += ret;
        max -= ret;
    }

    errno = EOVERFLOW;

    return -1;
}
