file(GLOB FACTORY_SRC
        ${CMAKE_SOURCE_DIR}/app/factory/task.h
        ${CMAKE_SOURCE_DIR}/app/factory/connection.h

        ${CMAKE_SOURCE_DIR}/app/factory/dns-task-impl.cpp
        ${CMAKE_SOURCE_DIR}/app/factory/http-task-impl.cpp
        ${CMAKE_SOURCE_DIR}/app/factory/file-task-impl.cpp

        ${CMAKE_SOURCE_DIR}/app/factory/graph-task.h
        ${CMAKE_SOURCE_DIR}/app/factory/graph-task.cpp

        ${CMAKE_SOURCE_DIR}/app/factory/task-factory.h
        ${CMAKE_SOURCE_DIR}/app/factory/task-factory.cpp

        ${CMAKE_SOURCE_DIR}/app/factory/workflow.h
        ${CMAKE_SOURCE_DIR}/app/factory/workflow.cpp

        ${CMAKE_SOURCE_DIR}/app/factory/resource-pool.h
        ${CMAKE_SOURCE_DIR}/app/factory/resource-pool.cpp
        )
