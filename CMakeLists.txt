project(Jarvis LANGUAGES CXX C)

cmake_minimum_required(VERSION 3.5)

set(DEBUG ON)

set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 0)
set(PROJECT_VERSION_PATCH 0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(DEBUG)
    add_definitions(-g -DDEBUG)
else()
    add_definitions(-w)
endif()

add_definitions(
        -D LOG_TAG=\\"${CMAKE_PROJECT_NAME}\\"
)


find_package (PkgConfig)

pkg_check_modules(GIO REQUIRED gio-2.0)
pkg_check_modules(OPENSSL REQUIRED openssl)

enable_testing()
set_tests_properties(${noArgsTests} PROPERTIES TIMEOUT 1000)
set_tests_properties(${withArgsTests} PROPERTIES TIMEOUT 20000)

add_subdirectory(app)
add_subdirectory(demo)
add_subdirectory(test)
