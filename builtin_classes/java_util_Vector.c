#include "../jvm.h"
#include "../jvm_internal.h"
#include "../object.h"
#include "../class_linker.h"
#include <assert.h>
#include <stdint.h>

//Sorry this part of code was write mostly by CursorAI, because i was too lazy to write it myself, init part was done by me


extern classlinker_class_t java_lang_Object;

#define VECTOR_DEFAULT_SIZE 16
#define VECTOR_GROW_BY 16

static jvm_error_t vector_generic_init(jvm_frame_t* frame, int initialCapacity, int capacityIncrement){
    objectmanager_object_t* self = JVM_TO_C_VALUE(frame->locals[0],objectmanager_object_t*);
    classlinker_field_t* element_data = objectmanager_class_object_get_field(frame,objectmanager_get_class_object_info(self), "elementData");
    classlinker_field_t* field_cincrement = objectmanager_class_object_get_field(frame,objectmanager_get_class_object_info(self), "capacityIncrement");
    classlinker_field_t* field_elcount = objectmanager_class_object_get_field(frame,objectmanager_get_class_object_info(self), "elementCount");


    C_TO_JVM_VALUE(element_data->value,objectmanager_new_array_object(frame,EJVT_REFERENCE, initialCapacity));
    C_TO_JVM_VALUE(field_cincrement->value,capacityIncrement);
    C_TO_JVM_VALUE(field_elcount->value,((uint32_t)0));

    if(JVM_TO_C_VALUE(element_data->value,objectmanager_object_t*) == NULL)
        return JVM_OOM;
    else return JVM_OK;
}

static jvm_error_t vector_init(jvm_frame_t* frame){
    return vector_generic_init(frame,VECTOR_DEFAULT_SIZE,VECTOR_GROW_BY);
}

static jvm_error_t vector_init_sz(jvm_frame_t* frame){
    uint32_t initial_cappacity = JVM_TO_C_VALUE(frame->locals[1],uint32_t);
    return vector_generic_init(frame,initial_cappacity,VECTOR_GROW_BY);
}

static jvm_error_t vector_init_szci(jvm_frame_t* frame){
    uint32_t initial_cappacity = JVM_TO_C_VALUE(frame->locals[1],uint32_t);
    uint32_t cappacity_increment = JVM_TO_C_VALUE(frame->locals[2],uint32_t);
    return vector_generic_init(frame,initial_cappacity,cappacity_increment);    
}

static jvm_error_t vector_resize(jvm_frame_t* frame, objectmanager_object_t* self){
    jvm_error_t err = JVM_OK;
    
    classlinker_field_t* element_data_field = objectmanager_class_object_get_field(frame, objectmanager_get_class_object_info(self), "elementData");
    classlinker_field_t* capacity_increment_field = objectmanager_class_object_get_field(frame, objectmanager_get_class_object_info(self), "capacityIncrement");
    classlinker_field_t* element_count_field = objectmanager_class_object_get_field(frame, objectmanager_get_class_object_info(self), "elementCount");
    
    objectmanager_object_t* old_array = JVM_TO_C_VALUE(element_data_field->value, objectmanager_object_t*);
    objectmanager_array_object_t* old_array_info = objectmanager_get_array_object_info(old_array);
    uint32_t capacity_increment = JVM_TO_C_VALUE(capacity_increment_field->value, uint32_t);
    uint32_t element_count = JVM_TO_C_VALUE(element_count_field->value, uint32_t);
    
    uint32_t new_capacity = old_array_info->count;
    if(capacity_increment > 0){
        new_capacity += capacity_increment;
    } else {
        new_capacity *= 2;
        if(new_capacity == 0) new_capacity = 1;
    }
    
    jvm_lock(frame->jvm);
    objectmanager_object_t* new_array = objectmanager_new_array_object(frame, EJVT_REFERENCE, new_capacity);
    if(!new_array){
        jvm_unlock(frame->jvm);
        err = JVM_OOM;
        goto exit;
    }
    
    objectmanager_array_object_t* new_array_info = objectmanager_get_array_object_info(new_array);
    
    // Copy existing elements
    for(uint32_t i = 0; i < element_count && i < new_capacity; i++){
        new_array_info->elements[i] = old_array_info->elements[i];
    }
    
    // Update elementData field
    C_TO_JVM_VALUE(element_data_field->value, new_array);
    jvm_unlock(frame->jvm);
    
exit:
    return err;
}

