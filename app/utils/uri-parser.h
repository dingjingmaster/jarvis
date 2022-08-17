//
// Created by dingjing on 8/17/22.
//

#ifndef JARVIS_URI_PARSER_H
#define JARVIS_URI_PARSER_H

#include <map>
#include <string>
#include <vector>
#include <stdlib.h>

#define URI_STATE_INIT		    0
#define URI_STATE_SUCCESS	    1
#define URI_STATE_INVALID	    2
#define URI_STATE_ERROR		    3

// RAII: YES
class ParsedURI
{
public:
    ParsedURI() { init(); }
    virtual ~ParsedURI() { deinit(); }

    //copy constructor
    ParsedURI(const ParsedURI& uri) { copy(uri); }
    //copy operator
    ParsedURI& operator= (const ParsedURI& uri)
    {
        if (this != &uri) {
            deinit();
            copy(uri);
        }

        return *this;
    }

    //move constructor
    ParsedURI(ParsedURI&& uri);
    //move operator
    ParsedURI& operator= (ParsedURI&& uri);

private:
    void init()
    {
        scheme = NULL;
        userinfo = NULL;
        host = NULL;
        port = NULL;
        path = NULL;
        query = NULL;
        fragment = NULL;
        state = URI_STATE_INIT;
        error = 0;
    }

    void deInit()
    {
        free(scheme);
        free(userinfo);
        free(host);
        free(port);
        free(path);
        free(query);
        free(fragment);
    }

    void copy(const ParsedURI& uri);


public:
    char *scheme;
    char *userInfo;
    char *host;
    char *port;
    char *path;
    char *query;
    char *fragment;
    int state;
    int error;
};

// static class
class URIParser
{
public:
    // return 0 mean succ, -1 mean fail
    static int parse(const char *str, ParsedURI& uri);
    static int parse(const std::string& str, ParsedURI& uri)
    {
        return parse(str.c_str(), uri);
    }

    static std::map<std::string, std::vector<std::string>>
    split_query_strict(const std::string &query);

    static std::map<std::string, std::string>
    split_query(const std::string &query);

    static std::vector<std::string> split_path(const std::string &path);
};

#endif //JARVIS_URI_PARSER_H
