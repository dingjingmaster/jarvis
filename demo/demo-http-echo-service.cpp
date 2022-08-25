//
// Created by dingjing on 8/25/22.
//

#include <string>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "../app/modules/server.h"
#include "../app/manager/facilities.h"
#include "../app/modules/http-server.h"
#include "../app/protocol/http/http-util.h"
#include "../app/protocol/http/http-message.h"

void process(HttpTask *server_task)
{
    protocol::HttpRequest *req = server_task->getReq();
    protocol::HttpResponse *resp = server_task->getResp();
    long long seq = server_task->getTaskSeq();
    protocol::HttpHeaderCursor cursor(req);
    std::string name;
    std::string value;
    char buf[8192];
    int len;

    /* Set response message body. */
    resp->appendOutputBodyNocopy("<html>", 6);
    len = snprintf(buf, 8192, "<p>%s %s %s</p>", req->getMethod(),
                   req->getRequestUri(), req->getHttpVersion());
    resp->appendOutputBody(buf, len);

    while (cursor.next(name, value)) {
        len = snprintf(buf, 8192, "<p>%s: %s</p>", name.c_str(), value.c_str());
        resp->appendOutputBody(buf, len);
    }

    resp->appendOutputBodyNocopy("</html>", 7);

    /* Set status line if you like. */
    resp->setHttpVersion("HTTP/1.1");
    resp->setStatusCode("200");
    resp->setReasonPhrase("OK");

    resp->addHeaderPair("Content-Type", "text/html");
    if (seq == 9) /* no more than 10 requests on the same connection. */
        resp->addHeaderPair("Connection", "close");

    /* print some log */
    char addrstr[128];
    struct sockaddr_storage addr;
    socklen_t l = sizeof addr;
    unsigned short port = 0;

    server_task->getPeerAddr((struct sockaddr *)&addr, &l);
    if (addr.ss_family == AF_INET)
    {
        struct sockaddr_in *sin = (struct sockaddr_in *)&addr;
        inet_ntop(AF_INET, &sin->sin_addr, addrstr, 128);
        port = ntohs(sin->sin_port);
    }
    else if (addr.ss_family == AF_INET6)
    {
        struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&addr;
        inet_ntop(AF_INET6, &sin6->sin6_addr, addrstr, 128);
        port = ntohs(sin6->sin6_port);
    }
    else
        strcpy(addrstr, "Unknown");

    fprintf(stderr, "Peer address: %s:%d, seq: %lld.\n",
            addrstr, port, seq);
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

    signal(SIGINT, sig_handler);

    HttpServer server(process);
    port = atoi(argv[1]);
    if (server.start(port) == 0) {
        waitGroup.wait();
        server.stop();
    } else {
        perror("Cannot start server");
        exit(1);
    }

    return 0;
}

