#include "object.h"

#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType) \
  (type *)allocateObject(sizeof(type), objectType)

static Obj *allocateObject(size_t size, ObjType type) {
  Obj *obj = (Obj *)reallocate(NULL, 0, size);
  obj->type = type;
  obj->isMarked = false;
  obj->next = vm.objects;
  vm.objects = obj;

#ifdef DEBUG_LOG_GC
  printf("%p allocate %ld for %d\n", (void *)obj, size, type);
#endif /* ifdef DEBUG_LOG_GC */

  return obj;
}

// FNV-1a hash
static uint32_t hashString(const char *key, int length) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= key[i];
    hash *= 16777619;
  }

  return hash;
}

static ObjString *allocateString(char *chars, int length, uint32_t hash) {
  ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
  string->length = length;
  string->chars = chars;
  string->hash = hash;

  push(OBJ_VAL(string));

  tableSet(&vm.strings, string, NIL_VAL);

  pop();

  return string;
}

ObjString *takeString(char *chars, int length) {
  uint32_t hash = hashString(chars, length);

  ObjString *interned = tableFindString(&vm.strings, chars, length, hash);

  if (interned != NULL) {
    FREE_ARRAY(char, chars, length + 1);
    return interned;
  }

  return allocateString(chars, length, hash);
}

ObjString *copyString(const char *chars, int length) {
  uint32_t hash = hashString(chars, length);

  ObjString *interned = tableFindString(&vm.strings, chars, length, hash);

  if (interned != NULL) return interned;

  char *heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';
  return allocateString(heapChars, length, hash);
}

static void printFunc(ObjFunc *func) {
  if (func->name == NULL) {
    printf("<script>");
    return;
  }
  printf("<fn %s>", func->name->chars);
}

void printObject(Value value) {
  switch (AS_OBJ(value)->type) {
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
    case OBJ_CLASS:
      printf("<class %s>", AS_CLASS(value)->name->chars);
      break;
    case OBJ_INSTANCE:
      printf("<%s instance>", AS_INSTANCE(value)->cclass->name->chars);
      break;
    case OBJ_BOUND_METHOD:
      printFunc(AS_BOUND_METHOD(value)->method->function);
      break;
    default:
      printf("Unknown object type\n");
  }
}

ObjFunc *newFunction() {
  ObjFunc *func = ALLOCATE_OBJ(ObjFunc, OBJ_FUNCTION);
  func->arity = 0;
  func->name = NULL;
  func->upvalueCount = 0;
  initChunk(&func->chunk);
  return func;
}

ObjNative *newNative(NativeFn function) {
  ObjNative *native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
  native->function = function;
  return native;
}

ObjClosure *newClosure(ObjFunc *function) {
  ObjUpvalue **upvalues = ALLOCATE(ObjUpvalue *, function->upvalueCount);

  for (int i = 0; i < function->upvalueCount; i++) {
    upvalues[i] = NULL;
  }

  ObjClosure *closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
  closure->function = function;
  closure->upvalues = upvalues;
  closure->upvalueCount = function->upvalueCount;
  return closure;
}

ObjUpvalue *newUpvalue(Value *slot) {
  ObjUpvalue *upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
  upvalue->location = slot;
  upvalue->next = NULL;
  upvalue->closed = NIL_VAL;
  return upvalue;
}

ObjClass *newClass(ObjString *name) {
  ObjClass *cclass = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
  cclass->name = name;
  initTable(&cclass->methods);
  return cclass;
}

ObjInstance *newInstance(ObjClass *cclass) {
  ObjInstance *instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
  instance->cclass = cclass;
  initTable(&instance->fields);
  return instance;
}

ObjBoundMethod *newBoundMethod(Value receiver, ObjClosure *method) {
  ObjBoundMethod *bound = ALLOCATE_OBJ(ObjBoundMethod, OBJ_BOUND_METHOD);

  bound->receiver = receiver;
  bound->method = method;
  return bound;
}
