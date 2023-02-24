#include "vm.h"
#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

VM vm;

void initVM() {
  resetStack();
  vm.objects = NULL;
  initTable(&vm.strings);
  initTable(&vm.globals);
}

void freeVM() {
  freeTable(&vm.strings);
  freeTable(&vm.globals);
  freeObjects();
}

bool isNumber(Value v) {
  if (v.type == VAL_NUMBER) {
    return true;
  }
  return false;
}

Value binaryEval(char ch) {
  if (isNumber(peek(0)) && isNumber(peek(1))) {
    double b = pop().as.number;
    double a = pop().as.number;

    Value v = {VAL_NUMBER};
    switch (ch) {
    case '+':
      double sum = a + b;
      v.as.number = sum;
      return v;
    case '-':
      double sub = a - b;
      v.as.number = sub;
      return v;
    case '*':
      double mul = a * b;
      v.as.number = mul;
      return v;
    case '/':
      double div = a / b;
      v.as.number = div;
      return v;
    case '<':
      Value lt = {VAL_BOOL};
      lt.as.boolean = a < b ? true : false;
      return lt;
    case '>':
      Value gt = {VAL_BOOL};
      gt.as.boolean = a > b ? true : false;
      return gt;
    }

  } else if (IS_STRING(peek(0)) && IS_STRING(peek(1)) && ch == '+') {
    return concatenate();
  } else {
    switch (ch) {
    case '+':
      runtimeError("Operands must be number or string.");
      break;
    default:
      runtimeError("Operands must be number.");
      break;
    }
  }
}

static InterpretResult run() {
  CallFrame *frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define READ_SHORT()                                                           \
  (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

  // #define BINARY_OP(op)                                                          \
//   do {                                                                         \
//     double b = pop().as.number;                                                \
//     double a = pop().as.number;                                                \
//     double s = a op b;                                                         \
//     Value v = {.VAL_NUMBER, .as = s};                                          \
//     push(v);                                                                   \
//   }
  //   while (false)

  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    printf("      ");
    for (Value *slot = vm.stack + 1; slot < vm.stackTop; slot++) {
      printf("[ ");
      printValue(*slot);
      printf(" ]");
    }
    printf("\n");
    disassembleInstruction(&frame->function->chunk,
                           (int)(frame->ip - frame->function->chunk.code));
#endif

    uint8_t instruction;
    // 解码、指令分派
    switch (instruction = READ_BYTE()) {
    case OP_CONSTANT: {
      Value constant = READ_CONSTANT();
      // printf("run %f\n", constant);
      push(constant);
      break;
    }

    case OP_NEGATE:
      if (!isNumber(peek(0))) {
        runtimeError("Operand must a number.");
        return INTERPRET_RUNTIME_ERROR;
      }
      Value v = pop();
      v.as.number = -v.as.number;
      push(v);
      break;

    case OP_ADD:
      // BINARY_OP(+);
      push(binaryEval('+'));
      break;

    case OP_SUBTRACT:
      // BINARY_OP(-);
      push(binaryEval('-'));
      break;

    case OP_MULTIPLY:
      // BINARY_OP(*);
      push(binaryEval('*'));
      break;

    case OP_DIVIDE:
      // BINARY_OP(/);
      push(binaryEval('/'));
      break;
    case OP_NIL:
      push(NIL_VAL);
      break;
    case OP_TRUE:
      // Value t = {VAL_BOOL, .as.boolean = true};
      push(BOOL_VAL(true));
      break;
    case OP_FALSE:
      push(BOOL_VAL(false));
      break;
    case OP_NOT:
      Value notValue = {VAL_BOOL};
      notValue.as.boolean = isFalsey(pop());
      push(notValue);
      break;
    case OP_EQUAL:
      Value b = pop();
      Value a = pop();
      push(BOOL_VAL(valueEqual(a, b)));
      break;
    case OP_LESS:
      push(binaryEval('<'));
      break;
    case OP_GREATER:
      push(binaryEval('>'));
      break;
    case OP_PRINT:
      printValue(pop());
      printf("\n");
      break;
    case OP_POP:
      pop();
      // printValue(pop());
      // printf("\n");
      break;
    case OP_DEFINE_GLOBAL:
      ObjString *var_name = READ_STRING();
      tableSet(&vm.globals, var_name, peek(0));
      pop();
      break;
    case OP_GET_GLOBAL:
      ObjString *vname = READ_STRING();
      Value value;
      if (!tableGet(&vm.globals, vname, &value)) {
        runtimeError("undfined variable `%s`.", vname->chars);
        return INTERPRET_RUNTIME_ERROR;
      } else {
        push(value);
      }
      break;
    case OP_SET_GLOBAL:
      ObjString *sname = READ_STRING();
      if (tableSet(&vm.globals, sname, peek(0))) {
        tableDelete(&vm.globals, sname);
        runtimeError("undefined variable `%s`.", sname->chars);
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    case OP_GET_LOCAL:
      uint8_t slot = READ_BYTE();
      push(frame->slots[slot]);
      break;
    case OP_SET_LOCAL:
      uint8_t slot2 = READ_BYTE();
      frame->slots[slot2] = peek(0);
      break;
    case OP_JUMP_IF_FALSE:
      uint16_t offset = READ_SHORT();
      Value val = peek(0);
      if ((val.type == VAL_BOOL && !val.as.boolean) || val.type == VAL_NIL) {
        frame->ip += offset;
      }
      break;
    case OP_JUMP:
      uint16_t jump_offset = READ_SHORT();
      frame->ip += jump_offset;
      break;
    case OP_LOOP:
      uint16_t loop_offset = READ_SHORT();
      frame->ip -= loop_offset;
      break;
    case OP_CALL:
      uint8_t argCount = READ_BYTE();
      if (!callValue(peek(argCount), argCount)) {
        return INTERPRET_RUNTIME_ERROR;
      }
      frame = &vm.frames[vm.frameCount - 1];
      break;
    case OP_RETURN: {
      Value res = pop();
      vm.frameCount -= 1;
      if (vm.frameCount == 0) {
        pop();
        return INTERPRET_OK;
      }
      vm.stackTop = frame->slots;
      push(res);
      frame = &vm.frames[vm.frameCount - 1];
      break;
    }
    } // switch end
  }

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_STRING
#undef READ_SHORT
  // #undef BINARY_OP
}

InterpretResult interpret(const char *source) {
  ObjFunction *function = compiler(source);

  if (function == NULL) {
    return INTERPRET_COMPILE_ERROR;
  }

  push(OBJ_VAL(function));
  // CallFrame *frame = &vm.frames[vm.frameCount];
  // frame->function = function;
  // frame->ip = function->chunk.code;
  // frame->slots = vm.stack;
  // vm.frameCount += 1;
  // 顶层函数执行
  call_(function, 0);

  InterpretResult result = run();
  return result;
}

static void resetStack() {
  vm.stackTop = vm.stack;
  vm.frameCount = 0;
}
static void runtimeError(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);
  for (int i = vm.frameCount - 1; i >= 0; i--) {
    CallFrame *frame = &vm.frames[i];
    size_t instruction = frame->ip - frame->function->chunk.code - 1;
    int line = frame->function->chunk.lines[instruction];
    fprintf(stderr, "error: [line %d] in ", line);
    if (frame->function->name == NULL) {
      fprintf(stderr, "script.\n");
    } else {
      fprintf(stderr, "%s().\n", frame->function->name->chars);
    }
  }
  resetStack();
  exit(0);
}

