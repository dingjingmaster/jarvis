project(jarvis-gui LANGUAGES C CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(${CMAKE_SOURCE_DIR}/3thrd/3thrd.cmake)
include(${CMAKE_SOURCE_DIR}/common/common.cmake)
include(${CMAKE_SOURCE_DIR}/gui/main/main.cmake)

file(GLOB GUI_SRC
        ${SINGLEAPP_GUI_SRC}
        ${MAIN_SRC} ${CMAKE_SOURCE_DIR}/data/res.qrc
        )

add_executable(jarvis-gui main.cpp ${GUI_SRC} ${COMMON_SRC} )

target_link_libraries(jarvis-gui ${X11_LIBRARIES} ${GLIB_LIBRARIES}
        Qt5::Core Qt5::Network Qt5::Gui Qt5::Widgets Qt5::X11Extras)

target_include_directories(jarvis-gui
        PUBLIC ${QT5_INCLUDE_DIRS} ${GLIB_INCLUDE_DIRS} ${X11_INCLUDE_DIRS}
        PUBLIC ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/gui)

target_compile_definitions(jarvis-gui
        PUBLIC -D QT_NO_KEYWORDS -D QT_MESSAGELOGCONTEXT
        PUBLIC -D LOG_TAG="jarvis-gui"
        PUBLIC -D WINDOW_TITLE="终端数据防泄露系统"
        )