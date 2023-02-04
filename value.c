#include "value.h"
#include "memory.h"
#include "object.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void initVlaueArray(ValueArray *array) {
  array->values = NULL;
  array->capacity = 0;
  array->count = 0;
}

void writeVlaueArray(ValueArray *array, Value value) {
  if (array->capacity < array->count + 1) {
    uint32_t oldCapacity = array->capacity;
    array->capacity = GROW_CAPACITY(oldCapacity);
    array->values =
        GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
  }

  array->values[array->count] = value;
  array->count += 1;
}

void freeVlaueArray(ValueArray *array) {
  FREE_ARRAY(Value, array->values, 0);
  initVlaueArray(array);
  // FREE(array);
}

void printValue(Value value) {
  switch (value.type) {
  case VAL_BOOL:
    if (value.as.boolean == true) {
      printf("true");
    } else {
      printf("false");
    }
    break;
  case VAL_NUMBER:
    printf("%g", value.as.number);
    break;
  case VAL_NIL:
    printf("nil");
    break;
  case VAL_OBJ:
    printObject(value);
    break;
  }
}

bool valueEqual(Value a, Value b) {
  if (a.type != b.type) {
    return false;
  }
  switch (a.type) {
  case VAL_BOOL:
    return AS_BOOL(a) == AS_BOOL(b);
  case VAL_NUMBER:
    return AS_NUMBER(a) == AS_NUMBER(b);
  case VAL_OBJ:
    ObjString *astring = AS_STRING(a);
    ObjString *bstring = AS_STRING(b);
    return (astring->length == bstring->length) &&
           memcmp(astring->chars, bstring->chars, astring->length) == 0;
  default:
    return false;
  }
}
