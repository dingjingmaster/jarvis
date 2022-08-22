include(${CMAKE_SOURCE_DIR}/app/protocol/dns/dns.cmake)
include(${CMAKE_SOURCE_DIR}/app/protocol/http/http.cmake)

set(PROTOCOL_SRC ${HTTP_SRC} ${DNS_SRC}
        ${CMAKE_SOURCE_DIR}/app/protocol/protocol-message.h

        ${CMAKE_SOURCE_DIR}/app/protocol/package-wrapper.h
        ${CMAKE_SOURCE_DIR}/app/protocol/package-wrapper.cpp

        ${CMAKE_SOURCE_DIR}/app/protocol/consul-data-types.h
        ${CMAKE_SOURCE_DIR}/app/protocol/consul-data-types.cpp
        )
