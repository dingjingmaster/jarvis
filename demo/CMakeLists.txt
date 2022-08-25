project(demo LANGUAGES CXX C)

include(${CMAKE_SOURCE_DIR}/app/modules/modules.cmake)

add_executable(demo-http-server ${CMAKE_SOURCE_DIR}/demo/demo-http-server.cpp ${HTTP_SERVER_SRC})
target_link_libraries(demo-http-server
        PRIVATE
        PUBLIC
        ${OPENSSL_LIBRARIES})

add_executable(demo-wget ${CMAKE_SOURCE_DIR}/demo/demo-wget.cpp ${HTTP_SERVER_SRC})
target_link_libraries(demo-wget
        PRIVATE
        PUBLIC
        ${OPENSSL_LIBRARIES})

add_executable(demo-http-echo-service ${CMAKE_SOURCE_DIR}/demo/demo-http-echo-service.cpp ${HTTP_SERVER_SRC})
target_link_libraries(demo-http-echo-service
        PRIVATE
        PUBLIC
        ${OPENSSL_LIBRARIES})

add_executable(demo-http-proxy ${CMAKE_SOURCE_DIR}/demo/demo-http-proxy.cpp ${HTTP_SERVER_SRC})
target_link_libraries(demo-http-proxy
        PRIVATE
        PUBLIC
        ${OPENSSL_LIBRARIES})
