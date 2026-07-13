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

#include <stdio.h>
#include <string.h>

#include "parser.h"
#include "stream.h"
#include "loader.h"


static char* system_prefixes[] = {"java/"};

static char s_path[256 + sizeof(".class") + 1] = {0};
static char s_app_classpath[256] = {0};
static char s_system_classpath[256] = {0};

static bool is_system_class(char* class_name){
    for(unsigned i = 0; i < sizeof(system_prefixes) / sizeof(system_prefixes[0]); i++){
        if(strncmp(class_name, system_prefixes[i], strlen(system_prefixes[i])) == 0) return true;
    }

    return false;
}

static int init_classtream(char* class_name, ClassStream_t* classstream){
    char* class_path = is_system_class(class_name) ? s_system_classpath : s_app_classpath;

    memset(s_path, 0, sizeof(s_path));
    snprintf(s_path, sizeof(s_path), "%s/%s.class", class_path, class_name);

    return classstream_init_file(classstream, fopen(s_path, "rb"));

    return -1;
}

JRawClass_t* loader_load_class(char* name){
    ClassStream_t stream = {0};
    if(init_classtream(name, &stream)) return NULL;

    return parser_parse_class(&stream);
}

int loader_set_apppath(char* path){
    if(!path || strlen(path) >= sizeof(s_app_classpath)) return 1;
    strncpy(s_app_classpath, path, sizeof(s_app_classpath));

    return 0;
}

int loader_set_systempath(char* path){
    if(!path || strlen(path) >= sizeof(s_system_classpath)) return 1;
    strncpy(s_system_classpath, path, sizeof(s_system_classpath));

    return 0;    
}

