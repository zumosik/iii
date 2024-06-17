#ifndef iii_chunk_h
#define iii_chunk_h

#include "value.h"
#include "common.h"

typedef enum
{
    OP_CONSTANT, // push a constant to the stack

    OP_DEFINE_GLOBAL, // define a global variable
    OP_GET_GLOBAL,    // get global variable
    OP_SET_GLOBAL,    // set global variable

    OP_SET_LOCAL, // set local variable
    OP_GET_LOCAL, // get local variable

    OP_NIL,   // nil
    OP_TRUE,  // true
    OP_FALSE, // false

    OP_NOT,     // not
    OP_EQUAL,   // equal
    OP_GREATER, // greater
    OP_LESS,    // less

    OP_NEGATE,   // negate
    OP_ADD,      // add
    OP_SUBTRACT, // subtract
    OP_MULTIPLY, // multiply
    OP_DIVIDE,   // divide

    OP_RETURN, // return the top of the stack

    OP_POP, // pop the top of the stack

    OP_JUMP_FALSE, // jump to a specific offset when false
    OP_JUMP,       // jump to a specific offset

    OP_CALL, // call a function

    OP_CLOSURE,

    OP_LOOP, // works like jump but with negative offset

} OpCode;

typedef struct
{
    int count;
    int capacity;
    uint8_t *code;
    int *lines;
    ValueArray constants;
} Chunk;

void initChunk(Chunk *chunk);
void writeChunk(Chunk *chunk, uint8_t byte, int line);
void writeConstant(Chunk *chunk, Value value, int line);
void freeChunk(Chunk *chunk);

int addConst(Chunk *chunk, Value value);

#endif