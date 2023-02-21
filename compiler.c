#include "compiler.h"
#include "chunk.h"
#include "common.h"
#include "object.h"
#include "scanner.h"
#include "value.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
  Token current;
  Token previous;
  bool hadError;
  bool panicMode;
} Parser;

Parser parser;
Compiler *current = NULL;
Chunk *compilingChunk;

static Chunk *currentChunk() { return compilingChunk; }

static void initCompiler(Compiler *compiler) {
  compiler->localCount = 0;
  compiler->scopeDepth = 0;
  current = compiler;
}

static void addLocal(Token name) {
  if (current->localCount == UINT8_COUNT) {
    error("Too many variables in function.");
    return;
  }
  Local local;
  local.name = name;
  // local.depth = current->scopeDepth;
  local.depth = -1;
  current->locals[current->localCount] = local;
  current->localCount += 1;
}

static void errorAt(Token *token, const char *msg) {
  if (parser.panicMode) {
    return;
  }
  parser.panicMode = true;

  fprintf(stderr, "[line %d] Error", token->line);
  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " end.");
  } else if (token->type == TOKEN_ERROR) {
    // nothing
  } else {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", msg);
  parser.hadError = true;
}

static void errorAtCurrent(const char *messaeg) {
  errorAt(&parser.current, messaeg);
}

static void error(const char *message) { errorAt(&parser.previous, message); }

// painc synchronize
static void synchronize() {
  parser.panicMode = false;
  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON) {
      return;
    }
    switch (parser.current.type) {
    case TOKEN_CLASS:
    case TOKEN_FUN:
    case TOKEN_FOR:
    case TOKEN_VAR:
    case TOKEN_IF:
    case TOKEN_WHILE:
    case TOKEN_PRINT:
    case TOKEN_RETURN:
      return;
    default:
      break;
    }
    advance();
  }
}

// advance 前进
static void advance() {
  parser.previous = parser.current;
  for (;;) {
    parser.current = scanToken();
    if (parser.current.type != TOKEN_ERROR) {
      break;
    }
    errorAtCurrent(parser.current.start);
  }
}

// 消费 token
static void consume(TokenType type, const char *message) {
  // skip current token
  if (parser.current.type == type) {
    advance();
  } else {
    errorAtCurrent(message);
  }
}

static void emitByte(uint8_t byte) {
  writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t opcode, uint8_t byte2) {
  emitByte(opcode);
  emitByte(byte2);
}

static int emitJump(uint8_t instruction) {
  emitByte(instruction);
  emitByte(0xff); // u16
  emitByte(0xff);
  return currentChunk()->count - 2;
}
// 88 -81 -2 = 5
static void patchJump(int offset) {
  int jump = currentChunk()->count - offset - 2;
  if (jump > UINT16_MAX) {
    error("Too much code to jump over.");
  }
  currentChunk()->code[offset] = (jump >> 8) & 0xff;
  currentChunk()->code[offset + 1] = jump & 0xff;
}

static void emitLoop(int loopStart) {
  emitByte(OP_LOOP);
  int offset = currentChunk()->count - loopStart + 2;
  if (offset > UINT16_MAX) {
    error("loop body too large");
  }
  emitByte((offset >> 8) & 0xff);
  emitByte(offset & 0xff);
}

static void emitReturn() { emitByte(OP_RETURN); }

static uint8_t makeConstant(Value value) {
  int constantIndex = addConstant(currentChunk(), value);
  if (constantIndex > UINT8_MAX) {
    error("too many constant in one chunk.");
    return 0;
  }

  return (uint8_t)constantIndex;
}

static void emitConstant(Value value) {
  emitBytes(OP_CONSTANT, makeConstant(value));
}

static void endCompiler() {
  emitReturn();
#ifdef DEBUG_PRINT_CODE
  if (!parser.hadError) {
    disassembleChunk(currentChunk(), "compiler code");
  }
#endif
}

static bool match(TokenType type) {
  if (!check(type)) {
    return false;
  } else {
    advance();
    return true;
  }
}

static bool check(TokenType type) { return parser.current.type == type; }

static void parsePrecedence(Precedence precedence) {
  // 1 ; eof
  // p c
  advance();
  ParseFn prefixRule = getRule(parser.previous.type)->prefix;
  if (prefixRule == NULL) {
    error("no match prefix function");
    return;
  }
  bool canAssign = precedence <= PREC_ASSIGNMENT;
  prefixRule(canAssign);

  while (precedence <= getRule(parser.current.type)->precedence) {
    advance();
    ParseFn infixRule = getRule(parser.previous.type)->infix;
    infixRule(canAssign);
  }

  if (!canAssign && match(TOKEN_EQUAL)) {
    // var a=1; var b = 2 + a = 1;
    error("Invalid assignment target. example `var foo = 1 + bar = 1;`");
  }
}

