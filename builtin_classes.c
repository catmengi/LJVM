#include "builtin_classes.h"
#include "jvm.h"

extern classlinker_class_t java_lang_Object;
extern classlinker_class_t java_lang_Clonable;
extern classlinker_class_t java_lang_String;
extern classlinker_class_t java_io_OutputStream;
extern classlinker_class_t java_lang_Throwable;
extern classlinker_class_t java_io_PrintStream;
extern classlinker_class_t java_lang_Exception;
extern classlinker_class_t java_lang_RuntimeException;
extern classlinker_class_t java_lang_Class;
extern classlinker_class_t java_io_IOException;
extern classlinker_class_t java_lang_System;
extern classlinker_class_t java_lang_ClassCastException;
extern classlinker_class_t java_lang_NullPointerException;
extern classlinker_class_t java_lang_ArrayIndexOutOfBoundsException;
extern classlinker_class_t java_lang_NegativeArraySizeException;
extern classlinker_class_t java_util_Vector;
extern classlinker_class_t java_util_NoSuchElementException;
extern classlinker_class_t java_util_Enumeration;
extern classlinker_class_t vector_enumerator;
extern classlinker_class_t java_lang_IndexOutOfBoundsException;
extern classlinker_class_t java_lang_StringIndexOutOfBoundsException;

classlinker_class_t* builtin_classes[] = {
    &java_lang_Object,
    &java_lang_Clonable,
    &java_lang_Throwable,
    &java_lang_Exception,
    &java_lang_RuntimeException,
    &java_lang_ClassCastException,
    &java_lang_NullPointerException,
    &java_lang_ArrayIndexOutOfBoundsException,
    &java_lang_NegativeArraySizeException,
    &java_lang_IndexOutOfBoundsException,
    &java_lang_StringIndexOutOfBoundsException, 
    &vector_enumerator,
    &java_util_Vector,
    &java_util_Enumeration,
    &java_util_NoSuchElementException,
    &java_lang_Class,
    &java_lang_String,
    &java_io_IOException,
    &java_io_OutputStream,
    &java_io_PrintStream,
    &java_lang_System,
};

void builtin_classes_init(classlinker_instance_t* linker){
    for(unsigned i = 0; i < sizeof(builtin_classes) / sizeof(builtin_classes[0]); i++){
        classlinker_class_t* class = builtin_classes[i];

        classlinker_class_t* copy = arena_calloc(linker->arena,1,sizeof(*copy));
        assert(copy);

        *copy = *class;
        INIT_LIST_HEAD(&copy->list);
        list_add_tail(&copy->list,&linker->loaded_classes);
    }
}
