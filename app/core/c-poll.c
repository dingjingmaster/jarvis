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
    struct timespec         timeout;
    CPollNode*              res;
};

struct _CPoll
{
    size_t                  maxOpenFiles;
    CPollMessage* (*createMessage) (void *);
    int (*partialWritten)(size_t, void *);
    void (*cb)(CPollResult *, void *);
    void *ctx;

    pthread_t tid;
    int pfd;
    int timerfd;
    int pipe_rd;
    int pipe_wr;
    int stopped;
    RBRoot timeo_tree;
    RBNode *tree_first;
    RBNode *tree_last;
    struct list_head timeo_list;
    struct list_head no_timeo_list;
    CPollNode **nodes;
    pthread_mutex_t mutex;
    char buf[C_POLL_BUF_SIZE];
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

static inline int _poll_mod_fd(int fd, int old_event, int new_event, void *data, CPoll *poll)
{
    struct epoll_event ev = {
            .events		=	new_event,
            .data		=	{
                    .ptr	=	data
            }
    };
    return epoll_ctl(poll->pfd, EPOLL_CTL_MOD, fd, &ev);
}

static inline int _poll_create_timerfd()
{
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

static inline int _poll_set_timerfd(int fd, const struct timespec *abstime,
                                       CPoll *poll)
{
    struct itimerspec timer = {
            .it_interval	=	{ },
            .it_value		=	*abstime
    };
    return timerfd_settime(fd, TFD_TIMER_ABSTIME, &timer, NULL);
}

typedef struct epoll_event _poll_event_t;

static inline int _poll_wait(_poll_event_t *events, int maxevents,
                                CPoll *poll)
{
    return epoll_wait(poll->pfd, events, maxevents, -1);
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
    RBNode **p = &poll->timeo_tree.rbNode;
    RBNode *parent = NULL;
    CPollNode *entry;

    entry = RB_ENTRY(poll->tree_last, CPollNode, rb);
    if (!*p) {
        poll->tree_first = &node->rb;
        poll->tree_last = &node->rb;
    } else if (__timeout_cmp(node, entry) >= 0) {
        parent = poll->tree_last;
        p = &parent->rbRight;
        poll->tree_last = &node->rb;
    } else {
        do {
            parent = *p;
            entry = RB_ENTRY(*p, CPollNode, rb);
            if (__timeout_cmp(node, entry) < 0)
                p = &(*p)->rbLeft;
            else
                p = &(*p)->rbRight;
        } while (*p);

        if (p == &poll->tree_first->rbLeft)
            poll->tree_first = &node->rb;
    }

    node->inRBTree = 1;
    rb_link_node(&node->rb, parent, p);
    rb_insert_color(&node->rb, &poll->timeo_tree);
}

static inline void _poll_tree_erase(CPollNode *node, CPoll *poll)
{
    if (&node->rb == poll->tree_first)
        poll->tree_first = rb_next(&node->rb);

    if (&node->rb == poll->tree_last)
        poll->tree_last = rb_prev(&node->rb);

    rb_erase(&node->rb, &poll->timeo_tree);
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
        res = (CPollNode*)malloc(sizeof (CPollNode));
        if (!res)
            return -1;

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

    if (event == node->event)
        return 0;

    pthread_mutex_lock(&poll->mutex);
    if (!node->removed) {
        ret = _poll_mod_fd(node->data.fd, node->event, event, node, poll);
        if (ret >= 0)
            node->event = event;
    } else {
        ret = 0;
    }

    pthread_mutex_unlock(&poll->mutex);

    return ret;
}

static void _poll_handle_read(CPollNode *node, CPoll *poll)
{
    ssize_t nleft;
    size_t n;
    char *p;

    while (1) {
        p = poll->buf;
        if (!node->data.ssl) {
            nleft = read(node->data.fd, p, C_POLL_BUF_SIZE);
            if (nleft < 0) {
                if (errno == EAGAIN)
                    return;
            }
        } else {
            nleft = SSL_read(node->data.ssl, p, C_POLL_BUF_SIZE);
            if (nleft < 0) {
                if (_poll_handle_ssl_error(node, nleft, poll) >= 0)
                    return;
            }
        }

        if (nleft <= 0)
            break;

        do {
            n = nleft;
            if (_poll_append_message(p, &n, node, poll) >= 0) {
                nleft -= n;
                p += n;
            } else {
                nleft = -1;
            }
        } while (nleft > 0);

        if (nleft < 0)
            break;
    }

    if (_poll_remove_node(node, poll))
        return;

    if (nleft == 0) {
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
    struct iovec *iov = node->data.writeIOV;
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

    node->data.writeIOV = iov;
    if (node->data.iovec > 0 && ret >= 0)
    {
        if (count == 0)
            return;

        if (poll->partialWritten(count, node->data.context) >= 0)
            return;
    }

    if (_poll_remove_node(node, poll))
        return;

    if (node->data.iovec == 0)
    {
        node->error = 0;
        node->state = PR_ST_FINISHED;
    }
    else
    {
        node->error = errno;
        node->state = PR_ST_ERROR;
    }

    poll->cb((CPollResult *)node, poll->ctx);
}

static void _poll_handle_listen(CPollNode *node, CPoll *poll)
{
    CPollNode *res = node->res;
    struct sockaddr_storage ss;
    socklen_t len;
    int sockfd;
    void *p;

    while (1) {
        len = sizeof (struct sockaddr_storage);
        sockfd = accept(node->data.fd, (struct sockaddr *)&ss, &len);
        if (sockfd < 0) {
            if (errno == EAGAIN || errno == EMFILE || errno == ENFILE)
                return;
            else if (errno == ECONNABORTED)
                continue;
            else
                break;
        }

        p = node->data.accept((const struct sockaddr *)&ss, len,
                              sockfd, node->data.context);
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

    if (_poll_remove_node(node, poll))
        return;

    node->error = errno;
    node->state = PR_ST_ERROR;
    free(node->res);
    poll->cb((CPollResult *)node, poll->ctx);
}

static void _poll_handle_connect(CPollNode *node,
                                    CPoll *poll)
{
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
    CPollNode **node = (CPollNode **)poll->buf;
    int stop = 0;
    int n;
    int i;

    n = read(poll->pipe_rd, node, C_POLL_BUF_SIZE) / sizeof (void *);
    for (i = 0; i < n; i++) {
        if (node[i]) {
            free(node[i]->res);
            poll->cb((CPollResult *)node[i], poll->ctx);
        } else {
            stop = 1;
        }
    }

    return stop;
}

static void _poll_handle_timeout(const CPollNode *time_node, CPoll *poll)
{
    CPollNode *node;
    struct list_head *pos, *tmp;
    LIST_HEAD(timeo_list);

    pthread_mutex_lock(&poll->mutex);
    list_for_each_safe(pos, tmp, &poll->timeo_list) {
        node = list_entry(pos, CPollNode, list);
        if (__timeout_cmp(node, time_node) <= 0) {
            if (node->data.fd >= 0) {
                poll->nodes[node->data.fd] = NULL;
                _poll_del_fd(node->data.fd, node->event, poll);
            }

            list_move_tail(pos, &timeo_list);
        } else {
            break;
        }
    }

    if (poll->tree_first) {
        while (1) {
            node = RB_ENTRY(poll->tree_first, CPollNode, rb);
            if (__timeout_cmp(node, time_node) < 0) {
                if (node->data.fd >= 0) {
                    poll->nodes[node->data.fd] = NULL;
                    _poll_del_fd(node->data.fd, node->event, poll);
                }

                poll->tree_first = rb_next(poll->tree_first);
                rb_erase(&node->rb, &poll->timeo_tree);
                list_add_tail(&node->list, &timeo_list);
                if (poll->tree_first)
                    continue;

                poll->tree_last = NULL;
            }
            break;
        }
    }

    pthread_mutex_unlock(&poll->mutex);
    while (!list_empty(&timeo_list)) {
        node = list_entry(timeo_list.next, CPollNode, list);
        list_del(&node->list);

        node->error = ETIMEDOUT;
        node->state = PR_ST_ERROR;
        free(node->res);
        poll->cb((CPollResult *)node, poll->ctx);
    }
}

static void _poll_set_timer(CPoll *poll)
{
    CPollNode *node = NULL;
    CPollNode *first;
    struct timespec abstime;

    pthread_mutex_lock(&poll->mutex);
    if (!list_empty(&poll->timeo_list))
        node = list_entry(poll->timeo_list.next, CPollNode, list);

    if (poll->tree_first) {
        first = RB_ENTRY(poll->tree_first, CPollNode, rb);
        if (!node || __timeout_cmp(first, node) < 0)
            node = first;
    }

    if (node) {
        abstime = node->timeout;
    } else {
        abstime.tv_sec = 0;
        abstime.tv_nsec = 0;
    }

    _poll_set_timerfd(poll->timerfd, &abstime, poll);
    pthread_mutex_unlock(&poll->mutex);
}

static void* _poll_hread_routine(void *arg)
{
    CPoll *poll = (CPoll *)arg;
    _poll_event_t events[C_POLL_EVENTS_MAX];
    CPollNode time_node;
    CPollNode *node;
    int has_pipe_event;
    int nevents;
    int i;

    while (1) {
        _poll_set_timer(poll);
        nevents = _poll_wait(events, C_POLL_EVENTS_MAX, poll);
        clock_gettime(CLOCK_MONOTONIC, &time_node.timeout);
        has_pipe_event = 0;
        for (i = 0; i < nevents; i++) {
            node = (CPollNode *)_poll_event_data(&events[i]);
            if (node > (CPollNode *)1) {
                switch (node->data.operation) {
                    case PD_OP_READ:
                        _poll_handle_read(node, poll);
                        break;
                    case PD_OP_WRITE:
                        _poll_handle_write(node, poll);
                        break;
                    case PD_OP_LISTEN:
                        _poll_handle_listen(node, poll);
                        break;
                    case PD_OP_CONNECT:
                        _poll_handle_connect(node, poll);
                        break;
                    case PD_OP_SSL_ACCEPT:
                        _poll_handle_ssl_accept(node, poll);
                        break;
                    case PD_OP_SSL_CONNECT:
                        _poll_handle_ssl_connect(node, poll);
                        break;
                    case PD_OP_SSL_SHUTDOWN:
                        _poll_handle_ssl_shutdown(node, poll);
                        break;
                    case PD_OP_EVENT:
                        _poll_handle_event(node, poll);
                        break;
                    case PD_OP_NOTIFY:
                        _poll_handle_notify(node, poll);
                        break;
                }
            } else if (node == (CPollNode *)1) {
                has_pipe_event = 1;
            }
        }

        if (has_pipe_event) {
            if (_poll_handle_pipe(poll))
                break;
        }

        _poll_handle_timeout(&time_node, poll);
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
    int pipefd[2];

    if (pipe(pipefd) >= 0) {
        if (_poll_add_fd(pipefd[0], EPOLLIN, (void *)1, poll) >= 0) {
            poll->pipe_rd = pipefd[0];
            poll->pipe_wr = pipefd[1];
            return 0;
        }

        close(pipefd[0]);
        close(pipefd[1]);
    }

    return -1;
}

static int _poll_create_timer(CPoll *poll)
{
    int timerfd = _poll_create_timerfd();

    if (timerfd >= 0) {
        if (_poll_add_timerfd(timerfd, poll) >= 0) {
            poll->timerfd = timerfd;
            return 0;
        }

        close(timerfd);
    }

    return -1;
}

CPoll *_poll_create(void **nodes_buf, const CPollParams *params)
{
    CPoll *poll = (CPoll *)malloc(sizeof (CPoll));
    int ret;

    if (!poll)
        return NULL;

    poll->pfd = _poll_create_pfd();
    if (poll->pfd >= 0) {
        if (_poll_create_timer(poll) >= 0) {
            ret = pthread_mutex_init(&poll->mutex, NULL);
            if (ret == 0) {
                poll->nodes = (CPollNode **)nodes_buf;
                poll->maxOpenFiles = params->maxOpenFiles;
                poll->createMessage = params->createMessage;
                poll->partialWritten = params->partialWritten;
                poll->cb = params->callback;
                poll->ctx = params->context;

                poll->timeo_tree.rbNode = NULL;
                poll->tree_first = NULL;
                poll->tree_last = NULL;
                INIT_LIST_HEAD(&poll->timeo_list);
                INIT_LIST_HEAD(&poll->no_timeo_list);

                poll->stopped = 1;
                return poll;
            }

            errno = ret;
            close(poll->timerfd);
        }

        close(poll->pfd);
    }

    free(poll);
    return NULL;
}

CPoll* poll_create(const CPollParams *params)
{
    CPoll* poll;
    void** nodesBuf = (void **)calloc(params->maxOpenFiles, sizeof (void *));

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
    pthread_mutex_destroy(&poll->mutex);
    close(poll->timerfd);
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
    pthread_t tid;
    int ret;

    pthread_mutex_lock(&poll->mutex);
    if (_poll_open_pipe(poll) >= 0) {
        ret = pthread_create(&tid, NULL, _poll_hread_routine, poll);
        if (ret == 0) {
            poll->tid = tid;
            poll->stopped = 0;
        } else {
            errno = ret;
            close(poll->pipe_wr);
            close(poll->pipe_rd);
        }
    }

    pthread_mutex_unlock(&poll->mutex);
    return -poll->stopped;
}

static void _poll_insert_node(CPollNode *node, CPoll *poll)
{
    CPollNode *end;

    end = list_entry(poll->timeo_list.prev, CPollNode, list);
    if (list_empty(&poll->timeo_list)) {
        list_add(&node->list, &poll->timeo_list);
        end = RB_ENTRY(poll->tree_first, CPollNode, rb);
    } else if (__timeout_cmp(node, end) >= 0) {
        list_add_tail(&node->list, &poll->timeo_list);
        return;
    } else {
        _poll_tree_insert(node, poll);
        if (&node->rb != poll->tree_first)
            return;

        end = list_entry(poll->timeo_list.next, CPollNode, list);
    }

    if (!poll->tree_first || __timeout_cmp(node, end) < 0)
        _poll_set_timerfd(poll->timerfd, &node->timeout, poll);
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
    int need_res;
    int event;

    if ((size_t)data->fd >= poll->maxOpenFiles)
    {
        errno = data->fd < 0 ? EBADF : EMFILE;
        return -1;
    }

    need_res = _poll_data_get_event(&event, data);
    if (need_res < 0)
        return -1;

    if (need_res)
    {
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
        if (timeout >= 0)
            _poll_node_set_timeout(timeout, node);

        pthread_mutex_lock(&poll->mutex);
        if (!poll->nodes[data->fd]) {
            if (_poll_add_fd(data->fd, event, node, poll) >= 0) {
                if (timeout >= 0) {
                    _poll_insert_node(node, poll);
                } else {
                    list_add_tail(&node->list, &poll->no_timeo_list);
                }

                poll->nodes[data->fd] = node;
                node = NULL;
            }
        }

        pthread_mutex_unlock(&poll->mutex);
        if (node == NULL)
            return 0;

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
            write(poll->pipe_wr, &node, sizeof (void *));
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
        if (timeout >= 0)
            _poll_node_set_timeout(timeout, node);

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
                    write(poll->pipe_wr, &old, sizeof (void *));
                }

                if (timeout >= 0)
                    _poll_insert_node(node, poll);
                else
                    list_add_tail(&node->list, &poll->no_timeo_list);

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

    if ((size_t)fd >= poll->maxOpenFiles)
    {
        errno = fd < 0 ? EBADF : EMFILE;
        return -1;
    }

    if (timeout >= 0)
        _poll_node_set_timeout(timeout, &time_node);

    pthread_mutex_lock(&poll->mutex);
    node = poll->nodes[fd];
    if (node)
    {
        if (node->inRBTree)
            _poll_tree_erase(node, poll);
        else
            list_del(&node->list);

        if (timeout >= 0)
        {
            node->timeout = time_node.timeout;
            _poll_insert_node(node, poll);
        }
        else
            list_add_tail(&node->list, &poll->no_timeo_list);
    }
    else
        errno = ENOENT;

    pthread_mutex_unlock(&poll->mutex);
    return -!node;
}

int poll_add_timer(const struct timespec *value, void *context,
                     CPoll *poll)
{
    CPollNode *node;

    node = (CPollNode *)malloc(sizeof (CPollNode));
    if (node)
    {
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
        if (node->timeout.tv_nsec >= 1000000000)
        {
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

    write(poll->pipe_wr, &p, sizeof (void *));
    pthread_join(poll->tid, NULL);
    poll->stopped = 1;

    pthread_mutex_lock(&poll->mutex);
    poll->nodes[poll->pipe_rd] = NULL;
    poll->nodes[poll->pipe_wr] = NULL;
    close(poll->pipe_wr);
    _poll_handle_pipe(poll);
    close(poll->pipe_rd);

    poll->tree_first = NULL;
    poll->tree_last = NULL;
    while (poll->timeo_tree.rbNode) {
        node = RB_ENTRY(poll->timeo_tree.rbNode, CPollNode, rb);
        rb_erase(&node->rb, &poll->timeo_tree);
        list_add(&node->list, &poll->timeo_list);
    }

    list_splice_init(&poll->no_timeo_list, &poll->timeo_list);
    list_for_each_safe(pos, tmp, &poll->timeo_list) {
        node = list_entry(pos, CPollNode, list);
        list_del(&node->list);
        if (node->data.fd >= 0) {
            poll->nodes[node->data.fd] = NULL;
            _poll_del_fd(node->data.fd, node->event, poll);
        }

        node->error = 0;
        node->state = PR_ST_STOPPED;
        free(node->res);
        poll->cb((CPollResult *)node, poll->ctx);
    }

    pthread_mutex_unlock(&poll->mutex);
}
