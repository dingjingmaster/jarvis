//
// Created by dingjing on 8/14/22.
//

#ifndef JARVIS_CONSUL_DATA_TYPES_H
#define JARVIS_CONSUL_DATA_TYPES_H

#include <map>
#include <vector>
#include <atomic>
#include <string>
#include <assert.h>

namespace protocol
{
    class ConsulConfig
    {
    public:
        ConsulConfig();
        ConsulConfig(ConsulConfig&& move);
        ConsulConfig(const ConsulConfig& copy);
        virtual ~ConsulConfig();

        ConsulConfig& operator= (ConsulConfig& copy);
        ConsulConfig& operator= (ConsulConfig&& move);

        // common config
        std::string getToken () const;
        void setToken (const std::string& token);

        std::string getDataCenter () const;
        void setDataCenter (const std::string& dataCenter);

        std::string getNearNode () const;
        void setNearNode (const std::string& nearNode);

        std::string getFilterExpr () const;
        void setFilterExpr (const std::string& filterExpr);

        int getWaitTTL () const;
        void setWaitTTL (int waitTTL);

        bool blockingQuery () const;
        void setBlockingQuery (bool enable);

        bool getPassing () const;
        void setPassing (bool passing);

        // register config
        bool getReplaceChecks () const;
        void setReplaceChecks (bool replaceChecks);

        std::string getCheckName () const;
        void setCheckName (const std::string& checkName);

        std::string getCheckHttpURL () const;
        void setCheckHttpURL (const std::string& url);

        std::string getCheckHttpMethod () const;
        void setCheckHttpMethod (const std::string& method);

        const std::map<std::string, std::vector<std::string>>* getHttpHeaders () const;
        void addHttpHeader (const std::string& key, const std::vector<std::string>& val);

        std::string getHttpBody () const;
        void setHttpBody (const std::string& body);

        int getCheckInterval () const;
        void setCheckInterval (int interval);

        int getCheckTimeout () const;
        void setCheckTimeout (int timeout);

        std::string getCheckNotes () const;
        void setCheckNotes (const std::string& notes);

        std::string getCheckTcp () const;
        void setCheckTcp (const std::string& tcpAddr);

        std::string getInitialStatus () const;
        void setInitialStatus (const std::string& initialStatus);

        int getAutoDeregisterTime () const;
        void setAutoDeregisterTime (int milliseconds);

        int getSuccessTimes () const;
        void setSuccessTimes (int times);

        int getFailureTimes () const;
        void setFailureTimes (int times);

        bool getHealthCheck () const;
        void setHealthCheck (bool enable);

    private:
        typedef struct _HealthCheckConfig
        {
            std::string                                         checkName;
            std::string                                         notes;
            std::string                                         httpURL;
            std::string                                         httpMethod;
            std::string                                         httpBody;
            std::string                                         tcpAddress;
            std::string                                         initialStatus;

            std::map<std::string,std::vector<std::string>>      headers;

            int                                                 autoDeregisterTime;
            int                                                 interval;
            int                                                 timeout;                // default 10000
            int                                                 successTimes;           // default:0 success times before passing
            int                                                 failureTimes;           // default:0 failure before critical

            bool                                                healthCheck;
        } HealthCheckConfig;

        typedef struct _Config
        {
            // common config
            std::string                 token;

            // discover config
            std::string                 dc;
            std::string                 near;
            std::string                 filter;

            int                         waitTTL;
            bool                        blockingQuery;
            bool                        passing;

            // register config
            bool                        replaceChecks;
            HealthCheckConfig           checkCfg;
        } Config;


    private:
        Config*                         mPtr;
        std::atomic<int>*               mRef;
    };


    // key: address, value: port
    using ConsulAddress = std::pair<std::string, unsigned short>;

    typedef struct _ConsulService
    {
        std::string                         serviceName;
        std::string                         serviceNameSpace;
        std::string                         serviceID;
        std::vector<std::string>            tags;
        ConsulAddress                       serviceAddress;
        ConsulAddress                       lan;
        ConsulAddress                       lanIpv4;
        ConsulAddress                       lanIpv6;
        ConsulAddress                       virtualAddress;
        ConsulAddress                       wan;
        ConsulAddress                       wanIpv4;
        ConsulAddress                       wanIpv6;
        std::map<std::string, std::string>  meta;
        bool                                tagOverride;
    } ConsulService;

    typedef struct _ConsulServiceInstance
    {
        // node info
        std::string                         nodeID;
        std::string                         nodeName;
        std::string                         nodeAddress;
        std::string                         dc;
        std::map<std::string, std::string>  nodeMeta;
        long long                           createIndex;
        long long                           modifyIndex;

        // service info
        ConsulService                       service;

        // service health check
        std::string                         checkName;
        std::string                         checkID;
        std::string                         checkNotes;
        std::string                         checkOutput;
        std::string                         checkStatus;
        std::string                         checkType;
    } ConsulServiceInstance;

    typedef struct _ConsulServiceTags
    {
        std::string                         serviceName;
        std::vector<std::string>            tags;
    } ConsulServiceTags;
};

#endif //JARVIS_CONSUL_DATA_TYPES_H
