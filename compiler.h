#ifndef clox_compiler_h
#define clox_compiler_h
#include "chunk.h"
#include "scanner.h"
#include <stdint.h>

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

typedef void (*ParseFn)(bool canAssign);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

// local var
typedef struct {
  Token name;
  int depth;
} Local;

typedef struct {
  Local locals[UINT8_COUNT];
  int localCount;
  int scopeDepth;
} Compiler;

bool compiler(const char *source, Chunk *chunk);
static void initCompiler(Compiler *compiler);
static void error(const char *message);
// static void parsePrecedence(Precedence precedence);
static void advance();
static void binary(bool canAssign);
static void grouping(bool canAssign);
static ParseRule *getRule(TokenType type);
static void literal(bool canAssign);
static void string(bool canAssign);
static void variable(bool canAssign);
static void declaration();
static void statement();
static bool match(TokenType type);
static bool check(TokenType type);
static void printStatement();
static void expressionStatement();
static void synchronize();
static void varDeclaration();
static uint8_t parseVariable(const char *msg);
static void defineVariable(uint8_t global);
static uint8_t identifierConstant(Token *token);
static void block();
static void beginScope();
static void endScope();
static void declareVariable();
static void addLocal(Token name);
static bool identifierEqual(Token *a, Token *b);
static int resolveLocal(Compiler *compiler, Token *name);
static void markInitialized();
static void ifStatement();
static int emitJump(uint8_t instruction);
static void patchJump(int offset);
static void and_(bool canAssign);
static void or_(bool canAssign);
static void whileStatement();
static void emitLoop(int loopStart);
static void forStatement();
#endif
