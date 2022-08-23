//
// Created by dingjing on 8/17/22.
//

#ifndef JARVIS_HTTP_UTIL_H
#define JARVIS_HTTP_UTIL_H

#include <string>
#include <vector>
#include <unordered_map>

#include "http-parser.h"
#include "http-message.h"

#define HTTP_METHOD_GET                 "GET"
#define HTTP_METHOD_HEAD                "HEAD"
#define HTTP_METHOD_POST                "POST"
#define HTTP_METHOD_PUT                 "PUT"
#define HTTP_METHOD_PATCH               "PATCH"
#define HTTP_METHOD_DELETE              "DELETE"
#define HTTP_METHOD_CONNECT             "CONNECT"
#define HTTP_METHOD_OPTIONS             "OPTIONS"
#define HTTP_METHOD_TRACE               "TRACE"

enum
{
    HttpStatusContinue                      = 100, // RFC 7231, 6.2.1
    HttpStatusSwitchingProtocols            = 101, // RFC 7231, 6.2.2
    HttpStatusProcessing                    = 102, // RFC 2518, 10.1

    HttpStatusOK                            = 200, // RFC 7231, 6.3.1
    HttpStatusCreated                       = 201, // RFC 7231, 6.3.2
    HttpStatusAccepted                      = 202, // RFC 7231, 6.3.3
    HttpStatusNonAuthoritativeInfo          = 203, // RFC 7231, 6.3.4
    HttpStatusNoContent                     = 204, // RFC 7231, 6.3.5
    HttpStatusResetContent                  = 205, // RFC 7231, 6.3.6
    HttpStatusPartialContent                = 206, // RFC 7233, 4.1
    HttpStatusMultiStatus                   = 207, // RFC 4918, 11.1
    HttpStatusAlreadyReported               = 208, // RFC 5842, 7.1
    HttpStatusIMUsed                        = 226, // RFC 3229, 10.4.1

    HttpStatusMultipleChoices               = 300, // RFC 7231, 6.4.1
    HttpStatusMovedPermanently              = 301, // RFC 7231, 6.4.2
    HttpStatusFound                         = 302, // RFC 7231, 6.4.3
    HttpStatusSeeOther                      = 303, // RFC 7231, 6.4.4
    HttpStatusNotModified                   = 304, // RFC 7232, 4.1
    HttpStatusUseProxy                      = 305, // RFC 7231, 6.4.5

    HttpStatusTemporaryRedirect             = 307, // RFC 7231, 6.4.7
    HttpStatusPermanentRedirect             = 308, // RFC 7538, 3

