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

add_executable(demo-parallel-wget ${CMAKE_SOURCE_DIR}/demo/demo-parallel-wget.cpp ${HTTP_SERVER_SRC})
target_link_libraries(demo-parallel-wget
        PRIVATE
        PUBLIC
        ${OPENSSL_LIBRARIES})

add_executable(demo-sort-task ${CMAKE_SOURCE_DIR}/demo/demo-sort-task.cpp ${HTTP_SERVER_SRC})
target_link_libraries(demo-sort-task
        PRIVATE
        PUBLIC
        ${OPENSSL_LIBRARIES})

add_executable(demo-matrix-multiply ${CMAKE_SOURCE_DIR}/demo/demo-matrix-multiply.cpp ${HTTP_SERVER_SRC})
target_link_libraries(demo-matrix-multiply
        PRIVATE
        PUBLIC
        ${OPENSSL_LIBRARIES})

add_executable(demo-http-file-server ${CMAKE_SOURCE_DIR}/demo/demo-http-file-server.cpp ${HTTP_SERVER_SRC})
target_link_libraries(demo-http-file-server
        PRIVATE
        PUBLIC
        ${OPENSSL_LIBRARIES})

add_executable(demo-graph-task ${CMAKE_SOURCE_DIR}/demo/demo-graph-task.cpp ${HTTP_SERVER_SRC})
target_link_libraries(demo-graph-task
        PRIVATE
        PUBLIC
        ${OPENSSL_LIBRARIES})

add_executable(demo-name-service ${CMAKE_SOURCE_DIR}/demo/demo-name-service.cpp ${HTTP_SERVER_SRC})
target_link_libraries(demo-name-service
        PRIVATE
        PUBLIC
        ${OPENSSL_LIBRARIES})
