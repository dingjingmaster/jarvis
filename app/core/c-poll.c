//
// Created by dingjing on 8/11/22.
//

#include "c-poll.h"

#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "c-list.h"
#include "c-poll.h"
#include "rb-tree.h"
#include "c-log.h"

#define C_POLL_BUF_SIZE			    (256 * 1024)
#define C_POLL_EVENTS_MAX		     256

typedef struct _CPollNode           CPollNode;

struct _CPollNode
{
    int                     state;
    int                     error;
    CPollData               data;
#pragma pack(1)
    union {
        struct list_head    list;
        RBNode              rb;
    };
#pragma pack()
    char                    inRBTree;
    char                    removed;
    int                     event;
    struct timespec         timeout;                    // 超时返回,
    CPollNode*              res;
};

struct _CPoll
{
    size_t                  maxOpenFiles;
    CPollMessage* (*createMessage) (void*);
    int (*partialWritten)(size_t, void*);
    void (*cb)(CPollResult *, void*);
    void *ctx;

    pthread_t               tid;
    int                     timerFd;                    // 定时返回 timerfd_create()

    int                     pfd;                        // epoll fd
    int                     pipeRd;                     // 管道 读
    int                     pipeWr;                     // 管道 写

    int                     stopped;                    // 启动/停止标志

    RBRoot                  timeoutTree;
    RBNode*                 treeFirst;
    RBNode*                 treeLast;

    struct list_head        timeoutList;
    struct list_head        noTimeoutList;

    CPollNode**             nodes;                      //
    pthread_mutex_t         mutex;                      // 操作任何 CPoll 结构体成员都该上锁

    char                    buf[C_POLL_BUF_SIZE];       // CPollNode，用于管道
};

static inline int _poll_create_pfd()
{
    return epoll_create(1);
}

static inline int _poll_add_fd(int fd, int event, void *data, CPoll *poll)
{
    struct epoll_event ev = {
            .events		=	event,
            .data		=	{
                    .ptr	=	data
            }
    };

    return epoll_ctl(poll->pfd, EPOLL_CTL_ADD, fd, &ev);
}

static inline int _poll_del_fd(int fd, int event, CPoll *poll)
{
    return epoll_ctl(poll->pfd, EPOLL_CTL_DEL, fd, NULL);
}

static inline int _poll_mod_fd(int fd, int oldEvent, int newEvent, void *data, CPoll *poll)
{
    struct epoll_event ev = {
            .events		=	newEvent,
            .data		=	{
                    .ptr	=	data
            }
    };

    return epoll_ctl(poll->pfd, EPOLL_CTL_MOD, fd, &ev);
}

static inline int _poll_create_timerfd()
{
    // CLOCK_REALTIME: 可设置的系统范围实时时钟
    // CLOCK_MONOTONIC: 不可设置的单调递增时钟，从固定某个时间点开始
    // CLOCK_BOOTTIME: 类似 CLOCK_MONOTONIC,
    // CLOCK_REALTIME_ALARM: 类似CLOCK_REALTIME，如果系统挂起会被唤醒，需要进程有对应权限
    // CLOCK_BOOTTIME_ALARM
    // TFD_NONBLOCK
    // TFD_CLOEXEC
    return timerfd_create(CLOCK_MONOTONIC, 0);
}

static inline int _poll_add_timerfd(int fd, CPoll *poll)
{
    struct epoll_event ev = {
            .events		=	EPOLLIN | EPOLLET,
            .data		=	{
                    .ptr	=	NULL
            }
    };
    return epoll_ctl(poll->pfd, EPOLL_CTL_ADD, fd, &ev);
}

static inline int _poll_set_timer_fd(int fd, const struct timespec *absTime, CPoll *poll)
{
    struct itimerspec timer = {
            .it_interval	=	0,
            .it_value		=	*absTime
    };

    return timerfd_settime(fd, TFD_TIMER_ABSTIME, &timer, NULL);
}

typedef struct epoll_event _poll_event_t;

static inline int _poll_wait(_poll_event_t *events, int maxEvents, CPoll *poll)
{
    return epoll_wait(poll->pfd, events, maxEvents, -1);
}

static inline void* _poll_event_data(const _poll_event_t *event)
{
    return event->data.ptr;
}

static inline long __timeout_cmp(const CPollNode *node1, const CPollNode *node2)
{
    long ret = node1->timeout.tv_sec - node2->timeout.tv_sec;

    if (ret == 0) {
        ret = node1->timeout.tv_nsec - node2->timeout.tv_nsec;
    }

    return ret;
}

