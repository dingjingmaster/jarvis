//
// Created by dingjing on 10/18/22.
//

#ifndef JARVIS_HTTP_ROUTER_H
#define JARVIS_HTTP_ROUTER_H

#include "factory/task-factory.h"

class HttpRouter
{
public:
    static HttpRouter* getInstance();

    std::string requestStaticResource (HttpTask& task) const;

    bool responseStaticResource (HttpTask* task);
    bool responseDynamicResource (HttpTask* task);

private:
    HttpRouter();

private:
    static HttpRouter*          gInstance;
};




#endif //JARVIS_HTTP_ROUTER_H
