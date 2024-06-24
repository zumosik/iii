#ifndef iii_vm_h
#define iii_vm_h

#include "chunk.h"
#include "value.h"
#include "object.h"
#include "table.h"

#define UINT8_COUNT (UINT8_MAX + 1)

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)
// Stack overflow handling only for frames

typedef struct
{
    ObjClosure *closure;
    uint8_t *ip;
    Value *slots;
} CallFrame;

typedef enum
{
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

typedef struct
{
    CallFrame frames[FRAMES_MAX];
    int frameCount;

    Value stack[STACK_MAX];
    Value *stackTop;

    Table strings;
    Table globals;

    ObjUpvalue *openUpvalues;

    Obj *objects;
} VM;

extern VM vm;

void initVM();
void freeVM();

InterpretResult interpret(const char *source);

void push(Value value);
Value pop();

#endif
