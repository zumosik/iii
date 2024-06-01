#include "chunk.h"
#include "debug.h"
#include "vm.h"

#include <stdio.h>

int main(int argc, const char *argv[])
{
    initVM();

    Chunk chunk;
    initChunk(&chunk);

    // To test the OP_CONSTANT_LONG instruction
    // for (int i = 0; i < 300; i++)
    // {
    //     writeConstant(&chunk, i, i);
    // }

    // create a simple ast
    // -((1.2 + 3.4) / 5.6) = -0.821429
    writeConstant(&chunk, 1.2, 1);
    writeConstant(&chunk, 3.4, 2);
    writeChunk(&chunk, OP_ADD, 3);
    writeConstant(&chunk, 5.6, 4);
    writeChunk(&chunk, OP_DIVIDE, 5);
    writeChunk(&chunk, OP_NEGATE, 5);
    writeChunk(&chunk, OP_RETURN, 5);

    disassembleChunk(&chunk, "test chunk");

    printf("\n== running ==\n");

    interpret(&chunk);

    freeChunk(&chunk);
    freeVM();

    return 0;
}