static void _poll_tree_insert(CPollNode *node, CPoll *poll)
{
    RBNode **p = &poll->timeoutTree.rbNode;
    RBNode *parent = NULL;
    CPollNode *entry;

    entry = RB_ENTRY(poll->treeLast, CPollNode, rb);
    if (!*p) {
        poll->treeFirst = &node->rb;
        poll->treeLast = &node->rb;
    } else if (__timeout_cmp(node, entry) >= 0) {
        parent = poll->treeLast;
        p = &parent->rbRight;
        poll->treeLast = &node->rb;
    } else {
        do {
            parent = *p;
            entry = RB_ENTRY(*p, CPollNode, rb);
            if (__timeout_cmp(node, entry) < 0)
                p = &(*p)->rbLeft;
            else
                p = &(*p)->rbRight;
        } while (*p);

        if (p == &poll->treeFirst->rbLeft)
            poll->treeFirst = &node->rb;
    }

    node->inRBTree = 1;
    rb_link_node(&node->rb, parent, p);
    rb_insert_color(&node->rb, &poll->timeoutTree);
}

static inline void _poll_tree_erase(CPollNode *node, CPoll *poll)
{
    if (&node->rb == poll->treeFirst)
        poll->treeFirst = rb_next(&node->rb);

    if (&node->rb == poll->treeLast)
        poll->treeLast = rb_prev(&node->rb);

    rb_erase(&node->rb, &poll->timeoutTree);
    node->inRBTree = 0;
}

static int _poll_remove_node(CPollNode *node, CPoll *poll)
{
    int removed;

    pthread_mutex_lock(&poll->mutex);
    removed = node->removed;
    if (!removed)
    {
        poll->nodes[node->data.fd] = NULL;

        if (node->inRBTree)
            _poll_tree_erase(node, poll);
        else
            list_del(&node->list);

        _poll_del_fd(node->data.fd, node->event, poll);
    }

    pthread_mutex_unlock(&poll->mutex);
    return removed;
}

static int _poll_append_message(const void *buf, size_t *n, CPollNode *node, CPoll *poll)
{
    CPollMessage *msg = node->data.message;
    CPollNode *res;
    int ret;

    if (!msg) {
        res = (CPollNode*) malloc (sizeof(CPollNode));
        if (!res) {
            return -1;
        }

        msg = poll->createMessage(node->data.context);
        if (!msg) {
            free(res);
            return -1;
        }

        node->data.message = msg;
        node->res = res;
    } else {
        res = node->res;
    }

    ret = msg->append(buf, n, msg);
    if (ret > 0) {
        res->data = node->data;
        res->error = 0;
        res->state = PR_ST_SUCCESS;
        poll->cb((CPollResult *)res, poll->ctx);

        node->data.message = NULL;
        node->res = NULL;
    }

    return ret;
}

static int _poll_handle_ssl_error(CPollNode *node, int ret, CPoll *poll)
{
    int error = SSL_get_error(node->data.ssl, ret);
    int event;

    switch (error) {
        case SSL_ERROR_WANT_READ:
            event = EPOLLIN | EPOLLET;
            break;
        case SSL_ERROR_WANT_WRITE:
            event = EPOLLOUT | EPOLLET;
            break;
        default:
            errno = -error;
        case SSL_ERROR_SYSCALL:
            return -1;
    }

    if (event == node->event) {
        return 0;
    }

    pthread_mutex_lock(&poll->mutex);
    if (!node->removed) {
        ret = _poll_mod_fd(node->data.fd, node->event, event, node, poll);
        if (ret >= 0) {
            node->event = event;
        }
    } else {
        ret = 0;
    }

    pthread_mutex_unlock(&poll->mutex);

    return ret;
}

static void _poll_handle_read(CPollNode *node, CPoll *poll)
{
    ssize_t nLeft;
    size_t n;
    char* p = NULL;

    while (1) {
        p = poll->buf;
        if (!node->data.ssl) {
            nLeft = read(node->data.fd, p, C_POLL_BUF_SIZE);
            if (nLeft < 0) {
                if (errno == EAGAIN) {
                    return;
                }
            }
        }
        else {
            nLeft = SSL_read(node->data.ssl, p, C_POLL_BUF_SIZE);
            if (nLeft < 0) {
                if (_poll_handle_ssl_error(node, nLeft, poll) >= 0) {
                    return;
                }
            }
        }

        if (nLeft <= 0) {
            break;
        }

        do {
            n = nLeft;
            if (_poll_append_message(p, &n, node, poll) >= 0) {
                nLeft -= n;
                p += n;
            } else {
                nLeft = -1;
            }
        } while (nLeft > 0);

        if (nLeft < 0) {
            break;
        }
    }

    if (_poll_remove_node(node, poll)) {
        return;
    }

    if (nLeft == 0) {
        node->error = 0;
        node->state = PR_ST_FINISHED;
    } else {
        node->error = errno;
        node->state = PR_ST_ERROR;
    }

    free(node->res);
    poll->cb((CPollResult *)node, poll->ctx);
}

