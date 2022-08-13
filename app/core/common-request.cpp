//
// Created by dingjing on 8/13/22.
//

#include "common-request.h"

void CommonRequest::handle(int state, int error)
{
    mState = state;
    mError = error;
    if (error != ETIMEDOUT) {
        mTimeoutReason = TOR_NOT_TIMEOUT;
    } else if (!mTarget) {
        mTimeoutReason = TOR_WAIT_TIMEOUT;
    } else if (!getMessageOut()) {
        mTimeoutReason = TOR_CONNECT_TIMEOUT;
    } else {
        mTimeoutReason = TOR_TRANSMIT_TIMEOUT;
    }

    subTaskDone();
}
