#ifndef clox_debug_h
#define clox_debug_h

#include <stdint.h>
#include "chunk.h"

// 反汇编字节码块
void disassembleChunk(Chunk* chunk, const char* name);
uint32_t disassembleInstruction(Chunk* chunk, uint32_t offset);
#endif