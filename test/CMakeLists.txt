project(test LANGUAGES CXX)

include(../app/core/core.cmake)


find_package(GTest)
pkg_check_modules(GTEST REQUIRED gtest)

add_executable(test-rb-tree ${CMAKE_SOURCE_DIR}/test/test-rb-tree.cpp ${CORE_SRC})
target_link_libraries(test-rb-tree
        PRIVATE ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES} ${GMOCK_LIBRARIES}
        PUBLIC
        ${OPENSSL_LIBRARIES})
gtest_discover_tests(test-rb-tree)

add_executable(test-c-poll ${CMAKE_SOURCE_DIR}/test/test-c-poll.cpp ${CORE_SRC})
target_link_libraries(test-c-poll
        PRIVATE ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES} ${GMOCK_LIBRARIES}
        PUBLIC
        ${OPENSSL_LIBRARIES})
gtest_discover_tests(test-c-poll)
