#include <stdio.h>
#include "../headers/common.h"
#include "../headers/compiler.h"
#include "../headers/scanner.h"

void compile(const char* source) {
  initScanner(source);

  int line = -1;
  for (;;) {
    Token token = scanToken();
    if (token.line != line) {
      printf("%4d ", token.line);
      line = token.line;
    } else {
      printf("    | ");
    }
    // Using * lets you pass the precision as an argument.
    // This lets you straight up get the token by value, no fancy pointer or dynamic arrays stuff
    printf("%2d '%.*s'\n", token.type, token.length, token.start);

    if (token.type == TOKEN_EOF) break;
  }
}