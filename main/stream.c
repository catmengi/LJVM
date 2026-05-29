#include "stream.h"
#include "lb_endian.h"
#include <string.h>

int fstream_readU8(void* readdata, uint8_t* output){
    return !(fread(output, 1, 1, readdata) == 1);
}

int fstream_readU16(void* readdata, uint16_t* output){
    uint16_t tmp = 0;

    if(fread(&tmp, sizeof(tmp), 1, readdata) == 1){
        *output = be16_to_cpu(tmp);
    } else return 1;
    return 0;
}

int fstream_readU32(void* readdata, uint32_t* output){
    uint32_t tmp = 0;

    if(fread(&tmp, sizeof(tmp), 1, readdata) == 1){
        *output = be32_to_cpu(tmp);
    } else return 1;
    return 0;
}

int fstream_readU64(void* readdata, uint64_t* output){
    uint64_t tmp = 0;

    if(fread(&tmp, sizeof(tmp), 1, readdata) == 1){
        *output = be64_to_cpu(tmp);
    } else return 1;
    return 0;
}

int fstream_readBUF(void* readdata, uint8_t* output, size_t size){
    return !(fread(output, size, 1, readdata) == 1);
}

int fstream_skip(void* readdata, size_t n){
    return fseek(readdata, n, SEEK_CUR) != 0;
}

//Old code reuse
typedef enum{
    MSERR_EOF = 1,
    MSERR_OK = 0,
}memstream_error_t;

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

//==============


int classstream_readU8(ClassStream_t* stream, uint8_t* output){
    return stream->readU8(stream->readdata, output);
}

int classstream_readU16(ClassStream_t* stream, uint16_t* output){
    return stream->readU16(stream->readdata, output);
}

int classstream_readU32(ClassStream_t* stream, uint32_t* output){
    return stream->readU32(stream->readdata, output);
}

int classstream_readU64(ClassStream_t* stream, uint64_t* output){
    return stream->readU64(stream->readdata, output);
}

int classstream_readBUF(ClassStream_t* stream, uint8_t* output, size_t size){
    return stream->readBUF(stream->readdata, output, size);
}

int classstream_skip(ClassStream_t* stream, size_t n){
    return stream->skip(stream->readdata, n);
}
//=========================================================================


int classstream_init_file(ClassStream_t* stream, FILE* file){
    stream->readdata = file;
    if(!stream->readdata) return 1;
    
    stream->readU8 = fstream_readU8;
    stream->readU16 = fstream_readU16;
    stream->readU32 = fstream_readU32;
    stream->readU64 = fstream_readU64;
    stream->readBUF = fstream_readBUF;
    stream->skip = fstream_skip;

    return 0;
}

int classstream_init_memstream(ClassStream_t* stream, memstream_t* memstream){
    stream->readdata = memstream;

    stream->readU8 = (int(*)(void*, uint8_t*))memstream_readU8;
    stream->readU16 = (int(*)(void*, uint16_t*))memstream_readU16;
    stream->readU32 = (int(*)(void*, uint32_t*))memstream_readU32;
    stream->readU64 = (int(*)(void*, uint64_t*))memstream_readU64;
    stream->readBUF = (int(*)(void*, uint8_t*, size_t))memstream_readBUF;
    stream->skip = (int (*)(void*, size_t))memstream_skip;

    return 0;
}