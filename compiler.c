#include "compiler.h"
#include "chunk.h"
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
Chunk *compilingChunk;

static Chunk *currentChunk() { return compilingChunk; }

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
  prefixRule();
  while (precedence <= getRule(parser.current.type)->precedence) {
    advance();
    ParseFn infixRule = getRule(parser.previous.type)->infix;
    infixRule();
  }
}

static void expression() { parsePrecedence(PREC_ASSIGNMENT); }

static void declaration() {
  statement();
}

static void statement() {
  if (match(TOKEN_PRINT)) {
    printStatement();
  }
}

static void printStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "expect `;` after value.");
  emitByte(OP_PRINT);
}

static void number() {
  double value = strtod(parser.previous.start, NULL);
  // printf("line %d ", parser.previous.line);
  // if (value != 0) {
  //   printf("literal is %f\n", value);
  // }
  Value v = {VAL_NUMBER};
  v.as.number = value;
  emitConstant(v);
}

static void unary() {
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
    [TOKEN_IDENTIFIER] = {NULL, NULL, PREC_NONE},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, NULL, PREC_NONE},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, NULL, PREC_NONE},
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

static void grouping() {
  expression();
  consume(TOKEN_RIGHT_PAREN, "expect ')' after expression");
}

static void binary() {
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

static void literal() {
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

static void string() {
  emitConstant(OBJ_VAL(
      copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

bool compiler(const char *source, Chunk *chunk) {
  initScanner(source);
  compilingChunk = chunk;
  parser.hadError = false;
  parser.panicMode = false;

  advance();
  while (!match(TOKEN_EOF)) {
    declaration();
    advance();
  }
  endCompiler();
  return parser.hadError;
}
