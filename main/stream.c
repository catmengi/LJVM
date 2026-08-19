/*
JEspressoVM - project to bring java bytecode execution to esp32 (and others)

Copyright (C) 2026  Vladislav Potrashkov

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; If not, see <http://www.gnu.org/licenses/>.
*/

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
    if(size == 0) return 0; //?
    return !(fread(output, size, 1, readdata) == 1);
}

int fstream_skip(void* readdata, size_t n){
    return fseek(readdata, n, SEEK_CUR) != 0;
}

void fstream_close(void* readdata){
    fclose(readdata);
}


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

void classstream_close(ClassStream_t* stream){
    stream->close(stream->readdata);
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
    stream->close = fstream_close;

    return 0;
}