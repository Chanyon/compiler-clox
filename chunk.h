#ifndef clox_chunk_h
#define clox_chunk_h // chunk 字节码序列
#include "common.h"
#include "value.h"
#include <stdint.h>

// 操作码
typedef enum {
  OP_CONSTANT, // = 0
  OP_RETURN,
  OP_ADD,
  OP_NEGATE,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_NIL,
  OP_TRUE,
  OP_FALSE,
  OP_NOT,
  OP_EQUAL,
  OP_LESS,
  OP_GREATER,
  OP_PRINT,
  OP_POP,
  OP_DEFINE_GLOBAL,
  OP_GET_GLOBAL,
  OP_SET_GLOBAL,
  OP_GET_LOCAL,
  OP_SET_LOCAL,
  OP_JUMP_IF_FLASE,
  OP_JUMP,
} Opcode;

// Chunk 保存所有指令
// 指令动态数组， code指针指向的内存地址
// count 已使用
// capacity 总容量
// 使用大端编码解决常量池index保存到uint8_t code中类型问题
typedef struct {
  int capacity;
  int count;
  uint8_t *code;
  uint32_t *lines;
  ValueArray constants;
} Chunk;

void initChunk(Chunk *chunk);
void writeChunk(Chunk *chunk, uint8_t byte, uint32_t line);
void freeChunk(Chunk *chunk);
uint32_t addConstant(Chunk *chunk, Value value);
#endif