void push(Value value) {
  *vm.stackTop = value;
  vm.stackTop++;
}

Value pop() {
  vm.stackTop--;
  return *vm.stackTop;
}

static Value peek(int distance) { return vm.stackTop[-1 - distance]; }

static bool isFalsey(Value value) {
  bool res;
  if (value.type == VAL_NIL) {
    res = true;
  }
  if (value.type == VAL_BOOL && !value.as.boolean) {
    res = true;
  } else {
    res = false;
  }
  return res;
}

// 字符串连接
static Value concatenate() {
  ObjString *bstring = AS_STRING(pop());
  ObjString *astring = AS_STRING(pop());
  int length = astring->length + bstring->length;
  char *headChars = ALLOCATE(char, length + 1);
  memcpy(headChars, astring->chars, astring->length);
  memcpy(headChars + astring->length, bstring->chars, bstring->length);
  headChars[length] = '\0';
  ObjString *result = takeString(headChars, length);
  return OBJ_VAL(result);
}

static bool call_(ObjFunction *function, uint8_t argCount) {
  if (function->arity != argCount) {
    runtimeError("expect %d arguments but got %d.", function->arity, argCount);
    return false;
  }

  if (vm.frameCount == FRAME_MAX) {
    runtimeError("stack overflow.");
    return false;
  }

  CallFrame *frame = &vm.frames[vm.frameCount];
  frame->function = function;
  frame->ip = function->chunk.code;
  frame->slots = vm.stackTop - argCount - 1;
  vm.frameCount += 1;
  return true;
}

static bool callValue(Value callee, uint8_t argCount) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
    case OBJ_FUNCTION:
      return call_(AS_FUNCTION(callee), argCount);
    default:
      break;
    }
  }
  runtimeError("can only call function and classes.");
  return false;
}
