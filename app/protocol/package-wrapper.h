//
// Created by dingjing on 8/14/22.
//

#ifndef JARVIS_PACKAGE_WRAPPER_H
#define JARVIS_PACKAGE_WRAPPER_H

#include "protocol-message.h"

namespace protocol
{
    class PackageWrapper : public ProtocolWrapper
    {
    public:
        PackageWrapper (ProtocolMessage* msg) : ProtocolWrapper(msg)
        {

        }
        PackageWrapper(PackageWrapper&& wrapper) = default;
        PackageWrapper& operator = (PackageWrapper&& wrapper) = default;

    protected:
        virtual int append (const void* buf, size_t* size);
        virtual int encode (struct iovec vectors[], int max);

    private:
        virtual ProtocolMessage* next (ProtocolMessage* msg)
        {
            return NULL;
        }
    };

};


#endif //JARVIS_PACKAGE_WRAPPER_H
