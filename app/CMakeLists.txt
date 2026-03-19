
include(${CMAKE_SOURCE_DIR}/common/common.cmake)
include(${CMAKE_SOURCE_DIR}/app/modules/modules.cmake)

add_executable(jarvis ${CMAKE_SOURCE_DIR}/app/main/main.cpp
        ${CMAKE_SOURCE_DIR}/app/db/db.h
        ${CMAKE_SOURCE_DIR}/app/db/db.cpp

        ${CMAKE_SOURCE_DIR}/app/config/configure.h
        ${CMAKE_SOURCE_DIR}/app/config/configure.cpp

        ${CMAKE_SOURCE_DIR}/app/main/http-router.h
        ${CMAKE_SOURCE_DIR}/app/main/http-router.cpp

        ${HTTP_SERVER_SRC} ${COMMON_SRC})
target_link_libraries(jarvis
        PRIVATE
        PUBLIC
        ${OPENSSL_LIBRARIES} ${GLIB_LIBRARIES} ${QT5CORE_LIBRARIES} ${MYSQL_LIBRARIES})

target_include_directories(jarvis PUBLIC ${GLIB_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR} ${QT5CORE_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/3thrd/ormpp ${CMAKE_SOURCE_DIR}/3thrd)

target_compile_definitions(jarvis PUBLIC -D LOG_TAG="jarvis" -DORMPP_ENABLE_MYSQL)
