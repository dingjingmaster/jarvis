//
// Created by dingjing on 8/17/22.
//

#ifndef JARVIS_CRC32C_H
#define JARVIS_CRC32C_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

uint32_t crc32c(uint32_t crc, const void *buf, size_t len);

void crc32c_global_init (void);

#ifdef __cplusplus
}
#endif

#endif //JARVIS_CRC32C_H
