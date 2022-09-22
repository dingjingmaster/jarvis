//
// Created by dingjing on 9/22/22.
//

#ifndef JARVIS_SPIDER_H
#define JARVIS_SPIDER_H

#include <map>
#include <string>
#include <functional>

#include "../factory/task-factory.h"
#include "../protocol/http/http-util.h"
#include "../protocol/http/http-message.h"

namespace spider
{
    using Request = protocol::HttpRequest;
    using Response = protocol::HttpResponse;

    using SpiderContext = std::string;
    using RootParser = std::function<void (SpiderContext*)>;
    using Parser = std::function<std::string (SpiderContext*, void* data)>;

    class Spider
    {
    public:
        explicit Spider (std::string& name, std::string& uri, RootParser& rootParser, const std::string& method=HTTP_METHOD_GET);

        void run ();

        void addRule (std::string& name, Parser& parser);
        bool executeRule(std::string& rule, void* udata);


    private:
        static void http_request_callback(HttpTask *task, void*);


    public:
        std::map<std::string, std::string>      mField;         // use for save result

    private:
        std::string                             mSpiderName;
        std::string                             mBaseUrl;

        SpiderContext                           mContext;

        RootParser                              mRootParser;
        std::map<std::string, Parser>           mParser;

        HttpTask*                               mHttpTask;
    };
};



#endif //JARVIS_SPIDER_H