static jvm_error_t vector_addElement(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;
    
    objectmanager_object_t* self = JVM_TO_C_VALUE(frame->locals[0],objectmanager_object_t*);
    objectmanager_object_t* elementData = JVM_TO_C_VALUE(objectmanager_class_object_get_field(frame,objectmanager_get_class_object_info(self), "elementData")->value,objectmanager_object_t*);

    classlinker_field_t* element_count_field = objectmanager_class_object_get_field(frame,objectmanager_get_class_object_info(self), "elementCount");

    uint32_t elementCount = JVM_TO_C_VALUE(element_count_field->value,uint32_t);
    if(elementCount >= objectmanager_get_array_object_info(elementData)->count){
        FAIL_SET_JUMP(vector_resize(frame, self) == JVM_OK, err, JVM_OOM, exit);
        // Re-get elementData after resize
        elementData = JVM_TO_C_VALUE(objectmanager_class_object_get_field(frame,objectmanager_get_class_object_info(self), "elementData")->value,objectmanager_object_t*);
    }

    objectmanager_array_object_t* vector_array = objectmanager_get_array_object_info(elementData);
    vector_array->elements[elementCount++] = frame->locals[1]; 

    C_TO_JVM_VALUE(element_count_field->value,elementCount);

exit:
    return err;
}

static jvm_error_t vector_capacity(jvm_frame_t* frame){
    objectmanager_object_t* self = JVM_TO_C_VALUE(frame->locals[0],objectmanager_object_t*);
    classlinker_field_t* element_data = objectmanager_class_object_get_field(frame,objectmanager_get_class_object_info(self), "elementData");

    objectmanager_array_object_t* vector_array = objectmanager_get_array_object_info(JVM_TO_C_VALUE(element_data->value,objectmanager_object_t*));
    jvm_value_t retval = {0};
    C_TO_JVM_VALUE(retval,vector_array->count);
    jvm_native_return(frame,retval);

    return JVM_OK;
}

static jvm_error_t vector_size(jvm_frame_t* frame){
    objectmanager_object_t* self = JVM_TO_C_VALUE(frame->locals[0],objectmanager_object_t*);
    classlinker_field_t* element_count_field = objectmanager_class_object_get_field(frame,objectmanager_get_class_object_info(self), "elementCount");
    
    uint32_t elementCount = JVM_TO_C_VALUE(element_count_field->value, uint32_t);
    jvm_value_t retval = {0};
    C_TO_JVM_VALUE(retval, elementCount);
    jvm_native_return(frame, retval);
    
    return JVM_OK;
}

static jvm_error_t vector_elementAt(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;
    
    objectmanager_object_t* self = JVM_TO_C_VALUE(frame->locals[0], objectmanager_object_t*);
    uint32_t index = JVM_TO_C_VALUE(frame->locals[1], uint32_t);
    
    classlinker_field_t* element_count_field = objectmanager_class_object_get_field(frame, objectmanager_get_class_object_info(self), "elementCount");
    classlinker_field_t* element_data_field = objectmanager_class_object_get_field(frame, objectmanager_get_class_object_info(self), "elementData");
    
    uint32_t elementCount = JVM_TO_C_VALUE(element_count_field->value, uint32_t);
    
    FAIL_SET_JUMP(index < elementCount, err, ({
        jvm_error_t errs = JVM_OK;
        jvm_lock(frame->jvm);
        objectmanager_object_t* exception = objectmanager_new_class_object(frame, classlinker_find_class(frame->jvm->linker, "java/lang/ArrayIndexOutOfBoundsException"));
        objectmanager_class_object_t* ecobject = objectmanager_get_class_object_info(exception);
        if(!exception){
            errs = JVM_OOM;
            jvm_unlock(frame->jvm);
        } else {
            jvm_value_t init_args[1] = {0};
            C_TO_JVM_VALUE(init_args[0], exception);
            errs = jvm_invoke(frame->jvm, frame, objectmanager_class_object_get_method(frame, ecobject, "<init>", "()V"), 1, init_args);
            if(errs == JVM_OK){
                errs = jvm_throw(frame, exception);
            }
            jvm_unlock(frame->jvm);
        }
        (errs);
    }), exit);
    
    objectmanager_object_t* elementData = JVM_TO_C_VALUE(element_data_field->value, objectmanager_object_t*);
    objectmanager_array_object_t* vector_array = objectmanager_get_array_object_info(elementData);
    
    jvm_native_return(frame, vector_array->elements[index]);
    
exit:
    return err;
}

