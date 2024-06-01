#ifndef iii_vm_h
#define iii_vm_h

#include "chunk.h"

typedef enum {
 INTERPRET_OK,
 INTERPRET_COMPILE_ERROR,
 INTERPRET_RUNTIME_ERROR
} InterpretResult;

typedef struct {
    Chunk* chunk;
    uint8_t* ip;
} VM;

void initVM();
void freeVM();

InterpretResult interpret(Chunk* chunk);

#endif