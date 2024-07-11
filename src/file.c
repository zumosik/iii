#include "file.h"

#include <stdio.h>
#include <stdlib.h>

#include "vm.h"

int readFile(char *filename, char **content) {
  FILE *file = fopen(filename, "rb");
  if (file == NULL) {
    // File could not be opened
    return 0;
  }

  // Move the file pointer to the end of the file to get the file size
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  rewind(file);

  // Allocate memory for the file content (+1 for the null terminator)
  *content = (char *)malloc(file_size + 1);
  if (*content == NULL) {
    // Memory allocation failed
    fclose(file);
    return 0;
  }

  // Read the file into the allocated memory
  size_t read_size = fread(*content, 1, file_size, file);
  if (read_size != (size_t)file_size) {
    // Reading the file failed
    free(*content);
    fclose(file);
    return 0;
  }

  // Null-terminate the string
  (*content)[file_size] = '\0';

  // Close the file
  fclose(file);
  return 1;
}

void runFile(char *path) {
  char *source = NULL;
  int res = readFile(path, &source);

  if (res == 0) {
    printf("Can't read file\n");
    exit(1);
  }

  InterpretResult result = interpret(source);
  free(source);
  if (result == INTERPRET_COMPILE_ERROR) exit(1);
  if (result == INTERPRET_RUNTIME_ERROR) exit(1);
}
