// #include <cstddef>
#include <stdlib.h>
#include "compiler.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif /* ifdef DEBUG_LOG_GC */

void *reallocate(void *pointer, size_t oldSize, size_t newSize)
{
    #ifdef DEBUG_STRESS_GC
      if (newSize > oldSize) {
          collectGarbage();
      }
    #endif /* ifdef DEBUG_STRESS_GC */

    if (newSize == 0)
    {
        free(pointer);
        return NULL;
    }

    void *res = realloc(pointer, newSize);
    if (res == NULL)
        exit(1);

    return res;
}

void markObject(Obj *obj) {
    if (obj == NULL) return;
    
    #ifdef DEBUG_LOG_GC
        printf("%p mark ", (void*)obj);
        printValue(OBJ_VAL(obj));
        printf("\n"); 
    #endif /* ifdef DEBUG_LOG_GC */

    obj->isMarked = true;
}

void markValue(Value value) {
    if (!IS_OBJ(value)) return;
    markObject(AS_OBJ(value));
}

static void freeObj(Obj *obj)
{
    #ifdef DEBUG_LOG_GC
        printf("%p free type %d\n", (void*)obj, obj->type);
    #endif /* ifdef DEBUG_LOG_GC */

    switch (obj->type)
    {
    case OBJ_STRING:
        ObjString *string = (ObjString *)obj;
        FREE_ARRAY(char, string->chars, string->length + 1);
        FREE(ObjString, obj);
        break;
    case OBJ_FUNCTION:
    {
        ObjFunc *func = (ObjFunc *)obj;
        freeChunk(&func->chunk);
        FREE(ObjFunc, obj);
        break;
    }
    case OBJ_NATIVE:
        FREE(ObjNative, obj);
        break;
    case OBJ_CLOSURE:
    {
        ObjClosure *closure = (ObjClosure *)obj;
        FREE_ARRAY(ObjUpvalue *, closure->upvalues,
                   closure->upvalueCount);
        break;
    }
    case OBJ_UPVALUE:
        FREE(ObjUpvalue, obj);
        break;
    default:
        break;
    }
}

void freeObjects()
{
    // CS 101 textbook implementation of walking a linked list and freeing its nodes
    Obj *object = vm.objects;
    while (object != NULL)
    {
        Obj *next = object->next;
        freeObj(object);
        object = next;
    }
}

static void markRoots() {
    // stack
    for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
        markValue(*slot);
    }

    // closures
    for (int i = 0; i < vm.frameCount; i++) {
      markObject((Obj*)vm.frames[i].closure);
    }

    // open upvalues 
    for (ObjUpvalue* upvalue = vm.openUpvalues; upvalue != NULL; upvalue = upvalue->next) {
        markObject((Obj*)upvalue); 
    }
    
    // table of globals
    markTable(&vm.globals);

    // mark compiler roots 
    markCompilerRoots(); 
}

void collectGarbage() {
  #ifdef DEBUG_LOG_GC
    printf(" -- GC begin\n");
  #endif /* ifdef DEBUG_LOG_GC */  
 
  markRoots(); 

  #ifdef DEBUG_LOG_GC
    printf(" -- GC end\n");
  #endif /* ifdef DEBUG_LOG_GC */  
}
