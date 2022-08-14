//
// Created by dingjing on 8/14/22.
//

#ifndef JARVIS_PROTOCOL_MESSAGE_H
#define JARVIS_PROTOCOL_MESSAGE_H

#include <errno.h>
#include <stddef.h>

#include <utility>

#include "../core/communicator.h"


namespace protocol
{
    class Attachment
    {
    public:
        virtual ~Attachment() {}
    };


    class ProtocolMessage : public CommMessageOut, public CommMessageIn
    {
        friend class ProtocolWrapper;
    protected:
        virtual int encode (struct iovec vectors[], int max)
        {
            errno = ENOSYS;
            return -1;
        }

        virtual int append (const void* buf, size_t* size)
        {
            return append(buf, *size);
        }

        virtual int append (const void* buf, size_t size)
        {
            errno = ENOSYS;

            return -1;
        }

        virtual int feedback (const void* buf, size_t size)
        {
            if (mWrapper) {
                return mWrapper->feedback(buf, size);
            }

            return CommMessageIn::feedback(buf, size);
        }

        virtual void reNew ()
        {
            if (mWrapper) {
                return mWrapper->reNew();
            }

            return CommMessageIn::reNew();
        }
    public:
        ProtocolMessage ()
        {
            mSizeLimit = (size_t) - 1;
            mAttachment = NULL;
            mWrapper = NULL;
        }

        ProtocolMessage (ProtocolMessage&& msg)
        {
            mSizeLimit = msg.mSizeLimit;
            msg.mSizeLimit = (size_t) - 1;

            mAttachment = msg.mAttachment;
            msg.mAttachment = NULL;
        }

        ProtocolMessage& operator = (ProtocolMessage&& msg)
        {
            if (&msg != this) {
                mSizeLimit = msg.mSizeLimit;
                msg.mSizeLimit = (size_t) - 1;

                if (mAttachment)    delete mAttachment;
                msg.mAttachment = NULL;
            }

            return *this;
        }

        virtual ~ProtocolMessage()
        {
            if (mAttachment)        delete mAttachment;
        }

        void setSizeLimit (size_t limit) {mSizeLimit = limit;};
        size_t getSizeLimit () const {return mSizeLimit;}

    protected:
        size_t                      mSizeLimit;

    private:
        Attachment*                 mAttachment;
        ProtocolMessage*            mWrapper;
    };


    class ProtocolWrapper : public ProtocolMessage
    {
    public:
        ProtocolWrapper (ProtocolMessage* msg)
        {
            msg->mWrapper = this;
            mMsg = msg;
        }

        ProtocolWrapper(ProtocolWrapper&& wrapper) : ProtocolMessage(std::move(wrapper))
        {
            wrapper.mMsg->mWrapper = this;
            mMsg = wrapper.mMsg;
            wrapper.mMsg = NULL;
        }

        ProtocolWrapper& operator = (ProtocolWrapper&& wrapper)
        {
            if (&wrapper != this) {
                *(ProtocolMessage *)this = std::move(wrapper);
                wrapper.mMsg->mWrapper = this;
                mMsg = wrapper.mMsg;
                wrapper.mMsg = NULL;
            }

            return *this;
        }
    protected:
        virtual int encode (struct iovec vectors[], int max)
        {
            return mMsg->encode(vectors, max);
        }

        virtual int append (const void* buf,size_t* size)
        {
            return mMsg->append(buf, size);
        }

    protected:
        ProtocolMessage*            mMsg;
    };
};


#endif //JARVIS_PROTOCOL_MESSAGE_H
