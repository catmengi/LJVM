#pragma once

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>

//THIS PIECE OF CODE IS ONLY FOR READING BIG ENDIAN CLASS FORMAT FOR LOADER

typedef struct{
    void* readdata;

    int (*readU8)(void* readdata, uint8_t* output);
    int (*readU16)(void* readdata, uint16_t* output);
    int (*readU32)(void* readdata, uint32_t* output);
    int (*readU64)(void* readdata, uint64_t* output);
    int (*readBUF)(void* readdata, uint8_t* output, size_t size);
    int (*skip)(void* readdata, size_t n);
}ClassStream_t;

typedef struct{
    uint8_t* buffer;
    size_t buffer_size, pos;
}memstream_t;

int classstream_readU8(ClassStream_t* stream, uint8_t* output);
int classstream_readU16(ClassStream_t* stream, uint16_t* output);
int classstream_readU32(ClassStream_t* stream, uint32_t* output);
int classstream_readU64(ClassStream_t* stream, uint64_t* output);
int classstream_readBUF(ClassStream_t* stream, uint8_t* output, size_t size);
int classstream_skip(ClassStream_t* stream, size_t n);
//=========================================================================

int classstream_init_file(ClassStream_t* stream, FILE* file);
int classstream_init_memstream(ClassStream_t* stream, memstream_t* memstream);

void memstream_init(memstream_t* memstream, uint8_t* buffer, size_t size);