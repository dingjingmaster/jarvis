file(GLOB CORE_SRC
        ${CMAKE_SOURCE_DIR}/app/core/c-list.h

        ${CMAKE_SOURCE_DIR}/app/core/rb-tree.c
        ${CMAKE_SOURCE_DIR}/app/core/rb-tree.h

        ${CMAKE_SOURCE_DIR}/app/core/c-poll.c
        ${CMAKE_SOURCE_DIR}/app/core/c-poll.h

        ${CMAKE_SOURCE_DIR}/app/core/msg-queue.c
        ${CMAKE_SOURCE_DIR}/app/core/msg-queue.h

        ${CMAKE_SOURCE_DIR}/app/core/thread-pool.c
        ${CMAKE_SOURCE_DIR}/app/core/thread-pool.h


        ${CMAKE_SOURCE_DIR}/app/core/sub-task.h
        ${CMAKE_SOURCE_DIR}/app/core/sub-task.cpp

        ${CMAKE_SOURCE_DIR}/app/core/executor.h
        ${CMAKE_SOURCE_DIR}/app/core/executor.cpp
)
