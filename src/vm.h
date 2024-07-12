#ifndef iii_vm_h
#define iii_vm_h

#include <stdint.h>

#include "chunk.h"
#include "object.h"
#include "table.h"
#include "value.h"

// NOTE:
// VM wouldn't validate any bytecode
// so unvalid bytecode can lead to crash or very unxecped behaviour

#define UINT8_COUNT (UINT8_MAX + 1)

#define FRAMES_MAX 64
#define MODULE_MAX 64  // TODO: make it dynamic
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

// FIXME: Stack overflow handling only for frames

typedef struct {
  ObjClosure *closure;
  uint8_t *ip;
  Value *slots;
  uint32_t globalOwner;
  Table *globals;
} CallFrame;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

typedef struct Module {
  Table globals;                 // globals
  CallFrame frames[FRAMES_MAX];  // frames
  int frameCount;                // count of frames
  ObjUpvalue *openUpvalues;      // all open upvalues
  bool isDefault;                // if module default
  char *name;                    // name of module
  struct Module *origin;         // from where module was imported
} Module;

Module *getModByHash(uint32_t hash);
Module *getCurrMod();

typedef struct {
  Value stack[STACK_MAX];  // stack
  Value *stackTop;         // pointer to stack top

  Module modules[MODULE_MAX];     // all already imported modules
  int modCount;                   // module count
  uint32_t modNames[MODULE_MAX];  // hashed names of modules (faster than
                                  // comparing strings)
  Module *currMod;                // current mod

  Table strings;  // table of strings (for optimization)

  ObjString *initString;  // init method name

  // variables to know when call GC
  size_t bytesAllocated;
  size_t nextGC;

  Obj *objects;  // objects

  // for GC
  int grayCount;
  int grayCapacity;
  Obj **grayStack;
} VM;

extern VM vm;

void initVM();
void freeVM();

InterpretResult interpret(const char *source);

void push(Value value);
Value pop();

#endif
