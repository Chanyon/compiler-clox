#ifndef clox_memory_h
#define clox_memory_h

#include "chunk.h"
#include "common.h"
#include "object.h"
#include "value.h"

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity)*2)

#define GROW_ARRAY(type, pointer, oldCount, newCount)                          \
  (type *)reallocate(pointer, sizeof(type) * (oldCount),                       \
                     sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount)                                    \
  reallocate(pointer, sizeof(type) * (oldCount), 0)

#define ALLOCATE(type, count) (type *)reallocate(NULL, 0, sizeof(type) * count)
#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)
// 申请内存空间
void *reallocate(void *pointer, size_t oldSize, size_t newSize);
void freeObjects();
void collectGarbage();
static void markRoots();
void markValue(Value value);
void markObject(Obj* object);
static void traceReferences();
static void blackenObject(Obj *obj);
static void markArray(ValueArray *array);
static void sweep();
#endif