static jvm_error_t vector_removeElementAt(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;
    
    objectmanager_object_t* self = JVM_TO_C_VALUE(frame->locals[0], objectmanager_object_t*);
    uint32_t index = JVM_TO_C_VALUE(frame->locals[1], uint32_t);
    
    classlinker_field_t* element_count_field = objectmanager_class_object_get_field(frame, objectmanager_get_class_object_info(self), "elementCount");
    classlinker_field_t* element_data_field = objectmanager_class_object_get_field(frame, objectmanager_get_class_object_info(self), "elementData");
    
    uint32_t elementCount = JVM_TO_C_VALUE(element_count_field->value, uint32_t);
    
    FAIL_SET_JUMP(index < elementCount, err, ({
        jvm_error_t errs = JVM_OK;
        jvm_lock(frame->jvm);
        objectmanager_object_t* exception = objectmanager_new_class_object(frame, classlinker_find_class(frame->jvm->linker, "java/lang/ArrayIndexOutOfBoundsException"));
        objectmanager_class_object_t* ecobject = objectmanager_get_class_object_info(exception);
        if(!exception){
            errs = JVM_OOM;
            jvm_unlock(frame->jvm);
        } else {
            jvm_value_t init_args[1] = {0};
            C_TO_JVM_VALUE(init_args[0], exception);
            errs = jvm_invoke(frame->jvm, frame, objectmanager_class_object_get_method(frame, ecobject, "<init>", "()V"), 1, init_args);
            if(errs == JVM_OK){
                errs = jvm_throw(frame, exception);
            }
            jvm_unlock(frame->jvm);
        }
        (errs);
    }), exit);
    
    objectmanager_object_t* elementData = JVM_TO_C_VALUE(element_data_field->value, objectmanager_object_t*);
    objectmanager_array_object_t* vector_array = objectmanager_get_array_object_info(elementData);
    
    // Shift elements left
    for(uint32_t i = index; i < elementCount - 1; i++){
        vector_array->elements[i] = vector_array->elements[i + 1];
    }
    
    // Clear last element
    vector_array->elements[elementCount - 1].type = EJVT_REFERENCE;
    *(void**)vector_array->elements[elementCount - 1].value = NULL;
    
    elementCount--;
    C_TO_JVM_VALUE(element_count_field->value, elementCount);
    
exit:
    return err;
}

static jvm_error_t vector_insertElementAt(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;
    
    objectmanager_object_t* self = JVM_TO_C_VALUE(frame->locals[0], objectmanager_object_t*);
    objectmanager_object_t* obj = JVM_TO_C_VALUE(frame->locals[1], objectmanager_object_t*);
    uint32_t index = JVM_TO_C_VALUE(frame->locals[2], uint32_t);
    
    classlinker_field_t* element_count_field = objectmanager_class_object_get_field(frame, objectmanager_get_class_object_info(self), "elementCount");
    classlinker_field_t* element_data_field = objectmanager_class_object_get_field(frame, objectmanager_get_class_object_info(self), "elementData");
    
    uint32_t elementCount = JVM_TO_C_VALUE(element_count_field->value, uint32_t);
    
    FAIL_SET_JUMP(index <= elementCount, err, ({
        jvm_error_t errs = JVM_OK;
        jvm_lock(frame->jvm);
        objectmanager_object_t* exception = objectmanager_new_class_object(frame, classlinker_find_class(frame->jvm->linker, "java/lang/ArrayIndexOutOfBoundsException"));
        objectmanager_class_object_t* ecobject = objectmanager_get_class_object_info(exception);
        if(!exception){
            errs = JVM_OOM;
            jvm_unlock(frame->jvm);
        } else {
            jvm_value_t init_args[1] = {0};
            C_TO_JVM_VALUE(init_args[0], exception);
            errs = jvm_invoke(frame->jvm, frame, objectmanager_class_object_get_method(frame, ecobject, "<init>", "()V"), 1, init_args);
            if(errs == JVM_OK){
                errs = jvm_throw(frame, exception);
            }
            jvm_unlock(frame->jvm);
        }
        (errs);
    }), exit);
    
    objectmanager_object_t* elementData = JVM_TO_C_VALUE(element_data_field->value, objectmanager_object_t*);
    
    // Resize if necessary
    if(elementCount >= objectmanager_get_array_object_info(elementData)->count){
        FAIL_SET_JUMP(vector_resize(frame, self) == JVM_OK, err, JVM_OOM, exit);
        elementData = JVM_TO_C_VALUE(element_data_field->value, objectmanager_object_t*);
    }
    
    objectmanager_array_object_t* vector_array = objectmanager_get_array_object_info(elementData);
    
    // Shift elements right
    for(uint32_t i = elementCount; i > index; i--){
        vector_array->elements[i] = vector_array->elements[i - 1];
    }
    
    // Insert element
    C_TO_JVM_VALUE(vector_array->elements[index], obj);
    
    elementCount++;
    C_TO_JVM_VALUE(element_count_field->value, elementCount);
    
exit:
    return err;
}

