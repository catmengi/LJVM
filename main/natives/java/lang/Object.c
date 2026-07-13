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

#include "../../../native_methods_service.h"
#include "../../../monitor.h"
#include "../../../heap.h"

#include <assert.h>

static NativeMethodReturnValue_t wait(Interpreter_t* ctx, Method_t* self, int32_t* args){
    assert(ctx->thread && "Cannot be run from bootstrap context");
    return (NativeMethodReturnValue_t){monitor_wait((Object_t*)args[0]), {0}};
}
static NativeMethodReturnValue_t waitMillis(Interpreter_t* ctx, Method_t* self, int32_t* args){
    assert(ctx->thread && "Cannot be run from bootstrap context");
    return (NativeMethodReturnValue_t){monitor_waitTimeout((Object_t*)args[0], (*(int64_t*)&args[1] * 1000000)), {0}};
}

static NativeMethodReturnValue_t waitMillisNanos(Interpreter_t* ctx, Method_t* self, int32_t* args){
    assert(ctx->thread && "Cannot be run from bootstrap context");
    return (NativeMethodReturnValue_t){monitor_waitTimeout((Object_t*)args[0], (*(int64_t*)&args[1] * 1000000) + args[3]), {0}};
}

static NativeMethodReturnValue_t notify(Interpreter_t* ctx, Method_t* self, int32_t* args){
    assert(ctx->thread && "Cannot be run from bootstrap context");
    return (NativeMethodReturnValue_t){monitor_notify((Object_t*)args[0]), {0}};
}

static NativeMethodReturnValue_t notifyAll(Interpreter_t* ctx, Method_t* self, int32_t* args){
    assert(ctx->thread && "Cannot be run from bootstrap context");
    return (NativeMethodReturnValue_t){monitor_notifyAll((Object_t*)args[0]), {0}};
}

static NativeMethodReturnValue_t hashCode(Interpreter_t* ctx, Method_t* self, int32_t* args){
    NativeMethodReturnValue_t retval = {0};
    retval.err = JERR_OK;
    *(int32_t*)retval.value = ((Object_t*)args[0])->ident;

    return retval;
}

static NativeMethodReturnValue_t getClass(Interpreter_t* ctx, Method_t* self, int32_t* args){
    NativeMethodReturnValue_t retval = {0};
    retval.err = JERR_OK;
    *(Object_t**)retval.value = ((Object_t*)args[0])->class->class_object;

    return retval;    
}

NativeClass_t java_lang_Object = {
    .name = "java/lang/Object",
    .methods_count = 7,
    .methods = (NativeMethodDescriptor_t[7]){
        {"wait@()V", wait},
        {"wait@(J)V", waitMillis},
        {"wait@(JI)V", waitMillisNanos},
        {"notify@()V",notify},
        {"notifyAll@()V", notifyAll},
        {"hashCode@()I", hashCode},
        {"getClass@()Ljava/lang/Class;", getClass},
    },
};