//
// Created by dingjing on 8/24/22.
//

#include <netdb.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>

#include "../app/manager/facilities.h"
#include "../app/factory/task-factory.h"

#include "../app/protocol/http/http-util.h"
#include "../app/protocol/http/http-message.h"

#define REDIRECT_MAX    5
#define RETRY_MAX       2

void wget_callback(HttpTask *task)
{
    protocol::HttpRequest *req = task->getReq();
    protocol::HttpResponse *resp = task->getResp();
    int state = task->getState();
    int error = task->getError();

    switch (state)
    {
        case TASK_STATE_SYS_ERROR:
            fprintf(stderr, "system error: %s\n", strerror(error));
            break;
        case TASK_STATE_DNS_ERROR:
            fprintf(stderr, "DNS error: %s\n", gai_strerror(error));
            break;
        case TASK_STATE_SSL_ERROR:
            fprintf(stderr, "SSL error: %d\n", error);
            break;
        case TASK_STATE_TASK_ERROR:
            fprintf(stderr, "Task error: %d\n", error);
            break;
        case TASK_STATE_SUCCESS:
            break;
    }

    if (state != TASK_STATE_SUCCESS)
    {
        fprintf(stderr, "Failed. Press Ctrl-C to exit.\n");
        return;
    }

    std::string name;
    std::string value;

    /* Print request. */
    fprintf(stderr, "%s %s %s\r\n", req->getMethod(),
            req->getHttpVersion(),
            req->getRequestUri());

    protocol::HttpHeaderCursor req_cursor(req);
    while (req_cursor.next(name, value))
        fprintf(stderr, "%s: %s\r\n", name.c_str(), value.c_str());
    fprintf(stderr, "\r\n");

    /* Print response header. */
    fprintf(stderr, "%s %s %s\r\n", resp->getHttpVersion(),
            resp->getStatusCode(),
            resp->getReasonPhrase());

    protocol::HttpHeaderCursor resp_cursor(resp);

    while (resp_cursor.next(name, value))
        fprintf(stderr, "%s: %s\r\n", name.c_str(), value.c_str());
    fprintf(stderr, "\r\n");

    /* Print response body. */
    const void *body;
    size_t body_len;

    resp->getParsedBody(&body, &body_len);
    fwrite(body, 1, body_len, stdout);
    fflush(stdout);

    fprintf(stderr, "\nSuccess. Press Ctrl-C to exit.\n");
}

static Facilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
    wait_group.done();
}


int main(int argc, char *argv[])
{
    HttpTask *task;

    if (argc != 2)
    {
        fprintf(stderr, "USAGE: %s <http URL>\n", argv[0]);
        exit(1);
    }

    signal(SIGINT, sig_handler);

    std::string url = argv[1];
    if (strncasecmp(argv[1], "http://", 7) != 0 &&
        strncasecmp(argv[1], "https://", 8) != 0)
    {
        url = "http://" + url;
    }

    task = TaskFactory::createHttpTask(url, REDIRECT_MAX, RETRY_MAX, wget_callback);
    protocol::HttpRequest *req = task->getReq();
    req->addHeaderPair("Accept", "*/*");
    req->addHeaderPair("User-Agent", "Wget/1.14 (linux-gnu)");
    req->addHeaderPair("Connection", "close");
    task->start();

    wait_group.wait();

    return 0;
}
