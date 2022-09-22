project(demo LANGUAGES CXX C)

include(${CMAKE_SOURCE_DIR}/app/modules/modules.cmake)

add_executable(jarvis ${CMAKE_SOURCE_DIR}/app/main/main.cpp ${HTTP_SERVER_SRC})
target_link_libraries(jarvis
        PRIVATE
        PUBLIC
        ${OPENSSL_LIBRARIES})

