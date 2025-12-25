#pragma once

#include <stdio.h>
#include <stdbool.h>

typedef struct{
    void* userctx;
    int (*read_int)(void* userctx, void* output_int, char int_size);
    int (*read_bytes)(void* userctx, void* output_bytes, unsigned size);
    bool (*seek)(void* userctx, int n);
}file_reader_t;

int file_open_class(file_reader_t* reader, const char* path); //inits reader and open class at path

int file_read_int(file_reader_t* reader, void* output_int, char int_size);
int file_read_bytes(file_reader_t* reader, void* output_bytes, unsigned size);
bool file_seek(file_reader_t* reader, int n);
