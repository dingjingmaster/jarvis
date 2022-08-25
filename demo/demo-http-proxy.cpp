//
// Created by dingjing on 8/25/22.
//

#include <stdio.h>
#include <utility>
#include <signal.h>
#include <stdlib.h>

#include "../app/factory/workflow.h"
#include "../app/protocol/http/http-util.h"
#include "../app/protocol/http/http-message.h"

#include "../app/manager/facilities.h"
#include "../app/modules/http-server.h"

struct tutorial_series_context
{
    std::string url;
    HttpTask *proxy_task;
    bool is_keep_alive;
};

void reply_callback(HttpTask *proxy_task)
{
    SeriesWork *series = seriesOf(proxy_task);
    tutorial_series_context *context =
            (tutorial_series_context *)series->getContext();
    auto *proxy_resp = proxy_task->getResp();
    size_t size = proxy_resp->getOutputBodySize();

    if (proxy_task->getState() == TASK_STATE_SUCCESS)
        fprintf(stderr, "%s: Success. Http Status: %s, BodyLength: %zu\n",
                context->url.c_str(), proxy_resp->getStatusCode(), size);
    else /* WFT_STATE_SYS_ERROR*/
        fprintf(stderr, "%s: Reply failed: %s, BodyLength: %zu\n",
                context->url.c_str(), strerror(proxy_task->getError()), size);
}

void http_callback(HttpTask *task)
{
    int state = task->getState();
    int error = task->getError();
    auto *resp = task->getResp();
    SeriesWork *series = seriesOf(task);
    tutorial_series_context *context =
            (tutorial_series_context *)series->getContext();
    auto *proxy_resp = context->proxy_task->getResp();

    if (state == TASK_STATE_SUCCESS) {
        const void *body;
        size_t len;

        /* set a callback for getting reply status. */
        context->proxy_task->setCallback(reply_callback);

        /* Copy the remote webserver's response, to proxy response. */
        resp->getParsedBody(&body, &len);
        resp->appendOutputBodyNocopy(body, len);
        *proxy_resp = std::move(*resp);
        if (!context->is_keep_alive)
            proxy_resp->setHeaderPair("Connection", "close");
    }
    else
    {
        const char *err_string;

        if (state == TASK_STATE_SYS_ERROR)
            err_string = strerror(error);
        else if (state == TASK_STATE_DNS_ERROR)
            err_string = gai_strerror(error);
        else if (state == TASK_STATE_SSL_ERROR)
            err_string = "SSL error";
        else /* if (state == WFT_STATE_TASK_ERROR) */
            err_string = "URL error (Cannot be a HTTPS proxy)";

        fprintf(stderr, "%s: Fetch failed. state = %d, error = %d: %s\n",
                context->url.c_str(), state, error, err_string);

        /* As a tutorial, make it simple. And ignore reply status. */
        proxy_resp->setStatusCode("404");
        proxy_resp->appendOutputBodyNocopy(
                "<html>404 Not Found.</html>", 27);
    }
}

void process(HttpTask *proxy_task)
{
    auto *req = proxy_task->getReq();
    SeriesWork *series = seriesOf(proxy_task);
    HttpTask *http_task; /* for requesting remote webserver. */

    tutorial_series_context *context = new tutorial_series_context;
    context->url = req->getRequestUri();
    context->proxy_task = proxy_task;

    series->setContext(context);
    series->setCallback([](const SeriesWork *series) {
        delete (tutorial_series_context *)series->getContext();
    });

    context->is_keep_alive = req->isKeepAlive();
    http_task = TaskFactory::createHttpTask(req->getRequestUri(), 0, 0, http_callback);

    const void *body;
    size_t len;

    /* Copy user's request to the new task's reuqest using std::move() */
    req->setRequestUri(http_task->getReq()->getRequestUri());
    req->getParsedBody(&body, &len);
    req->appendOutputBodyNocopy(body, len);
    *http_task->getReq() = std::move(*req);

    /* also, limit the remote webserver response size. */
    http_task->getResp()->setSizeLimit(200 * 1024 * 1024);

    *series << http_task;
}

static Facilities::WaitGroup waitGroup(1);

void sig_handler(int signo)
{
    waitGroup.done();
}

int main(int argc, char *argv[])
{
    unsigned short port;

    if (argc != 2)
    {
        fprintf(stderr, "USAGE: %s <port>\n", argv[0]);
        exit(1);
    }

    port = atoi(argv[1]);
    signal(SIGINT, sig_handler);

    ServerParams params = HTTP_SERVER_PARAMS_DEFAULT;
    /* for safety, limit request size to 8MB. */
    params.requestSizeLimit = 8 * 1024 * 1024;

    HttpServer server(&params, process);

    if (server.start(port) == 0) {
        waitGroup.wait();
        server.stop();
    } else {
        perror("Cannot start server");
        exit(1);
    }

    return 0;
}

