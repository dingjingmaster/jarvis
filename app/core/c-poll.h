//
// Created by dingjing on 8/11/22.
//

#ifndef JARVIS_C_POLL_H
#define JARVIS_C_POLL_H

#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <openssl/ssl.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct _CPoll           CPoll;
typedef struct _CPollData       CPollData;
typedef struct _CPollResult     CPollResult;
typedef struct _CPollParams     CPollParams;
typedef struct _CPollMessage    CPollMessage;

struct _CPollMessage
{
    int (*append) (const void*, size_t*, CPollMessage*);
    char data[0];
};

struct _CPollData
{
#define PD_OP_READ              1
#define PD_OP_WRITE             2
#define PD_OP_LISTEN            3
#define PD_OP_CONNECT		    4
#define PD_OP_SSL_READ		    PD_OP_READ
#define PD_OP_SSL_WRITE		    PD_OP_WRITE
#define PD_OP_SSL_ACCEPT	    5
#define PD_OP_SSL_CONNECT	    6
#define PD_OP_SSL_SHUTDOWN	    7
#define PD_OP_EVENT			    8
#define PD_OP_NOTIFY		    9
#define PD_OP_TIMER			    10

    short                       operation;
    unsigned short              iovec;
    int                         fd;
    union {
        SSL*   ssl;
        void* (*accept)(const struct sockaddr*, socklen_t, int, void*);
        void* (*event) (void*);
        void* (*notify)(void*, void*);
    };

    void*                       context;

    union {
        CPollMessage*           message;
        struct iovec*           writeIov;
        void*                   result;
    };
};

struct _CPollResult
{
#define PR_ST_SUCCESS		    0
#define PR_ST_FINISHED		    1
#define PR_ST_ERROR			    2
#define PR_ST_DELETED		    3
#define PR_ST_MODIFIED		    4
#define PR_ST_STOPPED		    5

    int                         state;
    int                         error;
    CPollData*                  data;
};

struct _CPollParams
{
    size_t                      maxOpenFiles;
    CPollMessage* (*createMessage) (void*);
    int (*partialWritten) (size_t, void *);
    void (*callback) (CPollResult *, void *);
    void* context;
};


CPoll*  poll_create         (const CPollParams* params);
int     poll_start          (CPoll* poll);
int     poll_add            (const CPollData* data, int timeout, CPoll* poll);
int     poll_del            (int fd, CPoll* poll);
int     poll_mod            (const CPollData* data, int timeout, CPoll* poll);
int     poll_set_timeout    (int fd, int timeout, CPoll* poll);
int     poll_add_timer      (const struct timespec* val, void* context, CPoll* poll);
void    poll_stop           (CPoll* poll);
void    poll_destroy        (CPoll* poll);


#ifdef __cplusplus
};
#endif
#endif //JARVIS_C_POLL_H
