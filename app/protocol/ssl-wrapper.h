//
// Created by dingjing on 8/23/22.
//

#ifndef JARVIS_SSL_WRAPPER_H
#define JARVIS_SSL_WRAPPER_H

#include <openssl/ssl.h>
#include "protocol-message.h"

namespace protocol
{

    class SSLHandshaker : public ProtocolMessage
    {
    public:
        virtual int encode(struct iovec vectors[], int max);
        virtual int append(const void *buf, size_t *size);

    protected:
        SSL *ssl;
        int ssl_ex_data_index;

    public:
        SSLHandshaker(SSL *ssl)
        {
            this->ssl = ssl;
            this->ssl_ex_data_index = 0;
        }

        SSLHandshaker(SSL *ssl, int ssl_ex_data_index)
        {
            this->ssl = ssl;
            this->ssl_ex_data_index = ssl_ex_data_index;
        }

    public:
        SSLHandshaker(SSLHandshaker&& handshaker) = default;
        SSLHandshaker& operator = (SSLHandshaker&& handshaker) = default;
    };

    class SSLWrapper : public ProtocolWrapper
    {
    protected:
        virtual int encode(struct iovec vectors[], int max);
        virtual int append(const void *buf, size_t *size);

    protected:
        virtual int feedback(const void *buf, size_t size);

    protected:
        int append_message();

    protected:
        SSL *ssl;
        int ssl_ex_data_index;

    public:
        SSLWrapper(ProtocolMessage *msg, SSL *ssl) :
                ProtocolWrapper(msg)
        {
            this->ssl = ssl;
            this->ssl_ex_data_index = 0;
        }

        SSLWrapper(ProtocolMessage *msg, SSL *ssl, int ssl_ex_data_index) :
                ProtocolWrapper(msg)
        {
            this->ssl = ssl;
            this->ssl_ex_data_index = ssl_ex_data_index;
        }

    public:
        SSLWrapper(SSLWrapper&& wrapper) = default;
        SSLWrapper& operator = (SSLWrapper&& wrapper) = default;
    };

    class ServiceSSLWrapper : public SSLWrapper
    {
    protected:
        virtual int append(const void *buf, size_t *size);

    public:
        ServiceSSLWrapper(ProtocolMessage *msg, SSL *ssl) : SSLWrapper(msg, ssl)
        {
        }

    public:
        ServiceSSLWrapper(ServiceSSLWrapper&& wrapper) = default;
        ServiceSSLWrapper& operator = (ServiceSSLWrapper&& wrapper) = default;
    };

}

#endif //JARVIS_SSL_WRAPPER_H
