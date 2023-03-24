#include "object.h"
#include "chunk.h"
#include "memory.h"
#include "table.h"
#include "value.h"
#include "vm.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ALLOCATE_OBJ(type, objectType)                                         \
  (type *)allocateObject(sizeof(type), objectType)

static Obj *allocateObject(size_t size, ObjType type) {
  Obj *object = (Obj *)reallocate(NULL, 0, size);
  object->type = type;
  object->is_marked = false;
  object->next = vm.objects;
  vm.objects = object;

#ifdef DEBUG_LOG_GC
  char *ty;
  switch (type) {
  case 0:
    ty = "OBJ_STRING";
    break;
  case 1:
    ty = "OBJ_FUNCTION";
    break;
  case 2:
    ty = "OBJ_NATIVE";
    break;
  case 3:
    ty = "OBJ_CLOSURE";
    break;
  case 4:
    ty = "OBJ_UPVALUE";
    break;
  default:
    ty = "Other";
    break;
  }
  printf("--- %p allocate %zu for %s\n", (void *)object, size, ty);
#endif
  return object;
}

ObjString *allocateString(char *chars, int length, uint32_t hash) {
  ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
  string->length = length;
  string->chars = chars;
  string->hash = hash;
  push(OBJ_VAL(string)); // GC
  tableSet(&vm.strings, string, NIL_VAL);
  pop();
  return string;
}

// FNV-1a
static uint32_t hashString(const char *key, int length) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }
  return hash;
}

ObjString *copyString(const char *chars, int length) {
  uint32_t hash = hashString(chars, length);

  ObjString *interned = tableFindString(&vm.strings, chars, length, hash);
  if (interned != NULL) {
    return interned;
  }

  char *heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';
  return allocateString(heapChars, length, hash);
}

static void printFunction(ObjFunction *function) {
  if (function->name == NULL) {
    printf("<script>");
  } else {
    printf("<fn %s>", function->name->chars);
  }
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
  case OBJ_STRING:
    printf("%s", AS_CSTRING(value));
    break;
  case OBJ_FUNCTION:
    printFunction(AS_FUNCTION(value));
    break;
  case OBJ_NATIVE:
    printf("<native fn>");
    break;
  case OBJ_CLOSURE:
    printFunction(AS_CLOSURE(value)->function);
    break;
  case OBJ_UPVALUE:
    printf("upvalue");
    break;
  case OBJ_CLASS:
    printf("class %s", AS_CLASS(value)->name->chars);
    break;
  case OBJ_INSTANCE:
    printf("%s instance",AS_INSTANCE(value)->kclass->name->chars);
    break;
  }
}

ObjString *takeString(char *chars, int length) {
  uint32_t hash = hashString(chars, length);
  ObjString *interned = tableFindString(&vm.strings, chars, length, hash);
  if (interned != NULL) {
    FREE_ARRAY(char, chars, length + 1);
    return interned;
  }
  return allocateString(chars, length, hash);
}

ObjFunction *newFunction() {
  ObjFunction *function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
  function->arity = 0;
  function->name = NULL;
  function->upvalue_count = 0;
  initChunk(&function->chunk);
  return function;
}

ObjNative *newNative(NativeFn function) {
  ObjNative *native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
  native->function = function;
  return native;
}

ObjClosure *newClosure(ObjFunction *function) {
  ObjUpvalue **upvalues = ALLOCATE(ObjUpvalue *, function->upvalue_count);
  for (int i = 0; i < function->upvalue_count; i++) {
    upvalues[i] = NULL;
  }
  ObjClosure *closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
  closure->function = function;
  closure->upvalues = upvalues;
  closure->upvalue_count = function->upvalue_count;
  return closure;
}

ObjUpvalue *newUpvalue(Value *slot) {
  ObjUpvalue *upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
  upvalue->location = slot;
  upvalue->next = NULL;
  upvalue->closed = NIL_VAL;
  return upvalue;
}

ObjClass *newClass(ObjString *name) {
  ObjClass *class = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
  class->name = name;
  return class;
}

ObjInstance *newInstance(ObjClass *kclass) {
  ObjInstance *instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
  instance->kclass = kclass;
  initTable(&instance->fields);
  return instance;
}