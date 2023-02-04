#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "value.h"
#include <stdint.h>
#define STACK_MAX 256

typedef struct {
  // 指令集合
  Chunk *chunk;
  // 即将执行的指令,指令指针
  uint8_t *ip;
  Value stack[STACK_MAX]; // VM stack
  Value *stackTop;
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} InterpretResult;

void initVM();
void freeVM();
InterpretResult interpret(const char *source);
static void resetStack();
void push(Value value);
Value pop();
static Value peek(int distance);
static void runtimeError(const char *format, ...);
static bool isFalsey(Value value);
static Value concatenate();
#endif
