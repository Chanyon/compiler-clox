#include "compiler.h"
#include "chunk.h"
#include "common.h"
#include "memory.h"
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
ClassCompiler *current_class = NULL;

static Chunk *currentChunk() { return &current->function->chunk; }

static void initCompiler(Compiler *compiler, FunctionType type) {
  compiler->enclosing = current;
  compiler->localCount = 0;
  compiler->scopeDepth = 0;
  compiler->type = type;
  compiler->function = newFunction();
  current = compiler;

  if (type == TYPE_FUNCTION) {
    current->function->name =
        copyString(parser.previous.start, parser.previous.length);
  }

  Local *local = &current->locals[current->localCount++];
  local->depth = 0;
  local->is_captured = false;

  if (type != TYPE_FUNCTION) {
    local->name.start = "this";
    local->name.length = 4;
  } else {
    local->name.start = "";
    local->name.length = 0;
  }
}

static void addLocal(Token name) {
  if (current->localCount == UINT8_COUNT) {
    error("Too many variables in function.");
    return;
  }
  fprintf(stderr, "=======> '%.*s' \n", name.length, name.start);
  Local local;
  local.name = name;
  // local.depth = current->scopeDepth;
  local.depth = -1;
  local.is_captured = false;
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

static void emitReturn() {
  if (current->type == TYPE_INITIALIZER) {
    emitBytes(OP_GET_LOCAL, 0);
    emitByte(OP_RETURN);
  } else {
    emitByte(OP_NIL);
    emitByte(OP_RETURN);
  }
}

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

static ObjFunction *endCompiler() {
  emitReturn();
  ObjFunction *function = current->function;
#ifdef DEBUG_PRINT_CODE
  if (!parser.hadError) {
    const char *fname =
        function->name != NULL ? function->name->chars : "<script>";
    disassembleChunk(currentChunk(), fname);
  }
#endif
  current = current->enclosing;
  return function;
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
  } else if (match(TOKEN_FUN)) {
    funDeclaration();
  } else if (match(TOKEN_CLASS)) {
    classDeclaration();
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
  } else if (match(TOKEN_IF)) {
    ifStatement();
  } else if (match(TOKEN_LEFT_BRACE)) {
    beginScope();
    block();
    endScope();
  } else if (match(TOKEN_WHILE)) {
    whileStatement();
  } else if (match(TOKEN_FOR)) {
    forStatement();
  } else if (match(TOKEN_RETURN)) {
    returnStatement();
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
  if (current->scopeDepth == 0) {
    return;
  }
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
    // bug?
    if (current->locals[current->localCount - 1].is_captured) {
      emitByte(OP_CLOSE_UPVALUE);
    } else {
      emitByte(OP_POP);
    }
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
  // }
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

  emitByte(OP_POP);
  statement();
  emitLoop(loopStart);

  patchJump(exitJump);
  emitByte(OP_POP);
}

static void forStatement() {
  beginScope();
  consume(TOKEN_LEFT_PAREN, "expect `(` after for.");
  // init , var foo = 0;
  if (match(TOKEN_SEMICOLON)) {
    // no initializer
  } else if (match(TOKEN_VAR)) {
    varDeclaration();
  } else {
    expressionStatement();
  }
  int loopStart = currentChunk()->count;
  // Condtion
  int exitJump = -1;
  if (!match(TOKEN_SEMICOLON)) {
    expression();
    consume(TOKEN_SEMICOLON, "expect `;` after loop condtion.");
    exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP); // pop loop Condtion value
  }
  // Increment clause
  if (!match(TOKEN_RIGHT_PAREN)) {
    int boodJump = emitJump(OP_JUMP);
    int incrementStart = currentChunk()->count;
    expression();
    emitByte(OP_POP);
    consume(TOKEN_RIGHT_PAREN, "expect `)` after for clauses.");
    emitLoop(loopStart);
    loopStart = incrementStart;
    patchJump(boodJump);
  }
  // block statement
  statement();

  emitLoop(loopStart);
  // if have Condtion statement
  if (exitJump != -1) {
    patchJump(exitJump);
    emitByte(OP_POP); // if condtion is false, pop condtion value
  }
  endScope();
}

// 函数被绑定到一个变量中
static void funDeclaration() {
  uint8_t global = parseVariable("expect function name.");
  markInitialized();
  // function body
  function(TYPE_FUNCTION);
  defineVariable(global);
}

