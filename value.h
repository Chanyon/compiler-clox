#ifndef clox_value_h
#define clox_value_h
// 如何在虚拟机中表示值？
#include "common.h"
#include "stdint.h"

typedef enum {
  VAL_BOOL,
  VAL_NIL,
  VAL_NUMBER,
  VAL_OBJ,
} ValueType;

typedef struct Obj Obj;
typedef struct ObjString ObjString;
typedef struct ObjFunction ObjFunction;
typedef struct ObjNative ObjNative;

// 值类型
typedef struct {
  ValueType type;
  union {
    bool boolean;
    double number;
    Obj *obj;
  } as;
} Value;

typedef struct {
  ObjString *key;
  Value value;
} Entry;

#define BOOL_VAL(value) ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})
#define OBJ_VAL(object) ((Value){VAL_OBJ, {.obj = (Obj *)object}})

#define IS_BOOL(value) ((Value).type == VAL_BOOL)
#define IS_NIL(value) ((Value).type == VAL_NIL)
#define IS_NUMBER(value) ((Value).type == VAL_NUMBER)
#define IS_OBJ(value) ((value).type == VAL_OBJ)

#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)
#define AS_OBJ(value) ((value).as.obj)

// 常量池
typedef struct {
  uint32_t capacity;
  uint32_t count;
  Value *values;
} ValueArray;

void initVlaueArray(ValueArray *array);
void writeVlaueArray(ValueArray *array, Value value);
void freeVlaueArray(ValueArray *array);
void printValue(Value value);
bool valueEqual(Value a, Value b);

#endif
