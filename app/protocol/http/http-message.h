//
// Created by dingjing on 8/14/22.
//

#ifndef JARVIS_HTTP_MESSAGE_H
#define JARVIS_HTTP_MESSAGE_H
#include <string>
#include <utility>
#include <string.h>

#include "http-parser.h"
#include "../../core/c-log.h"
#include "../../core/c-list.h"
#include "../protocol-message.h"


namespace protocol
{
    typedef struct _HttpMessageHeader           HttpMessageHeader;

    struct _HttpMessageHeader
    {
        const void*             name;
        size_t                  nameLen;

        const void*             value;
        size_t                  valueLen;
    };

    class HttpMessage : public ProtocolMessage
    {
    public:
        HttpMessage(bool isResp) : mParser (new HttpParser)
        {
            logv("");
            http_parser_init(isResp, mParser);

            INIT_LIST_HEAD(&mOutputBody);
            mCurSize = 0;
            mOutputBodySize = 0;
        }

        ~HttpMessage()
        {
            logv("");
            clearOutputBody();
            if (mParser) {
                http_parser_deinit(mParser);
                delete mParser;
            }
        }

        const char* getHttpVersion () const
        {
            logv("");
            return http_parser_get_version(mParser);
        }

        bool setHttpVersion (const char* version)
        {
            logv("");
            return http_parser_set_version(version, mParser);
        }

        bool isChunked () const
        {
            logv("");
            return http_parser_chunked(mParser);
        }

        bool isKeepAlive () const
        {
            logv("");
            return http_parser_keep_alive(mParser);
        }

        bool addHeader (const HttpMessageHeader* header)
        {
            logv("");
            return http_parser_add_header(header->name, header->nameLen, header->value, header->valueLen, mParser) == 0;
        }

        bool addHeaderPair (const char* name, const char* value)
        {
            logv("");
            return http_parser_add_header(name, strlen(name), value, strlen(value), mParser) == 0;
        }

        bool setHeader (const HttpMessageHeader* header)
        {
            logv("");
            return http_parser_set_header(header->name, header->nameLen, header->value, header->valueLen, mParser) == 0;
        }

        bool setHeaderPair (const char* name, const char* value)
        {
            logv("");
            return http_parser_set_header(name, strlen(name), value, strlen(value), mParser) == 0;
        }

        bool isHeaderComplete () const
        {
            logv("");
            return http_parser_header_complete(mParser);
        }

        bool hasConnectionHeader() const
        {
            logv("");
            return http_parser_has_connection(mParser);
        }

        bool hasContentLengthHeader () const
        {
            logv("");
            return http_parser_has_content_length(mParser);
        }

        bool hasKeepAliveHeader () const
        {
            logv("");
            return http_parser_has_keep_alive(mParser);
        }

        bool getParsedBody (const void** body, size_t* size) const
        {
            return http_parser_get_body(body, size, mParser) == 0;
        }

        void endParsing()
        {
            logv("");
            http_parser_close_message(mParser);
        }

        bool appendOutputBody(const void* buf, size_t size);
        bool appendOutputBody(const void* buf)
        {
            logv("");
            return appendOutputBody(buf, strlen(static_cast<const char*>(buf)));
        }

        bool appendOutputBodyNocopy (const void* buf, size_t size);
        bool appendOutputBodyNocopy (const char* buf)
        {
            logv("");
            return appendOutputBodyNocopy(buf, strlen(static_cast<const char*>(buf)));
        }

        void clearOutputBody();

        size_t getOutputBodySize () const;

        bool getHttpVersion (std::string& version) const
        {
            logv("");
            const char* str = getHttpVersion();
            if (str) {
                version.assign(str);
                return true;
            }
            return false;
        }

        bool setHttpVersion (const std::string& version)
        {
            logv("");
            return setHttpVersion(version.c_str());
        }

        bool addHeaderPair (const std::string& name, const std::string& value)
        {
            logv("");
            return http_parser_add_header(name.c_str(), name.size(), value.c_str(), value.size(), mParser) == 0;
        }

