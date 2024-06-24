#include <string.h>
#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"
#include "table.h"
#include <stdio.h>

#define ALLOCATE_OBJ(type, objectType) \
    (type *)allocateObject(sizeof(type), objectType)

static Obj *allocateObject(size_t size, ObjType type)
{
    Obj *object = (Obj *)reallocate(NULL, 0, size);
    object->type = type;

    object->next = vm.objects;
    vm.objects = object;

    return object;
}

// FNV-1a hash
static uint32_t hashString(const char *key, int length)
{
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++)
    {
        hash ^= key[i];
        hash *= 16777619;
    }

    return hash;
}

static ObjString *allocateString(char *chars, int length, uint32_t hash)
{

    ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    tableSet(&vm.strings, string, NIL_VAL);

    return string;
}

ObjString *takeString(char *chars, int length)
{
    uint32_t hash = hashString(chars, length);

    ObjString *interned = tableFindString(&vm.strings, chars, length, hash);

    if (interned != NULL)
    {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return allocateString(chars, length, hash);
}

ObjString *copyString(const char *chars, int length)
{
    uint32_t hash = hashString(chars, length);

    ObjString *interned = tableFindString(&vm.strings, chars, length, hash);

    if (interned != NULL)
        return interned;

    char *heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(heapChars, length, hash);
}

ObjFunc *newFunction()
{
    ObjFunc *func = ALLOCATE_OBJ(ObjFunc, OBJ_FUNCTION);
    func->arity = 0;
    func->name = NULL;
    func->upvalueCount = 0;
    initChunk(&func->chunk);
    return func;
}

static void printFunc(ObjFunc *func)
{
    if (func->name == NULL)
    {
        printf("<script>");
        return;
    }
    printf("<fn %s>", func->name->chars);
}

void printObject(Value value)
{
    switch (AS_OBJ(value)->type)
    {
    case OBJ_STRING:
        printf("%s", AS_CSTRING(value));
        break;
    case OBJ_FUNCTION:
        printFunc(AS_FUNCTION(value));
        break;
    case OBJ_NATIVE:
        printf("<native fn>");
        break;
    case OBJ_CLOSURE:
        printFunc(AS_CLOSURE(value)->function);
        break;
    case OBJ_UPVALUE:
        printf("upvalue");
        break;
    default:
        printf("Unknown object type\n");
    }
}

ObjNative *newNative(NativeFn function)
{
    ObjNative *native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    native->function = function;
    return native;
}

ObjClosure *newClosure(ObjFunc *function)
{
    ObjUpvalue** upvalues  = ALLOCATE(ObjUpvalue*, function->upvalueCount);

    for (int i = 0; i < function->upvalueCount; i++)
    {
        upvalues[i] = NULL;
    }

    ObjClosure *closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;
    return closure;
}

ObjUpvalue *newUpvalue(Value *slot)
{
    ObjUpvalue *upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
    upvalue->location = slot;
    upvalue->next = NULL;
    upvalue->closed = NIL_VAL;
    return upvalue;
}