static jvm_error_t vector_ensureCapacity(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;
    
    objectmanager_object_t* self = JVM_TO_C_VALUE(frame->locals[0], objectmanager_object_t*);
    uint32_t minCapacity = JVM_TO_C_VALUE(frame->locals[1], uint32_t);
    
    classlinker_field_t* element_data_field = objectmanager_class_object_get_field(frame, objectmanager_get_class_object_info(self), "elementData");
    objectmanager_object_t* elementData = JVM_TO_C_VALUE(element_data_field->value, objectmanager_object_t*);
    objectmanager_array_object_t* vector_array = objectmanager_get_array_object_info(elementData);
    
    if(vector_array->count < minCapacity){
        err = vector_resize(frame, self);
        if(err == JVM_OK){
            elementData = JVM_TO_C_VALUE(element_data_field->value, objectmanager_object_t*);
            vector_array = objectmanager_get_array_object_info(elementData);
            while(vector_array->count < minCapacity){
                err = vector_resize(frame, self);
                if(err != JVM_OK) break;
                elementData = JVM_TO_C_VALUE(element_data_field->value, objectmanager_object_t*);
                vector_array = objectmanager_get_array_object_info(elementData);
            }
        }
    }
    
    return err;
}

static jvm_error_t vector_removeElement(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;
    
    objectmanager_object_t* self = JVM_TO_C_VALUE(frame->locals[0], objectmanager_object_t*);
    objectmanager_object_t* obj = JVM_TO_C_VALUE(frame->locals[1], objectmanager_object_t*);
    
    classlinker_field_t* element_count_field = objectmanager_class_object_get_field(frame, objectmanager_get_class_object_info(self), "elementCount");
    classlinker_field_t* element_data_field = objectmanager_class_object_get_field(frame, objectmanager_get_class_object_info(self), "elementData");
    
    uint32_t elementCount = JVM_TO_C_VALUE(element_count_field->value, uint32_t);
    objectmanager_object_t* elementData = JVM_TO_C_VALUE(element_data_field->value, objectmanager_object_t*);
    objectmanager_array_object_t* vector_array = objectmanager_get_array_object_info(elementData);
    
    bool found = false;
    for(uint32_t i = 0; i < elementCount; i++){
        if(!found && vector_array->elements[i].type == EJVT_REFERENCE && JVM_TO_C_VALUE(vector_array->elements[i], objectmanager_object_t*) == obj){
            found = true;
        }
        if(found && i < elementCount - 1){
            vector_array->elements[i] = vector_array->elements[i + 1];
        }
    }
    
    if(found){
        vector_array->elements[elementCount - 1].type = EJVT_REFERENCE;
        *(void**)vector_array->elements[elementCount - 1].value = NULL;
        elementCount--;
        C_TO_JVM_VALUE(element_count_field->value, elementCount);
        
        jvm_value_t retval = {0};
        C_TO_JVM_VALUE(retval, true);
        jvm_native_return(frame, retval);
    } else {
        jvm_value_t retval = {0};
        C_TO_JVM_VALUE(retval, false);
        jvm_native_return(frame, retval);
    }
    
    return JVM_OK;
}