static void expression() { parsePrecedence(PREC_ASSIGNMENT); }

static void declaration() {
  if (match(TOKEN_VAR)) {
    varDeclaration();
  } else if (match(TOKEN_IF)) {
    ifStatement();
  } else if (match(TOKEN_LEFT_BRACE)) {
    beginScope();
    block();
    endScope();
  } else if (match(TOKEN_WHILE)) {
    whileStatement();
  } else {
    statement();
  }
  if (parser.panicMode) {
    synchronize();
  }
}

static void statement() {
  if (match(TOKEN_PRINT)) {
    printStatement();
  } else {
    expressionStatement();
  }
}

static void printStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "expect `;` after value.");
  emitByte(OP_PRINT);
}

static void expressionStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "expect `;` after expression.");
  emitByte(OP_POP);
}

static void varDeclaration() {
  // parseVariable() return constant index
  uint8_t global = parseVariable("expect variable name.");
  if (match(TOKEN_EQUAL)) {
    expression();
  } else {
    emitByte(OP_NIL);
  }
  consume(TOKEN_SEMICOLON, "expect `;` variable declaration.");
  defineVariable(global);
}

static uint8_t parseVariable(const char *msg) {
  consume(TOKEN_IDENTIFIER, msg);
  declareVariable();
  if (current->scopeDepth > 0) {
    return 0;
  }
  // global var
  return identifierConstant(&parser.previous);
}

static uint8_t identifierConstant(Token *token) {
  return makeConstant(OBJ_VAL(copyString(token->start, token->length)));
}

// return bytecode
static void defineVariable(uint8_t global) {
  if (current->scopeDepth > 0) {
    markInitialized();
    return;
  }
  emitBytes(OP_DEFINE_GLOBAL, global);
}

static void markInitialized() {
  current->locals[current->localCount - 1].depth = current->scopeDepth;
}

// local variable
static void declareVariable() {
  if (current->scopeDepth == 0) {
    return;
  }
  Token *name = &parser.previous;
  for (int i = current->localCount - 1; i >= 0; i--) {
    Local *local = &current->locals[i];
    if (local->depth != -1 && local->depth < current->scopeDepth) {
      break;
    }
    if (identifierEqual(name, &local->name)) {
      error("Already a variable with this name in this scope.");
    }
  }
  addLocal(*name);
}

static bool identifierEqual(Token *a, Token *b) {
  if (a->length != b->length) {
    return false;
  }
  return memcmp(a->start, b->start, a->length) == 0;
}

static void block() {
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    declaration();
  }
  consume(TOKEN_RIGHT_BRACE, "expect `}` after block.");
}

static void beginScope() { current->scopeDepth += 1; }

static void endScope() {
  current->scopeDepth -= 1;
  while (current->localCount > 0 &&
         current->locals[current->localCount - 1].depth > current->scopeDepth) {
    emitByte(OP_POP);
    current->localCount -= 1;
  }
}

static void ifStatement() {
  consume(TOKEN_LEFT_PAREN, "expect `(` after if.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "expect `)` after condition.");

  int thenJump = emitJump(OP_JUMP_IF_FALSE);
  // then
  emitByte(OP_POP);
  statement();

  int elseJump = emitJump(OP_JUMP);
  // 回填正确的跳转操作数
  patchJump(thenJump);

  // else
  emitByte(OP_POP);
  if (match(TOKEN_ELSE)) {
    statement();
  }
  patchJump(elseJump);
}

static void whileStatement() {
  // 往回跳位置
  int loopStart = currentChunk()->count;
  consume(TOKEN_LEFT_PAREN, "expect `(` after while.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "expect `)` after while.");

  int exitJump = emitJump(OP_JUMP_IF_FALSE);
  consume(TOKEN_RIGHT_BRACE, "expect `{` after right paren `)`.");
  emitByte(OP_POP);
  while (!match(TOKEN_RIGHT_BRACE) && !match(TOKEN_EOF)) {
    statement();
  }
  emitLoop(loopStart);
  // consume(TOKEN_RIGHT_BRACE, "expect `}` after while.");
  patchJump(exitJump);
  emitByte(OP_POP);
}

static void number(bool canAssign) {
  double value = strtod(parser.previous.start, NULL);
  // printf("line %d ", parser.previous.line);
  // if (value != 0) {
  //   printf("literal is %f\n", value);
  // }
  Value v = {VAL_NUMBER};
  v.as.number = value;
  emitConstant(v);
}

static void unary(bool canAssign) {
  TokenType operatorType = parser.previous.type;
  parsePrecedence(PREC_UNARY);

  switch (operatorType) {
  case TOKEN_MINUS:
    emitByte(OP_NEGATE);
    break;
  case TOKEN_BANG:
    emitByte(OP_NOT);
    break;
  default:
    return;
  }
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, NULL, PREC_NONE},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, NULL, PREC_NONE},
    [TOKEN_BANG] = {unary, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, and_, PREC_AND},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, or_, PREC_OR},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

