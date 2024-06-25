#ifndef iii_object_h
#define iii_object_h

#include "value.h"
#include "chunk.h"

#define IS_STRING(value) isObjType(value, OBJ_STRING)
#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)
#define IS_CLOSURE(value) isObjType(value, OBJ_CLOSURE)

#define AS_STRING(value) ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString *)AS_OBJ(value))->chars)
#define AS_FUNCTION(value) ((ObjFunc *)AS_OBJ(value))
#define AS_NATIVE(value) (((ObjNative *)AS_OBJ(value))->function)
#define AS_CLOSURE(value) ((ObjClosure *)AS_OBJ(value))

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

typedef enum
{
    OBJ_STRING,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_CLOSURE,
    OBJ_UPVALUE,
} ObjType;

struct Obj
{
    ObjType type;
    struct Obj *next;
    bool isMarked;
};

static inline bool isObjType(Value value, ObjType type)
{
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

void printObject(Value value);

typedef struct
{
    Obj obj;
    int arity;
    uint16_t upvalueCount;
    Chunk chunk;
    ObjString *name;
} ObjFunc;

ObjFunc *newFunction();

typedef Value (*NativeFn)(int argCount, Value *args);

typedef struct
{
    Obj obj;
    NativeFn function;
} ObjNative;

ObjNative *newNative(NativeFn function);

struct ObjString
{
    Obj obj;
    int length;
    char *chars;
    uint32_t hash;
};

ObjString *takeString(char *chars, int length);
ObjString *copyString(const char *chars, int length);

typedef struct ObjUpvalue
{
    Obj obj;
    Value *location;
    Value closed;
    struct ObjUpvalue* next;
} ObjUpvalue;

ObjUpvalue *newUpvalue(Value *slot);

typedef struct
{
    Obj obj;
    ObjFunc *function;
    ObjUpvalue** upvalues;
    int upvalueCount;
} ObjClosure;

ObjClosure *newClosure(ObjFunc *function);

#endif // iii_object_h
