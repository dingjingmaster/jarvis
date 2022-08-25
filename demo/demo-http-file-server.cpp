//
// Created by dingjing on 8/25/22.
//

#include <string>
#include <fcntl.h>
#include <stdio.h>
#include <utility>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

#include "../app/modules/server.h"
#include "../app/factory/workflow.h"
#include "../app/manager/facilities.h"
#include "../app/factory/task-factory.h"
#include "../app/protocol/http/http-util.h"
#include "../app/protocol/http/http-message.h"
#include "../app/modules/http-server.h"

using namespace protocol;

void pread_callback(FileIOTask *task)
{
    FileIOArgs *args = task->getArgs();
    long ret = task->getRetVal();
    HttpResponse *resp = (HttpResponse *)task->mUserData;

    close(args->fd);
    if (task->getState() != TASK_STATE_SUCCESS || ret < 0) {
        resp->setStatusCode("503");
        resp->appendOutputBody("<html>503 Internal Server Error.</html>");
    }
    else /* Use '_nocopy' carefully. */
        resp->appendOutputBodyNocopy(args->buf, ret);
}

void process(HttpTask *server_task, const char *root)
{
    HttpRequest *req = server_task->getReq();
    HttpResponse *resp = server_task->getResp();
    const char *uri = req->getRequestUri();
    const char *p = uri;

    printf("Request-URI: %s\n", uri);
    while (*p && *p != '?')
        p++;

    std::string abs_path(uri, p - uri);
    abs_path = root + abs_path;
    if (abs_path.back() == '/')
        abs_path += "index.html";

    resp->addHeaderPair("Server", "Sogou C++ Workflow Server");

    int fd = open(abs_path.c_str(), O_RDONLY);
    if (fd >= 0) {
        size_t size = lseek(fd, 0, SEEK_END);
        void *buf = malloc(size); /* As an example, assert(buf != NULL); */
        FileIOTask *pread_task;

        pread_task = TaskFactory::createPReadTask(fd, buf, size, 0, pread_callback);
        /* To implement a more complicated server, please use series' context
         * instead of tasks' user_data to pass/store internal data. */
        pread_task->mUserData = resp;	/* pass resp pointer to pread task. */
        server_task->mUserData = buf;	/* to free() in callback() */
        server_task->setCallback([](HttpTask *t){ free(t->mUserData); });
        seriesOf(server_task)->pushBack(pread_task);
    }
    else
    {
        resp->setStatusCode("404");
        resp->appendOutputBody("<html>404 Not Found.</html>");
    }
}

static Facilities::WaitGroup waitGroup(1);

void sig_handler(int signo)
{
    waitGroup.done();
}

int main(int argc, char *argv[])
{
    if (argc != 2 && argc != 3 && argc != 5)
    {
        fprintf(stderr, "%s <port> [root path] [cert file] [key file]\n",
                argv[0]);
        exit(1);
    }

    signal(SIGINT, sig_handler);

    unsigned short port = atoi(argv[1]);
    const char *root = (argc >= 3 ? argv[2] : ".");
    auto&& proc = std::bind(process, std::placeholders::_1, root);
    HttpServer server(proc);
    std::string scheme;
    int ret;

    if (argc == 5) {
        ret = server.start(port, argv[3], argv[4]);	/* https server */
        scheme = "https://";
    } else {
        ret = server.start(port);
        scheme = "http://";
    }

    if (ret < 0) {
        perror("start server");
        exit(1);
    }

    /* Test the server. */
    auto&& create = [&scheme, port](RepeaterTask *)->SubTask *{
        char buf[1024];
        *buf = '\0';
        printf("Input file name: (Ctrl-D to exit): ");
        scanf("%1023s", buf);
        if (*buf == '\0')
        {
            printf("\n");
            return NULL;
        }

        std::string url = scheme + "127.0.0.1:" + std::to_string(port) + "/" + buf;
        HttpTask *task = TaskFactory::createHttpTask(url, 0, 0,
                                                           [](HttpTask *task) {
                                                               auto *resp = task->getResp();
                                                               if (strcmp(resp->getStatusCode(), "200") == 0) {
                                                                   std::string body = protocol::HttpUtil::decodeChunkedBody(resp);
                                                                   fwrite(body.c_str(), body.size(), 1, stdout);
                                                                   printf("\n");
                                                               } else {
                                                                   printf("%s %s\n", resp->getStatusCode(), resp->getReasonPhrase());
                                                               }
                                                           });

        return task;
    };

    Facilities::WaitGroup wg(1);
    RepeaterTask *repeater;
    repeater = TaskFactory::createRepeaterTask(create, [&wg](RepeaterTask *) {
        wg.done();
    });

    repeater->start();
    wg.wait();

    server.stop();
    return 0;
}
