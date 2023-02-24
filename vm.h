#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include <stdint.h>
#define STACK_MAX 2048
#define FRAME_MAX 64

// callFrame 正在执行的函数调用
typedef struct {
  ObjFunction *function;
  // 指令指针
  uint8_t *ip;
  Value *slots;
} CallFrame;

typedef struct {
  CallFrame frames[FRAME_MAX];
  int frameCount;
  Value stack[STACK_MAX]; // VM stack
  Value *stackTop;
  Obj *objects;
  Table strings; // string interning
  Table globals;
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} InterpretResult;

extern VM vm;

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
static bool callValue(Value callee, uint8_t argCount);
static bool call_(ObjFunction *function, uint8_t argCount);
#endif
