#include "debug.h"
#include "chunk.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void disassembleChunk(Chunk *chunk, const char *name) {
  printf("== %s ==\n", name);
  for (uint32_t offset = 0; offset < chunk->count;) {
    offset = disassembleInstruction(chunk, offset);
  }
}

static uint32_t simpleInstruction(const char *name, uint32_t offset) {
  printf("opcode:%s\n", name);
  return offset + 1;
}

static uint32_t constantInstruction(const char *name, Chunk *chunk,
                                    uint32_t offset) {
  uint8_t constant_idx = chunk->code[offset + 1];
  printf("opcode:%-16s opcode_index:%1d constant_index:%1d \"", name, offset,
         constant_idx);
  printValue(chunk->constants.values[constant_idx]);
  printf("\"\n");
  return offset + 2; // 下一条指令起始位置的偏移量
}

uint32_t disassembleInstruction(Chunk *chunk, uint32_t offset) {
  printf("%04d ", offset);

  if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
    printf("  |     ");
  } else {
    printf("line:%d  ", chunk->lines[offset]);
  }

  uint8_t instruction = chunk->code[offset];
  switch (instruction) {
  case OP_RETURN:
    return simpleInstruction("OP_RETURN", offset);
  case OP_CONSTANT:
    return constantInstruction("OP_CONSTANT", chunk, offset);
  case OP_ADD:
    return simpleInstruction("OP_ADD", offset);
  case OP_NEGATE:
    return simpleInstruction("OP_NEGATE", offset);
  case OP_SUBTRACT:
    return simpleInstruction("OP_SUBTRACT", offset);
  case OP_MULTIPLY:
    return simpleInstruction("OP_MULTIPLY", offset);
  case OP_DIVIDE:
    return simpleInstruction("OP_DIVIDE", offset);
  case OP_NIL:
    return simpleInstruction("OP_NIL", offset);
  case OP_TRUE:
    return simpleInstruction("OP_TRUE", offset);
  case OP_FALSE:
    return simpleInstruction("OP_FALSE", offset);
  case OP_NOT:
    return simpleInstruction("OP_NOT", offset);
  case OP_EQUAL:
    return simpleInstruction("OP_EQUAL", offset);
  case OP_LESS:
    return simpleInstruction("OP_LESS", offset);
  case OP_GREATER:
    return simpleInstruction("OP_GREATER", offset);
  case OP_PRINT:
    return simpleInstruction("OP_PRINT", offset);
  case OP_POP:
    return simpleInstruction("OP_POP", offset);
  default:
    printf("Unknown opcode %d\n", instruction);
    return offset + 1;
  }
}
