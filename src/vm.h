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
    CallFrame frames[FRAMES_MAX]; // frames 
    int frameCount; // count of frames 

    Value stack[STACK_MAX]; // stack 
    Value *stackTop; // pointer to stack top 

    Table strings; // table of strings (for optimization)
    Table globals; // table of globals 

    ObjUpvalue *openUpvalues; // all open upvalues 

    Obj *objects; // objects 

    // for GC
    int grayCount; 
    int grayCapacity;
    Obj** grayStack;
} VM;

extern VM vm;

void initVM();
void freeVM();

InterpretResult interpret(const char *source);

void push(Value value);
Value pop();

#endif
