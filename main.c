#include "../headers/common.h"
#include "../headers/chunk.h"
#include "../disassembler/debug.h"
#include "../headers/vm.h"

int main(int argc, const char* argv[]) {
  initVM();

  Chunk chunk;
  initChunk(&chunk);

  // Note how the output of the OP_ADD implicitly flows into being an operand of OP_DIVIDE without either instruction being directly coupled to each other.
  // That’s the magic of the stack. It lets us freely compose instructions without them needing any complexity or awareness of the data flow. 
  // The stack acts like a shared workspace that they all read from and write to.
  int constant = addConstant(&chunk, 1.2);
  writeChunk(&chunk, OP_CONSTANT, 123);
  writeChunk(&chunk, constant, 123);

  

  constant = addConstant(&chunk, 3.4);
  writeChunk(&chunk, OP_CONSTANT, 123);
  writeChunk(&chunk, constant, 123);

  writeChunk(&chunk, OP_ADD, 123);

  constant = addConstant(&chunk, 5.6);
  writeChunk(&chunk, OP_CONSTANT, 123);
  writeChunk(&chunk, constant, 123);

  writeChunk(&chunk, OP_DIVIDE, 123);
  

  
  writeChunk(&chunk, OP_NEGATE, 123);
  writeChunk(&chunk, OP_RETURN, 123);

  //disassembleChunk(&chunk, "test chunk");

  interpret(&chunk);
  freeVM();
  freeChunk(&chunk);
  return 0;
}