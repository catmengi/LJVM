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

#include "config.h"
#include "stringpool.h"
#include "class.h"

#include <assert.h>
#include "loader.h"


int app_main(){
    JEspresso_init();

    loader_set_apppath("java_src");
    loader_set_systempath("java_src");

    Class_t* main_class = NULL;
    assert(class_load_bynameid(stringpool_add("JEspressoTest"), &main_class) == JERR_OK);

    Method_t* main = class_find_method(main_class, stringpool_add("main@([Ljava/lang/String;)V"));
    assert(main);

    Class_t* system = NULL;
    assert(class_load_bynameid(stringpool_add("java/lang/System"), &system) == JERR_OK);


    return 0;
}

int main(){
    return app_main();
}