#ifndef clox_vm_h
#define clox_vm_h
#include "value.h"
#include "chunk.h"

#define STACK_MAX 256

typedef struct {
  Chunk* chunk;
  /*We use an actual real C pointer pointing right into the middle of the bytecode array instead of something like an integer index
   because it’s faster to dereference a pointer than look up an element in an array by index.*/
  // ip is the instruction pointer, it tracks what instruction we're on
  uint8_t* ip;
  Value stack[STACK_MAX];
  Value* stackTop; 
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

void initVM();
void freeVM();
InterpretResult interpret(const char* source);
void push(Value value);
Value pop();
#endif 