include(${CMAKE_SOURCE_DIR}/app/protocol/http/http.cmake)

file(GLOB PROTOCOL_SRC
        ${CMAKE_SOURCE_DIR}/app/protocol/protocol-message.h

        ${CMAKE_SOURCE_DIR}/app/protocol/package-wrapper.h
        ${CMAKE_SOURCE_DIR}/app/protocol/package-wrapper.cpp

        ${HTTP_SRC})
