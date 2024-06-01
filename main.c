#include "chunk.h"
#include "debug.h"
#include "vm.h"

#include <stdio.h>

int main(int argc, const char* argv[]) {
    initVM();

    Chunk chunk;
    initChunk(&chunk);

    for (int i = 0; i < 300; i++)
    {
        writeConstant(&chunk, i, i);
    }    
    writeChunk(&chunk, OP_RETURN, 401);

    disassembleChunk(&chunk, "test chunk");
    
    interpret(&chunk);

    freeChunk(&chunk);
    freeVM();

    return 0;
}
