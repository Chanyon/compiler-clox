#ifndef clox_object_h
#define clox_object_h
#include "chunk.h"
#include "common.h"
#include "table.h"
#include "value.h"
#include <stdint.h>

#define OBJ_TYPE(value) (AS_OBJ(value)->type)
#define IS_STRING(value) isObjType(value, OBJ_STRING)
#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)
#define IS_CLOSURE(value) isObjType(value, OBJ_CLOSURE)
#define IS_CLASS(value) isObjType(value, OBJ_CLASS)
#define IS_INSTANCE(value) isObjType(value, OBJ_INSTANCE)

#define AS_STRING(value) ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString *)AS_OBJ(value))->chars)
#define AS_FUNCTION(value) ((ObjFunction *)AS_OBJ(value))
#define AS_NATIVE(value) (((ObjNative *)AS_OBJ(value))->function)
#define AS_CLOSURE(value) ((ObjClosure *)AS_OBJ(value))
#define AS_CLASS(value) ((ObjClass *)AS_OBJ(value))
#define AS_INSTANCE(value) ((ObjInstance *)AS_OBJ(value))

typedef Value (*NativeFn)(uint8_t argCount, Value *args);

typedef enum {
  OBJ_STRING,
  OBJ_FUNCTION,
  OBJ_NATIVE,
  OBJ_CLOSURE,
  OBJ_UPVALUE,
  OBJ_CLASS,
  OBJ_INSTANCE,
} ObjType;

struct Obj {
  ObjType type;
  bool is_marked;
  struct Obj *next;
};

struct ObjString {
  Obj obj;
  int length;
  char *chars;
  uint32_t hash;
};

struct ObjFunction {
  Obj obj;
  int arity; // 参数数量 number of parameters
  Chunk chunk;
  ObjString *name;
  int upvalue_count;
};

struct ObjNative {
  Obj obj;
  NativeFn function;
};

struct ObjClosure {
  Obj obj;
  ObjFunction *function;
  ObjUpvalue **upvalues;
  int upvalue_count;
};

struct ObjUpvalue {
  Obj obj;
  Value *location;
  ObjUpvalue *next;
  Value closed;
};

struct ObjClass {
  Obj obj;
  ObjString *name;
};

struct ObjInstance {
  Obj obj;
  ObjClass *kclass;
  Table fields;
};

static inline bool isObjType(Value value, ObjType type) {
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}
ObjString *copyString(const char *chars, int length);
void printObject(Value value);
ObjString *takeString(char *chars, int length);
ObjFunction *newFunction();
ObjNative *newNative(NativeFn function);
ObjClosure *newClosure(ObjFunction *function);
ObjUpvalue *newUpvalue(Value *slot);
ObjClass *newClass(ObjString *name);
ObjInstance *newInstance(ObjClass *kclass);
#endif
