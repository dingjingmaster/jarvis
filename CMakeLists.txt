project(Jarvis LANGUAGES CXX C)

cmake_minimum_required(VERSION 3.5)

set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 0)
set(PROJECT_VERSION_PATCH 0)

#set(DEBUG ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(DEBUG "USE DEBUG" ON)

if(DEBUG)
    set(INSTALL_NAME ${CMAKE_BINARY_DIR}/app/jarvis)
    add_definitions(-O0 -g -rdynamic -Wl,--export-dynamic -DDEBUG -D WEB_HOME=\\"/data/code/skynet-web/dist/\\")
else()
    set(INSTALL_NAME /usr/local/jarvis/bin/jarvis)
    add_definitions(-O0 -g -rdynamic -Wl,--export-dynamic -D WEB_HOME=\\"/usr/local/jarvis/html/\\")
endif()

add_definitions(
        -D LOG_TAG=\\"${CMAKE_PROJECT_NAME}\\"
        -D INSTALL_NAME=\\"${INSTALL_NAME}\\"
)


find_package (PkgConfig)

pkg_check_modules(GLIB REQUIRED gio-2.0 json-glib-1.0)
pkg_check_modules(SQLITE REQUIRED sqlite3)
pkg_check_modules(OPENSSL REQUIRED openssl)
#pkg_check_modules(JSON REQUIRED json-glib-1.0)

enable_testing()
set_tests_properties(${noArgsTests} PROPERTIES TIMEOUT 1000)
set_tests_properties(${withArgsTests} PROPERTIES TIMEOUT 20000)

#include_directories(${JSON_INCLUDE_DIRS})

if(DEBUG)
add_subdirectory(demo)
endif()


add_subdirectory(app)
add_subdirectory(test)
add_subdirectory(tools)
