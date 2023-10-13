project(jarvis-daemon LANGUAGES C CXX)

include(${CMAKE_SOURCE_DIR}/3thrd/3thrd.cmake)
include(${CMAKE_SOURCE_DIR}/common/common.cmake)
include(${CMAKE_SOURCE_DIR}/daemon/main/main.cmake)
include(${CMAKE_SOURCE_DIR}/daemon/plugins/plugins.cmake)
include(${CMAKE_SOURCE_DIR}/daemon-interface/daemon-interface.cmake)

file(GLOB MAIN_SRC ${SINGLEAPP_SRC} ${MAIN_SRC} ${DAEMON_IFACE_SRC})

add_executable(jarvis-daemon main.cpp ${MAIN_SRC} ${COMMON_SRC} ${PLUGINS_SRC})

target_link_libraries(jarvis-daemon ${GLIB_LIBRARIES} Qt5::Core Qt5::Network ${PROTOBUF_LIBRARIES})

target_include_directories(jarvis-daemon
        PUBLIC ${GLIB_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/daemon
        PUBLIC ${PROTOBUF_INCLUDE_DIRS}
        )

target_compile_definitions(jarvis-daemon
        PUBLIC -D QT_NO_KEYWORDS -D QT_MESSAGELOGCONTEXT
        PUBLIC -D LOG_TAG="jarvis-daemon"
        PUBLIC -D PLUGINS_DIR="${INSTALL_DIR}/plugins"
        )