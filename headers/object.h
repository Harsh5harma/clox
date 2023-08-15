#ifndef clox_object_h
#define clox_object_h

#include "../headers/common.h"
#include "../headers/value.h"

#define OBJ_TYPE(value)       (AS_OBJ(value)->type)

#define IS_STRING(value)      isObjType(value, OBJ_STRING)

// down-casting Obj* to a VALID ObjString 
#define AS_STRING(value)      ((ObjString*)AS_OBJ(value))

// This fetches the character array holding the actual string by casting the value to an ObjString ptr
// and then dereferencing the chars field.
#define AS_CSTRING(value)     (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
  OBJ_STRING,
} ObjType;

struct Obj {
  ObjType type;
  struct Obj* next;
};

struct ObjString {
  Obj obj;
  int length;
  char* chars;
};

ObjString* takeString(char* chars, int length);

ObjString* copyString(const char* chars, int length);

void printObject(Value value);

// Why not just place this in the macro itself?
// Macros evaluate the passed expressions as many times as they appear in the code,
// So, if the expression's evaluation has some side effects, they get compounded as many times they get called.
static inline bool isObjType(Value value, ObjType type) {
  return IS_OBJ(value) && (AS_OBJ(value)->type == type);
}

#endif 