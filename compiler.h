#ifndef clox_compiler_h
#define clox_compiler_h
#include "chunk.h"
#include "scanner.h"

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT, // =
  PREC_OR,         // or
  PREC_AND,        // and
  PREC_EQUALITY,   // == !=
  PREC_COMPARISON, // < > <= >=
  PREC_TERM,       // + -
  PREC_FACTOR,     // * /
  PREC_UNARY,      // ! -
  PREC_CALL,       // . ()
  PREC_PRIMARY     // a.b | arr[0]
} Precedence;

typedef void (*ParseFn)();

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

bool compiler(const char *source, Chunk *chunk);
// static void parsePrecedence(Precedence precedence);
static void binary();
static void grouping();
static ParseRule *getRule(TokenType type);
static void literal();
static void string();
static void declaration();
static void statement();
static bool match(TokenType type);
static bool check(TokenType type);
static void printStatement();
#endif