static jvm_error_t vector_removeAllElements(jvm_frame_t* frame){
    objectmanager_object_t* self = JVM_TO_C_VALUE(frame->locals[0], objectmanager_object_t*);
    
    classlinker_field_t* element_count_field = objectmanager_class_object_get_field(frame, objectmanager_get_class_object_info(self), "elementCount");
    classlinker_field_t* element_data_field = objectmanager_class_object_get_field(frame, objectmanager_get_class_object_info(self), "elementData");
    
    objectmanager_object_t* elementData = JVM_TO_C_VALUE(element_data_field->value, objectmanager_object_t*);
    objectmanager_array_object_t* vector_array = objectmanager_get_array_object_info(elementData);
    uint32_t elementCount = JVM_TO_C_VALUE(element_count_field->value, uint32_t);
    
    // Clear all elements
    for(uint32_t i = 0; i < elementCount; i++){
        vector_array->elements[i].type = EJVT_REFERENCE;
        *(void**)vector_array->elements[i].value = NULL;
    }
    
    C_TO_JVM_VALUE(element_count_field->value, (uint32_t)0);
    
    return JVM_OK;
}

static jvm_error_t vector_firstElement(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;
    
    objectmanager_object_t* self = JVM_TO_C_VALUE(frame->locals[0], objectmanager_object_t*);
    
    classlinker_field_t* element_count_field = objectmanager_class_object_get_field(frame, objectmanager_get_class_object_info(self), "elementCount");
    classlinker_field_t* element_data_field = objectmanager_class_object_get_field(frame, objectmanager_get_class_object_info(self), "elementData");
    
    uint32_t elementCount = JVM_TO_C_VALUE(element_count_field->value, uint32_t);
    
    FAIL_SET_JUMP(elementCount > 0, err, ({
        jvm_error_t errs = JVM_OK;
        jvm_lock(frame->jvm);
        objectmanager_object_t* exception = objectmanager_new_class_object(frame, classlinker_find_class(frame->jvm->linker, "java/util/NoSuchElementException"));
        objectmanager_class_object_t* ecobject = objectmanager_get_class_object_info(exception);
        if(!exception){
            errs = JVM_OOM;
            jvm_unlock(frame->jvm);
        } else {
            jvm_value_t init_args[1] = {0};
            C_TO_JVM_VALUE(init_args[0], exception);
            errs = jvm_invoke(frame->jvm, frame, objectmanager_class_object_get_method(frame, ecobject, "<init>", "()V"), 1, init_args);
            if(errs == JVM_OK){
                errs = jvm_throw(frame, exception);
            }
            jvm_unlock(frame->jvm);
        }
        (errs);
    }), exit);
    
    objectmanager_object_t* elementData = JVM_TO_C_VALUE(element_data_field->value, objectmanager_object_t*);
    objectmanager_array_object_t* vector_array = objectmanager_get_array_object_info(elementData);
    
    jvm_native_return(frame, vector_array->elements[0]);
    
exit:
    return err;
}

static jvm_error_t vector_lastElement(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;
    
    objectmanager_object_t* self = JVM_TO_C_VALUE(frame->locals[0], objectmanager_object_t*);
    
    classlinker_field_t* element_count_field = objectmanager_class_object_get_field(frame, objectmanager_get_class_object_info(self), "elementCount");
    classlinker_field_t* element_data_field = objectmanager_class_object_get_field(frame, objectmanager_get_class_object_info(self), "elementData");
    
    uint32_t elementCount = JVM_TO_C_VALUE(element_count_field->value, uint32_t);
    
    FAIL_SET_JUMP(elementCount > 0, err, ({
        jvm_error_t errs = JVM_OK;
        jvm_lock(frame->jvm);
        objectmanager_object_t* exception = objectmanager_new_class_object(frame, classlinker_find_class(frame->jvm->linker, "java/util/NoSuchElementException"));
        objectmanager_class_object_t* ecobject = objectmanager_get_class_object_info(exception);
        if(!exception){
            errs = JVM_OOM;
            jvm_unlock(frame->jvm);
        } else {
            jvm_value_t init_args[1] = {0};
            C_TO_JVM_VALUE(init_args[0], exception);
            errs = jvm_invoke(frame->jvm, frame, objectmanager_class_object_get_method(frame, ecobject, "<init>", "()V"), 1, init_args);
            if(errs == JVM_OK){
                errs = jvm_throw(frame, exception);
            }
            jvm_unlock(frame->jvm);
        }
        (errs);
    }), exit);
    
    objectmanager_object_t* elementData = JVM_TO_C_VALUE(element_data_field->value, objectmanager_object_t*);
    objectmanager_array_object_t* vector_array = objectmanager_get_array_object_info(elementData);
    
    jvm_native_return(frame, vector_array->elements[elementCount - 1]);
    
exit:
    return err;
}