#ifndef IOV_MAX
# ifdef UIO_MAXIOV
#  define IOV_MAX	UIO_MAXIOV
# else
#  define IOV_MAX	1024
# endif
#endif

static void _poll_handle_write(CPollNode *node, CPoll *poll)
{
    struct iovec *iov = node->data.writeIov;
    size_t count = 0;
    ssize_t nleft;
    int iovcnt;
    int ret;

    while (node->data.iovec > 0)
    {
        if (!node->data.ssl)
        {
            iovcnt = node->data.iovec;
            if (iovcnt > IOV_MAX)
                iovcnt = IOV_MAX;

            nleft = writev(node->data.fd, iov, iovcnt);
            if (nleft < 0)
            {
                ret = errno == EAGAIN ? 0 : -1;
                break;
            }
        }
        else if (iov->iov_len > 0)
        {
            nleft = SSL_write(node->data.ssl, iov->iov_base, iov->iov_len);
            if (nleft <= 0)
            {
                ret = _poll_handle_ssl_error(node, nleft, poll);
                break;
            }
        }
        else
            nleft = 0;

        count += nleft;
        do
        {
            if (nleft >= iov->iov_len)
            {
                nleft -= iov->iov_len;
                iov->iov_base = (char *)iov->iov_base + iov->iov_len;
                iov->iov_len = 0;
                iov++;
                node->data.iovec--;
            }
            else
            {
                iov->iov_base = (char *)iov->iov_base + nleft;
                iov->iov_len -= nleft;
                break;
            }
        } while (node->data.iovec > 0);
    }

    node->data.writeIov = iov;
    if (node->data.iovec > 0 && ret >= 0) {
        if (count == 0)
            return;

        if (poll->partialWritten(count, node->data.context) >= 0)
            return;
    }

    if (_poll_remove_node(node, poll))
        return;

    if (node->data.iovec == 0) {
        node->error = 0;
        node->state = PR_ST_FINISHED;
    } else {
        node->error = errno;
        node->state = PR_ST_ERROR;
    }

    poll->cb((CPollResult *)node, poll->ctx);
}

static void _poll_handle_listen(CPollNode *node, CPoll *poll)
{
    logv("");
    void*                       p;
    int                         sockFd;
    struct sockaddr_storage     ss;
    socklen_t                   len;
    CPollNode*                  res = node->res;

    while (1) {
        len = sizeof (struct sockaddr_storage);
        logv("accept fd: %d", node->data.fd);
        sockFd = accept(node->data.fd, (struct sockaddr *)&ss, &len);
        if (sockFd < 0) {
            if (errno == EAGAIN || errno == EMFILE || errno == ENFILE) {
                logd("accept return error(EAGAIN | EMFILE | ENFILE): %d", errno);
                return;
            } else if (errno == ECONNABORTED) {
                logd("accept return error(ECONNABORTED): %d", sockFd);
                continue;
            } else {
                logd("accept return error: %d", sockFd);
                break;
            }
        }

        p = node->data.accept((const struct sockaddr *)&ss, len, sockFd, node->data.context);
        if (!p) {
            logd("node->data.accept error!");
            break;
        }

        res->data = node->data;
        res->data.result = p;
        res->error = 0;
        res->state = PR_ST_SUCCESS;
        poll->cb((CPollResult *)res, poll->ctx);

        res = (CPollNode *)malloc(sizeof (CPollNode));
        node->res = res;
        if (!res) {
            logd("malloc CPollNode error!");
            break;
        }
    }

    if (_poll_remove_node(node, poll))
        return;

    node->error = errno;
    node->state = PR_ST_ERROR;
    free(node->res);
    poll->cb((CPollResult *)node, poll->ctx);
}

