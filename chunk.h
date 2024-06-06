#ifndef iii_chunk_h
#define iii_chunk_h

#include "value.h"
#include "common.h"

typedef enum
{
    OP_CONSTANT,
    OP_CONSTANT_LONG,

    OP_DEFINE_GLOBAL,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
    OP_DEFINE_GLOBAL_LONG,
    OP_GET_GLOBAL_LONG,
    OP_SET_GLOBAL_LONG,

    OP_NIL,
    OP_TRUE,
    OP_FALSE,

    OP_NOT,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,

    OP_NEGATE,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,

    OP_RETURN,

    OP_POP,

    OP_PRINT, // FIXME: shouldn't be opcode
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