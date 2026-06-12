#pragma once

typedef enum{
    JERR_OK,
    JERR_OOM,
    JERR_BADPARAM,
    JERR_UNKNOWN,
    JERR_NOTFOUND,
    JERR_NOCLASSDEF,
    JERR_SCHEDULE, //Not a error, but rather a interpreter's saying to scheduler loop that it exited but thread must still be alive
    JERR_EXCEPTION, //Way of saying from native method that interpreter must take exception from retval
    JERR_ORPHAN_RETURN, //Interpreter way of saing that root method non void returned!
}Error_t;

#define __FSJ_DO_BREAK__

#ifdef __FSJ_DO_BREAK__
    static void FSJ_BREAK(){}
    #define FAIL_SET_JUMP(expression, var, value, label) {if(!(expression)){(var) = (value); printf("%s:%d ERROR HAPPENED, CODE: %d\n",__PRETTY_FUNCTION__,__LINE__,(unsigned)(size_t)(value)); FSJ_BREAK(); goto label;}}
#else
    #define FAIL_SET_JUMP(expression, var, value, label) {if(!(expression)){(var) = (value); goto label;}}
#endif