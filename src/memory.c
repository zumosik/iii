#include <stdlib.h>

#include "memory.h"
#include "vm.h"


void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    void* res =  realloc(pointer, newSize);
    if (res == NULL) exit(1);

    return res;
}

static void freeObj(Obj* obj) {
    switch (obj->type)
    {
    case OBJ_STRING:
        ObjString* string = (ObjString*)obj;
        FREE_ARRAY(char, string->chars, string->length + 1);
        FREE(ObjString, obj);
        break;
    
    default:
        break;
    }
} 

void freeObjects() {
    // CS 101 textbook implementation of walking a linked list and freeing its nodes
    Obj* object = vm.objects;
    while (object != NULL) {
        Obj* next = object->next;
        freeObj(object);
        object = next;
    }
}