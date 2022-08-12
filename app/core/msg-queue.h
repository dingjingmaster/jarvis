//
// Created by dingjing on 8/12/22.
//

#ifndef JARVIS_MSG_QUEUE_H
#define JARVIS_MSG_QUEUE_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif

typedef struct _MsgQueue            MsgQueue;

void* msg_queue_get (MsgQueue* queue);
void msg_queue_destroy (MsgQueue* queue);
void msg_queue_set_block (MsgQueue* queue);
void msg_queue_set_nonblock (MsgQueue* queue);
void msg_queue_put (void* msg, MsgQueue* queue);
MsgQueue* msg_queue_create (size_t maxLen, int linkOff);

#ifdef __cplusplus
};
#endif
#endif //JARVIS_MSG_QUEUE_H
