//
// Created by dingjing on 8/19/22.
//

#ifndef JARVIS_TASK_ERROR_H
#define JARVIS_TASK_ERROR_H


enum
{
    //COMMON
    TASK_ERROR_ERR_URI_PARSE_FAILED             = 1001,             //< URI, parse failed
    TASK_ERROR_ERR_URI_SCHEME_INVALID           = 1002,             //< URI, invalid scheme
    TASK_ERROR_ERR_URI_PORT_INVALID             = 1003,             //< URI, invalid port
    TASK_ERROR_ERR_UPSTREAM_UNAVAILABLE         = 1004,             //< Upstream, all target server down

    //HTTP
    TASK_ERROR_ERR_HTTP_BAD_REDIRECT_HEADER     = 2001,             //< Http, 301/302/303/307/308 Location header value is NULL
    TASK_ERROR_ERR_HTTP_PROXY_CONNECT_FAILED    = 2002,             //< Http, proxy CONNECT return non 200

    //REDIS
    TASK_ERROR_ERR_REDIS_ACCESS_DENIED          = 3001,             //< Redis, invalid password
    TASK_ERROR_ERR_REDIS_COMMAND_DISALLOWED     = 3002,             //< Redis, command disabled, cannot be "AUTH"/"SELECT"

    //MYSQL
    TASK_ERROR_ERR_MYSQL_HOST_NOT_ALLOWED       = 4001,             //< MySQL
    TASK_ERROR_ERR_MYSQL_ACCESS_DENIED          = 4002,             //< MySQL, authentication failed
    TASK_ERROR_ERR_MYSQL_INVALID_CHARACTER_SET  = 4003,             //< MySQL, invalid charset, not found in MySQL-Documentation
    TASK_ERROR_ERR_MYSQL_COMMAND_DISALLOWED     = 4004,             //< MySQL, sql command disabled, cannot be "USE"/"SET NAMES"/"SET CHARSET"/"SET CHARACTER SET"
    TASK_ERROR_ERR_MYSQL_QUERY_NOT_SET          = 4005,             //< MySQL, query not set sql, maybe forget please check
    TASK_ERROR_ERR_MYSQL_SSL_NOT_SUPPORTED      = 4006,             //< MySQL, SSL not supported by the server

    //KAFKA
    TASK_ERROR_ERR_KAFKA_PARSE_RESPONSE_FAILED  = 5001,             //< Kafka parse response failed
    TASK_ERROR_ERR_KAFKA_PRODUCE_FAILED         = 5002,
    TASK_ERROR_ERR_KAFKA_FETCH_FAILED           = 5003,
    TASK_ERROR_ERR_KAFKA_CGROUP_FAILED          = 5004,
    TASK_ERROR_ERR_KAFKA_COMMIT_FAILED          = 5005,
    TASK_ERROR_ERR_KAFKA_META_FAILED            = 5006,
    TASK_ERROR_ERR_KAFKA_LEAVEGROUP_FAILED      = 5007,
    TASK_ERROR_ERR_KAFKA_API_UNKNOWN            = 5008,             //< api type not supported
    TASK_ERROR_ERR_KAFKA_VERSION_DISALLOWED     = 5009,             //< broker version not supported
    TASK_ERROR_ERR_KAFKA_SASL_DISALLOWED        = 5010,             //< sasl not supported
    TASK_ERROR_ERR_KAFKA_ARRANGE_FAILED         = 5011,             //< arrange toppar failed
    TASK_ERROR_ERR_KAFKA_LIST_OFFSETS_FAILED    = 5012,
    TASK_ERROR_ERR_KAFKA_CGROUP_ASSIGN_FAILED   = 5013,

    //CONSUL
    TASK_ERROR_ERR_CONSUL_API_UNKNOWN           = 6001,             //< api type not supported
    TASK_ERROR_ERR_CONSUL_CHECK_RESPONSE_FAILED = 6002,             //< Consul http code failed
};


#endif //JARVIS_TASK_ERROR_H
