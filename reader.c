#include "reader.h"
#include "lb_endian.h"
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>


static int class_read_int(void* userctx, void* output_int, char int_size){
    char bytebuf[int_size];
    int err = fread(bytebuf,int_size,1,userctx);

    if(err > 0){
        switch(int_size){
            case sizeof(uint8_t):
                *(uint8_t*)output_int = *(uint8_t*)bytebuf;
                break;
            case sizeof(uint16_t):
                *(uint16_t*)output_int = be16_to_cpu(*(uint16_t*)bytebuf);
                break;
            case sizeof(uint32_t):
                *(uint32_t*)output_int = be32_to_cpu(*(uint32_t*)bytebuf);
                break;
            case sizeof(uint64_t):
                *(uint64_t*)output_int = be64_to_cpu(*(uint64_t*)bytebuf);
                break;
            
            default: assert(0);
        }
    }

    return err;
}
static int class_read_bytes(void* userctx, void* output, unsigned size){
    return fread(output,size,1,userctx);
}
static bool class_seek(void* userctx, int n){
    return !fseek(userctx,n,SEEK_CUR);
}

int file_open_class(file_reader_t* reader, const char* path){
    if(reader && path){
        reader->userctx = fopen(path, "rb");
        if(reader->userctx){
            reader->read_int = class_read_int;
            reader->read_bytes = class_read_bytes;
            reader->seek = class_seek;

            return 0;
        }
    }
    return 1;
}

int file_read_int(file_reader_t* reader, void* output_int, char int_size){
    return reader->read_int(reader->userctx, output_int,int_size);
}

int file_read_bytes(file_reader_t* reader, void* output, unsigned size){
    return reader->read_bytes(reader->userctx,output, size);
}

bool file_seek(file_reader_t* reader, int n){
    return reader->seek(reader->userctx,n);
}