#include <stdlib.h>

#include "memory.h"
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

void collectGarbage() {
  #ifdef DEBUG_LOG_GC
    printf(" -- GC begin\n");
  #endif /* ifdef DEBUG_LOG_GC */  
 
  // ... 

  #ifdef DEBUG_LOG_GC
    printf(" -- GC end\n");
  #endif /* ifdef DEBUG_LOG_GC */  
}
