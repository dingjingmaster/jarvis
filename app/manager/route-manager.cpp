//
// Created by dingjing on 8/19/22.
//

#include "route-manager.h"

#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <openssl/ssl.h>


#include <mutex>
#include <chrono>
#include <vector>
#include <string>
#include <algorithm>


#include "global.h"
#include "../core/c-list.h"
#include "../core/rb-tree.h"
#include "endpoint-params.h"
#include "../utils/md5-util.h"
#include "../utils/string-util.h"
#include "../core/common-scheduler.h"

#define GET_CURRENT_SECOND	std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count()
#define MTTR_SECOND			30

using RouteTargetTCP = RouteManager::RouteTarget;

class RouteTargetUDP : public RouteManager::RouteTarget
{
private:
    virtual int createConnectFd()
    {
        const struct sockaddr *addr;
        socklen_t addrLen;

        getAddr(&addr, &addrLen);
        return socket(addr->sa_family, SOCK_DGRAM, 0);
    }
};

class RouteTargetSCTP : public RouteManager::RouteTarget
{
private:
#ifdef IPPROTO_SCTP
    virtual int createConnectFd()
    {
        const struct sockaddr *addr;
        socklen_t addrLen;

        getAddr(&addr, &addrLen);
        return socket(addr->sa_family, SOCK_STREAM, IPPROTO_SCTP);
    }
#else
    virtual int create_connect_fd()
	{
		errno = EPROTONOSUPPORT;
		return -1;
	}
#endif
};

/* To support TLS SNI. */
class RouteTargetSNI : public RouteManager::RouteTarget
{
private:
    virtual int initSSL(SSL *ssl)
    {
        if (SSL_set_tlsext_host_name(ssl, mHostname.c_str()) > 0)
            return 0;
        else
            return -1;
    }

public:
    RouteTargetSNI(const std::string& name) : mHostname(name)
    {
    }

private:
    std::string                 mHostname;
};

//  protocol_name\n user\n pass\n dbname\n ai_addr ai_addrlen \n....
//

struct RouteParams
{
    TransportType                       transportType;
    const struct addrinfo*              addrInfo;
    uint64_t                            md516;
    SSL_CTX*                            sslCtx;
    int                                 connectTimeout;
    int                                 sslConnectTimeout;
    int                                 responseTimeout;
    size_t                              maxConnections;
    bool                                useTlsSni;
    const std::string&                  hostname;
};

class RouteResultEntry
{
public:
    RouteResultEntry(): mRequestObject(NULL), mGroup(NULL)
    {
        INIT_LIST_HEAD(&mBreakerList);
        mNLeft = 0;
        mNBreak = 0;
    }

public:
    void deInit();
    int init(const struct RouteParams *params);

    void checkBreaker();
    void notifyAvailable(CommSchedTarget *target);
    void notifyUnavailable(CommSchedTarget *target);

private:
    void freeList();
    int addGroupTargets(const struct RouteParams *params);
    CommSchedTarget *createTarget(const struct RouteParams *params, const struct addrinfo *addrInfo);


public:
    RBNode                              mRb;
    CommSchedObject*                    mRequestObject;
    CommSchedGroup*                     mGroup;
    std::mutex                          mMutex;
    std::vector<CommSchedTarget*>       mTargets;
    struct list_head                    mBreakerList;
    uint64_t                            mMd516;
    int                                 mNLeft;
    int                                 mNBreak;
};

typedef struct _BreakerNode             BreakerNode;
struct _BreakerNode
{
    CommSchedTarget*                    target;
    int64_t                             timeout;
    struct list_head                    breakerList;
};

CommSchedTarget *RouteResultEntry::createTarget(const struct RouteParams *params, const struct addrinfo *addr)
{
    CommSchedTarget *target;

    switch (params->transportType) {
        case TT_TCP_SSL:
            if (params->useTlsSni)
                target = new RouteTargetSNI(params->hostname);
            else
                case TT_TCP:
                    target = new RouteTargetTCP();
            break;
        case TT_UDP:
            target = new RouteTargetUDP();
            break;
        case TT_SCTP:
        case TT_SCTP_SSL:
            target = new RouteTargetSCTP();
            break;
        default:
            errno = EINVAL;
            return NULL;
    }

    if (target->init(addr->ai_addr, addr->ai_addrlen, params->sslCtx,
                     params->connectTimeout, params->sslConnectTimeout,
                     params->responseTimeout, params->maxConnections) < 0)
    {
        delete target;
        target = NULL;
    }

    return target;
}

