//
// Created by dingjing on 9/22/22.
//

#include "spider-task-factory.h"

spider::Spider *SpiderTaskFactory::createSpider(std::string name, std::string uri, spider::RootParser parser)
{
    return new spider::Spider(name, uri, parser);
}

spider::Spider *SpiderTaskFactory::createSpider(std::string &name, std::string &uri, spider::RootParser& parser)
{
    return new spider::Spider(name, uri, parser);
}

spider::Spider *SpiderTaskFactory::createSpider(std::string &name, std::string &uri, spider::RootParser &parser, const std::string &method)
{
    return new spider::Spider(name, uri, parser, method);
}

