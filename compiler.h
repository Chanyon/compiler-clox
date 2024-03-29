#ifndef clox_compiler_h
#define clox_compiler_h
#include "chunk.h"
#include "object.h"
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
  bool is_captured;
} Local;

typedef enum {
  TYPE_FUNCTION,
  TYPE_SCRIPT,
  TYPE_METHOD,
  TYPE_INITIALIZER,
} FunctionType;

typedef struct {
  bool is_local;
  int index;
} UpValue;

typedef struct Compiler {
  struct Compiler *enclosing;
  Local locals[UINT8_COUNT];
  int localCount;
  int scopeDepth;
  // function
  ObjFunction *function;
  FunctionType type;
  UpValue upvalues[UINT8_COUNT];
} Compiler;

typedef struct ClassCompiler {
  struct ClassCompiler *enclosing;
  bool has_super_class;
} ClassCompiler;

typedef struct Break {
  int patch_break; //修正jump位置
  struct Break *next;
} Break;

typedef struct Continue {
  int patch_continue;
  struct Continue *next;
} Continue;

ObjFunction *compiler(const char *source);
static void initCompiler(Compiler *compiler, FunctionType type);
static void error(const char *message);
// static void parsePrecedence(Precedence precedence);
static void advance();
static void binary(bool canAssign);
static void grouping(bool canAssign);
static ParseRule *getRule(TokenType type);
static void literal(bool canAssign);
static void string(bool canAssign);
static void variable(bool canAssign);
static void call(bool canAssign);
static uint8_t argumentList();
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
static void dot(bool canAssign);
static void whileStatement();
static void emitLoop(int loopStart);
static void forStatement();
static void funDeclaration();
static void function(FunctionType type);
static void returnStatement();
static void breakStatement();
static void continueStatement();
static void classDeclaration();
static void method();
static void this_(bool canAssign);
static void super_(bool canAssign);
static void namedVariable(Token name, bool canAssign);
static int addUpValue(Compiler *compiler, uint8_t local_idx, bool is_local);
static int resolveUpValue(Compiler *compiler, Token *name);
void markCompilerRoots();
#endif
