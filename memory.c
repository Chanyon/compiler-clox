#include "memory.h"
#include "chunk.h"
#include "compiler.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef DEBUG_LOG_GC
#include "common.h"
#include <stdio.h>
#endif

#define GC_HEAP_GROW_FACTOR 2

void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
  // GC
  // 在分配前进行内存回收
  vm.bytes_allocated += newSize - oldSize;
  //   if (newSize > oldSize) {
  // #ifdef DEBUG_STRESS_GC
  //     collectGarbage();
  // #endif
  //   }
  if (vm.bytes_allocated > vm.next_gc) {
    collectGarbage();
  }

  if (newSize == 0) {
    free(pointer);
    return NULL;
  }

  void *result = realloc(pointer, newSize);
  if (result == NULL) {
    exit(1);
  }

  return result;
}

void freeObject(Obj *object) {
#ifdef DEBUG_LOG_GC
  char *ty;
  switch (object->type) {
  case OBJ_STRING:
    ty = "OBJ_STRING";
    break;
  case OBJ_FUNCTION:
    ty = "OBJ_FUNCTION";
    break;
  case OBJ_NATIVE:
    ty = "OBJ_NATIVE";
    break;
  case OBJ_CLOSURE:
    ty = "OBJ_CLOSURE";
    break;
  case OBJ_UPVALUE:
    ty = "OBJ_UPVALUE";
    break;
  default:
    ty = "Other";
    break;    
  }
  printf("--- %p free type %s\n", (void *)object, ty);
#endif

  switch (object->type) {
  case OBJ_STRING:
    ObjString *string = (ObjString *)object;
    FREE_ARRAY(char, string->chars, string->length + 1);
    FREE(ObjString, object);
    break;
  case OBJ_FUNCTION:
    ObjFunction *function = (ObjFunction *)object;
    freeChunk(&function->chunk);
    FREE(ObjFunction, object);
    break;
  case OBJ_NATIVE:
    FREE(ObjNative, object);
    break;
  case OBJ_CLOSURE:
    ObjClosure *closure = (ObjClosure *)object;
    FREE_ARRAY(ObjUpvalue *, closure->upvalues, closure->upvalue_count);
    FREE(ObjClosure, object);
    break;
  case OBJ_UPVALUE:
    FREE(ObjUpvalue, object);
    break;
  case OBJ_CLASS:
    FREE(ObjClass, object);
    break;
  case OBJ_INSTANCE:
    ObjInstance *instance = (ObjInstance *)object;
    freeTable(&instance->fields);
    FREE(ObjInstance, object);
    break;
  }
}

void freeObjects() {
  Obj *object = vm.objects;
  while (object != NULL) {
    Obj *next = object->next;
    freeObject(object);
    object = next;
  }
  free(vm.gray_stack);
}

void markObject(Obj *object) {
  if (object == NULL) {
    return;
  }
  if (object->is_marked) {
    return;
  }
#ifdef DEBUG_LOG_GC
  printf("--- %p mark ", (void *)object);
  printValue(OBJ_VAL(object));
  printf("\n");
#endif
  object->is_marked = true;
  // gray mark stack
  if (vm.gray_capacity < vm.gray_count + 1) {
    vm.gray_capacity = GROW_CAPACITY(vm.gray_capacity);
    vm.gray_stack =
        (Obj **)realloc(vm.gray_stack, sizeof(Obj *) * vm.gray_capacity);
    if (vm.gray_stack == NULL) {
      printf("==== gray stack is null.====");
      exit(1);
    }
  }
  vm.gray_stack[vm.gray_count] = object;
  vm.gray_count += 1;
}

void markValue(Value value) {
  if (IS_OBJ(value)) {
    markObject(AS_OBJ(value));
  }
}

static void markRoots() {

  for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
    markValue(*slot);
  }
  // 不明显的root
  for (int i = 0; i < vm.frameCount; i++) {
    markObject((Obj *)vm.frames[i].closure);
  }

  for (ObjUpvalue *upvalue = vm.open_upvalues; upvalue != NULL;
       upvalue = upvalue->next) {
    markObject((Obj *)upvalue);
    printf("==========upvalue============\n");
  }
  markTable(&vm.globals);
  // compiler time
  markCompilerRoots();
}

static void markArray(ValueArray *array) {
  for (int i = 0; i < array->count; i++) {
    markValue(array->values[i]);
  }
}

static void blackenObject(Obj *obj) {
#ifdef DEBUG_LOG_GC
  printf("--- %p blacken ", (void *)obj);
  printValue(OBJ_VAL(obj));
  printf("\n");
#endif
  switch (obj->type) {
  case OBJ_NATIVE:
  case OBJ_STRING:
    break;
  case OBJ_UPVALUE:
    markValue(((ObjUpvalue *)obj)->closed);
    break;
  case OBJ_FUNCTION:
    ObjFunction *func = (ObjFunction *)obj;
    markObject((Obj *)func->name);
    markArray(&func->chunk.constants);
    break;
  case OBJ_CLOSURE:
    ObjClosure *closure = (ObjClosure *)obj;
    markObject((Obj *)closure->function);
    for (int i = 0; i < closure->upvalue_count; i++) {
      markObject((Obj *)closure->upvalues[i]);
    }
    break;
  case OBJ_CLASS:
    ObjClass *kclass = (ObjClass *)obj;
    markObject((Obj *)kclass->name);
    break;
  case OBJ_INSTANCE:
    ObjInstance *instance = (ObjInstance *)obj;
    markObject((Obj *)instance->kclass);
    markTable(&instance->fields);
    break;
  }
}

static void traceReferences() {
  while (vm.gray_count > 0) {
    Obj *obj = vm.gray_stack[vm.gray_count - 1];
    vm.gray_count -= 1;
    // 把对象标记为黑色, gray -> black
    blackenObject(obj);
  }
}

static void sweep() {
  Obj *previous = NULL;
  Obj *object = vm.objects;
  while (object != NULL) {
    if (object->is_marked) {
      object->is_marked = false;
      previous = object;
      object = object->next;
    } else {
      Obj *unreached = object;
      object = object->next;
      if (previous != NULL) {
        previous->next = object;
        // vm.objects = previous;
      } else {
        vm.objects = object;
      }

      freeObject(unreached);
    }
  }
}

void collectGarbage() {
#ifdef DEBUG_LOG_GC
  printf("--- gc begin\n");
  // size_t before = vm.bytes_allocated;
#endif
  //
  markRoots();
  traceReferences();
  tableRemoveWhite(&vm.strings);
  sweep();
  vm.next_gc = vm.bytes_allocated * GC_HEAP_GROW_FACTOR;
#ifdef DEBUG_LOG_GC
  printf("--- gc end\n");
  // printf("==== collected %zu bytes (from %zu to %zu) next at %zu ====\n",
  // (before - vm.bytes_allocated), before, vm.bytes_allocated, vm.next_gc);
#endif
}