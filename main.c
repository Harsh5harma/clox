#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./headers/common.h"
#include "./headers/chunk.h"
#include "./disassembler/debug.h"
#include "./headers/vm.h"

static void repl() {
  char line[1024];
  for (;;) {
    printf("> ");

    if (!fgets(line, sizeof(line), stdin)) {
      printf("\n");
      break;
    }

    interpret(line);
  }
}

static char* readFile(const char* path) {
  // read file in binary mode
  FILE* file = fopen(path, "rb");

  if (file == NULL) {
    fprintf(stderr, "Could not open file \"%s\".\n", path);
    exit(74);
  }
  
  // seek to end of file to get the size of the file
  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  // return to the start again
  rewind(file);

  // now read the file into a string buffer
  char* buffer = (char*)malloc(fileSize + 1); // +1 to make size for the null terminator

  if (buffer == NULL) {
    fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
    exit(74);
  }
  // kisme read, kitna bytes ka step lena, total kitne bytes read krne, kaunsi file se read krne
  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  if (bytesRead < fileSize) {
    fprintf(stderr, "Could not read file \"%s\".\n", path);
  }
  // mark EOF with a null character
  buffer[bytesRead] = '\0';
  fclose(file);
  return buffer;
}

static void runFile(const char* path) {
  char* source = readFile(path);
  InterpretResult result = interpret(source);
  free(source);

  if (result == INTERPRET_COMPILE_ERROR) exit(65);
  if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

int main(int argc, const char* argv[]) {
  initVM();

  if (argc == 1) {
    repl();
  }else if (argc == 2) {
    runFile(argv[1]);
  } else {
    fprintf(stderr, "Usage: clox [path]\n");
    exit(64);
  }

  // Note how the output of the OP_ADD implicitly flows into being an operand of OP_DIVIDE without either instruction being directly coupled to each other.
  // That’s the magic of the stack. It lets us freely compose instructions without them needing any complexity or awareness of the data flow. 
  // The stack acts like a shared workspace that they all read from and write to.

  freeVM();
  return 0;
}