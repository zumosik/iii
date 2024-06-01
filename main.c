#include "chunk.h"
#include "debug.h"
#include "vm.h"

#include <stdio.h>

int main(int argc, const char* argv[]) {
    initVM();

    Chunk chunk;
    initChunk(&chunk);

    // To test the OP_CONSTANT_LONG instruction
    // for (int i = 0; i < 300; i++)
    // {
    //     writeConstant(&chunk, i, i);
    // }    

    writeConstant(&chunk, 1.2, 123);
    writeChunk(&chunk, OP_NEGATE, 1);
    writeChunk(&chunk, OP_RETURN, 2);

    disassembleChunk(&chunk, "test chunk");
    
    printf("\n== running ==\n");

    interpret(&chunk);

    freeChunk(&chunk);
    freeVM();

    return 0;
}
