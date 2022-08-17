//
// Created by dingjing on 8/17/22.
//

#include "dns-routine.h"

#include <string>
#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <sys/un.h>
#include <string.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>

#define PORT_STR_MAX	5

DnsOutput::DnsOutput(DnsOutput&& move)
{
    error_ = move.error_;
    addrinfo_ = move.addrinfo_;

    move.error_ = 0;
    move.addrinfo_ = NULL;
}

DnsOutput& DnsOutput::operator= (DnsOutput&& move)
{
    if (this != &move)
    {
        if (addrinfo_)
            freeaddrinfo(addrinfo_);

        error_ = move.error_;
        addrinfo_ = move.addrinfo_;

        move.error_ = 0;
        move.addrinfo_ = NULL;
    }

    return *this;
}

void DnsRoutine::run_local_path(const std::string& path, DnsOutput *out)
{
    struct sockaddr_un *sun = NULL;

    if (path.size() + 1 <= sizeof sun->sun_path)
    {
        size_t size = sizeof (struct addrinfo) + sizeof (struct sockaddr_un);

        out->addrinfo_ = (struct addrinfo *)calloc(size, 1);
        if (out->addrinfo_)
        {
            sun = (struct sockaddr_un *)(out->addrinfo_ + 1);
            sun->sun_family = AF_UNIX;
            memcpy(sun->sun_path, path.c_str(), path.size());

            out->addrinfo_->ai_family = AF_UNIX;
            out->addrinfo_->ai_socktype = SOCK_STREAM;
            out->addrinfo_->ai_addr = (struct sockaddr *)sun;
            size = offsetof(struct sockaddr_un, sun_path) + path.size() + 1;
            out->addrinfo_->ai_addrlen = size;
            out->error_ = 0;
            return;
        }
    }
    else
        errno = EINVAL;

    out->error_ = EAI_SYSTEM;
}

void DnsRoutine::run(const DnsInput *in, DnsOutput *out)
{
    if (!in->host_.empty() && in->host_[0] == '/')
    {
        run_local_path(in->host_, out);
        return;
    }

    struct addrinfo hints = {
#ifdef AI_ADDRCONFIG
            .ai_flags    = AI_ADDRCONFIG,
#endif
            .ai_family   = AF_UNSPEC,
            .ai_socktype = SOCK_STREAM
    };
    char port_str[PORT_STR_MAX + 1];

    hints.ai_flags |= AI_NUMERICSERV;
    if (in->is_numeric_host())
        hints.ai_flags |= AI_NUMERICHOST;

    snprintf(port_str, PORT_STR_MAX + 1, "%u", in->port_);
    out->error_ = getaddrinfo(in->host_.c_str(),
                              port_str,
                              &hints,
                              &out->addrinfo_);
}

