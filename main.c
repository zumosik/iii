#include "chunk.h"
#include "debug.h"

#include <stdio.h>

int main(int argc, const char* argv[]) {
    printf("Hello\n");

    Chunk chunk;
    initChunk(&chunk);

    for (int i = 0; i < 400; i++)
    {
        writeConstant(&chunk, i * 2.5, i);
    }    


    writeChunk(&chunk, OP_RETURN, 401);

    disassembleChunk(&chunk, "test chunk");
    
    freeChunk(&chunk);

    return 0;
}
