#include <stdio.h>

#include "debug.h"
#include "chunk.h"
#include "object.h"
#include "value.h"




int simpleInstruction(const char *name, int offset)
{
    printf("%s\n", name);
    return offset + 1;
}

int byteInstruction(const char *name, Chunk *chunk, int offset)
{
    uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

int jumpInstruction(const char *name, int sign,
                    Chunk *chunk, int offset)
{
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset,
           offset + 3 + sign * jump);
    return offset + 3;
}

int byteInstructionLong(const char *name, Chunk *chunk, int offset)
{
    uint16_t slot = (chunk->code[offset + 1] << 8) |
                    chunk->code[offset + 2];
    printf("%-16s %6d\n", name, slot);
    return offset + 3;
}

int longConstantInstruction(const char *name, Chunk *chunk, int offset)
{
    uint16_t constant = (chunk->code[offset + 1] << 8) |
                        chunk->code[offset + 2];

    printf("%-16s %4d '", name, constant);
    printValue(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 3;
}


void disassembleChunk(Chunk *chunk, const char *name)
{
    printf("== %s ==\n", name);
    printf("length: %d\n", chunk->count);
    printf("\n");

    for (int offset = 0; offset < chunk->count;)
    {
        offset = disassembleInstruction(chunk, offset);
    }
}

int disassembleInstruction(Chunk *chunk, int offset)
{

    printf("%04d ", offset);

    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1])
    {
        printf("   | ");
    }
    else
    {
        printf("%4d ", chunk->lines[offset]);
    }

    uint8_t instruction = chunk->code[offset];
    switch (instruction)
    {
    case OP_RETURN:
        return simpleInstruction("OP_RETURN", offset);
    case OP_CONSTANT:
        return longConstantInstruction("OP_CONSTANT", chunk, offset);
    case OP_NEGATE:
        return simpleInstruction("OP_NEGATE", offset);
    case OP_ADD:
        return simpleInstruction("OP_ADD", offset);
    case OP_SUBTRACT:
        return simpleInstruction("OP_SUBTRACT", offset);
    case OP_MULTIPLY:
        return simpleInstruction("OP_MULTIPLY", offset);
    case OP_DIVIDE:
        return simpleInstruction("OP_DIVIDE", offset);
    case OP_TRUE:
        return simpleInstruction("OP_TRUE", offset);
    case OP_FALSE:
        return simpleInstruction("OP_FALSE", offset);
    case OP_NIL:
        return simpleInstruction("OP_NIL", offset);
    case OP_EQUAL:
        return simpleInstruction("OP_EQUAL", offset);
    case OP_GREATER:
        return simpleInstruction("OP_GREATER", offset);
    case OP_LESS:
        return simpleInstruction("OP_LESS", offset);
    case OP_NOT:
        return simpleInstruction("OP_NOT", offset);
    case OP_POP:
        return simpleInstruction("OP_POP", offset);
    case OP_GET_GLOBAL:
        return longConstantInstruction("OP_GET_GLOBAL", chunk, offset);
    case OP_SET_GLOBAL:
        return longConstantInstruction("OP_SET_GLOBAL", chunk, offset);
    case OP_DEFINE_GLOBAL:
        return longConstantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
    case OP_GET_LOCAL:
        return byteInstructionLong("OP_GET_LOCAL", chunk, offset);
    case OP_SET_LOCAL:
        return byteInstructionLong("OP_SET_LOCAL", chunk, offset);
    case OP_JUMP:
        return jumpInstruction("OP_JUMP", 1, chunk, offset);
    case OP_JUMP_FALSE:
        return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
    case OP_LOOP:
        return jumpInstruction("OP_LOOP", -1, chunk, offset);
    case OP_CALL:
        return byteInstruction("OP_CALL", chunk, offset);
    case OP_CLOSURE:
    {
        offset++;
        uint16_t constant = (chunk->code[offset] << 8) | chunk->code[offset + 1];
        offset += 2;
        printf("%-16s %6d ", "OP_CLOSURE", constant);
        printValue(chunk->constants.values[constant]);
        printf("\n");

        ObjFunc* func = AS_FUNCTION(chunk->constants.values[constant]);

        printf("\n%s\n", func->name->chars);

    

        for (int j = 0; j < func->upvalueCount; j++) {
          int isLocal = chunk->code[offset++];
          int index =  ((chunk->code[offset] << 8) |chunk->code[offset+1]);
          offset+=2;
          printf("%04d    |          | %s %d\n",
                 offset-3, isLocal ? "local" : "upvalue",
                 index);
        }

        return offset;
    }
    case OP_GET_UPVALUE: 
      return byteInstructionLong("OP_GET_UPVALUE", chunk, offset);
    case OP_SET_UPVALUE:
      return byteInstructionLong("OP_SET_UPVALUE", chunk, offset);
    default:
        printf("Unknown opcode %d\n", instruction);
        return offset + 1;
    }
}