static jvm_error_t vector_indexOf(jvm_frame_t* frame){
    objectmanager_object_t* self = JVM_TO_C_VALUE(frame->locals[0], objectmanager_object_t*);
    objectmanager_object_t* obj = JVM_TO_C_VALUE(frame->locals[1], objectmanager_object_t*);
    
    classlinker_field_t* element_count_field = objectmanager_class_object_get_field(frame, objectmanager_get_class_object_info(self), "elementCount");
    classlinker_field_t* element_data_field = objectmanager_class_object_get_field(frame, objectmanager_get_class_object_info(self), "elementData");
    
    uint32_t elementCount = JVM_TO_C_VALUE(element_count_field->value, uint32_t);
    objectmanager_object_t* elementData = JVM_TO_C_VALUE(element_data_field->value, objectmanager_object_t*);
    objectmanager_array_object_t* vector_array = objectmanager_get_array_object_info(elementData);
    
    int32_t index = -1;
    for(uint32_t i = 0; i < elementCount; i++){
        if(vector_array->elements[i].type == EJVT_REFERENCE && JVM_TO_C_VALUE(vector_array->elements[i], objectmanager_object_t*) == obj){
            index = (int32_t)i;
            break;
        }
    }
    
    jvm_value_t retval = {0};
    C_TO_JVM_VALUE(retval, (int32_t)index);
    jvm_native_return(frame, retval);
    
    return JVM_OK;
}

static jvm_error_t vector_lastIndexOf(jvm_frame_t* frame){
    objectmanager_object_t* self = JVM_TO_C_VALUE(frame->locals[0], objectmanager_object_t*);
    objectmanager_object_t* obj = JVM_TO_C_VALUE(frame->locals[1], objectmanager_object_t*);
    
    classlinker_field_t* element_count_field = objectmanager_class_object_get_field(frame, objectmanager_get_class_object_info(self), "elementCount");
    classlinker_field_t* element_data_field = objectmanager_class_object_get_field(frame, objectmanager_get_class_object_info(self), "elementData");
    
    uint32_t elementCount = JVM_TO_C_VALUE(element_count_field->value, uint32_t);
    objectmanager_object_t* elementData = JVM_TO_C_VALUE(element_data_field->value, objectmanager_object_t*);
    objectmanager_array_object_t* vector_array = objectmanager_get_array_object_info(elementData);
    
    int32_t index = -1;
    for(int32_t i = (int32_t)elementCount - 1; i >= 0; i--){
        if(vector_array->elements[i].type == EJVT_REFERENCE && JVM_TO_C_VALUE(vector_array->elements[i], objectmanager_object_t*) == obj){
            index = i;
            break;
        }
    }
    
    jvm_value_t retval = {0};
    C_TO_JVM_VALUE(retval, index);
    jvm_native_return(frame, retval);
    
    return JVM_OK;
}

// Made by me
extern classlinker_class_t java_util_Enumeration;

static jvm_error_t VENUMER_hasMoreElements(jvm_frame_t* frame){
    objectmanager_object_t* self = JVM_TO_C_VALUE(frame->locals[0],objectmanager_object_t*);
    objectmanager_class_object_t* cself = objectmanager_get_class_object_info(self);

    objectmanager_object_t* vector = JVM_TO_C_VALUE(objectmanager_class_object_get_field(frame, cself, "vector")->value,objectmanager_object_t*);
    objectmanager_class_object_t* cvector = objectmanager_get_class_object_info(vector);

    uint32_t index = JVM_TO_C_VALUE(objectmanager_class_object_get_field(frame, cself, "index")->value, uint32_t);
    uint32_t vector_sz = JVM_TO_C_VALUE(objectmanager_class_object_get_field(frame,cvector, "elementCount")->value,uint32_t);

    jvm_value_t returnvalue = {0};
    C_TO_JVM_VALUE(returnvalue,((bool)(index < vector_sz)));

    return JVM_OK;
}