int RouteResultEntry::init(const struct RouteParams *params)
{
    const struct addrinfo *addr = params->addrInfo;
    CommSchedTarget *target;

    if (addr == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (addr->ai_next == NULL) {
        target = createTarget(params, addr);
        if (target) {
            mTargets.push_back(target);
            mRequestObject = target;
            mMd516 = params->md516;
            return 0;
        }

        return -1;
    }

    mGroup = new CommSchedGroup();
    if (mGroup->init() >= 0) {
        if (addGroupTargets(params) >= 0) {
            mRequestObject = mGroup;
            mMd516 = params->md516;
            return 0;
        }

        mGroup->deInit();
    }

    delete mGroup;

    return -1;
}

int RouteResultEntry::addGroupTargets(const struct RouteParams *params)
{
    const struct addrinfo *addr;
    CommSchedTarget *target;

    for (addr = params->addrInfo; addr; addr = addr->ai_next) {
        target = createTarget(params, addr);
        if (target) {
            if (mGroup->add(target) >= 0) {
                mTargets.push_back(target);
                ++mNLeft;
                continue;
            }

            target->deInit();
            delete target;
        }

        for (auto *target : mTargets) {
            mGroup->remove(target);
            target->deInit();
            delete target;
        }
        return -1;
    }

    return 0;
}

void RouteResultEntry::deInit()
{
    for (auto *target : mTargets) {
        if (mGroup) {
            mGroup->remove(target);
        }
        target->deInit();
        delete target;
    }

    if (mGroup) {
        mGroup->deInit();
        delete mGroup;
    }

    struct list_head *pos, *tmp;
    BreakerNode *node;

    list_for_each_safe(pos, tmp, &mBreakerList) {
        node = list_entry(pos, BreakerNode, breakerList);
        list_del(pos);
        delete node;
    }
}

void RouteResultEntry::notifyUnavailable(CommSchedTarget *target)
{
    if (mTargets.size() <= 1) {
        return;
    }

    int errno_bak = errno;
    std::lock_guard<std::mutex> lock(mMutex);

    if (mNLeft <= 1) {
        return;
    }

    if (mGroup->remove(target) < 0) {
        errno = errno_bak;
        return;
    }

    auto *node = new BreakerNode;

    node->target = target;
    node->timeout = GET_CURRENT_SECOND + MTTR_SECOND;
    list_add_tail(&node->breakerList, &mBreakerList);
    ++mNBreak;
    --mNLeft;
}

void RouteResultEntry::notifyAvailable(CommSchedTarget *target)
{
    if (mTargets.size() <= 1 || mNBreak == 0) {
        return;
    }

    int errno_bak = errno;
    std::lock_guard<std::mutex> lock(mMutex);

    if (mGroup->add(target) == 0)
        ++mNLeft;
    else
        errno = errno_bak;
}

void RouteResultEntry::checkBreaker()
{
    if (mTargets.size() <= 1 || mNBreak == 0) {
        return;
    }

    struct list_head *pos, *tmp;
    BreakerNode *node;
    int errno_bak = errno;
    int64_t cur_time = GET_CURRENT_SECOND;
    std::lock_guard<std::mutex> lock(mMutex);

    list_for_each_safe(pos, tmp, &mBreakerList) {
        node = list_entry(pos, BreakerNode, breakerList);
        if (cur_time >= node->timeout) {
            if (mGroup->add(node->target) == 0)
                mNLeft++;
            else
                errno = errno_bak;

            list_del(pos);
            delete node;
            mNBreak--;
        }
    }
}

static inline int __addr_cmp(const struct addrinfo *x, const struct addrinfo *y)
{
    //todo ai_protocol
    if (x->ai_addrlen == y->ai_addrlen)
        return memcmp(x->ai_addr, y->ai_addr, x->ai_addrlen);
    else if (x->ai_addrlen < y->ai_addrlen)
        return -1;
    else
        return 1;
}

static inline bool __addr_less(const struct addrinfo *x, const struct addrinfo *y)
{
    return __addr_cmp(x, y) < 0;
}

static uint64_t __generate_key(TransportType type, const struct addrinfo *addrInfo, const std::string& otherInfo, const EndpointParams *endpointParams, const std::string& hostname)
{
    std::string str(std::to_string(type));

    str += '\n';
    if (!otherInfo.empty()) {
        str += otherInfo;
        str += '\n';
    }

    if (type == TT_TCP_SSL && endpointParams->useTlsSni) {
        str += hostname;
        str += '\n';
    }

    if (addrInfo->ai_next) {
        std::vector<const struct addrinfo *> sorted_addr;

        sorted_addr.push_back(addrInfo);
        addrInfo = addrInfo->ai_next;
        do {
            sorted_addr.push_back(addrInfo);
            addrInfo = addrInfo->ai_next;
        } while (addrInfo);

        std::sort(sorted_addr.begin(), sorted_addr.end(), __addr_less);
        for (const struct addrinfo *p : sorted_addr) {
            str += std::string((char *)p->ai_addr, p->ai_addrlen);
            str += '\n';
        }
    } else {
        str += std::string((char *)addrInfo->ai_addr, addrInfo->ai_addrlen);
    }

    return MD5Util::md5Integer16(str);
}

RouteManager::~RouteManager()
{
    RouteResultEntry *entry;

    while (mCache.rbNode) {
        entry = RB_ENTRY(mCache.rbNode, RouteResultEntry, mRb);
        rb_erase(mCache.rbNode, &mCache);
        entry->deInit();
        delete entry;
    }
}

int RouteManager::get(TransportType type, const struct addrinfo *addrInfo, const std::string& otherInfo, const EndpointParams *endpointParams, const std::string& hostname, RouteResult& result)
{
    uint64_t md516 = __generate_key(type, addrInfo, otherInfo, endpointParams, hostname);
    RBNode** p = &mCache.rbNode;
    RBNode* parent = NULL;
    RouteResultEntry *bound = NULL;
    RouteResultEntry *entry;
    std::lock_guard<std::mutex> lock(mMutex);

    while (*p) {
        parent = *p;
        entry = RB_ENTRY(*p, RouteResultEntry, mRb);
        if (md516 <= entry->mMd516) {
            bound = entry;
            p = &(*p)->rbLeft;
        } else {
            p = &(*p)->rbRight;
        }
    }

    if (bound && bound->mMd516 == md516) {
        entry = bound;
        entry->checkBreaker();
    } else {
        int ssl_connect_timeout = 0;
        SSL_CTX *ssl_ctx = NULL;

        if (type == TT_TCP_SSL || type == TT_SCTP_SSL) {
            static SSL_CTX *client_ssl_ctx = Global::getSslClientCtx();
            ssl_ctx = client_ssl_ctx;
            ssl_connect_timeout = endpointParams->sslConnectTimeout;
        }

        struct RouteParams params = {
                .transportType          =   type,
                .addrInfo               =   addrInfo,
                .md516                  =   md516,
                .sslCtx                 =   ssl_ctx,
                .connectTimeout         =   endpointParams->connectTimeout,
                .sslConnectTimeout      =   ssl_connect_timeout,
                .responseTimeout        =   endpointParams->responseTimeout,
                .maxConnections         =   endpointParams->maxConnections,
                .useTlsSni              =   endpointParams->useTlsSni,
                .hostname               =   hostname,
        };

        if (StringUtil::startWith(otherInfo, "?maxconn=")) {
            int maxconn = atoi(otherInfo.c_str() + 9);
            if (maxconn > 0) {
                params.maxConnections = maxconn;
            }
        }

        entry = new RouteResultEntry;
        if (entry->init(&params) >= 0) {
            rb_link_node(&entry->mRb, parent, p);
            rb_insert_color(&entry->mRb, &mCache);
        } else {
            delete entry;
            return -1;
        }
    }

    result.mCookie = entry;
    result.mRequestObject = entry->mRequestObject;

    return 0;
}

void RouteManager::notifyUnavailable(void *cookie, CommTarget *target)
{
    if (cookie && target) {
        ((RouteResultEntry *)cookie)->notifyUnavailable((CommSchedTarget*)target);
    }
}

void RouteManager::notifyAvailable(void *cookie, CommTarget *target)
{
    if (cookie && target) {
        ((RouteResultEntry *)cookie)->notifyAvailable((CommSchedTarget*)target);
    }
}

