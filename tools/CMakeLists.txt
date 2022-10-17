project(demo LANGUAGES CXX C)

include(${CMAKE_SOURCE_DIR}/app/modules/modules.cmake)

add_executable(gold-tool ${CMAKE_SOURCE_DIR}/tools/gold-tool.cpp ${HTTP_SERVER_SRC})
target_link_libraries(gold-tool
        PRIVATE
        PUBLIC
        ${OPENSSL_LIBRARIES} ${SQLITE_LIBRARIES})
