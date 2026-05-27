#pragma once

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>

//THIS PIECE OF CODE IS ONLY FOR READING BIG ENDIAN CLASS FORMAT FOR LOADER

typedef FILE ClassStream_t;


int classstream_readU8(ClassStream_t* stream, uint8_t* output);
int classstream_readU16(ClassStream_t* stream, uint16_t* output);
int classstream_readU32(ClassStream_t* stream, uint32_t* output);
int classstream_readU64(ClassStream_t* stream, uint64_t* output);
int classstream_readBUF(ClassStream_t* stream, uint8_t* output, size_t size);
int classstream_skip(ClassStream_t* stream, size_t n);
//=========================================================================