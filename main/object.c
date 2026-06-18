#include "object.h"
#include "class.h"
#include "config.h"
#include "jerror.h"
#include "thread.h"

#include <stdlib.h>
#include <assert.h>

Error_t object_class_alloc(Class_t* class, int32_t* output){
    Error_t err = JERR_OK;
    FAIL_SET_JUMP(class->flags.is_abstract == 0 && class->flags.is_interface == 0, err, JERR_TYPECHECK_FAILURE, exit);
    FAIL_SET_JUMP(class->flags.is_array == 0, err, JERR_BADPARAM, exit);
    FAIL_SET_JUMP(output, err, JERR_BADPARAM, exit);

    void* object_memory = calloc(1, class->object_size + sizeof(Object_t));
    assert(object_memory);

    ((Object_t*)object_memory)->class = class;
    ((Object_t*)object_memory)->forward_ptr = 0;
    
    *output = (int32_t)object_memory;

exit:
    return err;
}

Error_t object_class_get_field(Object_t* object, Field_t* field, void* output){
    Error_t err = JERR_OK;
    FAIL_SET_JUMP(class_is_compatible(object->class, field->class), err, JERR_TYPECHECK_FAILURE, exit);

exit:
    return err;
}

Error_t object_array_alloc(Class_t* class, int32_t count, int32_t* output){
    Error_t err = JERR_OK;
    FAIL_SET_JUMP(class->flags.is_array == 1, err, JERR_BADPARAM, exit);
    FAIL_SET_JUMP(output, err, JERR_BADPARAM, exit);
    FAIL_SET_JUMP(count >= 0, err, JERR_BADPARAM, exit);

exit:
    return err;
}