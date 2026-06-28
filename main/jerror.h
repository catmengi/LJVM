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

#pragma once

typedef enum{
    JERR_OK,
    JERR_OOM,
    JERR_BADPARAM,
    JERR_NOTFOUND,
    JERR_ORPHAN_RETURN,
    JERR_TYPECHECK_FAILURE,
    JERR_UNHANDLED_EXCEPTION,

    //Exception-generating errors:
    JERR_NOCLASSDEF,
    JERR_INVALIDMONITORSTATE,
    JERR_NULLPOINTER,
    JERR_NOSUCHFIELD,
    JERR_NOSUCHMETHOD,
    JERR_ABSTRACT,
    JERR_ILLEGALACCESS,
    JERR_EXCEPTION, //Made interpreter throw exception object from stack top
    JERR_INCOMPATIBLECLASSCHANGE,
    JERR_INSTANTIATION,
    JERR_NEGATIVESIZE,
    JERR_INDEXOOB,
    JERR_CAST,

    JERR_UNKNOWN,
}Error_t;

#define __FSJ_DO_BREAK__

//This macro is not entriely means error, it might used for better looking error code propagation (JERR_SCHEDULE is one of the examples)
#ifdef __FSJ_DO_BREAK__
    static void FSJ_BREAK(){}
    #define FAIL_SET_JUMP(expression, var, value, label) {if(!(expression)){(var) = (value); printf("%s:%d ERROR HAPPENED, CODE: %d\n",__PRETTY_FUNCTION__,__LINE__,(unsigned)(size_t)(value)); FSJ_BREAK(); goto label;}}
#else
    #define FAIL_SET_JUMP(expression, var, value, label) {if(!(expression)){(var) = (value); goto label;}}
#endif