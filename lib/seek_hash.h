// Copyright 2026 Zack Scholl, GPLv3.0
#ifndef SEEK_HASH_H
#define SEEK_HASH_H
#include <stddef.h>
#include <stdint.h>
typedef struct { uint32_t h[8]; uint64_t bytes; uint8_t block[64]; } seek_sha256_t;
void seek_sha256_init(seek_sha256_t *s);
void seek_sha256_update(seek_sha256_t *s, const void *data, size_t bytes);
void seek_sha256_final(seek_sha256_t *s, uint8_t digest[32]);
uint32_t seek_crc32(const void *data, size_t bytes);
#endif
