#include "stream.h"
#include "lb_endian.h"
#include <string.h>

int classstream_readU8(ClassStream_t* stream, uint8_t* output){
    return !(fread(output, 1, 1, stream) == 1);
}

int classstream_readU16(ClassStream_t* stream, uint16_t* output){
    uint16_t tmp = 0;

    if(fread(&tmp, sizeof(tmp), 1, stream) == 1){
        *output = be16_to_cpu(tmp);
    } else return 1;
    return 0;
}

int classstream_readU32(ClassStream_t* stream, uint32_t* output){
    uint32_t tmp = 0;

    if(fread(&tmp, sizeof(tmp), 1, stream) == 1){
        *output = be32_to_cpu(tmp);
    } else return 1;
    return 0;
}

int classstream_readU64(ClassStream_t* stream, uint64_t* output){
    uint64_t tmp = 0;

    if(fread(&tmp, sizeof(tmp), 1, stream) == 1){
        *output = be64_to_cpu(tmp);
    } else return 1;
    return 0;
}

int classstream_readBUF(ClassStream_t* stream, uint8_t* output, size_t size){
    return !(fread(output, size, 1, stream) == 1);
}

int classstream_skip(ClassStream_t* stream, size_t n){
    return fseek(stream, n, SEEK_CUR) != 0;
}
//=========================================================================