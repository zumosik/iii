#ifndef iii_object_h
#define iii_object_h

#include "chunk.h"
#include "table.h"
#include "value.h"

#define IS_STRING(value) isObjType(value, OBJ_STRING)
#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)
#define IS_CLOSURE(value) isObjType(value, OBJ_CLOSURE)
#define IS_CLASS(value) isObjType(value, OBJ_CLASS)
#define IS_INSTANCE(value) isObjType(value, OBJ_INSTANCE)

#define AS_STRING(value) ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString *)AS_OBJ(value))->chars)
#define AS_FUNCTION(value) ((ObjFunc *)AS_OBJ(value))
#define AS_NATIVE(value) (((ObjNative *)AS_OBJ(value))->function)
#define AS_CLOSURE(value) ((ObjClosure *)AS_OBJ(value))
#define AS_CLASS(value) ((ObjClass *)AS_OBJ(value))
#define AS_INSTANCE(value) ((ObjInstance *)AS_OBJ(value))

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

typedef enum {
  OBJ_STRING,
  OBJ_FUNCTION,
  OBJ_NATIVE,
  OBJ_CLASS,
  OBJ_INSTANCE,
  OBJ_CLOSURE,
  OBJ_UPVALUE,
} ObjType;

struct Obj {
  ObjType type;
  struct Obj *next;
  bool isMarked;
};

static inline bool isObjType(Value value, ObjType type) {
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

typedef struct {
  Obj obj;
  int arity;
  uint16_t upvalueCount;
  Chunk chunk;
  ObjString *name;
} ObjFunc;

typedef Value (*NativeFn)(int argCount, Value *args);

typedef struct {
  Obj obj;
  NativeFn function;
} ObjNative;

struct ObjString {
  Obj obj;
  int length;
  char *chars;
  uint32_t hash;
};
typedef struct ObjUpvalue {
  Obj obj;
  Value *location;
  Value closed;
  struct ObjUpvalue *next;
} ObjUpvalue;

typedef struct {
  Obj obj;
  ObjFunc *function;
  ObjUpvalue **upvalues;
  int upvalueCount;
} ObjClosure;

typedef struct {
  Obj obj;
  ObjString *name;
  Table methods;
} ObjClass;

typedef struct {
  Obj obj;
  ObjClass *cclass;  // cclas because class is reserved in Objective C/C++
  Table fields;
} ObjInstance;

ObjString *takeString(char *chars, int length);
ObjString *copyString(const char *chars, int length);

void printObject(Value value);

ObjFunc *newFunction();
ObjNative *newNative(NativeFn function);
ObjClosure *newClosure(ObjFunc *function);
ObjUpvalue *newUpvalue(Value *slot);
ObjClass *newClass(ObjString *name);
ObjInstance *newInstance(ObjClass *cclass);

#endif  // iii_object_h