    HttpStatusBadRequest                    = 400, // RFC 7231, 6.5.1
    HttpStatusUnauthorized                  = 401, // RFC 7235, 3.1
    HttpStatusPaymentRequired               = 402, // RFC 7231, 6.5.2
    HttpStatusForbidden                     = 403, // RFC 7231, 6.5.3
    HttpStatusNotFound                      = 404, // RFC 7231, 6.5.4
    HttpStatusMethodNotAllowed              = 405, // RFC 7231, 6.5.5
    HttpStatusNotAcceptable                 = 406, // RFC 7231, 6.5.6
    HttpStatusProxyAuthRequired             = 407, // RFC 7235, 3.2
    HttpStatusRequestTimeout                = 408, // RFC 7231, 6.5.7
    HttpStatusConflict                      = 409, // RFC 7231, 6.5.8
    HttpStatusGone                          = 410, // RFC 7231, 6.5.9
    HttpStatusLengthRequired                = 411, // RFC 7231, 6.5.10
    HttpStatusPreconditionFailed            = 412, // RFC 7232, 4.2
    HttpStatusRequestEntityTooLarge         = 413, // RFC 7231, 6.5.11
    HttpStatusRequestURITooLong             = 414, // RFC 7231, 6.5.12
    HttpStatusUnsupportedMediaType          = 415, // RFC 7231, 6.5.13
    HttpStatusRequestedRangeNotSatisfiable  = 416, // RFC 7233, 4.4
    HttpStatusExpectationFailed             = 417, // RFC 7231, 6.5.14
    HttpStatusTeapot                        = 418, // RFC 7168, 2.3.3
    HttpStatusEnhanceYourCaim               = 420, // Twitter Search
    HttpStatusMisdirectedRequest            = 421, // RFC 7540, 9.1.2
    HttpStatusUnprocessableEntity           = 422, // RFC 4918, 11.2
    HttpStatusLocked                        = 423, // RFC 4918, 11.3
    HttpStatusFailedDependency              = 424, // RFC 4918, 11.4
    HttpStatusTooEarly                      = 425, // RFC 8470, 5.2.
    HttpStatusUpgradeRequired               = 426, // RFC 7231, 6.5.15
    HttpStatusPreconditionRequired          = 428, // RFC 6585, 3
    HttpStatusTooManyRequests               = 429, // RFC 6585, 4
    HttpStatusRequestHeaderFieldsTooLarge   = 431, // RFC 6585, 5
    HttpStatusNoResponse                    = 444, // Nginx
    HttpStatusBlocked                       = 450, // Windows
    HttpStatusUnavailableForLegalReasons    = 451, // RFC 7725, 3
    HttpStatusTooLargeForNginx              = 494, // Nginx

    HttpStatusInternalServerError           = 500, // RFC 7231, 6.6.1
    HttpStatusNotImplemented                = 501, // RFC 7231, 6.6.2
    HttpStatusBadGateway                    = 502, // RFC 7231, 6.6.3
    HttpStatusServiceUnavailable            = 503, // RFC 7231, 6.6.4
    HttpStatusGatewayTimeout                = 504, // RFC 7231, 6.6.5
    HttpStatusHTTPVersionNotSupported       = 505, // RFC 7231, 6.6.6
    HttpStatusVariantAlsoNegotiates         = 506, // RFC 2295, 8.1
    HttpStatusInsufficientStorage           = 507, // RFC 4918, 11.5
    HttpStatusLoopDetected                  = 508, // RFC 5842, 7.2
    HttpStatusNotExtended                   = 510, // RFC 2774, 7
    HttpStatusNetworkAuthenticationRequired = 511, // RFC 6585, 6
};

namespace protocol
{
    class HttpUtil
    {
    public:
        static std::string decodeChunkedBody (const HttpMessage* msg);
        static void setResponseStatus (HttpResponse* resp, int statusCode);
    };

    class HttpHeaderMap
    {
    public:
        explicit HttpHeaderMap(const HttpMessage* msg);

        bool keyExists (std::string key);
        std::string get(std::string key);
        bool get(std::string key, std::string& value);
        std::vector<std::string> getStrict (std::string key);
        bool getStrict (std::string key, std::vector<std::string>& values);

    private:
        std::unordered_map<std::string, std::vector<std::string>>       mHeaderMap;
    };

    class HttpHeaderCursor
    {
    public:
        explicit HttpHeaderCursor (const HttpMessage* msg);
        virtual ~HttpHeaderCursor();

    public:
        void rewind ();
        bool next (HttpMessageHeader* header);
        bool next (std::string& name, std::string& value);

        bool find (HttpMessageHeader* header);
        bool find (const std::string& name, std::string& value);

    protected:
        ::HttpHeaderCursor               mCursor;
    };

    class HttpChunkCursor
    {
    public:
        explicit HttpChunkCursor (const HttpMessage* msg);
        virtual ~HttpChunkCursor () = default;

        bool next (const void** chunk, size_t* size);
        void rewind ();

    protected:
        bool                        mEnd;
        bool                        mChunked;
        const void*                 mBody;
        size_t                      mBodyLen;
        const void*                 mPos;
    };
};



#endif //JARVIS_HTTP_UTIL_H
