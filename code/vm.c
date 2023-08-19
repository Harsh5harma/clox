#include "../headers/common.h"
#include "../headers/vm.h"
#include "../disassembler/debug.h"
#include "../headers/object.h"
#include "../headers/memory.h"
#include "../headers/compiler.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

VM vm;

static void resetStack() {
  vm.stackTop = vm.stack; // our top pointer points to the start of the array
}

static void runtimeError(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  size_t instruction = vm.ip - vm.chunk->code - 1; // the interpreter moves over the bad instruction, hence the -1 to get to the right one
  int line = vm.chunk->lines[instruction];
  fprintf(stderr, "[line %d] in script\n", line);
  resetStack();
}

void initVM() {
  initTable(&vm.globals);
  resetStack();
  vm.objects = NULL;
  initTable(&vm.strings);
}

void freeVM() {
  freeTable(&vm.globals);
  freeTable(&vm.strings);
  freeObjects();
}

void push(Value value) {
  *vm.stackTop = value;
  vm.stackTop++;
}

Value pop() {
  vm.stackTop--;
  return *vm.stackTop;
}

static Value peek(int distance) {
  // it returns a Value from the stack but doesn't pop it
  // The distance argument is how far down the top of the stack to look
  return vm.stackTop[-1 - distance];
}

static bool isFalsey(Value value) {
  // false and nil are falsey, everything else is true
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate() {
  ObjString* b = AS_STRING(pop());
  ObjString* a = AS_STRING(pop());

  int length = a->length + b->length;
  char* chars = ALLOCATE(char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  // add null terminator
  chars[length] = '\0';

  // actually allocate a new object that the ObjString owns, assume that you can't take ownership of the characters you pass in the source.
  ObjString* result = takeString(chars, length);
  push(OBJ_VAL(result));
}

static InterpretResult run() {
  #define READ_BYTE() (*vm.ip++)
  #define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
  #define READ_STRING()   AS_STRING(READ_CONSTANT());
  #define READ_SHORT()    (vm.ip += 2, (uint16_t)((vm.ip[-2] << 8) | (vm.ip[-1])))

  // Using a do while loop in the macro looks funny, but it gives you a way to contain multiple statements
  // inside a block that also permits a semicolon at the end.
  #define BINARY_OP(valueType, op) \
    do { \
      if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
        runtimeError("Operands must be numbers."); \
        return INTERPRET_RUNTIME_ERROR; \
      }\
      double b = AS_NUMBER(pop()); \
      double a = AS_NUMBER(pop()); \
      push(valueType(a op b)); \
    } while (false)


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
      case OP_NIL: push(NIL_VAL); break;
      case OP_TRUE: push(BOOL_VAL(true)); break;
      case OP_FALSE: push(BOOL_VAL(false)); break;
      case OP_POP: pop(); break;
      case OP_GET_LOCAL: {
        /*
        It takes a single-byte operand for the stack slot where the local lives. 
        It loads the value from that index and then pushes it on top of the stack where later instructions can find it.
        Kind of redudant, since we're popping a value that already is down there in the stack. But that's how stack based bytecode instructions operate.
        Register based bytecode is better in this aspect that it juggles around the stack, but the instructions are larger and operands are more.
        */
        uint8_t slot = READ_BYTE();
        push(vm.stack[slot]);
        break;
      }
      case OP_SET_LOCAL: {
        // It takes the assigned value from the top of the stack and stores it in the stack slot corresponding to the local variable
        uint8_t slot = READ_BYTE();
        vm.stack[slot] = peek(0);
        break;
      }
      case OP_GET_GLOBAL: {
        ObjString* name = READ_STRING();
        Value value;
        // We pull the constant table index from the instruction’s operand and get the variable name.
        // Then we use that as a key to look up the variable’s value in the globals hash table.
        if (!tableGet(&vm.globals, name, &value)) {
          runtimeError("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        push(value);
        break;
      }
      case OP_DEFINE_GLOBAL: {
        ObjString* name = READ_STRING();
        tableSet(&vm.globals, name, peek(0));
        pop();
        break;
      }
      case OP_SET_GLOBAL : {
        ObjString* name = READ_STRING();
        // if the variable hasn't been defined yet, its a runtime error to try and assign it
        // Setting a variable doesn't pop the value off the stack. Since assignment is an expression, so it needs to leave that
        // value there in case the assignment is nested inside some larger expression.
        if (tableSet(&vm.globals, name, peek(0))) {
          tableDelete(&vm.globals, name);
          runtimeError("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_EQUAL: {
        Value b = pop();
        Value a = pop();
        push(BOOL_VAL(valuesEqual(a, b)));
        break;
      }
      case OP_GREATER: BINARY_OP(NUMBER_VAL, >); break;
      case OP_LESS: BINARY_OP(NUMBER_VAL, <); break;
      case OP_ADD: {
        if ((IS_STRING(peek(0)) && IS_STRING(peek(1)))) {
          concatenate();
        } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
          double b = AS_NUMBER(pop());
          double a = AS_NUMBER(pop());
          push(NUMBER_VAL(a+b));
        } else {
          runtimeError(
            "Operands must be two numbers or two strings."
          );
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
      case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
      case OP_DIVIDE: BINARY_OP(NUMBER_VAL, /); break;
      case OP_NOT: 
        push(BOOL_VAL(isFalsey(pop())));
        break;
      case OP_NEGATE:
        if (!IS_NUMBER(peek(0))) {
          runtimeError("Operand must be a number.");
          return INTERPRET_RUNTIME_ERROR;
        }
        push(NUMBER_VAL(-AS_NUMBER(pop()))); 
      break;
      case OP_PRINT: {
        printValue(pop());
        printf("\n");
        break;
      }
      case OP_JUMP: {
        uint16_t offset = READ_SHORT();

        // unlike if-else, this jump isn't optional.
        vm.ip += offset;
        break;
      }
      case OP_JUMP_IF_FALSE: {
        // get the actual length of the if then block.
        uint16_t offset = READ_SHORT();

        // if the statement is falsey, jump over it by the offset your compiler calculated.
        if (isFalsey(peek(0))) vm.ip += offset;
        break;
      }
      case OP_RETURN: {
        return INTERPRET_OK;
      }
    }
  }

  #undef READ_BYTE
  #undef READ_CONSTANT
  #undef READ_STRING
  #undef READ_SHORT
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

