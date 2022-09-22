//
// Created by dingjing on 9/22/22.
//

#ifndef JARVIS_SPIDER_TASK_FACTORY_H
#define JARVIS_SPIDER_TASK_FACTORY_H

#include "../spider/spider.h"
#include "../protocol/http/http-util.h"
#include "../protocol/http/http-message.h"

class SpiderTaskFactory
{
public:
    static spider::Spider* createSpider (std::string name, std::string uri, spider::RootParser parser);
    static spider::Spider* createSpider (std::string& name, std::string& uri, spider::RootParser& parser);
    static spider::Spider* createSpider (std::string& name, std::string& uri, spider::RootParser& parser, const std::string& method);

};


#endif //JARVIS_SPIDER_TASK_FACTORY_H
