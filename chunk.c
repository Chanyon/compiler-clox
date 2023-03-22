#include "chunk.h"
#include "memory.h"
#include "value.h"
#include "vm.h"
#include <stdint.h>
#include <stdio.h>

// 初始化一个新块
void initChunk(Chunk *chunk) {
  chunk->count = 0;
  chunk->capacity = 0;
  chunk->code = NULL;
  chunk->lines = NULL;
  initVlaueArray(&chunk->constants);
}

void writeChunk(Chunk *chunk, uint8_t byte, uint32_t line) {
  if (chunk->capacity < chunk->count + 1) {
    int oldCapacity = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(oldCapacity);
    chunk->code =
        GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);

    chunk->lines =
        GROW_ARRAY(uint32_t, chunk->lines, oldCapacity, chunk->capacity);
  }

  chunk->code[chunk->count] = byte;
  chunk->lines[chunk->count] = line;
  chunk->count += 1;
}

void freeChunk(Chunk *chunk) {
  FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
  FREE_ARRAY(uint32_t, chunk->lines, chunk->capacity);
  freeVlaueArray(&chunk->constants);
  initChunk(chunk);
  // FREE(chunk);
}

uint32_t addConstant(Chunk *chunk, Value value) {
  push(value); //GC
  writeVlaueArray(&chunk->constants, value);
  pop(); //GC
  // 返回常量在常量池的index
  return chunk->constants.count - 1;
}