static jvm_error_t VENUMER_nextElement(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;

    objectmanager_object_t* self = JVM_TO_C_VALUE(frame->locals[0],objectmanager_object_t*);
    objectmanager_class_object_t* cself = objectmanager_get_class_object_info(self);

    objectmanager_object_t* vector = JVM_TO_C_VALUE(objectmanager_class_object_get_field(frame, cself, "vector")->value,objectmanager_object_t*);
    objectmanager_class_object_t* cvector = objectmanager_get_class_object_info(vector);

    objectmanager_object_t* elements = JVM_TO_C_VALUE(objectmanager_class_object_get_field(frame, cvector, "elementData")->value,objectmanager_object_t*);
    objectmanager_array_object_t* vector_array = objectmanager_get_array_object_info(elements);

    uint32_t index = JVM_TO_C_VALUE(objectmanager_class_object_get_field(frame, cself, "index")->value, uint32_t);
    uint32_t vector_sz = JVM_TO_C_VALUE(objectmanager_class_object_get_field(frame,cvector, "elementCount")->value,uint32_t);

    FAIL_SET_JUMP(index < vector_sz, err, ({
        jvm_error_t errs = JVM_OK;
        jvm_lock(frame->jvm);
        objectmanager_object_t* exception = objectmanager_new_class_object(frame, classlinker_find_class(frame->jvm->linker, "java/util/NoSuchElementException"));
        objectmanager_class_object_t* ecobject = objectmanager_get_class_object_info(exception);
        if(!exception){
            errs = JVM_OOM;
            jvm_unlock(frame->jvm);
        } else {
            jvm_value_t init_args[1] = {0};
            C_TO_JVM_VALUE(init_args[0], exception);
            errs = jvm_invoke(frame->jvm, frame, objectmanager_class_object_get_method(frame, ecobject, "<init>", "()V"), 1, init_args);
            if(errs == JVM_OK){
                errs = jvm_throw(frame, exception);
            }
            jvm_unlock(frame->jvm);
        }
        (errs);
    }), exit);
    
    jvm_native_return(frame,vector_array->elements[index++]);
    C_TO_JVM_VALUE(objectmanager_class_object_get_field(frame, cself, "index")->value,index);

exit:
    return err;
}

static jvm_error_t VENUMER_init(jvm_frame_t* frame){
    objectmanager_object_t* self = JVM_TO_C_VALUE(frame->locals[0],objectmanager_object_t*);

    objectmanager_class_object_t* cself = objectmanager_get_class_object_info(self);
    jvm_value_t* vector_fv = &objectmanager_class_object_get_field(frame, cself, "vector")->value;
    jvm_value_t* index_fv = &objectmanager_class_object_get_field(frame, cself, "index")->value;

    *vector_fv = frame->locals[1];
    C_TO_JVM_VALUE(*index_fv, ((uint32_t)0));

    return JVM_OK;
}

static jvm_error_t vector_elements(jvm_frame_t* frame){
    jvm_error_t err = JVM_OK;
    
    objectmanager_object_t* self = JVM_TO_C_VALUE(frame->locals[0],objectmanager_object_t*);
    C_TO_JVM_VALUE(frame->locals[1],objectmanager_new_class_object(frame,classlinker_find_class(frame->jvm->linker,"ljvm/vector/enumerator")));

    objectmanager_object_t* enumerator = JVM_TO_C_VALUE(frame->locals[1],objectmanager_object_t*);
    
    FAIL_SET_JUMP(enumerator,err,JVM_OOM,exit);

    classlinker_method_t* init_method = objectmanager_class_object_get_method(frame,objectmanager_get_class_object_info(enumerator),"<init>", NULL);

    jvm_value_t args[2] = {0};
    C_TO_JVM_VALUE(args[0],enumerator);
    C_TO_JVM_VALUE(args[1],self);

    err = jvm_invoke(frame->jvm,frame,init_method,2,args);

    jvm_value_t returnvalue = {0};
    C_TO_JVM_VALUE(returnvalue,enumerator);

    jvm_native_return(frame,returnvalue);

exit:
    return err;
}

//=========================================

classlinker_normalclass_t vector_enumerator_info = {
    .fields_count = 2,
    .fields = (classlinker_field_t[]){
        {
            .name = "vector",
        },
        {
            .name = "index",
            .flags = ACC_PRIVATE,
        }
    },

    .methods_count = 3,
    .methods = (classlinker_method_t[]){
        {
            .name = "hasMoreElements",
            .flags = ACC_NATIVE,
            .raw_description = "()Z",
            .fn = VENUMER_hasMoreElements,
        },
        {
            .name = "nextElement",
            .flags = ACC_NATIVE,
            .raw_description = "()Ljava/lang/Object;",
            .fn = VENUMER_nextElement,
        },
        {
            .name = "<init>",
            .flags = ACC_NATIVE,
            .frame_descriptor.arguments_count = 1,
            .fn = VENUMER_init,
        }
    }
};

