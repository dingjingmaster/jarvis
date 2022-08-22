//
// Created by dingjing on 8/22/22.
//

#ifndef JARVIS_DNS_UTIL_H
#define JARVIS_DNS_UTIL_H

#include <netdb.h>
#include "dns-message.h"

namespace protocol
{

    class DnsUtil
    {
    public:
        static int getaddrinfo(const DnsResponse *resp,
                               unsigned short port,
                               struct addrinfo **res);
        static void freeaddrinfo(struct addrinfo *ai);
    };

    class DnsResultCursor
    {
    public:
        DnsResultCursor(const DnsResponse *resp) :
                parser(resp->get_parser())
        {
            dns_answer_cursor_init(&cursor, parser);
            record = NULL;
        }

        DnsResultCursor(DnsResultCursor&& move) = delete;
        DnsResultCursor& operator=(DnsResultCursor&& move) = delete;

        virtual ~DnsResultCursor() { }

        void reset_answer_cursor()
        {
            dns_answer_cursor_init(&cursor, parser);
        }

        void reset_authority_cursor()
        {
            dns_authority_cursor_init(&cursor, parser);
        }

        void reset_additional_cursor()
        {
            dns_additional_cursor_init(&cursor, parser);
        }

        bool next(struct dns_record **next_record)
        {
            int ret = dns_record_cursor_next(&record, &cursor);
            if (ret != 0)
                record = NULL;
            else
                *next_record = record;

            return ret == 0;
        }

        bool find_cname(const char *name, const char **cname)
        {
            return dns_record_cursor_find_cname(name, cname, &cursor) == 0;
        }

    private:
        const dns_parser_t *parser;
        dns_record_cursor_t cursor;
        struct dns_record *record;
    };

}

#endif //JARVIS_DNS_UTIL_H
