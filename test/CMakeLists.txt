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

add_executable(test-msg-queue ${CMAKE_SOURCE_DIR}/test/test-msg-queue.cpp ${CORE_SRC})
target_link_libraries(test-msg-queue
        PRIVATE ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES} ${GMOCK_LIBRARIES}
        PUBLIC
        ${OPENSSL_LIBRARIES})
gtest_discover_tests(test-msg-queue)

add_executable(test-thread-pool ${CMAKE_SOURCE_DIR}/test/test-thread-pool.cpp ${CORE_SRC})
target_link_libraries(test-thread-pool
        PRIVATE ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES} ${GMOCK_LIBRARIES}
        PUBLIC
        ${OPENSSL_LIBRARIES})
gtest_discover_tests(test-thread-pool)

add_executable(test-sub-task ${CMAKE_SOURCE_DIR}/test/test-sub-task.cpp ${CORE_SRC})
target_link_libraries(test-sub-task
        PRIVATE ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES} ${GMOCK_LIBRARIES}
        PUBLIC
        ${OPENSSL_LIBRARIES})
gtest_discover_tests(test-sub-task)