classlinker_class_t vector_enumerator = {
    .this_name = "ljvm/vector/enumerator",
    .parent = &java_lang_Object,
    .generation = 1,
    .implements_count = 1,
    .implements = (classlinker_class_t*[]){&java_util_Enumeration},
    .info = &vector_enumerator_info,
};

classlinker_normalclass_t Vector_info = {
    .fields_count = 3,
    .fields = (classlinker_field_t[]){
        {
            .name = "capacityIncrement",
        },
        {
            .name = "elementCount",
        },
        {
            .name = "elementData",
        },
    },

    .methods_count = 17,
    .methods = (classlinker_method_t[]){
        {
            .name = "<init>",
            .raw_description = "()V",
            .flags = ACC_NATIVE,
            .fn = vector_init,
        },
        {
            .name = "<init>",
            .raw_description = "(I)V",
            .frame_descriptor.arguments_count = 1,
            .flags = ACC_NATIVE,
            .fn = vector_init_sz,
        },
        {
            .name = "<init>",
            .raw_description = "(II)V",
            .frame_descriptor.arguments_count = 2,
            .flags = ACC_NATIVE,
            .fn = vector_init_szci,
        },
        {
            .name = "addElement",
            .raw_description = "(Ljava/lang/Object;)V",
            .frame_descriptor.arguments_count = 1,
            .flags = ACC_NATIVE,
            .fn = vector_addElement,
        },
        {
            .name = "capacity",
            .raw_description = "()I",
            .flags = ACC_NATIVE,
            .fn = vector_capacity,
        },
        {
            .name = "size",
            .raw_description = "()I",
            .flags = ACC_NATIVE,
            .fn = vector_size,
        },
        {
            .name = "elementAt",
            .raw_description = "(I)Ljava/lang/Object;",
            .frame_descriptor.arguments_count = 1,
            .flags = ACC_NATIVE,
            .fn = vector_elementAt,
        },
        {
            .name = "removeElementAt",
            .raw_description = "(I)V",
            .frame_descriptor.arguments_count = 1,
            .flags = ACC_NATIVE,
            .fn = vector_removeElementAt,
        },
        {
            .name = "insertElementAt",
            .raw_description = "(Ljava/lang/Object;I)V",
            .frame_descriptor.arguments_count = 2,
            .flags = ACC_NATIVE,
            .fn = vector_insertElementAt,
        },
        {
            .name = "ensureCapacity",
            .raw_description = "(I)V",
            .frame_descriptor.arguments_count = 1,
            .flags = ACC_NATIVE,
            .fn = vector_ensureCapacity,
        },
        {
            .name = "removeElement",
            .raw_description = "(Ljava/lang/Object;)Z",
            .frame_descriptor.arguments_count = 1,
            .flags = ACC_NATIVE,
            .fn = vector_removeElement,
        },
        {
            .name = "removeAllElements",
            .raw_description = "()V",
            .flags = ACC_NATIVE,
            .fn = vector_removeAllElements,
        },
        {
            .name = "firstElement",
            .raw_description = "()Ljava/lang/Object;",
            .flags = ACC_NATIVE,
            .fn = vector_firstElement,
        },
        {
            .name = "lastElement",
            .raw_description = "()Ljava/lang/Object;",
            .flags = ACC_NATIVE,
            .fn = vector_lastElement,
        },
        {
            .name = "indexOf",
            .raw_description = "(Ljava/lang/Object;)I",
            .frame_descriptor.arguments_count = 1,
            .flags = ACC_NATIVE,
            .fn = vector_indexOf,
        },
        {
            .name = "lastIndexOf",
            .raw_description = "(Ljava/lang/Object;)I",
            .frame_descriptor.arguments_count = 1,
            .flags = ACC_NATIVE,
            .fn = vector_lastIndexOf,
        },
        {
            .name = "elements",
            .raw_description = "()Ljava/util/Enumeration;",
            .frame_descriptor.locals_count = 1,
            .flags = ACC_NATIVE,
            .fn = vector_elements,
        }
    }
};


classlinker_class_t java_util_Vector = {
    .this_name = "java/util/Vector",
    .parent = &java_lang_Object,
    .generation = 1,
    .info = &Vector_info,
};