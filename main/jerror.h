#pragma once

typedef enum{
    JERR_OK,
    JERR_OOM,
    JERR_BADPARAM,
    JERR_UNKNOWN,
    JERR_NOTFOUND,
}Error_t;

#define __FSJ_DO_BREAK__

#ifdef __FSJ_DO_BREAK__
    static void FSJ_BREAK(){}
    #define FAIL_SET_JUMP(expression, var, value, label) {if(!(expression)){(var) = (value); printf("%s:%d ERROR HAPPENED, CODE: %d\n",__PRETTY_FUNCTION__,__LINE__,(unsigned)(size_t)(value)); FSJ_BREAK(); goto label;}}
#else
    #define FAIL_SET_JUMP(expression, var, value, label) {if(!(expression)){(var) = (value); goto label;}}
#endif