#include "memstream.h"
#include "lb_endian.h"
#include <string.h>

//THIS PIECE OF CODE IS ONLY FOR READING BIG ENDIAN CLASS FORMAT FOR LOADER
void memstream_init(memstream_t* memstream, uint8_t* buffer, size_t size){
    memstream->buffer = buffer;
    memstream->pos = 0;
    memstream->buffer_size = size;
}

memstream_error_t memstream_readU8(memstream_t* memstream, uint8_t* output){
    if(memstream->pos >= memstream->buffer_size) return MSERR_EOF;
    *output = memstream->buffer[memstream->pos++];

    return MSERR_OK;
}

memstream_error_t memstream_readU16(memstream_t* memstream, uint16_t* output){
    uint16_t be = 0;
    if(memstream->pos + sizeof(be) > memstream->buffer_size) return MSERR_EOF;

    memcpy(&be,&memstream->buffer[memstream->pos],sizeof(be));

    memstream->pos += sizeof(be);
    *output = be16_to_cpu(be);

    return MSERR_OK;
}

memstream_error_t memstream_readU32(memstream_t* memstream, uint32_t* output){
    uint32_t be = 0;
    if(memstream->pos + sizeof(be) > memstream->buffer_size) return MSERR_EOF;

    memcpy(&be,&memstream->buffer[memstream->pos],sizeof(be));

    memstream->pos += sizeof(be);
    *output = be32_to_cpu(be);

    return MSERR_OK;
}

memstream_error_t memstream_readU64(memstream_t* memstream, uint64_t* output){
    uint64_t be = 0;
    if(memstream->pos + sizeof(be) > memstream->buffer_size) return MSERR_EOF;

    memcpy(&be,&memstream->buffer[memstream->pos],sizeof(be));

    memstream->pos += sizeof(be);
    *output = be64_to_cpu(be);

    return MSERR_OK;
}

memstream_error_t memstream_readBUF(memstream_t* memstream, uint8_t* output, size_t size){
    if(memstream->pos + size > memstream->buffer_size) return MSERR_EOF;

    memcpy(output,&memstream->buffer[memstream->pos],size);
    memstream->pos += size;

    return MSERR_OK;
}

memstream_error_t memstream_skip(memstream_t* memstream, size_t n){
    if(memstream->pos + n > memstream->buffer_size) return MSERR_EOF;
    memstream->pos += n;
    return MSERR_OK;
}
//=========================================================================