static void function(FunctionType type) {
  Compiler compiler;
  initCompiler(&compiler, type);

  beginScope();
  consume(TOKEN_LEFT_PAREN, "expect `(` after function name.");
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      current->function->arity++;
      if (current->function->arity > 255) {
        errorAtCurrent("can't have more than 255 parameters.");
      }
      uint8_t constant = parseVariable("expect parameter name.");
      defineVariable(constant);
    } while (match(TOKEN_COMMA));
  }
  consume(TOKEN_RIGHT_PAREN, "expect `)` after function name.");
  endScope();
  consume(TOKEN_LEFT_BRACE, "expect `{` after function name.");
  block();

  ObjFunction *function = endCompiler();
  emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));
  for (int i = 0; i < function->upvalue_count; i++) {
    emitByte(compiler.upvalues[i].is_local ? 1 : 0);
    emitByte(compiler.upvalues[i].index);
  }
}

static Token syntheticToken(const char *text) {
  Token token;
  token.start = text;
  token.length = (int)strlen(text);
  return token;
}

static void classDeclaration() {
  consume(TOKEN_IDENTIFIER, "expect class name.");
  Token class_name = parser.previous;
  uint8_t nameConstant = identifierConstant(&parser.previous);
  declareVariable();

  emitBytes(OP_CLASS, nameConstant);
  defineVariable(
      nameConstant); // 在主体之前定义,用户就可以在类自己的方法主体中引用类本身

  ClassCompiler class_compiler;
  class_compiler.enclosing = current_class;
  class_compiler.has_super_class = false;
  current_class = &class_compiler;

  // extends SuperClassName
  if (match(TOKEN_LESS)) {
    consume(TOKEN_IDENTIFIER, "expect superclass name.");
    variable(false); // super name
    if (identifierEqual(&class_name, &parser.previous)) {
      error("A class can't inherit from itself.");
    }

    beginScope();
    addLocal(syntheticToken("super"));
    defineVariable(0);

    namedVariable(class_name, false);
    emitByte(OP_INHERIT);
    class_compiler.has_super_class = true;
  }

  namedVariable(class_name, false); // op_get_global
  consume(TOKEN_LEFT_BRACE, "expect `{` before class body.");
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    method();
  }
  consume(TOKEN_RIGHT_BRACE, "expect `}` after class body.");
  emitByte(OP_POP);

  if (class_compiler.has_super_class) {
    endScope();
  }
  current_class = current_class->enclosing;
}

static void method() {
  consume(TOKEN_IDENTIFIER, "expect method name.");
  uint8_t constant_idx = identifierConstant(&parser.previous);
  // function body
  FunctionType type = TYPE_METHOD;

  if (parser.previous.length == 4 &&
      (memcmp(parser.previous.start, "init", 4) == 0)) {
    type = TYPE_INITIALIZER;
  }
  function(type);
  emitBytes(OP_METHOD, constant_idx);
}

static void this_(bool canAssign) {
  if (current_class == NULL) {
    error("can't use `this` outside of a class.");
    return;
  }
  variable(false);
}

static void super_(bool canAssign) {
  if (current_class == NULL) {
    error("can't use `super` outsite of a class.");
  }
  if (!current_class->has_super_class) {
    error("can't use `super` in a class with no superclass.");
  }
  consume(TOKEN_DOT, "expect `.` after `super`.");
  consume(TOKEN_IDENTIFIER, "expect supercalss method name.");
  uint8_t name_idx = identifierConstant(&parser.previous);
  namedVariable(syntheticToken("this"), false);

  if (match(TOKEN_LEFT_PAREN)) {
    uint8_t args = argumentList();
    namedVariable(syntheticToken("super"),
                  false); // bug: OP_GET_GLOBAL => OP_GET_UPVALUE
    emitBytes(OP_SUPER_INVOKE, name_idx);
    emitByte(args);
  } else {
    namedVariable(syntheticToken("super"),
                  false); // bug: OP_GET_GLOBAL => OP_GET_UPVALUE
    emitBytes(OP_GET_SUPER, name_idx);
  }
}

