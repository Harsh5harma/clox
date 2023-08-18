/*
  Our compiler is the last piece we need in our language pipeline to make sure that flow gets out right.
  Since it pipes the scanner's output to the VM's input, we'll need to handle both the front-end and the back-end here
  We'll need to parse our tokens to get the syntactical grammar right, and we'll also need to convert all that grammar
  into bytecode instructions for our beloved VM <3. To make the our ends meet (no pun intended), we'll deploy something called a "Pratt parser".
  It does something that's called "top-down operator precedence parsing" which according to the author is an elegant way to parse expressions.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../headers/common.h"
#include "../headers/compiler.h"
#include "../headers/scanner.h"
#include "../headers/chunk.h"
#include "../headers/object.h"

#ifdef DEBUG_PRINT_CODE
#include "../disassembler/debug.h"
#endif

typedef struct {
  Token current;
  Token previous;
  bool hadError;
  bool panicMode;
} Parser;

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT,
  PREC_OR, 
  PREC_AND,
  PREC_EQUALITY,
  PREC_COMPARISON,
  PREC_TERM,
  PREC_FACTOR,
  PREC_UNARY,
  PREC_CALL,
  PREC_PRIMARY
} Precedence;

// function pointer, typedef is a stylistic choice here
typedef void (*ParseFn)(bool canAssign);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

typedef struct {
  Token name;
  int depth;
} Local;

typedef struct {
  Local locals[UINT8_COUNT];
  int localCount; // tracks how many locals are in scope (how much of the array is in use)
  int scopeDepth; // The number of blocks surrounding the current bit of code we're compiling.
} Compiler;

Parser parser;
Compiler* current = NULL;

// Error handling code below
static void errorAt(Token* token, const char* message) {
  // we got a panic mode flag so our parser doesn't start throwing one error after the another
  // C doesn't have exceptions, so we just display the first error and keep on moving
  // it's like the errors never occured.
  if (parser.panicMode) return;
  parser.panicMode = true;
  fprintf(stderr, "[line %d] Error", token->line);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type == TOKEN_ERROR) {
    // Nothing.
  } else {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);
  parser.hadError = true;
}

static void error(const char* message) {
  errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char* message) {
  errorAt(&parser.current, message);
}
Chunk* compilingChunk;

static Chunk* currentChunk() {
  return compilingChunk;
}

// parsing functions below

static void advance() {
  // get a reference to current token before we advance
  parser.previous = parser.current;

  for (;;) {
    parser.current = scanToken();
    // if valid token break loop.
    if (parser.current.type != TOKEN_ERROR) break;

    errorAtCurrent(parser.current.start);
  }
}

// wrote similar function in jlox
static void consume(TokenType type, const char* message) {
  // consume is like the advance() function but with type validation
  if (parser.current.type == type) {
    advance();
    return;
  }

  errorAtCurrent(message);
}

static bool check(TokenType type) {
  return parser.current.type == type;
}

static bool match(TokenType type) {
  if (!check(type)) return false;
  advance();
  return true;
}

// ______________ End of Parsing functions __________________________

// Bytecode generation utility functions below. emitByte(), emitBytes(), emitReturn(), endCompiler()
static void emitByte(uint8_t byte) {
  writeChunk(currentChunk(), byte, parser.previous.line);
} 

static void emitBytes(uint8_t byte1, uint8_t byte2) {
  emitByte(byte1);
  emitByte(byte2);
}

static void emitReturn() {
  emitByte(OP_RETURN);
}

static uint8_t makeConstant(Value value) {
  int constant = addConstant(currentChunk(), value);
  
  // checks if our constant table hasn't grown over 256
  if (constant > UINT8_MAX) {
    error("Too many constants in one chunk.");
    return 0;
  }

  return (uint8_t)constant;
}

static void emitConstant(Value value) {
  emitBytes(OP_CONSTANT, makeConstant(value));
}

static void initCompiler(Compiler* compiler) {
  compiler->localCount = 0;
  compiler->scopeDepth = 0;
  current = compiler;
}

static void endCompiler() {
  emitReturn();

  #ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
      disassembleChunk(currentChunk(), "code");
    }
  #endif
}

static void beginScope() {
  // all we need to do for a new scope is to increment the current depth
  // way faster approach than
  current->scopeDepth++;
}

static void endScope() {
  current->scopeDepth--;

  // When we pop a scope, we walk backward through the local array looking for any variables declared at the scope depth we just left.
  // We discard them by simply decrementing the length of the array.
  while (current->localCount > 0 && current->locals[current->localCount-1].depth > current->scopeDepth) {
    emitByte(OP_POP);
    current->localCount--;
  }
}
// forward declarations to handle recursive grammar
static void expression();
// blocks can contain declarations and control flow statements can contain other statements. 
static void declaration();
static void statement();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);
//_____________End of Forward Declarations__________________________

static uint8_t identifierConstant(Token* name) {
  return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static bool identifiersEqual(Token* a, Token* b) {
  if (a->length != b->length) return false;
  return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler* compiler, Token* name) {
  // We walk the array backward so we can check in previous scopes too.
  for (int i = compiler->localCount - 1; i>=0; i--) {
    Local* local = &compiler->locals[i];
    if (identifiersEqual(name, &local->name)) {
      if (local->depth == -1) {
        error("Can't read local variable in its own initializer");
      }
      return 1;
    }
  }

  // variable wasn't found in local scopes, must be global.
  return -1;

}

static void addLocal(Token name) {
  // The first error we deal with is a limitation of our VM's design. We only allow for 256 local variables in scope at a given time.
  // So we need to handle a case where we could go over that.
  if (current->localCount == UINT8_COUNT) {
    error("Too many local variables in function.");
    return;
  }

  // get a reference to the top of the stack and increment the count
  Local* local = &current->locals[current->localCount++];
  local->name = name;
  local->depth = -1;
}
  /*
    {
      var a = "first";
      var a = "second";
    }
    This is invalid Lox code because that declaration format is weird and probably a mistake.
    We allowed redeclarations for the REPL but we can't here. Shadowing is allowed though as those are different scopes.
    We handle this second error in declareVariable()
  */