        bool setHeaderPair (const std::string& name, const std::string& value)
        {
            logv("");
            return http_parser_set_header(name.c_str(), name.size(), value.c_str(), value.size(), mParser) == 0;
        }

        bool appendOutputBody (const std::string& buf)
        {
            logv("");
            return appendOutputBody(buf.c_str(), buf.size());
        }

        bool appendOutputBodyNocopy (const std::string& buf)
        {
            logv("");
            return appendOutputBodyNocopy(buf.c_str(), buf.size());
        }

        const HttpParser* getParser () const
        {
            logv("");
            return mParser;
        }

        HttpMessage (HttpMessage&& msg);
        HttpMessage& operator = (HttpMessage&& msg);

    protected:
        int append (const void* buf, size_t* size) override;
        int encode (struct iovec vectors[], int max) override;

    private:
        struct list_head* combineFrom (struct list_head* pos, size_t size);

    protected:
        HttpParser*                 mParser;
        size_t                      mCurSize;

    private:
        struct list_head            mOutputBody;
        size_t                      mOutputBodySize;
    };

    class HttpRequest : public HttpMessage
    {
    public:
        const char* getMethod() const
        {
            logv("");
            return http_parser_get_method(mParser);
        }

        const char* getRequestUri () const
        {
            logv("");
            return http_parser_get_uri(mParser);
        }

        bool setMethod (const char* method)
        {
            logv("");
            return http_parser_set_method(method, mParser) == 0;
        }

        bool setRequestUri (const char* uri)
        {
            logv("");
            return http_parser_set_uri(uri, mParser) == 0;
        }

        bool getMethod (std::string& method) const
        {
            logv("");
            const char* str = getMethod();
            if (str) {
                method.assign(str);
                return true;
            }

            return false;
        }

        bool getRequestUri (std::string& uri) const
        {
            logv("");
            const char* str = getRequestUri();

            if (str) {
                uri.append(str);
                return true;
            }
            return false;
        }

        bool setMethod (const std::string& method)
        {
            logv("");
            return this->setMethod(method.c_str());
        }

        bool setRequestUri (const std::string& uri)
        {
            logv("");
            return setRequestUri(uri.c_str());
        }

        HttpRequest() : HttpMessage(false)
        {
            logv("");
        }

        HttpRequest(HttpRequest&& req) = default;
        HttpRequest& operator = (HttpRequest&& req) = default;

    protected:
        int append (const void* buf, size_t* size) override ;

    private:
        int handleExpectContinue ();
    };

    class HttpResponse : public HttpMessage
    {
    public:
        HttpResponse() : HttpMessage(true){}
        HttpResponse(HttpResponse&& resp) = default;
        HttpResponse& operator = (HttpResponse&& resp) = default;

        const char* getStatusCode () const
        {
            logv("");
            return http_parser_get_code(mParser);
        }

        const char* getReasonPhrase () const
        {
            logv("");
            return http_parser_get_phrase(mParser);
        }

        bool setStatusCode (const char* code)
        {
            logv("");
            return http_parser_set_code(code, mParser) == 0;
        }

        bool setReasonPhrase (const char* phrase)
        {
            logv("");
            return http_parser_set_phrase(phrase, mParser) == 0;
        }

        void parseZeroBody ()
        {
            logv("");
            mParser->transferLength = 0;
        }

        bool getStatusCode (std::string& code) const
        {
            logv("");
            const char* str = getStatusCode();
            if (str) {
                code.assign(str);
                return true;
            }
            return false;
        }

        bool getReasonPhrase (std::string& phrase) const
        {
            logv("");
            const char* str = getReasonPhrase();
            if (str) {
                phrase.assign(str);
                return true;
            }
            return false;
        }

        bool setStatusCode (const std::string& code)
        {
            logv("");
            return setStatusCode(code.c_str());
        }

        bool setReasonPhrase (const std::string& phrase)
        {
            logv("");
            return setReasonPhrase(phrase.c_str());
        }

    protected:
        virtual int append (const void* buf, size_t* size);
    };
};



#endif //JARVIS_HTTP_MESSAGE_H
