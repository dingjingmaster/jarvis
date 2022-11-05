//
// Created by dingjing on 8/11/22.
//
#include "../app/core/c-poll.h"
#include <gtest/gtest.h>

TEST(CPOLL, CREATE) {
    CPollParams param = {
            .maxOpenFiles = 100,
            .createMessage = NULL,
            .partialWritten = NULL,
            .callback = NULL,
            .context = NULL
    };
    CPoll* poll = poll_create(&param);
    poll_destroy(poll);

    ASSERT_TRUE(poll);
};

TEST(CPOLL, RUN) {
    CPollParams param = {
            .maxOpenFiles = 100,
            .createMessage = NULL,
            .partialWritten = NULL,
            .callback = NULL,
            .context = NULL
    };
    CPoll* poll = poll_create(&param);

    ASSERT_TRUE(!poll_start(poll));

    poll_stop(poll);

    poll_destroy(poll);
};
