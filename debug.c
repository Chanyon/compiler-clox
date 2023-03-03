#include "debug.h"
#include "chunk.h"
#include "object.h"
#include "value.h"
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

static uint32_t localInstruction(const char *name, Chunk *chunk,
                                 uint32_t offset) {
  uint8_t slot = chunk->code[offset + 1];
  printf("opcode:%-16s opcode_index:%1d locals_slot:%1d", name, offset, slot);
  printf("\n");
  return offset + 2; // 下一条指令起始位置的偏移量
}

static int jumpInstruction(const char *name, int sign, Chunk *chunk,
                           int offset) {
  uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
  jump |= chunk->code[offset + 2];

  printf("%-16s %4d -> %d jump: %d \n", name, offset, offset + 3 + sign * jump,
         jump);
  return offset + 3;
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
  case OP_DEFINE_GLOBAL:
    return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
  case OP_GET_GLOBAL:
    return constantInstruction("OP_GET_GLOBAL", chunk, offset);
  case OP_SET_GLOBAL:
    return constantInstruction("OP_SET_GLOBAL", chunk, offset);
  case OP_SET_LOCAL:
    return localInstruction("OP_SET_LOCAL", chunk, offset);
  case OP_GET_LOCAL:
    return localInstruction("OP_GET_LOCAL", chunk, offset);
  case OP_JUMP_IF_FALSE:
    return jumpInstruction("OP_JUMP_IF_ELSE", 1, chunk, offset);
  case OP_JUMP:
    return jumpInstruction("OP_JUMP", 1, chunk, offset);
  case OP_LOOP:
    return jumpInstruction("OP_LOOP", -1, chunk, offset);
  case OP_CALL:
    return constantInstruction("OP_CALL", chunk, offset);
  case OP_CLOSURE:
    int i = offset;
    uint8_t constant_idx = chunk->code[i + 1];
    printf("opcode:%-16s constant_idx:%1d ", "OP_CLOSURE", constant_idx);
    printValue(chunk->constants.values[constant_idx]);
    printf("\n");
    ObjFunction *function = AS_FUNCTION(chunk->constants.values[constant_idx]);
    i += 2;
    for (int j = 0; j < function->upvalue_count; j++) {
      int is_local = chunk->code[i++];
      int idx = chunk->code[i++];
      printf("%04d  |       %s %d\n", offset - 2,
             is_local ? "local" : "upvalue", idx);
    }
    return offset + i;
  case OP_SET_UPVALUE:
    return constantInstruction("OP_SET_UPVALUE", chunk, offset);
  case OP_GET_UPVALUE:
    return constantInstruction("OP_GET_UPVALUE", chunk, offset);
  case OP_CLOSE_UPVALUE:
    return simpleInstruction("OP_CLOSE_UPVALUE", offset);
  default:
    printf("Unknown opcode %d\n", instruction);
    return offset + 1;
  }
}