static void _poll_handle_connect(CPollNode *node, CPoll *poll)
{
    logv("");
    socklen_t len = sizeof (int);
    int error;

    if (getsockopt(node->data.fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
        error = errno;

    if (_poll_remove_node(node, poll))
        return;

    if (error == 0) {
        node->error = 0;
        node->state = PR_ST_FINISHED;
    } else {
        node->error = error;
        node->state = PR_ST_ERROR;
    }

    poll->cb((CPollResult *)node, poll->ctx);
}

static void _poll_handle_ssl_accept(CPollNode *node, CPoll *poll)
{
    logv("");
    int ret = SSL_accept(node->data.ssl);

    if (ret <= 0) {
        if (_poll_handle_ssl_error(node, ret, poll) >= 0)
            return;
    }

    if (_poll_remove_node(node, poll))
        return;

    if (ret > 0) {
        node->error = 0;
        node->state = PR_ST_FINISHED;
    } else {
        node->error = errno;
        node->state = PR_ST_ERROR;
    }

    poll->cb((CPollResult *)node, poll->ctx);
}

static void _poll_handle_ssl_connect(CPollNode *node, CPoll *poll)
{
    logv("");
    int ret = SSL_connect(node->data.ssl);

    if (ret <= 0) {
        if (_poll_handle_ssl_error(node, ret, poll) >= 0)
            return;
    }

    if (_poll_remove_node(node, poll))
        return;

    if (ret > 0) {
        node->error = 0;
        node->state = PR_ST_FINISHED;
    } else {
        node->error = errno;
        node->state = PR_ST_ERROR;
    }

    poll->cb((CPollResult *)node, poll->ctx);
}

static void _poll_handle_ssl_shutdown(CPollNode *node, CPoll *poll)
{
    logv("");
    int ret = SSL_shutdown(node->data.ssl);

    if (ret <= 0) {
        if (_poll_handle_ssl_error(node, ret, poll) >= 0)
            return;
    }

    if (_poll_remove_node(node, poll))
        return;

    if (ret > 0) {
        node->error = 0;
        node->state = PR_ST_FINISHED;
    } else {
        node->error = errno;
        node->state = PR_ST_ERROR;
    }

    poll->cb((CPollResult *)node, poll->ctx);
}

static void _poll_handle_event(CPollNode *node, CPoll *poll)
{
    logv("");
    CPollNode *res = node->res;
    unsigned long long cnt = 0;
    unsigned long long value;
    ssize_t ret;
    void *p;

    while (1) {
        ret = read(node->data.fd, &value, sizeof (unsigned long long));
        if (ret == sizeof (unsigned long long))
            cnt += value;
        else {
            if (ret >= 0)
                errno = EINVAL;
            break;
        }
    }

    if (errno == EAGAIN) {
        while (1) {
            if (cnt == 0)
                return;

            cnt--;
            p = node->data.event(node->data.context);
            if (!p)
                break;

            res->data = node->data;
            res->data.result = p;
            res->error = 0;
            res->state = PR_ST_SUCCESS;
            poll->cb((CPollResult *)res, poll->ctx);

            res = (CPollNode *)malloc(sizeof (CPollNode));
            node->res = res;
            if (!res)
                break;
        }
    }

    if (cnt != 0)
        write(node->data.fd, &cnt, sizeof (unsigned long long));

    if (_poll_remove_node(node, poll))
        return;

    node->error = errno;
    node->state = PR_ST_ERROR;
    free(node->res);
    poll->cb((CPollResult *)node, poll->ctx);
}

static void _poll_handle_notify(CPollNode *node, CPoll *poll)
{
    logv("");
    CPollNode *res = node->res;
    ssize_t ret;
    void *p;

    while (1) {
        ret = read(node->data.fd, &p, sizeof (void *));
        if (ret == sizeof (void *))
        {
            p = node->data.notify(p, node->data.context);
            if (!p)
                break;

            res->data = node->data;
            res->data.result = p;
            res->error = 0;
            res->state = PR_ST_SUCCESS;
            poll->cb((CPollResult *)res, poll->ctx);

            res = (CPollNode *)malloc(sizeof (CPollNode));
            node->res = res;
            if (!res)
                break;
        }
        else if (ret < 0 && errno == EAGAIN)
            return;
        else
        {
            if (ret > 0)
                errno = EINVAL;
            break;
        }
    }

    if (_poll_remove_node(node, poll))
        return;

    if (ret == 0)
    {
        node->error = 0;
        node->state = PR_ST_FINISHED;
    }
    else
    {
        node->error = errno;
        node->state = PR_ST_ERROR;
    }

    free(node->res);
    poll->cb((CPollResult *)node, poll->ctx);
}

static int _poll_handle_pipe(CPoll *poll)
{
    CPollNode **node = (CPollNode**)poll->buf;
    int stop = 0;

    unsigned long n = read(poll->pipeRd, node, C_POLL_BUF_SIZE) / sizeof (void*);
    for (int i = 0; i < n; i++) {
        if (node[i]) {
            free(node[i]->res);
            poll->cb((CPollResult*) node[i], poll->ctx);
        } else {
            stop = 1;
        }
    }

    return stop;
}

static void _poll_handle_timeout(const CPollNode* timeNode, CPoll *poll)
{
    CPollNode*                      node;
    struct list_head                *pos, *tmp;

    LIST_HEAD(timeList);

    pthread_mutex_lock(&poll->mutex);
    list_for_each_safe(pos, tmp, &poll->timeoutList) {
        node = list_entry(pos, CPollNode, list);
        if (__timeout_cmp(node, timeNode) <= 0) {
            if (node->data.fd >= 0) {
                poll->nodes[node->data.fd] = NULL;
                _poll_del_fd(node->data.fd, node->event, poll);
            }

            list_move_tail(pos, &timeList);
        } else {
            break;
        }
    }

    if (poll->treeFirst) {
        while (1) {
            node = RB_ENTRY(poll->treeFirst, CPollNode, rb);
            if (__timeout_cmp(node, timeNode) < 0) {
                if (node->data.fd >= 0) {
                    poll->nodes[node->data.fd] = NULL;
                    _poll_del_fd(node->data.fd, node->event, poll);
                }

                poll->treeFirst = rb_next(poll->treeFirst);
                rb_erase(&node->rb, &poll->timeoutTree);
                list_add_tail(&node->list, &timeList);
                if (poll->treeFirst) {
                    continue;
                }

                poll->treeLast = NULL;
            }
            break;
        }
    }

    pthread_mutex_unlock(&poll->mutex);
    while (!list_empty(&timeList)) {
        node = list_entry(timeList.next, CPollNode, list);
        list_del(&node->list);

        node->error = ETIMEDOUT;
        node->state = PR_ST_ERROR;
        free(node->res);
        poll->cb((CPollResult *)node, poll->ctx);
    }
}

static void _poll_set_timer(CPoll *poll)
{
    CPollNode* node = NULL;
    CPollNode* first = NULL;
    struct timespec absTime;

    pthread_mutex_lock(&poll->mutex);
    if (!list_empty(&poll->timeoutList)) {
        node = list_entry(poll->timeoutList.next, CPollNode, list);
    }

    if (poll->treeFirst) {
        first = RB_ENTRY(poll->treeFirst, CPollNode, rb);
        if (!node || __timeout_cmp(first, node) < 0) {
            // first时间 < node时间 或 node == NULL
            node = first;
        }
    }

    if (node) {
        absTime = node->timeout;
    } else {
        absTime.tv_sec = 0;
        absTime.tv_nsec = 0;
    }

    _poll_set_timer_fd (poll->timerFd, &absTime, poll);
    pthread_mutex_unlock (&poll->mutex);
}

static void* _poll_handle_read_routine(void *arg)
{
    int                 nEvents;
    int                 hasPipeEvent;
    CPoll*              poll = (CPoll*)arg;
    _poll_event_t       events[C_POLL_EVENTS_MAX];          // epoll_event_t
    CPollNode           timeNode;
    CPollNode*          node;

    while (1) {
        _poll_set_timer(poll);
        nEvents = _poll_wait(events, C_POLL_EVENTS_MAX, poll);
        clock_gettime(CLOCK_MONOTONIC, &timeNode.timeout);
        hasPipeEvent = 0;
        for (int i = 0; i < nEvents; ++i) {
            node = (CPollNode*)_poll_event_data(&events[i]);
            if (node > (CPollNode*)1) {
                switch (node->data.operation) {
                    case PD_OP_READ: {
                        _poll_handle_read(node, poll);
                        break;
                    }
                    case PD_OP_WRITE: {
                        _poll_handle_write(node, poll);
                        break;
                    }
                    case PD_OP_LISTEN: {
                        _poll_handle_listen(node, poll);
                        break;
                    }
                    case PD_OP_CONNECT: {
                        _poll_handle_connect(node, poll);
                        break;
                    }
                    case PD_OP_SSL_ACCEPT: {
                        _poll_handle_ssl_accept(node, poll);
                        break;
                    }
                    case PD_OP_SSL_CONNECT: {
                        _poll_handle_ssl_connect(node, poll);
                        break;
                    }
                    case PD_OP_SSL_SHUTDOWN: {
                        _poll_handle_ssl_shutdown(node, poll);
                        break;
                    }
                    case PD_OP_EVENT: {
                        _poll_handle_event(node, poll);
                        break;
                    }
                    case PD_OP_NOTIFY: {
                        _poll_handle_notify(node, poll);
                        break;
                    }
                }
            }
            else if (node == (CPollNode*)1) {
                hasPipeEvent = 1;
            }
        }

        if (hasPipeEvent) {
            if (_poll_handle_pipe(poll)) {
                break;
            }
        }
        _poll_handle_timeout(&timeNode, poll);
    }

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    # ifdef CRYPTO_LOCK_ECDH
	ERR_remove_thread_state(NULL);
# else
	ERR_remove_state(0);
# endif
#endif
    return NULL;
}

static int _poll_open_pipe(CPoll *poll)
{
    int pipeFd[2];

    // pipe() 返回 pipeFd[0] 用于读, pipeFd[1] 用于写，实现进程通信
    if (pipe(pipeFd) >= 0) {
        if (_poll_add_fd(pipeFd[0], EPOLLIN, (void*)1, poll) >= 0) {
            poll->pipeRd = pipeFd[0];
            poll->pipeWr = pipeFd[1];
            return 0;
        }

        close(pipeFd[0]);
        close(pipeFd[1]);
    }

    return -1;
}

static int _poll_create_timer(CPoll *poll)
{
    int timerFd = _poll_create_timerfd();

    if (timerFd >= 0) {
        if (_poll_add_timerfd(timerFd, poll) >= 0) {
            poll->timerFd = timerFd;
            return 0;
        }

        close(timerFd);
    }

    return -1;
}

CPoll* _poll_create(void** nodesBuf, const CPollParams *params)
{
    CPoll *poll = (CPoll*) malloc (sizeof (CPoll));
    if (!poll) {
        return NULL;
    }

    int ret;
    poll->pfd = _poll_create_pfd();
    if (poll->pfd >= 0) {
        if (_poll_create_timer(poll) >= 0) {
            ret = pthread_mutex_init(&poll->mutex, NULL);
            if (ret == 0) {
                poll->nodes = (CPollNode**) nodesBuf;
                poll->maxOpenFiles = params->maxOpenFiles;
                poll->createMessage = params->createMessage;
                poll->partialWritten = params->partialWritten;
                poll->cb = params->callback;
                poll->ctx = params->context;

                poll->timeoutTree.rbNode = NULL;
                poll->treeFirst = NULL;
                poll->treeLast = NULL;
                INIT_LIST_HEAD(&poll->timeoutList);
                INIT_LIST_HEAD(&poll->noTimeoutList);

                poll->stopped = 1;

                return poll;
            }

            errno = ret;
            close(poll->timerFd);
        }
        close(poll->pfd);
    }

    free(poll);

    return NULL;
}

CPoll* poll_create(const CPollParams *params)
{
    CPoll* poll = NULL;
    void** nodesBuf = (void**) calloc (params->maxOpenFiles, sizeof(void*));

    if (nodesBuf) {
        poll = _poll_create(nodesBuf, params);
        if (poll) {
            return poll;
        }

        free(nodesBuf);
    }

    return NULL;
}

void _poll_destroy(CPoll *poll)
{
    logv("");
    pthread_mutex_destroy(&poll->mutex);
    close(poll->timerFd);
    close(poll->pfd);
    free(poll);
}

void poll_destroy(CPoll *poll)
{
    free(poll->nodes);
    _poll_destroy(poll);
}

int poll_start (CPoll *poll)
{
    int                 ret;
    pthread_t           tid;

    pthread_mutex_lock(&poll->mutex);
    if (_poll_open_pipe(poll) >= 0) {
        ret = pthread_create(&tid, NULL, _poll_handle_read_routine, poll);
        if (ret == 0) {
            poll->tid = tid;
            poll->stopped = 0;
        } else {
            errno = ret;
            loge("pthread_create error: %d", ret);
            close(poll->pipeWr);
            close(poll->pipeRd);
        }
    }

    pthread_mutex_unlock(&poll->mutex);
    return -poll->stopped;
}

static void _poll_insert_node(CPollNode *node, CPoll *poll)
{
    logv("");
    CPollNode *end;

    end = list_entry(poll->timeoutList.prev, CPollNode, list);
    if (list_empty(&poll->timeoutList)) {
        list_add(&node->list, &poll->timeoutList);
        end = RB_ENTRY(poll->treeFirst, CPollNode, rb);
    } else if (__timeout_cmp(node, end) >= 0) {
        list_add_tail(&node->list, &poll->timeoutList);
        return;
    } else {
        _poll_tree_insert(node, poll);
        if (&node->rb != poll->treeFirst)
            return;

        end = list_entry(poll->timeoutList.next, CPollNode, list);
    }

    if (!poll->treeFirst || __timeout_cmp(node, end) < 0) {
        _poll_set_timer_fd(poll->timerFd, &node->timeout, poll);
    }
}

static void _poll_node_set_timeout(int timeout, CPollNode *node)
{
    clock_gettime(CLOCK_MONOTONIC, &node->timeout);
    node->timeout.tv_sec += timeout / 1000;
    node->timeout.tv_nsec += timeout % 1000 * 1000000;
    if (node->timeout.tv_nsec >= 1000000000) {
        node->timeout.tv_nsec -= 1000000000;
        node->timeout.tv_sec++;
    }
}

static int _poll_data_get_event(int *event, const CPollData *data)
{
    switch (data->operation) {
        case PD_OP_READ:
            *event = EPOLLIN | EPOLLET;
            return !!data->message;
        case PD_OP_WRITE:
            *event = EPOLLOUT | EPOLLET;
            return 0;
        case PD_OP_LISTEN:
            *event = EPOLLIN;
            return 1;
        case PD_OP_CONNECT:
            *event = EPOLLOUT | EPOLLET;
            return 0;
        case PD_OP_SSL_ACCEPT:
            *event = EPOLLIN | EPOLLET;
            return 0;
        case PD_OP_SSL_CONNECT:
            *event = EPOLLOUT | EPOLLET;
            return 0;
        case PD_OP_SSL_SHUTDOWN:
            *event = EPOLLOUT | EPOLLET;
            return 0;
        case PD_OP_EVENT:
            *event = EPOLLIN | EPOLLET;
            return 1;
        case PD_OP_NOTIFY:
            *event = EPOLLIN | EPOLLET;
            return 1;
        default:
            errno = EINVAL;
            return -1;
    }
}

int poll_add (const CPollData *data, int timeout, CPoll *poll)
{
    CPollNode *res = NULL;
    CPollNode *node;
    int event;

    if ((size_t)data->fd >= poll->maxOpenFiles) {
        errno = data->fd < 0 ? EBADF : EMFILE;
        return -1;
    }

    int needRes = _poll_data_get_event(&event, data);
    if (needRes < 0) {
        return -1;
    }

    if (needRes) {
        res = (CPollNode *)malloc(sizeof (CPollNode));
        if (!res) {
            return -1;
        }
    }

    node = (CPollNode *)malloc(sizeof (CPollNode));
    if (node) {
        node->data = *data;
        node->event = event;
        node->inRBTree = 0;
        node->removed = 0;
        node->res = res;
        if (timeout >= 0) {
            _poll_node_set_timeout(timeout, node);
        }

        pthread_mutex_lock(&poll->mutex);
        if (!poll->nodes[data->fd]) {
            if (_poll_add_fd(data->fd, event, node, poll) >= 0) {
                if (timeout >= 0) {
                    _poll_insert_node(node, poll);
                } else {
                    list_add_tail(&node->list, &poll->noTimeoutList);
                }

                poll->nodes[data->fd] = node;
                node = NULL;
            }
        }

        pthread_mutex_unlock(&poll->mutex);
        if (node == NULL) {
            return 0;
        }

        free(node);
    }

    free(res);
    return -1;
}

int poll_del(int fd, CPoll *poll)
{
    CPollNode *node;

    if ((size_t)fd >= poll->maxOpenFiles) {
        errno = fd < 0 ? EBADF : EMFILE;
        return -1;
    }

    pthread_mutex_lock(&poll->mutex);
    node = poll->nodes[fd];
    if (node) {
        poll->nodes[fd] = NULL;

        if (node->inRBTree) {
            _poll_tree_erase(node, poll);
        } else {
            list_del(&node->list);
        }

        _poll_del_fd(fd, node->event, poll);

        node->error = 0;
        node->state = PR_ST_DELETED;
        if (poll->stopped) {
            free(node->res);
            poll->cb((CPollResult *)node, poll->ctx);
        } else {
            node->removed = 1;
            write(poll->pipeWr, &node, sizeof (void *));
        }
    } else {
        errno = ENOENT;
    }

    pthread_mutex_unlock(&poll->mutex);
    return -!node;
}

int poll_mod(const CPollData *data, int timeout, CPoll *poll)
{
    CPollNode *res = NULL;
    CPollNode *node;
    CPollNode *old;
    int need_res;
    int event;

    if ((size_t)data->fd >= poll->maxOpenFiles) {
        errno = data->fd < 0 ? EBADF : EMFILE;
        return -1;
    }

    need_res = _poll_data_get_event(&event, data);
    if (need_res < 0)
        return -1;

    if (need_res) {
        res = (CPollNode *)malloc(sizeof (CPollNode));
        if (!res)
            return -1;
    }

    node = (CPollNode *)malloc(sizeof (CPollNode));
    if (node) {
        node->data = *data;
        node->event = event;
        node->inRBTree = 0;
        node->removed = 0;
        node->res = res;
        if (timeout >= 0) {
            _poll_node_set_timeout(timeout, node);
        }

        pthread_mutex_lock(&poll->mutex);
        old = poll->nodes[data->fd];
        if (old) {
            if (_poll_mod_fd(data->fd, old->event, event, node, poll) >= 0) {
                if (old->inRBTree)
                    _poll_tree_erase(old, poll);
                else
                    list_del(&old->list);

                old->error = 0;
                old->state = PR_ST_MODIFIED;
                if (poll->stopped) {
                    free(old->res);
                    poll->cb((CPollResult *)old, poll->ctx);
                } else {
                    old->removed = 1;
                    write(poll->pipeWr, &old, sizeof (void *));
                }

                if (timeout >= 0)
                    _poll_insert_node(node, poll);
                else
                    list_add_tail(&node->list, &poll->noTimeoutList);

                poll->nodes[data->fd] = node;
                node = NULL;
            }
        } else {
            errno = ENOENT;
        }

        pthread_mutex_unlock(&poll->mutex);
        if (node == NULL)
            return 0;

        free(node);
    }

    free(res);
    return -1;
}

int poll_set_timeout(int fd, int timeout, CPoll *poll)
{
    CPollNode time_node;
    CPollNode *node;

    if ((size_t)fd >= poll->maxOpenFiles) {
        errno = fd < 0 ? EBADF : EMFILE;
        return -1;
    }

    if (timeout >= 0) {
        _poll_node_set_timeout(timeout, &time_node);
    }

    pthread_mutex_lock(&poll->mutex);
    node = poll->nodes[fd];
    if (node) {
        if (node->inRBTree) {
            _poll_tree_erase(node, poll);
        }
        else {
            list_del(&node->list);
        }

        if (timeout >= 0) {
            node->timeout = time_node.timeout;
            _poll_insert_node(node, poll);
        }
        else {
            list_add_tail(&node->list, &poll->noTimeoutList);
        }
    }
    else {
        errno = ENOENT;
    }

    pthread_mutex_unlock(&poll->mutex);
    return -!node;
}

int poll_add_timer(const struct timespec *value, void *context, CPoll *poll)
{
    CPollNode *node = (CPollNode *)malloc(sizeof (CPollNode));
    if (node) {
        memset(&node->data, 0, sizeof (CPollData));
        node->data.operation = PD_OP_TIMER;
        node->data.fd = -1;
        node->data.context = context;
        node->inRBTree = 0;
        node->removed = 0;
        node->res = NULL;

        clock_gettime(CLOCK_MONOTONIC, &node->timeout);
        node->timeout.tv_sec += value->tv_sec;
        node->timeout.tv_nsec += value->tv_nsec;
        if (node->timeout.tv_nsec >= 1000000000) {
            node->timeout.tv_nsec -= 1000000000;
            node->timeout.tv_sec++;
        }

        pthread_mutex_lock(&poll->mutex);
        _poll_insert_node(node, poll);
        pthread_mutex_unlock(&poll->mutex);

        return 0;
    }

    return -1;
}

void poll_stop (CPoll *poll)
{
    CPollNode *node;
    struct list_head *pos, *tmp;
    void *p = NULL;

    write(poll->pipeWr, &p, sizeof (void *));
    pthread_join(poll->tid, NULL);
    poll->stopped = 1;

    pthread_mutex_lock(&poll->mutex);
    poll->nodes[poll->pipeRd] = NULL;
    poll->nodes[poll->pipeWr] = NULL;
    close(poll->pipeWr);
    _poll_handle_pipe(poll);
    close(poll->pipeRd);

    poll->treeFirst = NULL;
    poll->treeLast = NULL;
    while (poll->timeoutTree.rbNode) {
        node = RB_ENTRY(poll->timeoutTree.rbNode, CPollNode, rb);
        rb_erase(&node->rb, &poll->timeoutTree);
        list_add(&node->list, &poll->timeoutList);
    }

    list_splice_init(&poll->noTimeoutList, &poll->timeoutList);
    list_for_each_safe(pos, tmp, &poll->timeoutList) {
        node = list_entry(pos, CPollNode, list);
        list_del(&node->list);
        if (node->data.fd >= 0) {
            poll->nodes[node->data.fd] = NULL;
            _poll_del_fd(node->data.fd, node->event, poll);
        }

        node->error = 0;
        node->state = PR_ST_STOPPED;
        free(node->res);
        poll->cb((CPollResult*)node, poll->ctx);
    }

    pthread_mutex_unlock(&poll->mutex);
}
