//
// Created by dingjing on 8/19/22.
//

#ifndef JARVIS_ROUTE_MANAGER_H
#define JARVIS_ROUTE_MANAGER_H

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <mutex>
#include <string>

#include "endpoint-params.h"
#include "../core/rb-tree.h"
#include "../factory/connection.h"
#include "../core/common-scheduler.h"

class RouteManager
{
public:
    class RouteResult
    {
    public:
        RouteResult() : mCookie(nullptr), mRequestObject(nullptr) {}
        void clear () {mCookie = nullptr; mRequestObject = nullptr;}

    public:
        void*                   mCookie;
        CommSchedObject*        mRequestObject;
    };


    class RouteTarget : public CommSchedTarget
    {
    public:
        RouteTarget () : mState(0)
        {

        }
    private:
        virtual Connection* newConnection(int connectFd)
        {
            return new Connection;
        }

    public:
        int                     mState;
    };


public:
    int get (TransportType type, const struct addrinfo* addrInfo, const std::string& otherInfo, const EndpointParams* endpointParams, const std::string& hostname, RouteResult& result);
    RouteManager ()
    {

    }

    ~RouteManager();

    static void notifyAvailable (void* cookie, CommTarget* target);
    static void notifyUnavailable (void* cookie, CommTarget* target);

private:
    std::mutex                  mMutex;
    RBRoot                      mCache;

};


#endif //JARVIS_ROUTE_MANAGER_H
