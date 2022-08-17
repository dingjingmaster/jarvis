project(demo LANGUAGES CXX)

include(${CMAKE_SOURCE_DIR}/app/utils/utils.cmake)
include(${CMAKE_SOURCE_DIR}/app/factory/factory.cmake)
include(${CMAKE_SOURCE_DIR}/app/modules/modules.cmake)

add_executable(demo-http-server ${CMAKE_SOURCE_DIR}/demo/demo-http-server.cpp
        ${FACTORY_SRC} ${HTTP_SERVER_SRC} ${UTILS_SRC})
target_link_libraries(demo-http-server
        PRIVATE
        PUBLIC
        ${OPENSSL_LIBRARIES})