//“Declaring” is when the variable is added to the scope, and “defining” is when it becomes available for use.
static void declareVariable() {
  // This is the point where the compiler records the existence of the variable. We only do this for locals.
  // if its a global variable, we bail out since they are late bound.
  if (current->scopeDepth == 0) return;

  // The compiler does need to remember that these variables exist, so we keep a track of them.
  Token* name = &parser.previous;

  for (int i = current->localCount - 1; i>=0; i--) {
    Local* local = &current->locals[i];

    // if we pop out of all scopes and hit global, break loop. 
    if (local->depth != -1 && local->depth < current->scopeDepth) {
      break;
    }

    // We're walking backwards and checking here.
    if (identifiersEqual(name, &local->name)) {
      error("Already a variable with this name in this scope");
    }
  }

  addLocal(*name);
}

static uint8_t parseVariable(const char* errorMessage) {
  consume(TOKEN_IDENTIFIER, errorMessage);

  declareVariable();
  if (current->scopeDepth > 0) return 0;

  return identifierConstant(&parser.previous);
}

static void markInitialized() {
  current->locals[current->localCount-1].depth = current->scopeDepth;
}

static void binary(bool canAssign) {
  TokenType operatorType = parser.previous.type;
  ParseRule* rule = getRule(operatorType);
  parsePrecedence((Precedence)(rule->precedence + 1));

  switch(operatorType) {
    case TOKEN_BANG_EQUAL:    emitBytes(OP_EQUAL, OP_NOT); break;
    case TOKEN_EQUAL_EQUAL:   emitByte(OP_EQUAL); break;
    case TOKEN_GREATER:       emitByte(OP_GREATER); break;
    case TOKEN_GREATER_EQUAL: emitBytes(OP_LESS, OP_NOT); break;
    case TOKEN_LESS:          emitByte(OP_LESS); break;
    case TOKEN_LESS_EQUAL:    emitBytes(OP_GREATER, OP_NOT); break;
    case TOKEN_PLUS:       emitByte(OP_ADD); break;
    case TOKEN_MINUS:      emitByte(OP_SUBTRACT); break;
    case TOKEN_STAR:       emitByte(OP_MULTIPLY); break;
    case TOKEN_SLASH:      emitByte(OP_DIVIDE); break;
    default: return;
  }
}

static void literal(bool canAssign) {
  switch(parser.previous.type) {
    case TOKEN_FALSE: emitByte(OP_FALSE); break;
    case TOKEN_NIL:   emitByte(OP_NIL);   break;
    case TOKEN_TRUE:  emitByte(OP_TRUE);  break;
    default: return; // Unreachable
  }
}

