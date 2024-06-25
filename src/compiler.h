#ifndef iii_compiler_h
#define iii_compiler_h

#include "chunk.h"
#include "object.h"
#include "scanner.h"

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT,  // =
  PREC_OR,          // ||
  PREC_AND,         // &&
  PREC_EQUALITY,    // == !=
  PREC_COMPARISON,  // < > <= >=
  PREC_TERM,        // + -
  PREC_FACTOR,      // * /
  PREC_UNARY,       // ! -
  PREC_CALL,        // . ()
  PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool canAssign);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

ObjFunc *compile(const char *source);
static ParseRule *getRule(TokenType type);
static void statement();
static void declaration();

// mark all compiler roots for GC
void markCompilerRoots();

#endif
