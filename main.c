#include "chunk.h"
#include "debug.h"

#include <stdio.h>

int main(int argc, const char* argv[]) {
    printf("Hello\n");

    Chunk chunk;
    initChunk(&chunk);

    writeChunk(&chunk, OP_RETURN);

    disassembleChunk(&chunk, "test");

    freeChunk(&chunk);

    return 0;
}
