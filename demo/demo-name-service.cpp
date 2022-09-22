//
// Created by dingjing on 8/25/22.
//

#include <string>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../app/manager/global.h"
#include "../app/manager/facilities.h"
#include "../app/factory/task-factory.h"
#include "../app/protocol/http/http-util.h"
#include "../app/nameservice/name-service.h"

// The example domonstrate the simplest user defined naming policy.

/* 'MyNSPolicy' is a naming policy, which use local file for naming.
 * The format of naming file is similar to 'hosts' file, but we allow
 * domain name and IP address as destination. For example:
 *
 * 127.0.0.1 localhost
 * 127.0.0.1 mydomain  # another alias for 127.0.0.1
 * www.sogou.com sogou # sogou -> www.sogou.com
 */
class MyNSPolicy : public NSPolicy
{
public:
    RouterTask *createRouterTask(const NSParams *params, RouterCallback callback, void*) override;

private:
    std::string path;

private:
    std::string read_from_fp(FILE *fp, const char *name);
    std::string parse_line(char *p, const char *name);

public:
    MyNSPolicy(const char *naming_file) : path(naming_file) { }
};

std::string MyNSPolicy::parse_line(char *p, const char *name)
{
    const char *dest = NULL;
    char *start;

    start = p;
    while (*start != '\0' && *start != '#')
        start++;
    *start = '\0';

    while (1)
    {
        while (isspace(*p))
            p++;

        start = p;
        while (*p != '\0' && !isspace(*p))
            p++;

        if (start == p)
            break;

        if (*p != '\0')
            *p++ = '\0';

        if (dest == NULL)
        {
            dest = start;
            continue;
        }

        if (strcasecmp(name, start) == 0)
            return std::string(dest);
    }

    return std::string();
}

std::string MyNSPolicy::read_from_fp(FILE *fp, const char *name)
{
    char *line = NULL;
    size_t bufsize = 0;
    std::string result;

    while (getline(&line, &bufsize, fp) > 0)
    {
        result = this->parse_line(line, name);
        if (result.size() > 0)
            break;
    }

    free(line);
    return result;
}

RouterTask *MyNSPolicy::createRouterTask(const NSParams *params, RouterCallback callback, void*)
{
    DnsResolver *dns_resolver = Global::getDnsResolver();

    if (params->mUri.host) {
        FILE *fp = fopen(this->path.c_str(), "r");
        if (fp) {
            std::string dest = this->read_from_fp(fp, params->mUri.host);
            if (dest.size() > 0) {
                /* Update the uri structure's 'host' field directly.
                 * You can also update the 'port' field if needed. */
                free(params->mUri.host);
                params->mUri.host = strdup(dest.c_str());
            }

            fclose(fp);
        }
    }

    /* Simply, use the global dns resolver to create a router task. */
    return dns_resolver->createRouterTask(params, std::move(callback), nullptr);
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "USAGE: %s <http url> <naming file>\n", argv[0]);
        exit(1);
    }

    ParsedURI uri;
    URIParser::parse(argv[1], uri);
    char *name = uri.host;
    if (name == NULL)
    {
        fprintf(stderr, "Invalid http URI\n");
        exit(1);
    }

    /* Create an naming policy. */
    MyNSPolicy *policy = new MyNSPolicy(argv[2]);

    /* Get the global name service object.*/
    NameService *ns = Global::getNameService();

    /* Add the our name with policy to global name service.
     * You can add mutilply names with one policy object. */
    ns->addPolicy(name, policy);

    Facilities::WaitGroup wg(1);
    HttpTask *task = TaskFactory::createHttpTask(argv[1], 2, 3,
                                                       [&wg](HttpTask *task, void*) {
                                                           int state = task->getState();
                                                           int error = task->getError();
                                                           if (state != TASK_STATE_SUCCESS) {
                                                               fprintf(stderr, "error: %s\n",
                                                                       Global::getErrorString(state, error));
                                                           } else {
                                                               auto *r = task->getResp();
                                                               std::string body = protocol::HttpUtil::decodeChunkedBody(r);
                                                               fwrite(body.c_str(), 1, body.size(), stdout);
                                                           }
                                                           wg.done();
                                                       }, nullptr);

    task->start();
    wg.wait();

    /* clean up */
    ns->delPolicy(name);
    delete policy;

    return 0;
}

