#include "file.h"

#include <stdio.h>
#include <stdlib.h>

#include "vm.h"

char *readFile(const char *path) {
  FILE *file = fopen(path, "rb");
  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);
  char *buffer = (char *)malloc(fileSize + 1);
  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  buffer[bytesRead] = '\0';
  fclose(file);
  return buffer;
}

void runFile(const char *path) {
  char *source = readFile(path);
  InterpretResult result = interpret(source);
  free(source);
  if (result == INTERPRET_COMPILE_ERROR) exit(1);
  if (result == INTERPRET_RUNTIME_ERROR) exit(1);
}
