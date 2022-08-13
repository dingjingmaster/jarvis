file(GLOB CORE_SRC
        ${CMAKE_SOURCE_DIR}/app/core/c-list.h
        ${CMAKE_SOURCE_DIR}/app/core/io-request.h
        ${CMAKE_SOURCE_DIR}/app/core/sleep-request.h

        ${CMAKE_SOURCE_DIR}/app/core/rb-tree.c
        ${CMAKE_SOURCE_DIR}/app/core/rb-tree.h

        ${CMAKE_SOURCE_DIR}/app/core/c-poll.c
        ${CMAKE_SOURCE_DIR}/app/core/c-poll.h

        ${CMAKE_SOURCE_DIR}/app/core/m-poll.h
        ${CMAKE_SOURCE_DIR}/app/core/m-poll.c

        ${CMAKE_SOURCE_DIR}/app/core/msg-queue.c
        ${CMAKE_SOURCE_DIR}/app/core/msg-queue.h

        ${CMAKE_SOURCE_DIR}/app/core/thread-pool.c
        ${CMAKE_SOURCE_DIR}/app/core/thread-pool.h

        ${CMAKE_SOURCE_DIR}/app/core/sub-task.h
        ${CMAKE_SOURCE_DIR}/app/core/sub-task.cpp

        ${CMAKE_SOURCE_DIR}/app/core/executor.h
        ${CMAKE_SOURCE_DIR}/app/core/executor.cpp

        ${CMAKE_SOURCE_DIR}/app/core/common-request.h
        ${CMAKE_SOURCE_DIR}/app/core/common-request.cpp

        ${CMAKE_SOURCE_DIR}/app/core/communicator.h
        ${CMAKE_SOURCE_DIR}/app/core/communicator.cpp

        ${CMAKE_SOURCE_DIR}/app/core/io-service.h
        ${CMAKE_SOURCE_DIR}/app/core/io-service.cpp

        ${CMAKE_SOURCE_DIR}/app/core/common-scheduler.h
        ${CMAKE_SOURCE_DIR}/app/core/common-scheduler.cpp
)