static ParseRule *getRule(TokenType type) { return &rules[type]; }

static void grouping(bool canAssign) {
  expression();
  consume(TOKEN_RIGHT_PAREN, "expect ')' after expression");
}

static void binary(bool canAssign) {
  TokenType operatorType = parser.previous.type;
  ParseRule *rule = getRule(operatorType);
  parsePrecedence((Precedence)(rule->precedence + 1));

  switch (operatorType) {
  case TOKEN_PLUS:
    emitByte(OP_ADD);
    break;
  case TOKEN_MINUS:
    emitByte(OP_SUBTRACT);
    break;
  case TOKEN_STAR:
    emitByte(OP_MULTIPLY);
    break;
  case TOKEN_SLASH:
    emitByte(OP_DIVIDE);
    break;
  case TOKEN_BANG_EQUAL:
    emitBytes(OP_EQUAL, OP_NOT);
    break;
  case TOKEN_EQUAL_EQUAL:
    emitByte(OP_EQUAL);
    break;
  case TOKEN_LESS:
    emitByte(OP_LESS);
    break;
  case TOKEN_LESS_EQUAL:
    emitBytes(OP_GREATER, OP_NOT);
    break;
  case TOKEN_GREATER:
    emitByte(OP_GREATER);
    break;
  case TOKEN_GREATER_EQUAL:
    emitBytes(OP_LESS, OP_NOT);
    break;
  default:
    return;
  }
}

static void literal(bool canAssign) {
  switch (parser.previous.type) {
  case TOKEN_TRUE:
    emitByte(OP_TRUE);
    break;
  case TOKEN_FALSE:
    emitByte(OP_FALSE);
    break;
  case TOKEN_NIL:
    emitByte(OP_NIL);
    break;
  default:
    return;
  }
}

static void string(bool canAssign) {
  emitConstant(OBJ_VAL(
      copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

// identifier parse
static void variable(bool canAssign) {
  uint8_t getOp, setOp;
  int arg = resolveLocal(current, &parser.previous);

  if (arg != -1) {
    getOp = OP_GET_LOCAL;
    setOp = OP_SET_LOCAL;
  } else {
    arg = identifierConstant(&parser.previous);
    getOp = OP_GET_GLOBAL;
    setOp = OP_SET_GLOBAL;
  }

  if (canAssign && match(TOKEN_EQUAL)) {
    expression();
    emitBytes(setOp, (uint8_t)arg);
  } else {
    emitBytes(getOp, (uint8_t)arg);
  }
}

static int resolveLocal(Compiler *compiler, Token *name) {
  for (int i = current->localCount - 1; i >= 0; i--) {
    Local *local = &compiler->locals[i];
    if (identifierEqual(name, &local->name)) {
      if (local->depth == -1) {
        error("can't read local variable in it's own initializer.");
      }
      return i;
    }
  }
  return -1;
}

static void and_(bool canAssign) {
  // 如果左边值为假则留在栈顶，跳过右值
  // 如果左边值为真则弹出栈顶，计算右值留在栈顶
  int endJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  parsePrecedence(PREC_AND);
  patchJump(endJump);
}

static void or_(bool canAssign) {
  int elseJump = emitJump(OP_JUMP_IF_FALSE);
  int endJump = emitJump(OP_JUMP);
  patchJump(elseJump);
  // right value
  emitByte(OP_POP);
  parsePrecedence(PREC_OR);
  patchJump(endJump);
}

bool compiler(const char *source, Chunk *chunk) {
  initScanner(source);
  Compiler compiler;
  initCompiler(&compiler);
  compilingChunk = chunk;
  parser.hadError = false;
  parser.panicMode = false;

  advance();
  while (!match(TOKEN_EOF)) {
    declaration();
  }
  endCompiler();
  return parser.hadError;
}
