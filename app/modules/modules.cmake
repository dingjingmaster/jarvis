include(${CMAKE_SOURCE_DIR}/app/core/core.cmake)
include(${CMAKE_SOURCE_DIR}/app/utils/utils.cmake)
include(${CMAKE_SOURCE_DIR}/app/client/client.cmake)
include(${CMAKE_SOURCE_DIR}/app/spider/spider.cmake)
include(${CMAKE_SOURCE_DIR}/app/factory/factory.cmake)
include(${CMAKE_SOURCE_DIR}/app/manager/manager.cmake)
include(${CMAKE_SOURCE_DIR}/app/protocol/protocol.cmake)
include(${CMAKE_SOURCE_DIR}/app/algorithm/algorithm.cmake)
include(${CMAKE_SOURCE_DIR}/app/nameservice/nameservice.cmake)


#
# modules 下各个项目互斥
#
file(GLOB HTTP_SERVER_SRC ${UTILS_SRC} ${ALGORITHM_SRC}
        ${SPIDER_SRC}
        ${PROTOCOL_SRC} ${HTTP_SRC} ${DNS_SRC} ${CORE_SRC} ${FACTORY_SRC} ${MANAGER_SRC} ${NAME_SERVICE_SRC} ${CLIENT_SRC}

        ${CMAKE_SOURCE_DIR}/app/modules/server.h
        ${CMAKE_SOURCE_DIR}/app/modules/server.cpp

        ${CMAKE_SOURCE_DIR}/app/modules/http-server.h
        ${CMAKE_SOURCE_DIR}/app/modules/http-server.cpp
        )

file(GLOB MODULE_SPIDER_SRC ${UTILS_SRC} ${ALGORITHM_SRC} ${CLIENT_SRC}
        ${CORE_SRC} ${SPIDER_SRC} ${FACTORY_SRC} ${HTTP_SRC} ${PROTOCOL_SRC} ${MANAGER_SRC} ${DNS_SRC} ${NAME_SERVICE_SRC}
        )

