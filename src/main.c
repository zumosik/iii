#include <stdio.h>
#include <stdlib.h>

#include "file.h"
#include "vm.h"

static void repl() {
  char line[1024];
  for (;;) {
    printf("> ");
    if (!fgets(line, sizeof(line), stdin)) {
      printf("\n");
      break;
    }
    interpret(line);
  }
}

int main(int argc, char *argv[]) {
  initVM();

  if (argc == 1) {
    repl();
  } else if (argc == 2) {
    runFile(argv[1]);
  } else {
    fprintf(stderr, "Usage: iii [path]\n");
    exit(1);
  }

  freeVM();

  return 0;
}
