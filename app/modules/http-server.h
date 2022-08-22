//
// Created by dingjing on 8/17/22.
//

#ifndef JARVIS_HTTP_SERVER_H
#define JARVIS_HTTP_SERVER_H

#include <utility>

#include "server.h"
#include "../factory/task-factory.h"
#include "../protocol/http/http-message.h"


using HttpProcess = std::function<void(HttpTask*)>;
using HttpServer = Server<protocol::HttpRequest, protocol::HttpResponse>;



#endif //JARVIS_HTTP_SERVER_H
