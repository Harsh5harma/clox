#include "../headers/common.h"
#include "../headers/vm.h"
#include "../disassembler/debug.h"
#include "../headers/compiler.h"
#include <stdio.h>

VM vm;

static void resetStack() {
  vm.stackTop = vm.stack; // our top pointer points to the start of the array
}
void initVM() {
  resetStack();
}

void freeVM() {

}

void push(Value value) {
  *vm.stackTop = value;
  vm.stackTop++;
}

Value pop() {
  vm.stackTop--;
  return *vm.stackTop;
}

static InterpretResult run() {
  #define READ_BYTE() (*vm.ip++)
  #define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])

  // Using a do while loop in the macro looks funny, but it gives you a way to contain multiple statements
  // inside a block that also permits a semicolon at the end.
  #define BINARY_OP(op) \
    do { \
    double b = pop();\
    double a = pop();\
    push(a op b); \
    } while(false);

  for (;;) {
    #ifdef DEBUG_TRACE_EXECUTION

      printf("        ");
      for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
        printf("[ ");
        printValue(*slot);
        printf(" ]");
      }
      printf("\n");

      disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
    
    #endif 
    uint8_t instruction;
    // the first read byte always reads the opcode
    switch(instruction = READ_BYTE()) {
      case OP_CONSTANT: {
        // this modified read byte will provide us with the offset because a constant instruction is 2 bytes.
        Value constant = READ_CONSTANT();
        push(constant);
        break;
      }
      case OP_ADD: BINARY_OP(+); break;
      case OP_SUBTRACT: BINARY_OP(-); break;
      case OP_MULTIPLY: BINARY_OP(*); break;
      case OP_DIVIDE: BINARY_OP(/); break;
      case OP_NEGATE: push(-pop()); break;
      case OP_RETURN: {
        printValue(pop());
        printf("\n");
        return INTERPRET_OK;
      }
    }
  }

  #undef READ_BYTE
  #undef READ_CONSTANT
  #undef BINARY_OP
}

InterpretResult interpret(const char* source) {
  Chunk chunk;
  initChunk(&chunk);

  if (!compile(source, &chunk)) {
    freeChunk(&chunk);
    return INTERPRET_COMPILE_ERROR;
  }

  vm.chunk = &chunk;
  vm.ip = vm.chunk->code;
  
  InterpretResult result = run();

  freeChunk(&chunk);
  return result;
}

