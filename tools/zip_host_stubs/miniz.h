/* SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 仅为主机测试把分段 raw inflate 适配到 zlib。
 * Adapt chunked raw inflate to zlib for host tests only.
 * 冻结：不替换 ZIP 解析，不用于验证 ROM 实现。/ Frozen: Do not replace ZIP parsing or claim ROM validation.
 */
#pragma once
#include <stddef.h>
#include <string.h>
#include <zlib.h>
typedef unsigned char mz_uint8;
typedef unsigned int mz_uint32;
// ROM状态不持有额外堆；把zlib工作区放入状态，前缀提前退出也可直接free。
// ROM state owns no external heap; inline zlib workspace so prefix exits can free the state.
typedef struct {
    z_stream stream;
    int initialized;
    size_t used;
    union { max_align_t align; unsigned char bytes[65536]; } arena;
} tinfl_decompressor;
static inline voidpf host_inflate_alloc(voidpf opaque, uInt items, uInt size) {
    tinfl_decompressor *state = opaque;
    size_t alignment = _Alignof(max_align_t);
    size_t at = (state->used + alignment - 1) / alignment * alignment;
    size_t bytes = (size_t)items * size;
    if ((size && bytes / size != items) || at > sizeof(state->arena.bytes) ||
        bytes > sizeof(state->arena.bytes) - at) return NULL;
    state->used = at + bytes;
    return state->arena.bytes + at;
}
static inline void host_inflate_free(voidpf opaque, voidpf address) {
    (void)opaque; (void)address;
}
typedef enum { TINFL_STATUS_FAILED = -1, TINFL_STATUS_DONE = 0,
               TINFL_STATUS_NEEDS_MORE_INPUT = 1, TINFL_STATUS_HAS_MORE_OUTPUT = 2 } tinfl_status;
#define TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF 4
#define TINFL_FLAG_HAS_MORE_INPUT 2
#define TINFL_FLAG_PARSE_ZLIB_HEADER 1
#define tinfl_init(r) memset((r), 0, sizeof(*(r)))
static inline tinfl_status tinfl_decompress(tinfl_decompressor* state, const mz_uint8* input,
    size_t* in_size, mz_uint8* start, mz_uint8* output, size_t* out_size, mz_uint32 flags) {
    (void)start;
    if (!state->initialized) {
        state->used = 0;
        state->stream.zalloc = host_inflate_alloc;
        state->stream.zfree = host_inflate_free;
        state->stream.opaque = state;
        // 带 zlib 头时按 zlib 流解，否则按 raw deflate。/ With a zlib header inflate a zlib stream, else raw deflate.
        if (inflateInit2(&state->stream, (flags & TINFL_FLAG_PARSE_ZLIB_HEADER) ? MAX_WBITS : -MAX_WBITS) != Z_OK) return TINFL_STATUS_FAILED;
        state->initialized = 1;
    }
    z_stream* stream = &state->stream;
    unsigned char empty;
    size_t available_in = *in_size, available_out = *out_size;
    stream->next_in = (Bytef*)input;
    stream->avail_in = (uInt)available_in;
    stream->next_out = available_out ? output : &empty;
    stream->avail_out = available_out ? (uInt)available_out : 1;
    int result = inflate(stream, Z_NO_FLUSH);
    *in_size = available_in - stream->avail_in;
    *out_size = available_out ? available_out - stream->avail_out : 0;
    if (!available_out && stream->avail_out != 1) result = Z_DATA_ERROR;
    if (result == Z_STREAM_END) { inflateEnd(stream); state->initialized = 0; return TINFL_STATUS_DONE; }
    if (result == Z_OK || result == Z_BUF_ERROR)
        return stream->avail_out == 0 ? TINFL_STATUS_HAS_MORE_OUTPUT : TINFL_STATUS_NEEDS_MORE_INPUT;
    inflateEnd(stream); state->initialized = 0;
    return TINFL_STATUS_FAILED;
}
