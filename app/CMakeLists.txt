project(demo LANGUAGES CXX C)

include(${CMAKE_SOURCE_DIR}/app/modules/modules.cmake)

add_executable(jarvis ${CMAKE_SOURCE_DIR}/app/main/main.cpp
        ${CMAKE_SOURCE_DIR}/app/main/http-router.h
        ${CMAKE_SOURCE_DIR}/app/main/http-router.cpp

        ${HTTP_SERVER_SRC})
target_link_libraries(jarvis
        PRIVATE
        PUBLIC
        ${OPENSSL_LIBRARIES} ${GLIB_LIBRARIES} -lsqlite3)

target_include_directories(jarvis PUBLIC ${GLIB_INCLUDE_DIRS})

