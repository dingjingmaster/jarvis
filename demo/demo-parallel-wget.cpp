//
// Created by dingjing on 8/25/22.
//

#include <string>
#include <stdio.h>
#include <utility>
#include <string.h>

#include "../app/factory/workflow.h"
#include "../app/manager/facilities.h"
#include "../app/factory/task-factory.h"
#include "../app/protocol/http/http-util.h"
#include "../app/protocol/http/http-message.h"

using namespace protocol;

#define REDIRECT_MAX    5
#define RETRY_MAX       2

struct tutorial_series_context
{
    std::string url;
    int state;
    int error;
    HttpResponse resp;
};

void callback(const ParallelWork *pwork)
{
    tutorial_series_context *ctx;
    const void *body;
    size_t size;
    size_t i;

    for (i = 0; i < pwork->size(); i++) {
        ctx = (tutorial_series_context *)pwork->seriesAt(i)->getContext();
        printf("%s\n", ctx->url.c_str());
        if (ctx->state == TASK_STATE_SUCCESS) {
            ctx->resp.getParsedBody(&body, &size);
            printf("%zu%s\n", size, ctx->resp.isChunked() ? " chunked" : "");
            fwrite(body, 1, size, stdout);
            printf("\n");
        } else
            printf("ERROR! state = %d, error = %d\n", ctx->state, ctx->error);

        delete ctx;
    }
}

int main(int argc, char *argv[])
{
    ParallelWork *pwork = Workflow::createParallelWork(callback);
    SeriesWork *series;
    HttpTask *task;
    HttpRequest *req;
    tutorial_series_context *ctx;
    int i;

    for (i = 1; i < argc; i++) {
        std::string url(argv[i]);

        if (strncasecmp(argv[i], "http://", 7) != 0 &&
            strncasecmp(argv[i], "https://", 8) != 0) {
            url = "http://" +url;
        }

        task = TaskFactory::createHttpTask(url, REDIRECT_MAX, RETRY_MAX,
                                               [](HttpTask *task, void*)
                                               {
                                                   tutorial_series_context *ctx =
                                                           (tutorial_series_context *)seriesOf(task)->getContext();
                                                   ctx->state = task->getState();
                                                   ctx->error = task->getError();
                                                   ctx->resp = std::move(*task->getResp());
                                               }, nullptr);

        req = task->getReq();
        req->addHeaderPair("Accept", "*/*");
        req->addHeaderPair("User-Agent", "Wget/1.14 (linux-gnu)");
        req->addHeaderPair("Connection", "close");

        ctx = new tutorial_series_context;
        ctx->url = std::move(url);
        series = Workflow::createSeriesWork(task, nullptr);
        series->setContext(ctx);
        pwork->addSeries(series);
    }

    Facilities::WaitGroup waitGroup(1);

    Workflow::startSeriesWork(pwork, [&waitGroup](const SeriesWork *) {
        waitGroup.done();
    });

    waitGroup.wait();
    return 0;
}

