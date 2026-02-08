#pragma once
#include <stdint.h>
#include <sys/types.h>

//THIS PIECE OF CODE IS ONLY FOR READING BIG ENDIAN CLASS FORMAT FOR LOADER
typedef enum{
    MSERR_EOF = 1,
    MSERR_OK = 0,
}memstream_error_t;

typedef struct{
    uint8_t* buffer;
    size_t buffer_size, pos;
}memstream_t;

void memstream_init(memstream_t* memstream, uint8_t* buffer, size_t size);
memstream_error_t memstream_readU8(memstream_t* memstream, uint8_t* output);
memstream_error_t memstream_readU16(memstream_t* memstream, uint16_t* output);
memstream_error_t memstream_readU32(memstream_t* memstream, uint32_t* output);
memstream_error_t memstream_readU64(memstream_t* memstream, uint64_t* output);
memstream_error_t memstream_readBUF(memstream_t* memstream, uint8_t* output, size_t size);
memstream_error_t memstream_skip(memstream_t* memstream, size_t n);
//=========================================================================