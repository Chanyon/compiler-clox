#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "value.h"
#include "vm.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void repl() {
  char line[1024];
  while (true) {
    printf("> ");
    if (!fgets(line, sizeof(line), stdin)) {
      printf("\n");
      break;
    }
    InterpretResult res = interpret(line);
    switch (res) {
    case INTERPRET_OK:
      // printf("eval ok\n");
      break;
    case INTERPRET_COMPILE_ERROR:
      printf("compiler error\n");
      break;
    case INTERPRET_RUNTIME_ERROR:
      printf("runtime error\n");
      break;
    }
  }
}
static char *readFile(const char *path) {
  FILE *file = fopen(path, "rb");

  if (file == NULL) {
    fprintf(stderr, "could not open file '%s'!", path);
    exit(-1);
  }

  fseek(file, 0L, SEEK_END); // 文件末端
  size_t fileSize = ftell(file);
  rewind(file); // 退回到起始位置

  char *buffer = (char *)malloc(fileSize + 1);

  if (buffer == NULL) {
    fprintf(stderr, "not enouh memory to read \"%s\"", path);
  }
  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);

  if (bytesRead < fileSize) {
    fprintf(stderr, "could not read file '%s'!", path);
  }
  buffer[bytesRead] = '\0';
  fclose(file);

  return buffer;
}

static void runFile(const char *path) {
  char *source = readFile(path);
  InterpretResult result = interpret(source);
  free(source);

  if (result == INTERPRET_COMPILE_ERROR) {
    exit(-1);
  }
  if (result == INTERPRET_RUNTIME_ERROR) {
    exit(-1);
  }
}

int main(int argc, const char *argv[]) {
  initVM();

  if (argc == 1) {
    repl();
  } else if (argc == 2) {
    runFile(argv[1]);
  } else {
    fprintf(stderr, "Usae: clox [path]\n");
    exit(64);
  }
  freeVM();
  return 0;
}
