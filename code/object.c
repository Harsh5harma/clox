#include <stdio.h>
#include <string.h>

#include "../headers/memory.h"
#include "../headers/object.h"
#include "../headers/value.h"
#include "../headers/vm.h"


/*
  Why don't we simply point to the strings located in the source file instead?
  Because, not all strings in Lox would be static, there would be extensions for operations like
  string concatenation, and that'd require the ability for dynamic allocation on the heap as necessary. 
  Which also means that we need to have the option to free memory held by temporary strings as well.
*/

#define ALLOCATE_OBJ(type, objectType) \
  (type*)allocateObject(sizeof(type), objectType)

// code to instantiate a base struct pointer that later gets downcasted to a specific type like String.
static Obj* allocateObject(size_t size, ObjType type) {
  Obj* object = (Obj*)reallocate(NULL, 0, size);
  object->type = type;

  // updating the head of the intrusive linked list each time
  object->next = vm.objects;
  vm.objects = object;
  
  return object;
}

// The real string creation happens here.
static ObjString* allocateString(char* chars, int length) {

  // This instantiates an ObjString type Object on the heap, kind of like calling a super constructor.
  ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);

  string->length = length;
  string->chars = chars;
  return string;
}

ObjString* takeString(char* chars, int length) {
  return allocateString(chars, length);
}

ObjString* copyString(const char* chars, int length) {
  char* heapChars =  ALLOCATE(char, length+1);

  // copy into, copy from, copy till
  memcpy(heapChars, chars, length);
  // All strings in C are null terminated, unline in Lox
  heapChars[length] = '\0';
  return allocateString(heapChars, length);
}

void printObject(Value value) {
  switch(OBJ_TYPE(value)) {
    case OBJ_STRING:
      printf("%s", AS_CSTRING(value));
      break;
  }
}
