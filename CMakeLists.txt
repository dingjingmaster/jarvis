cmake_minimum_required(VERSION 3.5)
project(Jarvis LANGUAGES CXX C)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 0)
set(PROJECT_VERSION_PATCH 0)

set(DEBUG ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(INSTALL_DIR "/usr/local/jarvis")

option(DEBUG "USE DEBUG" ON)

if(DEBUG)
    set(INSTALL_NAME ${CMAKE_BINARY_DIR}/app/jarvis)
    add_definitions(-O0 -g -rdynamic -Wl,--export-dynamic -D DEBUG -D WEB_HOME=\\"/data/code/skynet-web/dist/\\")
else()
    set(INSTALL_NAME ${INSTALL_DIR}/bin/jarvis)
    add_definitions(-O0 -g -rdynamic -Wl,--export-dynamic -D WEB_HOME=\\"/usr/local/jarvis/html/\\")
endif()

add_definitions(
        -D INSTALL_DIR=\\"${INSTALL_DIR}\\"
        -D INSTALL_NAME=\\"${INSTALL_NAME}\\"
        -D PACKAGE_NAME=\\"${CMAKE_PROJECT_NAME}\\"
        -D PACKAGE_VERSION=\\"${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}\\"
    )


find_package (PkgConfig)
find_package(Qt5 COMPONENTS Core Network Widgets Gui X11Extras REQUIRED)

pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(PROTO REQUIRED protobuf)
pkg_check_modules(SQLITE REQUIRED sqlite3)
pkg_check_modules(OPENSSL REQUIRED openssl)
pkg_check_modules(PROTOBUF REQUIRED protobuf)
pkg_check_modules(GLIB REQUIRED gio-2.0 json-glib-1.0)
#pkg_check_modules(QT5 REQUIRED Qt5Core Qt5Network Qt5Widgets Qt5Gui Qt5X11Extras)
#pkg_check_modules(JSON REQUIRED json-glib-1.0)

enable_testing()
set_tests_properties(${noArgsTests} PROPERTIES TIMEOUT 1000)
set_tests_properties(${withArgsTests} PROPERTIES TIMEOUT 20000)

#include_directories(${JSON_INCLUDE_DIRS})

include_directories(${GLIB_INCLUDE_DIRS})

if(DEBUG)
#add_subdirectory(demo)
endif()


add_subdirectory(app)
add_subdirectory(gui)
add_subdirectory(daemon)
#add_subdirectory(test)
#add_subdirectory(tools)
