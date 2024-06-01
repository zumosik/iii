#include "chunk.h"
#include "debug.h"

#include <stdio.h>

int main(int argc, const char* argv[]) {
    printf("Hello\n");

    Chunk chunk;
    initChunk(&chunk);

    int constant = addConst(&chunk, 1.2);
    writeChunk(&chunk, OP_CONSTANT, 1);
    writeChunk(&chunk, constant, 1);
    writeChunk(&chunk, OP_RETURN, 1);

    disassembleChunk(&chunk, "test");
    
    freeChunk(&chunk);

    return 0;
}
