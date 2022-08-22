//
// Created by dingjing on 8/22/22.
//

#ifndef JARVIS_DNS_MESSAGE_H
#define JARVIS_DNS_MESSAGE_H

#include <string>
#include <atomic>
#include <string.h>

#include "dns-parser.h"
#include "../protocol-message.h"

namespace protocol
{

    class DnsMessage : public ProtocolMessage
    {
    protected:
        virtual int encode(struct iovec vectors[], int max);
        virtual int append(const void *buf, size_t *size);

    public:
        int get_id() const
        {
            return parser->header.id;
        }
        int get_qr() const
        {
            return parser->header.qr;
        }
        int get_opcode() const
        {
            return parser->header.opcode;
        }
        int get_aa() const
        {
            return parser->header.aa;
        }
        int get_tc() const
        {
            return parser->header.tc;
        }
        int get_rd() const
        {
            return parser->header.rd;
        }
        int get_ra() const
        {
            return parser->header.ra;
        }
        int get_rcode() const
        {
            return parser->header.rcode;
        }
        int get_qdcount() const
        {
            return parser->header.qdcount;
        }
        int get_ancount() const
        {
            return parser->header.ancount;
        }
        int get_nscount() const
        {
            return parser->header.nscount;
        }
        int get_arcount() const
        {
            return parser->header.arcount;
        }

        void set_id(int id)
        {
            parser->header.id = id;
        }
        void set_qr(int qr)
        {
            parser->header.qr = qr;
        }
        void set_opcode(int opcode)
        {
            parser->header.opcode = opcode;
        }
        void set_aa(int aa)
        {
            parser->header.aa = aa;
        }
        void set_tc(int tc)
        {
            parser->header.tc = tc;
        }
        void set_rd(int rd)
        {
            parser->header.rd = rd;
        }
        void set_ra(int ra)
        {
            parser->header.ra = ra;
        }
        void set_rcode(int rcode)
        {
            parser->header.rcode = rcode;
        }

        int get_question_type() const
        {
            return parser->question.qtype;
        }
        int get_question_class() const
        {
            return parser->question.qclass;
        }
        std::string get_question_name() const
        {
            const char *name = parser->question.qname;
            if (name == NULL)
                return "";
            return name;
        }
        void set_question_type(int qtype)
        {
            parser->question.qtype = qtype;
        }
        void set_question_class(int qclass)
        {
            parser->question.qclass = qclass;
        }
        void set_question_name(const std::string& name)
        {
            char *pname = parser->question.qname;
            if (pname != NULL)
                free(pname);
            parser->question.qname = strdup(name.c_str());
        }

        // Inner use only
        bool is_single_packet() const
        {
            return parser->single_packet;
        }
        void set_single_packet(bool single)
        {
            parser->single_packet = single;
        }

    public:
        DnsMessage() :
                parser(new dns_parser_t)
        {
            dns_parser_init(parser);
            this->cur_size = 0;
        }

        virtual ~DnsMessage()
        {
            if (this->parser)
            {
                dns_parser_deinit(parser);
                delete this->parser;
            }
        }

        DnsMessage(DnsMessage&& msg);
        DnsMessage& operator = (DnsMessage&& msg);

    protected:
        dns_parser_t *parser;
        std::string msgbuf;
        size_t cur_size;

    private:
        int encode_reply();
        int encode_truncation_reply();

        // size of msgbuf, but in network byte order
        uint16_t msgsize;
    };

    class DnsRequest : public DnsMessage
    {
        static std::atomic<uint16_t> req_id_;
    public:
        DnsRequest()
        {
            dns_parser_set_id(req_id_++, this->parser);
        }

        DnsRequest(DnsRequest&& req) = default;
        DnsRequest& operator = (DnsRequest&& req) = default;

        void set_question(const char *host, uint16_t qtype, uint16_t qclass)
        {
            dns_parser_set_question(host, qtype, qclass, this->parser);
        }
    };

    class DnsResponse : public DnsMessage
    {
    public:
        DnsResponse()
        {
            this->request_id = 0;
        }

        DnsResponse(DnsResponse&& req) = default;
        DnsResponse& operator = (DnsResponse&& req) = default;

        const dns_parser_t *get_parser() const
        {
            return this->parser;
        }

        void set_request_id(uint16_t id)
        {
            this->request_id = id;
        }

        void set_request_name(const std::string& name)
        {
            std::string& req_name = this->request_name;

            req_name = name;
            while (req_name.size() > 1 && req_name.back() == '.')
                req_name.pop_back();
        }

    protected:
        virtual int append(const void *buf, size_t *size);

    private:
        uint16_t request_id;
        std::string request_name;
    };

}

#endif //JARVIS_DNS_MESSAGE_H
