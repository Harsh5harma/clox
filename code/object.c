#include <stdio.h>
#include <string.h>

#include "../headers/memory.h"
#include "../headers/object.h"
#include "../headers/value.h"
#include "../headers/vm.h"
#include "../headers/table.h"


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
static ObjString* allocateString(char* chars, int length, uint32_t hash) {

  // This instantiates an ObjString type Object on the heap, kind of like calling a super constructor.
  ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);

  string->length = length;
  string->chars = chars;
  string->hash = hash;
  // We're using the table more like a hash set than a hash table
  tableSet(&vm.strings, string, NIL_VAL);
  return string;
}

static uint32_t hashString(const char* key, int length) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i< length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 1677619;
  }
  return hash;
}

ObjString* takeString(char* chars, int length) {
  uint32_t hash = hashString(chars, length);

  // We look up the string in the string table first, if we find it, before we return it,
  // we free the memory for the string that was passed in.
  ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
  if (interned != NULL) {
    FREE_ARRAY(char, chars, length + 1);
    return interned;
  }
  return allocateString(chars, length, hash);
}

ObjString* copyString(const char* chars, int length) {
  uint32_t hash = hashString(chars, length);

  // when copying a string into a new lox string, we look it up in the string table first.
  // If we find it, we just retrun a reference to that string. Otherwise, we allocate a new string and store it in the table.
  ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
  if (interned != NULL) return interned;

  char* heapChars =  ALLOCATE(char, length+1);

  // copy into, copy from, copy till
  memcpy(heapChars, chars, length);
  // All strings in C are null terminated, unline in Lox
  heapChars[length] = '\0';
  return allocateString(heapChars, length, hash);
}

void printObject(Value value) {
  switch(OBJ_TYPE(value)) {
    case OBJ_STRING:
      printf("%s", AS_CSTRING(value));
      break;
  }
}
