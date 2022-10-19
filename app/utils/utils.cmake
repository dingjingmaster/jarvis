file(GLOB UTILS_SRC
        ${CMAKE_SOURCE_DIR}/app/utils/json-utils.h
        ${CMAKE_SOURCE_DIR}/app/utils/json-utils.cpp

        ${CMAKE_SOURCE_DIR}/app/utils/date-utils.h
        ${CMAKE_SOURCE_DIR}/app/utils/date-utils.cpp

        ${CMAKE_SOURCE_DIR}/app/utils/sqlite-utils.h
        ${CMAKE_SOURCE_DIR}/app/utils/sqlite-utils.cpp

        ${CMAKE_SOURCE_DIR}/app/utils/file-read-write.h
        ${CMAKE_SOURCE_DIR}/app/utils/file-read-write.cpp

        ${CMAKE_SOURCE_DIR}/app/utils/string-util.h
        ${CMAKE_SOURCE_DIR}/app/utils/string-util.cpp

        ${CMAKE_SOURCE_DIR}/app/utils/uri-parser.h
        ${CMAKE_SOURCE_DIR}/app/utils/uri-parser.cpp

        ${CMAKE_SOURCE_DIR}/app/utils/encode-stream.h
        ${CMAKE_SOURCE_DIR}/app/utils/encode-stream.cpp

        ${CMAKE_SOURCE_DIR}/app/utils/md5-util.h
        ${CMAKE_SOURCE_DIR}/app/utils/md5-util.cpp

        ${CMAKE_SOURCE_DIR}/app/utils/crc32c.h
        ${CMAKE_SOURCE_DIR}/app/utils/crc32c.c

        ${CMAKE_SOURCE_DIR}/app/utils/json-parser.h
        ${CMAKE_SOURCE_DIR}/app/utils/json-parser.c
)