static void returnStatement() {
  // 在任何函数之外使用return都会报错
  if (current->type == TYPE_SCRIPT) {
    error("can't return from top-level code.");
  }
  if (match(TOKEN_SEMICOLON)) {
    emitReturn();
  } else {
    if (current->type == TYPE_INITIALIZER) {
      error("can't return a value an initizlizer function.");
    }
    expression();
    consume(TOKEN_SEMICOLON, "expect `;` after return value.");
    emitByte(OP_RETURN);
  }
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
    [TOKEN_LEFT_PAREN] = {grouping, call, PREC_CALL},
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
    [TOKEN_DOT] = {NULL, dot, PREC_PRIMARY},
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
    [TOKEN_SUPER] = {super_, NULL, PREC_NONE},
    [TOKEN_THIS] = {this_, NULL, PREC_NONE},
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
  namedVariable(parser.previous, canAssign);
}

static void namedVariable(Token name, bool canAssign) {
  uint8_t getOp, setOp;
  int arg = resolveLocal(current, &name);

  if (arg != -1) {
    getOp = OP_GET_LOCAL;
    setOp = OP_SET_LOCAL;
  } else if ((arg = resolveUpValue(current, &name)) != -1) {
    //! bug local
    getOp = OP_GET_UPVALUE;
    setOp = OP_SET_UPVALUE;
  } else {
    arg = identifierConstant(&name);
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

//! bug
static int addUpValue(Compiler *compiler, uint8_t local_idx, bool is_local) {
  int upvalue_count = compiler->function->upvalue_count;

  for (int i = 0; i < upvalue_count; i++) {
    UpValue *upvalue = &compiler->upvalues[i];
    if (upvalue->index == local_idx && upvalue->is_local == is_local) {
      return i;
    }
  }

  if (upvalue_count == UINT8_COUNT) {
    error("too many closure variable in function.");
    return 0;
  }

  compiler->upvalues[upvalue_count].is_local = is_local;
  compiler->upvalues[upvalue_count].index = local_idx;

  return compiler->function->upvalue_count++;
}

static int resolveUpValue(Compiler *compiler, Token *name) {
  //! bug here.
  if (compiler->enclosing == NULL) {
    return -1;
  }
  int local_idx = resolveLocal(compiler->enclosing, name);
  if (local_idx != -1) {
    compiler->enclosing->locals[local_idx].is_captured = true;
    return addUpValue(compiler, (uint8_t)local_idx, true);
  }

  int upvalue = resolveUpValue(compiler->enclosing, name);
  if (upvalue != -1) {
    return addUpValue(compiler, (uint8_t)upvalue, false);
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

static void dot(bool canAssign) {
  consume(TOKEN_IDENTIFIER, "expect property name after `.`.");
  uint8_t nameCanstant = identifierConstant(&parser.previous);

  if (canAssign && match(TOKEN_EQUAL)) {
    expression();
    emitBytes(OP_SET_PROPERTY, nameCanstant);
  } else if (match(TOKEN_LEFT_PAREN)) {
    uint8_t argCount = argumentList();
    emitBytes(OP_INVOKE, nameCanstant);
    emitByte(argCount);
  } else {
    emitBytes(OP_GET_PROPERTY, nameCanstant);
  }
}

// foo(1,2);
static void call(bool canAssign) {
  uint8_t argCount = argumentList();
  emitBytes(OP_CALL, argCount);
}

static uint8_t argumentList() {
  uint8_t argCount = 0;
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      expression();
      if (argCount == 255) {
        error("can't have more than 255 arguments.");
      }
      argCount += 1;
    } while (match(TOKEN_COMMA));
  }
  consume(TOKEN_RIGHT_PAREN, "expect `)` after arguments.");
  return argCount;
}

void markCompilerRoots() {
  Compiler *compiler = current;
  // printf("====compiler start====\n");
  while (compiler != NULL) {
    markObject((Obj *)compiler->function);
    compiler = compiler->enclosing;
  }
  // printf("====compiler end====\n");
}

ObjFunction *compiler(const char *source) {
  initScanner(source);
  Compiler compiler;
  initCompiler(&compiler, TYPE_SCRIPT);
  parser.hadError = false;
  parser.panicMode = false;

  advance();
  while (!match(TOKEN_EOF)) {
    declaration();
  }
  ObjFunction *function = endCompiler();
  return parser.hadError ? NULL : function;
}