static void grouping(bool canAssign) {
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(bool canAssign) {
  double value = strtod(parser.previous.start, NULL);
  emitConstant(NUMBER_VAL(value));
}

static void string(bool canAssign) {
  // The +1 ignores the starting quote
  // String's length without the quotes would be len - 2 
  emitConstant(OBJ_VAL(copyString(parser.previous.start + 1,
                                  parser.previous.length - 2)));
}

static void namedVariable(Token name, bool canAssign) {
  uint8_t getOp, setOp;
  int arg = resolveLocal(current, &name);
  
  if (arg != -1) {
    getOp = OP_GET_LOCAL;
    setOp = OP_SET_LOCAL;
  } else {
    arg = identifierConstant(&name);
    getOp = OP_GET_GLOBAL;
    setOp = OP_SET_GLOBAL;
  }


  // We look for an equals sign after the identifier
  // if we find one, instead of emitting code for a variable access, we 
  // compile the assigned value then emit an assignment instruction.
  if (canAssign && match(TOKEN_EQUAL)) {
    expression();
    emitBytes(setOp, (uint8_t)arg);
  } else {
    emitBytes(getOp, (uint8_t)arg);
  }
}

static void variable(bool canAssign) {
  namedVariable(parser.previous, canAssign);
}

static void unary(bool canAssign) {
  TokenType operatorType = parser.previous.type;

  // Compile the operand;
  parsePrecedence(PREC_UNARY);

  // Emit the operator instruction
  switch(operatorType) {
    case TOKEN_BANG:  emitByte(OP_NOT);    break;
    case TOKEN_MINUS: emitByte(OP_NEGATE); break;
    default: return;
  }
}

ParseRule rules[] = {
  [TOKEN_LEFT_PAREN]    = {grouping, NULL,   PREC_NONE},
  [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE}, 
  [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
  [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
  [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
  [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},
  [TOKEN_BANG]          = {unary,    NULL,   PREC_NONE},
  [TOKEN_BANG_EQUAL]    = {NULL,     binary, PREC_EQUALITY},
  [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EQUAL_EQUAL]   = {NULL,     binary, PREC_EQUALITY},
  [TOKEN_GREATER]       = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_GREATER_EQUAL] = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_LESS]          = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_LESS_EQUAL]    = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_IDENTIFIER]    = {variable, NULL,   PREC_NONE},
  [TOKEN_STRING]        = {string,   NULL,   PREC_NONE},
  [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
  [TOKEN_AND]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
  [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
  [TOKEN_OR]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
  [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};

static void parsePrecedence(Precedence precedence) {
  advance();
  ParseFn prefixRule = getRule(parser.previous.type)->prefix;
  if (prefixRule == NULL) {
    error("Expect expression.");
    return;
  }

  bool canAssign = precedence <= PREC_ASSIGNMENT;
  prefixRule(canAssign);
  while (precedence <= getRule(parser.current.type)->precedence) {
    advance();
    ParseFn infixRule = getRule(parser.previous.type)->infix;
    infixRule(canAssign);
  }

  if (canAssign && match(TOKEN_EQUAL)) {
    error("Invalid assignment target");
  }
}


static void defineVariable(uint8_t global) {
  // There is no code to create a local variable at runtime.
  // The VM has already executed the code for the variable initializer and that value is sitting on the top of the stack.
  // There's nothing to do, that temporary value on stack top becomes the local variable. Efficient af.
  if (current->scopeDepth > 0) {
    markInitialized();
    return;
  }

  emitBytes(OP_DEFINE_GLOBAL, global);
}

static ParseRule* getRule(TokenType type) {
  return &rules[type];
}

static void expression() {
  parsePrecedence(PREC_ASSIGNMENT);
}

static void block() {
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    declaration();
  }

  consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void varDeclaration() {
  uint8_t global = parseVariable("Expect variable name.");

  if (match(TOKEN_EQUAL)) {
    expression();
  } else {
    emitByte(OP_NIL);
  }

  consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

  defineVariable(global);
}

static void expressionStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
  emitByte(OP_POP);
}

static void printStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after value.");
  emitByte(OP_PRINT);
}

static void synchronize() {
  parser.panicMode = false;

  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON) return;

    switch(parser.current.type) {
      case TOKEN_CLASS:
      case TOKEN_FUN:
      case TOKEN_VAR:
      case TOKEN_FOR:
      case TOKEN_IF:
      case TOKEN_WHILE:
      case TOKEN_PRINT:
      case TOKEN_RETURN:
        return;
      
      default:
       ; // do nothing.
    }

    advance();
  }

  // we skip tokens indiscriminantly until we reach something that looks like a statement boundary. 
  // We recognize the boundary by looking for a preceding token that can end a statement, like a semicolon. Or 
  // we look for a statement token that begins a statement, usually one of the control flow statements or a declaration of keywords.
}

static void declaration() {
  if (match(TOKEN_VAR)) {
    varDeclaration();
  } else {
    statement();
  }

  if (parser.panicMode) synchronize();
}

static void statement() {
  /*
  statement -> exprStmt
           | forStmt
           | ifStmt
           | printStmt
           | returnStmt
           | whileStmt
           | blockStmt
  
  block     -> "{" declaration* "}"

  */
  if (match(TOKEN_PRINT)) {
    printStatement();
  }else if (match(TOKEN_LEFT_BRACE)) {
    beginScope();
    block();
    endScope();
  } 
  else {
    expressionStatement();
  }
}

// Main compilation logic

bool compile(const char* source, Chunk* chunk) {
  Compiler compiler;
  initScanner(source);
  compilingChunk = chunk;
  parser.hadError = false;
  parser.panicMode = false;

  advance();

  while(!match(TOKEN_EOF)) {
    declaration();
  }

  endCompiler();
  return !parser.hadError;
}